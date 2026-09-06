#include "screen.h"
#include "sprites.h"

#include <vic20.h>

/* Screen matrix and color RAM addresses for this project's memory
 * configuration (8K+ expansion active): screen at $1000, color RAM at
 * $9400 -- see CLAUDE.md, "Graphics and colors". Both are indexed the same
 * way, one byte per screen cell.
 *
 * <vic20.h> defines its own COLOR_RAM at $9600, which is only correct for
 * the unexpanded/+3K configuration -- not used here, hence the local name. */
#define SCREEN_MATRIX ((volatile unsigned char *)0x1000)
#define SCREEN_COLOR_RAM ((volatile unsigned char *)0x9400)

/* row * SCREEN_COLS, precomputed for every row (0-22). row*22 reaches 484
 * for the bottom row, so this is a genuine 16-bit product -- LLVM's LTO
 * otherwise proves (correctly, but only for whichever call sites exist at
 * a given point in time) that today's callers never need more than 8 bits
 * and silently narrows the multiply to a truncating one, which breaks
 * silently the day a caller with a bigger row is added. A lookup table
 * has no multiply left to narrow, so it can't regress that way. */
static const unsigned int row_offset[SCREEN_ROWS] = {
    0,   22,  44,  66,  88,  110, 132, 154, 176, 198, 220, 242,
    264, 286, 308, 330, 352, 374, 396, 418, 440, 462, 484,
};

void screen_put(unsigned char row, unsigned char col, unsigned char code,
                 unsigned char color) {
    unsigned int offset = row_offset[row] + col;
    SCREEN_MATRIX[offset] = code;
    SCREEN_COLOR_RAM[offset] = color;
}

void screen_put_quad(unsigned char row, unsigned char col,
                      unsigned char first_code, unsigned char color) {
    screen_put(row, col, first_code, color);
    screen_put(row, col + 1, first_code + 1, color);
    screen_put(row + 1, col, first_code + 2, color);
    screen_put(row + 1, col + 1, first_code + 3, color);
}

void screen_clear_quad(unsigned char row, unsigned char col) {
    screen_put(row, col, CHAR_BLANK, COLOR_BLACK);
    screen_put(row, col + 1, CHAR_BLANK, COLOR_BLACK);
    screen_put(row + 1, col, CHAR_BLANK, COLOR_BLACK);
    screen_put(row + 1, col + 1, CHAR_BLANK, COLOR_BLACK);
}

void screen_put_pair(unsigned char row, unsigned char col,
                      unsigned char first_code, unsigned char color) {
    screen_put(row, col, first_code, color);
    screen_put(row, col + 1, first_code + 1, color);
}

void screen_clear_pair(unsigned char row, unsigned char col) {
    screen_put(row, col, CHAR_BLANK, COLOR_BLACK);
    screen_put(row, col + 1, CHAR_BLANK, COLOR_BLACK);
}

void screen_clear(void) {
    unsigned int i;

    /* Border ($900F bits 0-2) and screen background (bits 4-7) both black
     * -- the KERNAL's own startup default is cyan border / white
     * background, which the game never wants (and which makes
     * COLOR_WHITE sprites, e.g. the shot, invisible against it). */
    VIC.bg_border_color = (COLOR_BLACK << 4) | COLOR_BLACK;

    for (i = 0; i < (unsigned int)SCREEN_COLS * SCREEN_ROWS; i++) {
        SCREEN_MATRIX[i] = CHAR_BLANK;
        SCREEN_COLOR_RAM[i] = COLOR_BLACK;
    }
}
