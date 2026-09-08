/* Help screen (see CLAUDE.md, "Help screen"): reached from the title screen
 * with F1, F1 again to return. Same chrome as the title screen (marquee
 * border, twinkling starfield, the "FRUIT INVADERS" title) but no fruit
 * ticker -- in its place, a full list of every control, grouped by which
 * screen it's available on. */

#ifndef HELP_H
#define HELP_H

/* Draws the help screen and loops animating it (marquee chase + starfield
 * twinkle + back-prompt blink) until the player presses F1 again, then
 * returns so the caller (title.c) can redraw the title screen over it.
 * Caller must have already run sprites_load(), font_load(), bigfont_load()
 * and decor_load() (same preconditions as title_screen_run()). */
void help_screen_run(void);

#endif
