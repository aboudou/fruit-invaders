#include "loading.h"

#include "../graphics/font.h"
#include "../graphics/screen.h"

#include <vic20.h> /* VIC.bg_border_color ($900F), COLOR_* */

/* KERNAL jiffy clock, low byte -- same use as title.c/game.c/sound.c, paces
 * the overall 1-2s duration below. */
#define JIFFY_LOW ((volatile unsigned char *)0x00A2)

#define LOADING_JIFFIES 75 /* ~1.5s at 50Hz -- long enough to read, short enough not to drag */

/* Spin count between border writes -- deliberately NOT paced by the jiffy
 * clock like every other timed effect in this project: the whole point is
 * to change color several times per raster line (see CLAUDE.md's "loading
 * stripes" research -- real C64/VIC-20 loaders did the same via a tight
 * INC $D020-style loop), so it needs a sub-frame delay. Picked by eye in
 * VICE: enough cycles per step for a readably thick stripe, not so many
 * that whole stripes span multiple rows. */
#define STRIPE_SPIN 12

/* All 8 colors the border can actually show (see CLAUDE.md, "Graphics and
 * colors": border is only 3 bits, $900F bits 0-2). Only the border nibble
 * is touched below -- the background nibble stays COLOR_BLACK throughout,
 * so the flashing is confined to the border frame and the interior (where
 * LOADING_MESSAGE is printed) reads cleanly against solid black. */
static const unsigned char STRIPE_PALETTE[8] = {
    COLOR_BLACK,  COLOR_WHITE, COLOR_RED,  COLOR_CYAN,
    COLOR_PURPLE, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
};

#define LOADING_MESSAGE "GROWING FRUITS..."
#define LOADING_MESSAGE_LEN 17 /* strlen("GROWING FRUITS...") -- fixed literal, hand-counted like title.c's field widths */
#define LOADING_MESSAGE_ROW (SCREEN_ROWS / 2)
#define LOADING_MESSAGE_COL ((SCREEN_COLS - LOADING_MESSAGE_LEN) / 2)

void loading_screen_run(void) {
    unsigned char start = *JIFFY_LOW;
    unsigned char index = 0;
    volatile unsigned char spin;

    screen_clear();
    font_print(LOADING_MESSAGE_ROW, LOADING_MESSAGE_COL, LOADING_MESSAGE, COLOR_WHITE);

    while ((unsigned char)(*JIFFY_LOW - start) < LOADING_JIFFIES) {
        VIC.bg_border_color = (unsigned char)((COLOR_BLACK << 4) | STRIPE_PALETTE[index]);
        index = (unsigned char)((index + 1) % 8);
        for (spin = 0; spin < STRIPE_SPIN; spin++) {
        }
    }
}
