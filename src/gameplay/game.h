/* Placeholder game screen, shown after the player presses Space on the
 * title screen (see CLAUDE.md, "Current project state" -- the actual game
 * loop, entity state, collisions etc. are not implemented yet). */

#ifndef GAME_H
#define GAME_H

/* Draws the placeholder game screen and blocks until the player presses H,
 * then returns so the caller can go back to the title screen. Caller must
 * have already run sprites_load(), font_load() and screen_clear() (same
 * preconditions as title_screen_run()). */
void game_screen_run(void);

#endif
