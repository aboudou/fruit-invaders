/* Title screen (see CLAUDE.md, "Title screen"): game name, a chasing-light
 * marquee border and twinkling starfield (see graphics/decor.h), the four
 * animated fruit/vegetable sprites (ship and shot are deliberately not
 * shown here -- they belong to gameplay, not the title), a controls
 * reminder, and the start prompt. */

#ifndef TITLE_H
#define TITLE_H

/* Draws the title screen and loops animating it (fruit wobble + start-prompt
 * blink) until the player presses Space, then returns so the caller can
 * hand off to the game screen. Caller must have already run sprites_load(),
 * font_load() and screen_clear(). */
void title_screen_run(void);

#endif
