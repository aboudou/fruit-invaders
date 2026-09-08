/* Decorative pseudo-bitmap tiles for the title screen: a chasing-light
 * marquee border, a twinkling starfield, and a pair of arrow tiles framing
 * the start prompt (see CLAUDE.md, "Title screen"). The controls reminder
 * itself stays plain text (font.c) -- an earlier icon-based version (ship/
 * shot/note glyphs replacing the S/D/SPACE/M lines) was reverted at the
 * user's request: the old sentences read more clearly than the pictograms
 * did. Kept separate from sprites.c/font.c/bigfont.c since none of this is
 * gameplay- or text-rendering-related -- it's purely title-screen set
 * dressing, title.c is the only caller.
 */

#ifndef DECOR_H
#define DECOR_H

#include "bigfont.h" /* CHAR_BIG_END: base for this enum, same disjoint-range scheme */

/* Decor character codes, placed right after the big font's codes (disjoint
 * ranges in the same $1400 character set -- see charmem.h). */
enum {
    CHAR_BULB = CHAR_BIG_END, /* marquee border light */
    CHAR_STAR,                /* background twinkle */
    CHAR_ARROW_L,              /* start-prompt framing chevron */
    CHAR_ARROW_R,
};

/* Loads every decor glyph into character memory. Call once at startup,
 * alongside sprites_load()/font_load()/bigfont_load() (all four share the
 * $1400 character set, in disjoint code ranges -- order between them
 * doesn't matter). */
void decor_load(void);

#endif
