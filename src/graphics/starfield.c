#include "starfield.h"

#include "decor.h" /* CHAR_STAR */
#include "screen.h"

#include <vic20.h> /* COLOR_WHITE/COLOR_CYAN */

/* Fixed scatter of star positions -- originally authored for the title
 * screen's otherwise-empty background rows (picked to avoid every row its
 * title/ticker/text content occupies, see title.c), reused as-is for the
 * game screen so both screens share one consistent starfield look. The game
 * screen has no equivalent "avoid these rows" constraint (its sprites roam
 * over the whole field and restore whatever star was underneath when they
 * move on, see starfield_lookup() and game.c) so there was no reason to
 * author a second, different table. */
const unsigned char STARFIELD_ROW[STARFIELD_COUNT] = {
    1, 1, 4, 4, 5, 6, 6, 9, 9, 10, 11, 11, 12, 12, 17, 17, 19, 20, 20, 21,
};
const unsigned char STARFIELD_COL[STARFIELD_COUNT] = {
    2, 19, 1, 20, 10, 3, 18, 2, 19, 10, 4, 17, 8, 13, 2, 20, 9, 3, 18, 10,
};

unsigned char starfield_twinkle_color(unsigned char index, unsigned char blink) {
    return ((index ^ blink) & 1) ? COLOR_WHITE : COLOR_CYAN;
}

void starfield_draw_all(unsigned char blink) {
    unsigned char i;

    for (i = 0; i < STARFIELD_COUNT; i++) {
        screen_put(STARFIELD_ROW[i], STARFIELD_COL[i], CHAR_STAR,
                   starfield_twinkle_color(i, blink));
    }
}

unsigned char starfield_lookup(unsigned char row, unsigned char col, unsigned char blink,
                                unsigned char *out_color) {
    unsigned char i;

    for (i = 0; i < STARFIELD_COUNT; i++) {
        if (STARFIELD_ROW[i] == row && STARFIELD_COL[i] == col) {
            *out_color = starfield_twinkle_color(i, blink);
            return 1;
        }
    }
    return 0;
}
