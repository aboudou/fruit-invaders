/* Shared low-level loader for the custom character set in RAM -- used by
 * sprites.c, font.c and bigfont.c (see CLAUDE.md, "Sprite storage
 * format"). */

#ifndef CHARMEM_H
#define CHARMEM_H

/* Base address of the custom character set: one of the VIC-I's four fixed
 * character-generator windows ($1000/$1400/$1800/$1C00). $1000 is already
 * the relocated text screen in this project's memory configuration, so
 * $1400 is used here. Character code N's 8 bitmap bytes live at
 * CHARSET_BASE + N*8. */
#define CHARSET_BASE ((volatile unsigned char *)0x1400)

/* Copies `count` bitmap bytes into character memory starting at character
 * code `first_code`, inverting each byte on the way in: source data is
 * authored as "1 = the glyph's own body, 0 = empty", but the VIC-I's actual
 * standard-mode convention is the opposite (confirmed empirically in VICE
 * -- see CLAUDE.md, "Sprite storage format"). A hand-written volatile-aware
 * loop, not memcpy(), since the destination is a hardware-mapped table the
 * VIC-I reads every frame and the optimizer must not reorder or elide
 * writes to it. */
void charmem_load(unsigned char first_code, const unsigned char *src, unsigned char count);

#endif
