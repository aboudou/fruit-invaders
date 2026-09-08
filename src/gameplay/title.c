#include "title.h"

#include "../graphics/bigfont.h"
#include "../graphics/decor.h"
#include "../graphics/font.h"
#include "../graphics/screen.h"
#include "../graphics/sprites.h"
#include "../graphics/starfield.h"
#include "../sound/sound.h"
#include "help.h"
#include "lang.h"

#include <cbm.h>    /* cbm_k_getin(): KERNAL keyboard-buffer read, and CH_F1 (via vic20.h) */
#include <string.h> /* strlen(): centers the start prompt within its field, see below */

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
 * edges) belong to the chasing-light border (see decor_draw_marquee());
 * every other gap row not named here is background for the starfield (see
 * starfield_draw_all()). The controls themselves are no longer spelled out
 * here -- HELP_HINT_ROW just points at the dedicated help screen instead
 * (F1, see help.c) -- an earlier version had two full sentences on this
 * row and the next (and, before that, an icon-only version), both dropped
 * once the help screen took over listing every control. */
#define TITLE_ROW           2
#define FRUITS_ROW          7
#define HELP_HINT_ROW       13
#define MUTE_INDICATOR_ROW  16
#define START_ROW           18
#define MARQUEE_ROW_TOP     0
#define MARQUEE_ROW_BOTTOM  (SCREEN_ROWS - 1)

#define ANIM_JIFFIES 25 /* ~0.5s at 50Hz: fruit wobble and prompt blink share one pace */

/* Field widths for font_print_padded() calls on the language-dependent
 * strings below (see lang.h) -- each must be >= the longer of that string's
 * English/French variant, so toggling language with L never leaves the
 * previous, longer string's trailing glyphs on screen (see font.h's
 * font_print_padded() doc comment). Computed by hand from lang.c's string
 * literals: */
#define MUSIC_OFF_FIELD_WIDTH    11 /* FR "MUSIQUE OFF" (11) > EN "MUSIC OFF" (9) */
/* Width of the field both the help hint and the start prompt are centered
 * within (cols 1-20, i.e. the full row minus the two framing arrow-tile
 * columns the start prompt uses at col 0 and 21 -- see the main loop
 * below): the longer of the two languages' text fills less than this, so
 * it's centered rather than left-anchored, and a shorter translation
 * doesn't end up hugging one side. */
#define START_PROMPT_FIELD_WIDTH 20 /* EN "PRESS SPACE TO START" (20) > FR "APPUYEZ SUR ESPACE" (18) */

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

/* Marquee border: a row of CHAR_BULB tiles across the top and bottom edges
 * (MARQUEE_ROW_TOP/BOTTOM, which are just SCREEN_ROWS' first/last row --
 * see decor_draw_marquee()'s own doc comment). Moved into decor.c so the
 * help screen can reuse the exact same border (see help.c) instead of
 * duplicating this loop -- title.c just advances the shared phase counter
 * once per ANIM_JIFFIES tick and calls it (see title_screen_run()). */

/* Starfield: a fixed scatter of CHAR_STAR tiles filling the otherwise empty
 * background rows (between the title/ticker/icon rows above -- picked to
 * avoid every row those already occupy, so this never overwrites them). The
 * position table and twinkle-color logic live in
 * [starfield.c](../graphics/starfield.c)/starfield.h, shared with the game
 * screen's own starfield (see game.c) so both use the same look. Twinkle is
 * a per-star color swap between white and cyan rather than an on/off blink,
 * so the field never looks like it's vanishing -- driven by XORing each
 * star's own index with the caller's shared blink flag (the same one the
 * start prompt already toggles every ANIM_JIFFIES tick) so roughly half the
 * stars swap on any given tick and half swap on the next, instead of the
 * whole field flipping in lockstep. */

/* Shows or hides the "MUSIC OFF" indicator (see MUTE_INDICATOR_ROW) to
 * match the title tune's current mute state -- drawn in COLOR_BLACK
 * (invisible against the black screen background, same trick as the
 * blinking start prompt below) when unmuted, so no separate clear call is
 * needed. */
static void update_mute_indicator(void) {
    font_print_padded(MUTE_INDICATOR_ROW, 6, lang_music_off(),
                       sound_music_is_muted() ? COLOR_RED : COLOR_BLACK,
                       MUSIC_OFF_FIELD_WIDTH);
}

/* Redraws the language-dependent help hint in its current language,
 * centered the same way draw_start_prompt() below centers its own text --
 * called once from draw_static() and again, immediately, whenever L toggles
 * the language (see wait_jiffies_or_space() below) so the switch is visible
 * right away rather than waiting for the next unrelated redraw of that
 * line. Points at the dedicated help screen (F1, see help.c) rather than
 * spelling out every control here -- see HELP_HINT_ROW's comment above. */
static void draw_help_hint(void) {
    const char *hint = lang_help_hint();
    unsigned char hint_len = (unsigned char)strlen(hint);

    font_print_padded(HELP_HINT_ROW, 1, "", COLOR_BLACK, START_PROMPT_FIELD_WIDTH);
    font_print(HELP_HINT_ROW, 1 + (START_PROMPT_FIELD_WIDTH - hint_len) / 2, hint, COLOR_CYAN);
}

/* Everything that's drawn once and never changes again: the marquee border
 * and starfield (their own animation only recolors these same cells, see
 * decor_draw_marquee()/starfield_draw_all() and the main loop below), title,
 * the four fruit sprites at their starting ticker position (they then
 * scroll AND animate in place -- see scroll_tick() and CLAUDE.md, "movement
 * vs. animation"), and the help hint. Only the start prompt blinks and is
 * redrawn separately (see draw_start_prompt() below). Also re-run whenever
 * F1's help-screen excursion returns (see wait_jiffies_or_space() below),
 * since that screen overwrites the whole display. */
static void draw_static(void) {
    decor_draw_marquee(0);
    starfield_draw_all(0);

    bigfont_print(TITLE_ROW, 4, "FRUIT INVADERS");

    draw_fruits(scroll_offset);

    draw_help_hint();
}

/* Draws the blinking "PRESS SPACE TO START" prompt and its two framing
 * arrows at the given blink phase -- called from title_screen_run()'s main
 * loop, and also from wait_jiffies_or_space() below once, right after F1's
 * help-screen excursion redraws everything else, instead of leaving this
 * row blank for the rest of the current ANIM_JIFFIES window
 * (help_screen_run() clears the whole screen; draw_static() puts everything
 * else back but never touches this row itself, see its own comment). */
static void draw_start_prompt(unsigned char blink) {
    const char *prompt = lang_start_prompt();
    unsigned char prompt_len = (unsigned char)strlen(prompt);

    /* Centered within the field (cols 1-20, see START_PROMPT_FIELD_WIDTH)
     * rather than left-anchored -- EN fills the field so this is a no-op
     * offset for it, but FR is shorter and would otherwise hug the left
     * arrow. The field is blanked in full first (font_print_padded with an
     * empty string) since the centered column itself shifts with the
     * string's length -- drawing the new text alone wouldn't clear whatever
     * the previous language's differently-positioned text left outside its
     * own span. */
    font_print_padded(START_ROW, 1, "", COLOR_BLACK, START_PROMPT_FIELD_WIDTH);
    font_print(START_ROW, 1 + (START_PROMPT_FIELD_WIDTH - prompt_len) / 2, prompt,
               blink ? COLOR_WHITE : COLOR_BLACK);
    /* Chevrons framing the prompt, blinking in lockstep with it -- point
     * inward (right-pointing on the left, left-pointing on the right) so
     * they read as bracketing the text rather than as a scroll/move
     * affordance. */
    screen_put(START_ROW, 0, CHAR_ARROW_R, blink ? SHOT_COLOR : COLOR_BLACK);
    screen_put(START_ROW, 21, CHAR_ARROW_L, blink ? SHOT_COLOR : COLOR_BLACK);
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
 * outer loop iteration. Also handles F1: hands off to the help screen
 * (help_screen_run(), which blocks until F1 is pressed again there -- see
 * help.c) and, once it returns, redraws everything this screen owns, since
 * the help screen has overwritten the whole display in the meantime (same
 * reasoning as run_countdown()/show_lose_screen() being blocking sub-screens
 * in game.c). Doesn't stop or restart the title tune for this excursion --
 * only Space does that (see title_screen_run()) -- help_screen_run() ticks
 * it right alongside its own loop so it keeps playing underneath. */
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
        if (key == 'L') {
            lang_toggle();
            draw_help_hint();
            update_mute_indicator();
        }
        if (key == CH_F1) {
            help_screen_run();
            /* help_screen_run() leaves its own content (control list,
             * section headers, back prompt) on screen -- its row layout
             * doesn't line up with this screen's, so draw_static() alone
             * would leave stray glyphs wherever help.c drew on a row this
             * screen doesn't otherwise redraw (confirmed by hand in VICE:
             * without this clear, e.g. help's "TIR"/"RETOUR" left a
             * trailing letter past the end of this screen's shorter
             * "F1: AIDE" hint on the same row). Clear first, same as
             * main()'s own screen_clear() before every title_screen_run()
             * call, since this mid-loop redraw doesn't get that for free. */
            screen_clear();
            draw_static();
            update_mute_indicator();
            draw_start_prompt(1);
            start = *JIFFY_LOW; /* restart this wait's own window fresh from now */
        }
    }
    return 0;
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
        decor_draw_marquee(marquee_phase);

        blink ^= 1;
        starfield_draw_all(blink);
        draw_start_prompt(blink);
    }
}
