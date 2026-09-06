/* Sprite bitmaps and character-memory loading.
 *
 * All sprites are pseudo-graphics built from a custom character set (see
 * CLAUDE.md, "Sprite storage format"). Fruits/vegetables and the ship are
 * 2x2 character cells (16x16 px, 4 character codes each); the shot is 2x1
 * (16x8 px, 2 codes side by side) -- same width as everything else it needs
 * to line up with, so it never sits at a half-character offset from the
 * ship or a fruit's own two columns (see the comment in sprites.c). Every
 * on-screen instance of a given fruit type shares the same 4 codes and
 * therefore animates in lockstep -- this is a hardware constraint, not a
 * shortcut (see the comment in sprites.c).
 */

#ifndef SPRITES_H
#define SPRITES_H

#include <vic20.h> /* COLOR_* constants */

/* Character codes in the custom set, starting at $1400 (code N lives at
 * $1400 + N*8). Order within each 2x2 sprite is top-left, top-right,
 * bottom-left, bottom-right. CHAR_LIFE_SHIP is the only single-cell (8x8)
 * ship variant -- a "reduced" ship icon for the HUD lives counter (see
 * game.c), separate from CHAR_SHIP_TL/TR/BL/BR since those only ever depict
 * a quarter of the full 2x2 ship and would not read as a ship on their
 * own. */
enum {
    CHAR_APPLE_TL = 0,
    CHAR_APPLE_TR,
    CHAR_APPLE_BL,
    CHAR_APPLE_BR,
    CHAR_CARROT_TL,
    CHAR_CARROT_TR,
    CHAR_CARROT_BL,
    CHAR_CARROT_BR,
    CHAR_GRAPES_TL,
    CHAR_GRAPES_TR,
    CHAR_GRAPES_BL,
    CHAR_GRAPES_BR,
    CHAR_PEPPER_TL,
    CHAR_PEPPER_TR,
    CHAR_PEPPER_BL,
    CHAR_PEPPER_BR,
    CHAR_SHIP_TL,
    CHAR_SHIP_TR,
    CHAR_SHIP_BL,
    CHAR_SHIP_BR,
    CHAR_LIFE_SHIP,
    CHAR_SHOT_L,
    CHAR_SHOT_R,
    CHAR_ENEMY_SHOT_L,
    CHAR_ENEMY_SHOT_R,
    CHAR_COUNTDOWN_TL,
    CHAR_COUNTDOWN_TR,
    CHAR_COUNTDOWN_BL,
    CHAR_COUNTDOWN_BR,
    CHAR_EXPLOSION_TL,
    CHAR_EXPLOSION_TR,
    CHAR_EXPLOSION_BL,
    CHAR_EXPLOSION_BR,
    CHAR_BLANK
};

/* Per-character foreground color (color RAM bits 2-0) only supports
 * COLOR_BLACK..COLOR_YELLOW from <vic20.h> -- the COLOR_ORANGE and lighter
 * variants are background/multicolor-only (see CLAUDE.md, "Graphics and
 * colors"). */
#define APPLE_COLOR  COLOR_RED
#define CARROT_COLOR COLOR_YELLOW
#define GRAPES_COLOR COLOR_PURPLE
#define PEPPER_COLOR COLOR_GREEN
#define SHIP_COLOR      COLOR_CYAN
#define SHOT_COLOR      COLOR_WHITE
#define ENEMY_SHOT_COLOR COLOR_RED
#define COUNTDOWN_COLOR COLOR_WHITE
#define EXPLOSION_COLOR COLOR_YELLOW

/* Points the VIC-I character generator at the custom set and copies every
 * sprite's initial bitmap (fruits/vegetables frame 0, ship, shot, a blank
 * glyph) into character memory. Call once at startup, before drawing
 * anything. */
void sprites_load(void);

/* Redefines a fruit/vegetable's character codes with the requested wobble
 * frame (0 or 1). Because every on-screen instance of a given type uses the
 * same 4 codes, this instantly animates all of them at once -- see
 * CLAUDE.md, "Graphics and colors" (live redraw) and "Sprite storage
 * format" (shared codes per fruit type). */
void sprites_set_apple_frame(unsigned char frame);
void sprites_set_carrot_frame(unsigned char frame);
void sprites_set_grapes_frame(unsigned char frame);
void sprites_set_pepper_frame(unsigned char frame);

/* Redefines the countdown digit's character codes to show '1', '2' or '3'
 * (the only values the game screen's pre-game countdown ever shows). Unlike
 * the fruits, the 2x2 screen cells using CHAR_COUNTDOWN_TL/TR/BL/BR are
 * only ever occupied by this single digit sprite at a time, so there is no
 * "frame" concept -- just the digit to display. Not preloaded by
 * sprites_load() since the countdown always sets an explicit digit before
 * first display. */
void sprites_set_countdown_digit(unsigned char digit);

/* Redefines the shared explosion effect's character codes with the
 * requested frame (0 or 1) -- reused by any dying entity (see CLAUDE.md,
 * "Sprite storage format", "Key constraint"); the ship is the first and
 * currently only caller (see game.c). Not preloaded by sprites_load(),
 * same reasoning as sprites_set_countdown_digit() above -- the caller sets
 * an explicit frame before the codes are first put on screen. */
void sprites_set_explosion_frame(unsigned char frame);

#endif
