/* "Big" title font: a multi-character-cell font for the game name on the
 * title screen (see CLAUDE.md, "Title screen"), distinct from the plain
 * single-8x8-cell font in font.c/font.h.
 *
 * Unlike the fruit/ship/explosion sprites, which are always a fixed 2x2
 * quad (16x16 px -- see CLAUDE.md, "Sprite storage format"), a font glyph
 * has no reason to share that exact shape: each letter here is its own
 * multi-cell sprite sized to what a bold letterform actually needs, not
 * forced into a 16x16 footprint. Concretely, every glyph is 1 column wide
 * x 2 rows tall (8x16 px) -- a vertical scale-up of the small 5x7
 * letterforms already authored in font.c, stretching their 7 meaningful
 * pixel rows evenly across the full 16-row cell (split across a top and a
 * bottom character code) rather than a plain 2x doubling, so every letter
 * fills the same height with no dead rows at the bottom (see bigfont.c for
 * why a plain doubling doesn't do that uniformly). Only stretched in one
 * dimension, so the title keeps font_print()'s one-column-per-character
 * layout width (a 14-letter "FRUIT INVADERS" still fits the 22-column
 * screen -- see title.c). Only the letters used by that title string are
 * defined.
 */

#ifndef BIGFONT_H
#define BIGFONT_H

/* Loads every big-font glyph (top and bottom half) into character memory.
 * Call once at startup, alongside sprites_load()/font_load() (all three
 * share the $1400 character set, in disjoint code ranges -- see
 * charmem.h and font.h's CHAR_FONT_END). */
void bigfont_load(void);

/* Writes `text` into consecutive screen cells starting at (row, col) and
 * (row+1, col) -- one character per column, spanning two rows. Each
 * letter's color cycles through a small fixed palette (the same colors
 * used for the fruit sprites below it on the title screen, see sprites.h)
 * rather than taking an explicit color argument -- this font only ever
 * draws the one title string, so a configurable color isn't needed.
 * Characters not covered by this font (anything but the letters listed in
 * bigfont.c) fall back to a blank 1x2 cell, which also covers plain
 * spaces. row/col follow screen_put()'s convention (0-based); row+1 must
 * also be a valid row.
 */
void bigfont_print(unsigned char row, unsigned char col, const char *text);

#endif
