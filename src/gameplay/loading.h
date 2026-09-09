/* Fake "cassette loading" screen shown once at startup, before the title
 * screen -- a nod to the classic C64/VIC-20 tape-loading border stripes
 * (border+background cycling through several colors in quick, unsynced
 * succession, which reads as horizontal bands since the display is drawn
 * line by line -- see CLAUDE.md's "loading stripes" research). Purely
 * cosmetic: nothing is actually being loaded from here. */

#ifndef LOADING_H
#define LOADING_H

/* Clears the screen (so no leftover KERNAL boot text shows through) and
 * flashes the border/background through the stripe palette for about 1-2
 * seconds, then returns with the border/background left however the last
 * stripe left them -- the caller's own screen_clear(), right before drawing
 * the title screen, resets that. */
void loading_screen_run(void);

#endif
