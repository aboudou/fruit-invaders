/* Minimal hand-authored font for title/menu text.
 *
 * The character generator can only point at one table at a time (see
 * CLAUDE.md, "Sprite storage format"), so once it's redirected to the
 * custom set at $1400 the ROM font is no longer reachable -- any text drawn
 * on screen needs its own glyphs in that same set. Rather than a full
 * charset, this covers only the uppercase letters, digits and punctuation
 * that actually appear in the title/game screen strings (see
 * src/gameplay/title.c and src/gameplay/game.c) -- lowercase is not
 * defined.
 */

#ifndef FONT_H
#define FONT_H

#include "sprites.h" /* CHAR_BLANK: base for this enum, same disjoint-range scheme */

/* Font character codes, placed right after the sprite codes (disjoint ranges
 * in the same $1400 character set -- see charmem.h). Exposed here (rather
 * than kept private to font.c) so another module needing its own disjoint
 * codes in the same character set -- bigfont.c, for its multi-cell title
 * lettering -- can start right after CHAR_FONT_END instead of hardcoding
 * font.c's glyph count. */
enum {
    CHAR_FONT_A = CHAR_BLANK + 1,
    CHAR_FONT_C,
    CHAR_FONT_D,
    CHAR_FONT_E,
    CHAR_FONT_F,
    CHAR_FONT_G,
    CHAR_FONT_H,
    CHAR_FONT_I,
    CHAR_FONT_K,
    CHAR_FONT_L,
    CHAR_FONT_M,
    CHAR_FONT_N,
    CHAR_FONT_O,
    CHAR_FONT_P,
    CHAR_FONT_R,
    CHAR_FONT_S,
    CHAR_FONT_T,
    CHAR_FONT_U,
    CHAR_FONT_V,
    CHAR_FONT_W,
    CHAR_FONT_Y,
    CHAR_FONT_SLASH,
    CHAR_FONT_COLON,
    CHAR_FONT_0,
    CHAR_FONT_1,
    CHAR_FONT_2,
    CHAR_FONT_3,
    CHAR_FONT_4,
    CHAR_FONT_5,
    CHAR_FONT_6,
    CHAR_FONT_7,
    CHAR_FONT_8,
    CHAR_FONT_9,
    CHAR_FONT_END /* one past the last font code -- not a real glyph */
};

/* Loads every font glyph into character memory. Call once at startup, in
 * addition to sprites_load() (both share the $1400 character set, in
 * disjoint code ranges -- order between the two calls doesn't matter). */
void font_load(void);

/* Writes `text` into consecutive screen cells starting at (row, col), one
 * character per cell, in the given per-cell foreground color. Characters
 * not covered by this font (anything but the letters/slash listed in
 * font.c) fall back to a blank cell, which also covers plain spaces.
 * row/col follow screen_put()'s convention (0-based). */
void font_print(unsigned char row, unsigned char col, const char *text, unsigned char color);

#endif
