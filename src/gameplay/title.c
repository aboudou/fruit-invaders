#include "title.h"

#include "../graphics/bigfont.h"
#include "../graphics/decor.h"
#include "../graphics/font.h"
#include "../graphics/screen.h"
#include "../graphics/sprites.h"
#include "../sound/sound.h"

#include <cbm.h> /* cbm_k_getin(): KERNAL keyboard-buffer read */

/* KERNAL jiffy clock, low byte: incremented by the IRQ at the PAL raster
 * rate (50/sec). Paces the fruit wobble, the fruit ticker scroll and the
 * start-prompt blink -- the real game loop will need its own timing story
 * (see CLAUDE.md, "Levels"). */
#define JIFFY_LOW ((volatile unsigned char *)0x00A2)

/* Screen rows (0-based, 23 rows available -- see screen.h) for each block
 * of the layout, chosen to spread the content over the 22x23 screen with
 * the start prompt near the bottom, arcade-title style. TITLE_ROW spans
 * two rows (TITLE_ROW, TITLE_ROW+1) -- bigfont_print()'s letters are 1
 * column wide x 2 rows tall (see bigfont.h) -- leaving rows 4-6 as a gap
 * before the fruit ticker. Rows 0 and MARQUEE_ROW_BOTTOM (the top/bottom
 * edges) belong to the chasing-light border (see draw_marquee() below);
 * every other gap row not named here is background for the starfield (see
 * draw_stars()). CONTROLS_ROW/MUTE_HINT_ROW stay full sentences (see
 * draw_static()) -- an icon-only version was tried and reverted, plain text
 * read more clearly. */
#define TITLE_ROW           2
#define FRUITS_ROW          7
#define CONTROLS_ROW        13
#define MUTE_HINT_ROW       15
#define MUTE_INDICATOR_ROW  16
#define START_ROW           18
#define MARQUEE_ROW_TOP     0
#define MARQUEE_ROW_BOTTOM  (SCREEN_ROWS - 1)

#define ANIM_JIFFIES 25 /* ~0.5s at 50Hz: fruit wobble and prompt blink share one pace */

/* Fruit ticker: the four fruit sprites drift left-to-right and wrap back in
 * from the left once they scroll off the right edge, an "infinite scroll"
 * across the full 22-column screen width (see CLAUDE.md-requested
 * behavior). FRUIT_COL_BASE/CODE/COLOR are the same per-type
 * code/color pairing draw_static() used to place each fruit before this
 * change (parallel arrays, same pattern as game.c's fruit_row_code/color).
 * The four base columns (2, 7, 12, 17) are already spaced 5 apart -- 2
 * columns of sprite + 3 of gap -- for a total span of exactly SCREEN_COLS
 * (22): adding the same scroll_offset to every base column and wrapping
 * modulo SCREEN_COLS therefore moves all four sprites together and always
 * preserves that spacing, so they can never collide however far they
 * scroll or wrap. Scrolling only ever repositions each fruit's *existing*
 * character codes into different screen cells (screen_put, cheap) -- it
 * never touches character memory, which is exactly what the fruit wobble
 * animation *does* touch (see sprites_set_apple_frame() and friends,
 * called independently from the main loop below on their own pace) -- so
 * the two operate on disjoint resources (screen matrix vs. character
 * memory) and can't step on each other: whichever wobble frame is
 * currently loaded is simply whatever draw_fruits()/scroll_tick() below
 * paints at the fruit's new position. */
#define SCROLL_STEP_JIFFIES 6 /* ~120ms at 50Hz per column step */

static const unsigned char FRUIT_COL_BASE[4] = {2, 7, 12, 17};
static const unsigned char FRUIT_CODE[4] = {
    CHAR_APPLE_TL, CHAR_CARROT_TL, CHAR_GRAPES_TL, CHAR_PEPPER_TL,
};
static const unsigned char FRUIT_COLOR[4] = {
    APPLE_COLOR, CARROT_COLOR, GRAPES_COLOR, PEPPER_COLOR,
};

static unsigned char scroll_offset;
static unsigned char last_scroll_jiffy;

/* screen_put_quad()/screen_clear_quad() (see screen.h) assume both columns
 * of a 2x2 sprite are contiguous and on-screen -- true for every other
 * sprite in the game, which only ever bounces off screen edges (see
 * game.c's fruit grid) rather than wrapping past them. The ticker needs the
 * one case those don't handle: a sprite whose right column has scrolled
 * past the last screen column and wrapped back to column 0, so its two
 * columns are placed independently, each wrapped modulo SCREEN_COLS. */
static void put_quad_wrapped(unsigned char row, unsigned char col,
                              unsigned char first_code, unsigned char color) {
    unsigned char col1 = (unsigned char)((col + 1) % SCREEN_COLS);

    screen_put(row, col, first_code, color);
    screen_put(row, col1, (unsigned char)(first_code + 1), color);
    screen_put(row + 1, col, (unsigned char)(first_code + 2), color);
    screen_put(row + 1, col1, (unsigned char)(first_code + 3), color);
}

static void clear_quad_wrapped(unsigned char row, unsigned char col) {
    unsigned char col1 = (unsigned char)((col + 1) % SCREEN_COLS);

    screen_put(row, col, CHAR_BLANK, COLOR_BLACK);
    screen_put(row, col1, CHAR_BLANK, COLOR_BLACK);
    screen_put(row + 1, col, CHAR_BLANK, COLOR_BLACK);
    screen_put(row + 1, col1, CHAR_BLANK, COLOR_BLACK);
}

static void draw_fruits(unsigned char offset) {
    unsigned char i;

    for (i = 0; i < 4; i++) {
        put_quad_wrapped(FRUITS_ROW, (unsigned char)((FRUIT_COL_BASE[i] + offset) % SCREEN_COLS),
                          FRUIT_CODE[i], FRUIT_COLOR[i]);
    }
}

static void erase_fruits(unsigned char offset) {
    unsigned char i;

    for (i = 0; i < 4; i++) {
        clear_quad_wrapped(FRUITS_ROW, (unsigned char)((FRUIT_COL_BASE[i] + offset) % SCREEN_COLS));
    }
}

/* Advances the ticker by one column every SCROLL_STEP_JIFFIES, independent
 * of the wobble/blink pace above -- called from wait_jiffies_or_space()'s
 * tight busy-wait loop (same reasoning as that function's own
 * sound_music_tick() call: cheap enough not to delay Space detection, and
 * that loop is the only one running often enough to drive a timer finer
 * than ANIM_JIFFIES). */
static void scroll_tick(void) {
    unsigned char now = *JIFFY_LOW;

    if ((unsigned char)(now - last_scroll_jiffy) < SCROLL_STEP_JIFFIES) {
        return;
    }
    last_scroll_jiffy = now;
    erase_fruits(scroll_offset);
    scroll_offset = (unsigned char)((scroll_offset + 1) % SCREEN_COLS);
    draw_fruits(scroll_offset);
}

/* Marquee border: a row of CHAR_BULB tiles across the top and bottom edges,
 * color-cycled through the same four fruit colors bigfont_print() already
 * uses for the title letters (see sprites.h) so the palette reads as one
 * consistent set rather than an unrelated new one. The two rows chase in
 * opposite directions (top rightward, bottom leftward, both driven by the
 * same phase) purely by indexing the palette from opposite ends of the row
 * -- an arcade-cabinet "running lights" effect for the price of one extra
 * screen_put() pair per column per tick, no extra character codes or timer
 * needed beyond the phase counter the caller already advances once per
 * ANIM_JIFFIES tick (see title_screen_run()). */
static const unsigned char MARQUEE_PALETTE[4] = {
    APPLE_COLOR, CARROT_COLOR, GRAPES_COLOR, PEPPER_COLOR,
};

static void draw_marquee(unsigned char phase) {
    unsigned char col;

    for (col = 0; col < SCREEN_COLS; col++) {
        screen_put(MARQUEE_ROW_TOP, col, CHAR_BULB,
                   MARQUEE_PALETTE[(unsigned char)((col + phase) % 4)]);
        screen_put(MARQUEE_ROW_BOTTOM, col, CHAR_BULB,
                   MARQUEE_PALETTE[(unsigned char)((SCREEN_COLS - 1 - col + phase) % 4)]);
    }
}

/* Starfield: a fixed scatter of CHAR_STAR tiles filling the otherwise empty
 * background rows (between the title/ticker/icon rows above -- picked to
 * avoid every row those already occupy, so this never overwrites them).
 * Twinkle is a per-star color swap between white and cyan rather than an
 * on/off blink, so the field never looks like it's vanishing -- driven by
 * XORing each star's own index with the caller's shared blink flag (the
 * same one the start prompt already toggles every ANIM_JIFFIES tick) so
 * roughly half the stars swap on any given tick and half swap on the next,
 * instead of the whole field flipping in lockstep. */
#define STAR_COUNT 20

static const unsigned char STAR_ROW[STAR_COUNT] = {
    1, 1, 4, 4, 5, 6, 6, 9, 9, 10, 11, 11, 12, 12, 17, 17, 19, 20, 20, 21,
};
static const unsigned char STAR_COL[STAR_COUNT] = {
    2, 19, 1, 20, 10, 3, 18, 2, 19, 10, 4, 17, 8, 13, 2, 20, 9, 3, 18, 10,
};

static void draw_stars(unsigned char blink) {
    unsigned char i;

    for (i = 0; i < STAR_COUNT; i++) {
        screen_put(STAR_ROW[i], STAR_COL[i], CHAR_STAR,
                   ((i ^ blink) & 1) ? COLOR_WHITE : COLOR_CYAN);
    }
}

/* Shows or hides the "MUSIC OFF" indicator (see MUTE_INDICATOR_ROW) to
 * match the title tune's current mute state -- drawn in COLOR_BLACK
 * (invisible against the black screen background, same trick as the
 * blinking start prompt below) when unmuted, so no separate clear call is
 * needed. */
static void update_mute_indicator(void) {
    font_print(MUTE_INDICATOR_ROW, 6, "MUSIC OFF",
               sound_music_is_muted() ? COLOR_RED : COLOR_BLACK);
}

/* Waits up to n jiffies, checking the keyboard buffer on every pass of the
 * busy loop (much more often than once per jiffy) so a Space press is
 * caught promptly instead of only being noticed on the next animation
 * tick. Returns early (1) the moment Space is seen, or 0 after the full
 * wait with no Space press. Also ticks the title tune (see sound.c,
 * sound_music_tick()) on every pass -- cheap enough not to delay Space
 * detection, and this is the only loop that runs often enough to drive a
 * note-length timer of its own -- and toggles mute on M, title-screen-only
 * per CLAUDE.md's request (game.c never calls any sound_music_*
 * function). Also ticks the fruit ticker scroll (see scroll_tick() above)
 * for the same reason -- its own SCROLL_STEP_JIFFIES pace is finer than
 * ANIM_JIFFIES, so it needs to be driven from here rather than once per
 * outer loop iteration. */
static unsigned char wait_jiffies_or_space(unsigned char n) {
    unsigned char start = *JIFFY_LOW;
    unsigned char key;

    while ((unsigned char)(*JIFFY_LOW - start) < n) {
        sound_music_tick();
        scroll_tick();
        key = cbm_k_getin();
        if (key == ' ') {
            return 1;
        }
        if (key == 'M') {
            sound_music_toggle_mute();
            update_mute_indicator();
        }
    }
    return 0;
}

/* Everything that's drawn once and never changes again: the marquee border
 * and starfield (their own animation only recolors these same cells, see
 * draw_marquee()/draw_stars() and the main loop below), title, the four
 * fruit sprites at their starting ticker position (they then scroll AND
 * animate in place -- see scroll_tick() and CLAUDE.md, "movement vs.
 * animation"), and the controls reminder. Only the start prompt blinks and
 * is redrawn from the main loop below. */
static void draw_static(void) {
    draw_marquee(0);
    draw_stars(0);

    bigfont_print(TITLE_ROW, 4, "FRUIT INVADERS");

    draw_fruits(scroll_offset);

    font_print(CONTROLS_ROW, 1, "S/D MOVE  SPACE FIRE", COLOR_CYAN);
    font_print(MUTE_HINT_ROW, 5, "M MUTE MUSIC", COLOR_CYAN);
}

void title_screen_run(void) {
    unsigned char frame = 0;
    unsigned char blink = 0;
    unsigned char marquee_phase = 0;

    scroll_offset = 0;
    last_scroll_jiffy = *JIFFY_LOW;

    draw_static();
    sound_music_start();
    update_mute_indicator(); /* sound_music_start() always resets to unmuted -- hide it */

    for (;;) {
        if (wait_jiffies_or_space(ANIM_JIFFIES)) {
            sound_music_stop();
            return;
        }

        frame ^= 1;
        sprites_set_apple_frame(frame);
        sprites_set_carrot_frame(frame);
        sprites_set_grapes_frame(frame);
        sprites_set_pepper_frame(frame);

        marquee_phase = (unsigned char)((marquee_phase + 1) % 4);
        draw_marquee(marquee_phase);

        blink ^= 1;
        draw_stars(blink);
        font_print(START_ROW, 1, "PRESS SPACE TO START", blink ? COLOR_WHITE : COLOR_BLACK);
        /* Chevrons framing the prompt, blinking in lockstep with it --
         * point inward (right-pointing on the left, left-pointing on the
         * right) so they read as bracketing the text rather than as a
         * scroll/move affordance. */
        screen_put(START_ROW, 0, CHAR_ARROW_R, blink ? SHOT_COLOR : COLOR_BLACK);
        screen_put(START_ROW, 21, CHAR_ARROW_L, blink ? SHOT_COLOR : COLOR_BLACK);
    }
}
