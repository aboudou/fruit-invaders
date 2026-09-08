/* Decorative pseudo-bitmap tiles for the title/help screens: a chasing-light
 * marquee border, a twinkling starfield, and a pair of arrow tiles framing
 * a blinking prompt (see CLAUDE.md, "Title screen"). The controls reminder
 * itself stays plain text (font.c) -- an earlier icon-based version (ship/
 * shot/note glyphs replacing the S/D/SPACE/M lines) was reverted at the
 * user's request: the old sentences read more clearly than the pictograms
 * did. Kept separate from sprites.c/font.c/bigfont.c since none of this is
 * gameplay- or text-rendering-related -- it's purely set dressing, shared by
 * title.c and help.c (see decor_draw_marquee() below and CLAUDE.md's
 * "Title screen"/"Help screen" sections).
 */

#ifndef DECOR_H
#define DECOR_H

#include "bigfont.h" /* CHAR_BIG_END: base for this enum, same disjoint-range scheme */

/* Decor character codes, placed right after the big font's codes (disjoint
 * ranges in the same $1400 character set -- see charmem.h). */
enum {
    CHAR_BULB = CHAR_BIG_END, /* marquee border light */
    CHAR_STAR,                /* background twinkle */
    CHAR_ARROW_L,              /* blinking-prompt framing chevron */
    CHAR_ARROW_R,
};

/* Loads every decor glyph into character memory. Call once at startup,
 * alongside sprites_load()/font_load()/bigfont_load() (all four share the
 * $1400 character set, in disjoint code ranges -- order between them
 * doesn't matter). */
void decor_load(void);

/* Draws the chasing-light marquee border across the top row (0) and bottom
 * row (SCREEN_ROWS-1) of the screen, color-cycled through the same four
 * fruit colors used elsewhere (see sprites.h) -- the two rows chase in
 * opposite directions off one shared phase counter the caller advances every
 * animation tick (see title.c's and help.c's main loops). Shared by both
 * screens since both frame their content the same way; each still owns its
 * own animation-tick loop and phase variable. */
void decor_draw_marquee(unsigned char phase);

#endif
