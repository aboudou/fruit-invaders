#include "help.h"

#include "../graphics/bigfont.h"
#include "../graphics/decor.h"
#include "../graphics/font.h"
#include "../graphics/screen.h"
#include "../graphics/sprites.h" /* SHOT_COLOR, COLOR_* */
#include "../graphics/starfield.h"
#include "../sound/sound.h"
#include "lang.h"

#include <cbm.h>    /* cbm_k_getin(): KERNAL keyboard-buffer read, and CH_F1 (via vic20.h) */
#include <string.h> /* strlen(): centers the back prompt within its field, see below */

/* KERNAL jiffy clock, low byte -- see title.c for the same use (paces
 * animation without blocking on key input). */
#define JIFFY_LOW ((volatile unsigned char *)0x00A2)

/* Screen rows (0-based, 23 rows available -- see screen.h), laid out like
 * title.c's own rows: the game name at the top (skipped only if it doesn't
 * fit -- see HELP_TITLE_ROW's use below), then two labeled sections --
 * "title screen only" and "in game" (see CLAUDE.md, "Controls" and "Help
 * screen") -- each listing its controls one per row, then a blinking
 * prompt to leave. Every row here comfortably fits the 22x23 screen, with
 * gaps left over for the starfield -- unlike the title screen, this
 * screen's dense text does cross a few of the shared starfield's fixed
 * star positions (see starfield.c), which is fine: stars are drawn first
 * as background and text is drawn over them (see draw_static() below),
 * permanently claiming those cells, and the twinkle loop only ever
 * recolors a cell still actually showing a star (see
 * starfield_twinkle_masked() and CLAUDE.md, "Sprite storage format"). */
#define HELP_TITLE_ROW        2  /* "FRUIT INVADERS", same bigfont as title.c */
#define SECTION_TITLE_ROW     6
#define LINE_LANGUAGE_ROW     7
#define LINE_MUTE_ROW         8
#define LINE_HELP_ROW         9
#define SECTION_GAME_ROW      11
#define LINE_MOVE_ROW         12
#define LINE_FIRE_ROW         13
#define LINE_PAUSE_ROW        14
#define LINE_TITLE_SCREEN_ROW 15
#define MUTE_INDICATOR_ROW    17 /* "MUSIC OFF", same trick as title.c's own */
#define BACK_ROW              19

#define ANIM_JIFFIES 25 /* ~0.5s at 50Hz -- same pace as title.c's marquee/blink */

/* Field widths for font_print_padded() calls on the language-dependent
 * strings below (see lang.h) -- each must be >= the longer of that string's
 * English/French variant, so toggling language with L never leaves the
 * previous, longer string's trailing glyphs on screen (see font.h's
 * font_print_padded() doc comment). Computed by hand from lang.c's string
 * literals: */
#define SECTION_TITLE_FIELD_WIDTH     12 /* EN "TITLE SCREEN" (12) > FR "ECRAN TITRE" (11) */
#define SECTION_GAME_FIELD_WIDTH      9  /* FR "EN PARTIE" (9) > EN "IN GAME" (7) */
#define LINE_LANGUAGE_FIELD_WIDTH     15 /* EN "L ... LANGUAGE" (15) > FR "L ... LANGUE" (13) */
#define LINE_MUTE_FIELD_WIDTH         21 /* FR "M ... COUPER MUSIQUE" (21) > EN (17) */
#define LINE_HELP_FIELD_WIDTH         11 /* EN "F1 ... HELP" (11) == FR "F1 ... AIDE" (11) */
#define LINE_MOVE_FIELD_WIDTH         13 /* FR "S/D ... BOUGER" (13) > EN "S/D ... MOVE" (11) */
#define LINE_FIRE_FIELD_WIDTH         11 /* EN "SPACE ... FIRE" (11) > FR "ESPACE TIR" (10) */
#define LINE_PAUSE_FIELD_WIDTH        12 /* EN/FR "P ... PAUSE" (12), same both languages */
#define LINE_TITLE_SCREEN_FIELD_WIDTH 19 /* EN "H ... TITLE SCREEN" (19) > FR (18) */
#define MUSIC_OFF_FIELD_WIDTH         11 /* FR "MUSIQUE OFF" (11) > EN "MUSIC OFF" (9), see title.c */
/* Width of the field the back prompt is centered within: cols 1-20, the
 * full row minus the two framing arrow-tile columns at col 0 and 21 (see
 * the main loop below) -- same span as title.c's own START_PROMPT_FIELD_WIDTH,
 * not just wide enough to fit the text, so the prompt reads as centered on
 * the screen rather than merely centered within a narrow field hugging the
 * left arrow. */
#define BACK_PROMPT_FIELD_WIDTH 20 /* EN "F1: BACK" (8), FR "F1: RETOUR" (10) */

/* Redraws every language-dependent line in its current language -- called
 * once from draw_static() and again, immediately, whenever L toggles the
 * language (see wait_jiffies_or_f1() below), same pattern as title.c's own
 * draw_help_hint(). Section headers use COLOR_YELLOW to stand out from the
 * COLOR_CYAN control lines below them. */
static void draw_help_text(void) {
    font_print_padded(SECTION_TITLE_ROW, 1, lang_help_section_title(), COLOR_YELLOW,
                       SECTION_TITLE_FIELD_WIDTH);
    font_print_padded(LINE_LANGUAGE_ROW, 1, lang_help_line_language(), COLOR_CYAN,
                       LINE_LANGUAGE_FIELD_WIDTH);
    font_print_padded(LINE_MUTE_ROW, 1, lang_help_line_mute(), COLOR_CYAN, LINE_MUTE_FIELD_WIDTH);
    font_print_padded(LINE_HELP_ROW, 1, lang_help_line_help(), COLOR_CYAN, LINE_HELP_FIELD_WIDTH);

    font_print_padded(SECTION_GAME_ROW, 1, lang_help_section_game(), COLOR_YELLOW,
                       SECTION_GAME_FIELD_WIDTH);
    font_print_padded(LINE_MOVE_ROW, 1, lang_help_line_move(), COLOR_CYAN, LINE_MOVE_FIELD_WIDTH);
    font_print_padded(LINE_FIRE_ROW, 1, lang_help_line_fire(), COLOR_CYAN, LINE_FIRE_FIELD_WIDTH);
    font_print_padded(LINE_PAUSE_ROW, 1, lang_help_line_pause(), COLOR_CYAN,
                       LINE_PAUSE_FIELD_WIDTH);
    font_print_padded(LINE_TITLE_SCREEN_ROW, 1, lang_help_line_title_screen(), COLOR_CYAN,
                       LINE_TITLE_SCREEN_FIELD_WIDTH);
}

/* Shows or hides the "MUSIC OFF" indicator to match the title tune's
 * current mute state -- same trick as title.c's own update_mute_indicator()
 * (drawn in COLOR_BLACK, invisible against the background, when unmuted).
 * `M` mutes/unmutes from here exactly as it does on the title screen (see
 * wait_jiffies_or_f1() below), and the resulting state is the same
 * persistent one that screen reads (see sound_music_start()), so this just
 * needs to reflect it, not own it. */
static void update_mute_indicator(void) {
    font_print_padded(MUTE_INDICATOR_ROW, 6, lang_music_off(),
                       sound_music_is_muted() ? COLOR_RED : COLOR_BLACK, MUSIC_OFF_FIELD_WIDTH);
}

/* Everything that's drawn once and never changes again: the starfield
 * (background, drawn first -- see the header comment above), the marquee
 * border, the game name (reusing title.c's exact bigfont_print() call and
 * position, since it comfortably fits above this screen's control list
 * too), and every language-dependent line (see draw_help_text() above). No
 * fruit ticker here (see CLAUDE.md, "Help screen"). Only the marquee chase,
 * the starfield twinkle and the back prompt animate from the main loop
 * below. */
static void draw_static(void) {
    starfield_draw_all(0);
    decor_draw_marquee(0);

    bigfont_print(HELP_TITLE_ROW, 4, "FRUIT INVADERS");

    draw_help_text();
    update_mute_indicator();
}

/* Draws the blinking "F1: BACK"/"F1: RETOUR" prompt and its two framing
 * arrows at the given blink phase -- same centering/framing approach as
 * title.c's own start prompt (see its comment for why the field is blanked
 * before redrawing: the centered column shifts with the string's length,
 * so a shorter translation can't just be drawn over a longer one). */
static void draw_back_prompt(unsigned char blink) {
    const char *prompt = lang_help_back_prompt();
    unsigned char prompt_len = (unsigned char)strlen(prompt);

    font_print_padded(BACK_ROW, 1, "", COLOR_BLACK, BACK_PROMPT_FIELD_WIDTH);
    font_print(BACK_ROW, 1 + (BACK_PROMPT_FIELD_WIDTH - prompt_len) / 2, prompt,
               blink ? COLOR_WHITE : COLOR_BLACK);
    screen_put(BACK_ROW, 0, CHAR_ARROW_R, blink ? SHOT_COLOR : COLOR_BLACK);
    screen_put(BACK_ROW, 21, CHAR_ARROW_L, blink ? SHOT_COLOR : COLOR_BLACK);
}

/* Waits up to n jiffies, checking the keyboard buffer on every pass of the
 * busy loop (much more often than once per jiffy) so an F1 press is caught
 * promptly instead of only being noticed on the next animation tick.
 * Returns early (1) the moment F1 is seen, or 0 after the full wait with no
 * F1 press. Also ticks the title tune (sound_music_tick()) on every pass,
 * same reasoning as title.c's own wait loop -- the tune keeps playing
 * underneath this screen exactly as it did on the title screen, since
 * neither screen starts or stops it here (only Space, back on the title
 * screen, does that). L toggles the language and redraws every
 * language-dependent line in place, same as title.c. M toggles mute exactly
 * like title.c's own M handler (see sound_music_toggle_mute()) -- the state
 * is shared and persists across both screens (see sound_music_start()), so
 * muting here and later returning to the title screen (or leaving this
 * screen and coming back) keeps the tune silent until unmuted again. */
static unsigned char wait_jiffies_or_f1(unsigned char n) {
    unsigned char start = *JIFFY_LOW;
    unsigned char key;

    while ((unsigned char)(*JIFFY_LOW - start) < n) {
        sound_music_tick();
        key = cbm_k_getin();
        if (key == CH_F1) {
            return 1;
        }
        if (key == 'L') {
            lang_toggle();
            draw_help_text();
            update_mute_indicator();
        }
        if (key == 'M') {
            sound_music_toggle_mute();
            update_mute_indicator();
        }
    }
    return 0;
}

void help_screen_run(void) {
    unsigned char blink = 0;
    unsigned char marquee_phase = 0;

    screen_clear();
    draw_static();

    for (;;) {
        if (wait_jiffies_or_f1(ANIM_JIFFIES)) {
            return;
        }

        marquee_phase = (unsigned char)((marquee_phase + 1) % 4);
        decor_draw_marquee(marquee_phase);

        blink ^= 1;
        starfield_twinkle_masked(blink);
        draw_back_prompt(blink);
    }
}
