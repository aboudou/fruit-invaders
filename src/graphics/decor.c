#include "decor.h"

#include "charmem.h"
#include "screen.h"
#include "sprites.h" /* APPLE_COLOR/CARROT_COLOR/GRAPES_COLOR/PEPPER_COLOR */

/* Marquee light: a solid disc, filling most of the cell so a full row of
 * them reads as a string of bulbs rather than a row of dots. Authored
 * "1 = the tile's own body" like every other sprite/font source array --
 * charmem_load() applies the same hardware inversion (see CLAUDE.md,
 * "Sprite storage format"). */
static const unsigned char bulb[8] = {
    0x38, 0x7C, 0xFE, 0xFE, 0xFE, 0xFE, 0x7C, 0x38,
};

/* Star: a sparse 4-point twinkle (a plus sign, not a filled disc) so it
 * reads as a distant point of light rather than another bulb. */
static const unsigned char star[8] = {
    0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00,
};

/* Left/right arrows: one shape hand-authored (a triangle, apex at column 1,
 * flat base at column 5) and its exact mirror for the other direction --
 * same "author one, mirror the rest" approach already used for the
 * explosion effect's quadrants (see sprites.c). */
static const unsigned char arrow_l[8] = {
    0x0C, 0x1C, 0x3C, 0x7C, 0x3C, 0x1C, 0x0C, 0x00,
};

static const unsigned char arrow_r[8] = {
    0x30, 0x38, 0x3C, 0x3E, 0x3C, 0x38, 0x30, 0x00,
};

void decor_load(void) {
    charmem_load(CHAR_BULB, bulb, 8);
    charmem_load(CHAR_STAR, star, 8);
    charmem_load(CHAR_ARROW_L, arrow_l, 8);
    charmem_load(CHAR_ARROW_R, arrow_r, 8);
}

/* Same four colors bigfont_print() cycles the title lettering through (see
 * sprites.h), so the marquee's palette visibly ties back to the rest of the
 * screen instead of introducing an unrelated one. */
static const unsigned char MARQUEE_PALETTE[4] = {
    APPLE_COLOR, CARROT_COLOR, GRAPES_COLOR, PEPPER_COLOR,
};

void decor_draw_marquee(unsigned char phase) {
    unsigned char col;

    for (col = 0; col < SCREEN_COLS; col++) {
        screen_put(0, col, CHAR_BULB, MARQUEE_PALETTE[(unsigned char)((col + phase) % 4)]);
        screen_put(SCREEN_ROWS - 1, col, CHAR_BULB,
                   MARQUEE_PALETTE[(unsigned char)((SCREEN_COLS - 1 - col + phase) % 4)]);
    }
}
