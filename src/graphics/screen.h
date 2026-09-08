/* Screen matrix and color RAM access -- see CLAUDE.md, "Graphics and
 * colors". */

#ifndef SCREEN_H
#define SCREEN_H

#define SCREEN_COLS 22
#define SCREEN_ROWS 23

/* Writes a character code and its per-cell foreground color at (row, col).
 * row/col are 0-based; row 0-22, col 0-21. */
void screen_put(unsigned char row, unsigned char col, unsigned char code,
                 unsigned char color);

/* Reads back the character code currently shown at (row, col) -- used to
 * tell whether a cell is still showing what was last drawn there (e.g. the
 * game screen's starfield restoration, see starfield.h, checking whether a
 * star is still uncovered before recoloring it for its twinkle). */
unsigned char screen_get(unsigned char row, unsigned char col);

/* Writes a 2x2 sprite (see CLAUDE.md, "Sprite storage format") at
 * (row, col)-(row+1, col+1): first_code/+1/+2/+3 go to
 * top-left/top-right/bottom-left/bottom-right, all sharing one color. */
void screen_put_quad(unsigned char row, unsigned char col,
                      unsigned char first_code, unsigned char color);

/* Blanks the 2x2 cells at (row, col)-(row+1, col+1) -- the counterpart to
 * screen_put_quad(), used to erase a sprite's old position when it moves. */
void screen_clear_quad(unsigned char row, unsigned char col);

/* Writes a 2x1 sprite (1 row, 2 columns -- e.g. the shot, see sprites.c) at
 * (row, col)-(row, col+1): first_code/+1 go to left/right, sharing one
 * color. */
void screen_put_pair(unsigned char row, unsigned char col,
                      unsigned char first_code, unsigned char color);

/* Blanks the 2x1 cells at (row, col)-(row, col+1) -- the counterpart to
 * screen_put_pair(). */
void screen_clear_pair(unsigned char row, unsigned char col);

/* Fills the whole screen with CHAR_BLANK. */
void screen_clear(void);

#endif
