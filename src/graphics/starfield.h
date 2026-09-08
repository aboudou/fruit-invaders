/* Fixed-position twinkling starfield, shared by the title screen (background
 * dressing behind the marquee/ticker, see title.c) and the game screen
 * (background dressing behind the fruit grid/ship/shots, see game.c). Reuses
 * CHAR_STAR (see decor.h/decor.c, which owns loading its bitmap into
 * character memory) -- this module only owns *where* the stars sit and their
 * twinkle color, not the glyph itself.
 *
 * The title screen never draws anything over a star row (see decor.c's
 * header comment), so it only ever needs starfield_draw_all(): redraw every
 * star, unconditionally, on every twinkle tick. The game screen's sprites
 * roam over the whole field, and the help screen's static text permanently
 * covers some star cells once drawn (see help.c) -- both need
 * starfield_twinkle_masked() instead, which only recolors a cell still
 * actually showing CHAR_STAR right now (screen_get(), see screen.h),
 * leaving whatever real content is covering the rest alone (see CLAUDE.md,
 * "Sprite storage format" -- there's no automatic background/z-order
 * restore in this text-mode engine, so callers have to track it
 * explicitly). The game screen also needs starfield_lookup() on top of that,
 * to restore a star when a *moving* sprite's trail is erased off of it. */

#ifndef STARFIELD_H
#define STARFIELD_H

#define STARFIELD_COUNT 20

extern const unsigned char STARFIELD_ROW[STARFIELD_COUNT];
extern const unsigned char STARFIELD_COL[STARFIELD_COUNT];

/* Twinkle color for star `index` at the given blink phase (0/1): swaps
 * between white and cyan (never fully dark, see title.c) by XORing the
 * star's own index against the shared blink flag, so roughly half the field
 * swaps on any given tick instead of all of it flipping in lockstep. */
unsigned char starfield_twinkle_color(unsigned char index, unsigned char blink);

/* Draws every star at its fixed position, at the given blink phase. */
void starfield_draw_all(unsigned char blink);

/* Recolors every star still showing CHAR_STAR right now, at the given blink
 * phase -- skips any star cell that's been permanently overwritten by other
 * content instead of redrawing a star over it (see the header comment
 * above). Shared by the game screen and the help screen; the title screen
 * uses the simpler starfield_draw_all() instead since it never has this
 * problem. */
void starfield_twinkle_masked(unsigned char blink);

/* Looks up whether (row, col) is one of the fixed star positions; if so,
 * returns 1 and writes its current twinkle color (at the given blink phase)
 * to *out_color. Returns 0 (out_color left untouched) otherwise. */
unsigned char starfield_lookup(unsigned char row, unsigned char col, unsigned char blink,
                                unsigned char *out_color);

#endif
