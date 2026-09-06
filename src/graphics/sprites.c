#include "sprites.h"

#include "charmem.h"

#include <vic20.h>

/* The inversion quirk (source arrays authored as "1 = body", VIC-I standard
 * mode actually wants the opposite) and the volatile-aware copy are handled
 * centrally by charmem_load() -- see charmem.h. */
static void load_char(unsigned char code, const unsigned char rows[8]) {
    charmem_load(code, rows, 8);
}

static void load_quad(unsigned char first_code, const unsigned char rows[4][8]) {
    charmem_load(first_code, &rows[0][0], 4 * 8);
}

/* Apple, 16x16 px (2x2 chars): a round fruit with a small stem. Frame 1
 * only redraws the top two rows (stem leaning) -- a minimal-diff wobble,
 * per "animation on two sprites is enough". Sub-array order matches
 * CHAR_APPLE_TL/TR/BL/BR. */
static const unsigned char apple_frame0[4][8] = {
    {0x03, 0x03, 0x0F, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF}, /* top-left */
    {0x00, 0x00, 0xF0, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF}, /* top-right */
    {0xFF, 0xFF, 0xFF, 0x3F, 0x3F, 0x0F, 0x03, 0x00}, /* bottom-left */
    {0xFF, 0xFF, 0xFF, 0xFC, 0xFC, 0xF0, 0xC0, 0x00}, /* bottom-right */
};

static const unsigned char apple_frame1[4][8] = {
    {0x06, 0x06, 0x1F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0xE0, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0x3F, 0x3F, 0x0F, 0x03, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFC, 0xFC, 0xF0, 0xC0, 0x00},
};

/* Carrot, 16x16 px (2x2 chars): leafy tuft tapering to a root tip. Frame 1
 * only redraws the top three rows (leaves swaying). */
static const unsigned char carrot_frame0[4][8] = {
    {0x11, 0x31, 0x03, 0x0F, 0x3F, 0x7F, 0x7F, 0x3F},
    {0x88, 0x8C, 0xC0, 0xF0, 0xFC, 0xFE, 0xFE, 0xFC},
    {0x1F, 0x0F, 0x07, 0x03, 0x03, 0x01, 0x01, 0x01},
    {0xF8, 0xF0, 0xE0, 0xC0, 0xC0, 0x80, 0x80, 0x00},
};

static const unsigned char carrot_frame1[4][8] = {
    {0x08, 0x18, 0x01, 0x0F, 0x3F, 0x7F, 0x7F, 0x3F},
    {0xC4, 0xC6, 0xE0, 0xF0, 0xFC, 0xFE, 0xFE, 0xFC},
    {0x1F, 0x0F, 0x07, 0x03, 0x03, 0x01, 0x01, 0x01},
    {0xF8, 0xF0, 0xE0, 0xC0, 0xC0, 0x80, 0x80, 0x00},
};

/* Grapes, 16x16 px (2x2 chars): two round berries at the top and one
 * berry at the bottom, shifted right of center. Frame 1 only redraws the
 * top-left quadrant (the stem between the two top berries leaning). */
static const unsigned char grapes_frame0[4][8] = {
    {0x01, 0x01, 0x0C, 0x1E, 0x3F, 0x3F, 0x3F, 0x1E}, /* top-left */
    {0x80, 0x80, 0x30, 0x78, 0xFC, 0xFC, 0xFC, 0x78}, /* top-right */
    {0x0C, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00}, /* bottom-left */
    {0x30, 0xF0, 0xF8, 0xF8, 0xF8, 0xF8, 0xF0, 0x00}, /* bottom-right */
};

static const unsigned char grapes_frame1[4][8] = {
    {0x03, 0x01, 0x0C, 0x1E, 0x3F, 0x3F, 0x3F, 0x1E},
    {0x00, 0x80, 0x30, 0x78, 0xFC, 0xFC, 0xFC, 0x78},
    {0x0C, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00},
    {0x30, 0xF0, 0xF8, 0xF8, 0xF8, 0xF8, 0xF0, 0x00},
};

/* Pepper, 16x16 px (2x2 chars): a curved chili -- wide at the stem end,
 * bending and tapering to a thin point (a "piment" silhouette rather than
 * a blocky bell pepper). Frame 1 only redraws the top-left quadrant (stem
 * leaning). */
static const unsigned char pepper_frame0[4][8] = {
    {0x01, 0x03, 0x1F, 0x0F, 0x0F, 0x0F, 0x07, 0x03}, /* top-left */
    {0x80, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0}, /* top-right */
    {0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* bottom-left */
    {0xC0, 0xE0, 0xE0, 0xE0, 0x70, 0x30, 0x10, 0x18}, /* bottom-right */
};

static const unsigned char pepper_frame1[4][8] = {
    {0x03, 0x03, 0x1F, 0x0F, 0x0F, 0x0F, 0x07, 0x03},
    {0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0},
    {0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xC0, 0xE0, 0xE0, 0xE0, 0x70, 0x30, 0x10, 0x18},
};

/* Ship, 16x16 px (2x2 chars): narrow turret over a wide base. Static --
 * only the fruits animate for now. */
static const unsigned char ship[4][8] = {
    {0x01, 0x01, 0x03, 0x03, 0x0F, 0x0F, 0x3F, 0x3F},
    {0x80, 0x80, 0xC0, 0xC0, 0xF0, 0xF0, 0xFC, 0xFC},
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00},
};

/* Reduced ship, 8x8 px (1 char): same narrow-turret-over-wide-base
 * silhouette as the full ship above, scaled down to a single cell for the
 * HUD lives counter (see game.c) -- shown once per remaining life instead
 * of a text label. */
static const unsigned char life_ship[8] = {
    0x18, 0x18, 0x3C, 0x3C, 0x7E, 0x7E, 0xFF, 0xFF,
};

/* Shot, 16x8 px (2 chars side by side, 1 row tall): a fork (kitchen theme)
 * -- three tines at the top (leading edge, since the shot travels upward
 * from the ship), a bridge, and a handle at the bottom. Deliberately as
 * wide as the ship and every fruit (2 chars) rather than a single 8px-wide
 * cell: a 1-char-wide shot has no cell it can occupy that's actually
 * centered under a 2-char sprite -- the true center always falls exactly on
 * the boundary between two cells, not inside either one, so the best a
 * single cell can do is sit flush against that boundary (still ~half the
 * fork's own width off). Matching the ship's full 2-column footprint
 * instead removes the question entirely: the shot's handle (rows 4-7) is
 * centered on the same column boundary as the ship's tip, and the whole
 * sprite occupies exactly (shot_col, shot_col+1) in game.c, the same pair
 * of columns a fruit will later occupy -- so a future collision check is a
 * plain "same two columns" comparison, no half-cell cases to special-case. */
static const unsigned char shot_l[8] = {
    0x09, 0x09, 0x09, 0x0F, 0x03, 0x03, 0x03, 0x03,
}; /* left half */

static const unsigned char shot_r[8] = {
    0x90, 0x90, 0x90, 0xF0, 0xC0, 0xC0, 0xC0, 0xC0,
}; /* right half */

/* Enemy shot, 16x8 px (2 chars side by side, 1 row tall): a red lightning
 * bolt zigzagging diagonally -- same 2-char-wide format and centering
 * rationale as the player's own shot above (see game.c, "Enemy fire"), but
 * fired by a fruit and travelling downward instead of up. Narrow at both
 * ends, widening into a diagonal cross-stroke through the middle rows. */
static const unsigned char enemy_shot_l[8] = {
    0x03, 0x07, 0x0E, 0x1F, 0x07, 0x00, 0x01, 0x03,
}; /* left half */

static const unsigned char enemy_shot_r[8] = {
    0x80, 0x00, 0x00, 0xE0, 0xF0, 0xE0, 0xC0, 0x80,
}; /* right half */

/* Countdown digits, 16x16 px (2x2 chars): a bold pixel-doubled scale-up of
 * the small 5x7 letterforms used elsewhere (see font.c), so the "3"/"2"/"1"
 * shown before play starts (see game.c) reads as a chunky, arcade-style
 * numeral instead of a single small 8x8 character lost in the middle of the
 * screen. Only one digit is ever on screen at a time, so all three share
 * the same 4 codes -- sprites_set_countdown_digit() just redefines them,
 * the same "animate in place" pattern as the fruit wobble frames above. */
static const unsigned char countdown_1[4][8] = {
    {0x00, 0x07, 0x07, 0x1F, 0x1F, 0x07, 0x07, 0x07}, /* top-left */
    {0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, /* top-right */
    {0x07, 0x07, 0x07, 0x07, 0x07, 0x1F, 0x1F, 0x00}, /* bottom-left */
    {0x80, 0x80, 0x80, 0x80, 0x80, 0xF8, 0xF8, 0x00}, /* bottom-right */
};

static const unsigned char countdown_2[4][8] = {
    {0x00, 0x07, 0x07, 0x18, 0x18, 0x00, 0x00, 0x00},
    {0x00, 0xE0, 0xE0, 0x18, 0x18, 0x18, 0x18, 0x60},
    {0x00, 0x01, 0x01, 0x06, 0x06, 0x1F, 0x1F, 0x00},
    {0x60, 0x80, 0x80, 0x00, 0x00, 0xF8, 0xF8, 0x00},
};

static const unsigned char countdown_3[4][8] = {
    {0x00, 0x1F, 0x1F, 0x00, 0x00, 0x01, 0x01, 0x00},
    {0x00, 0xC0, 0xC0, 0x18, 0x18, 0xE0, 0xE0, 0x18},
    {0x00, 0x00, 0x00, 0x18, 0x18, 0x07, 0x07, 0x00},
    {0x18, 0x18, 0x18, 0x18, 0x18, 0xE0, 0xE0, 0x00},
};

/* Shared explosion effect, 16x16 px (2x2 chars): a jagged burst radiating
 * from the sprite's center (the shared corner where all four quadrants
 * meet), reused by every dying entity (see CLAUDE.md, "Sprite storage
 * format", "Key constraint") -- the ship is the first user (see game.c).
 * Frame 1 is a sparser scatter of the same debris, further from center, for
 * a two-frame "burst then fade" animation. TR/BL/BR are each a mirror of
 * TL (horizontal, vertical, both) rather than independently authored, so
 * the burst is symmetric by construction. */
static const unsigned char explosion_frame0[4][8] = {
    {0x10, 0x08, 0x98, 0x74, 0x3E, 0x1F, 0x8F, 0x43}, /* top-left */
    {0x08, 0x10, 0x19, 0x2E, 0x7C, 0xF8, 0xF1, 0xC2}, /* top-right */
    {0x43, 0x8F, 0x1F, 0x3E, 0x74, 0x98, 0x08, 0x10}, /* bottom-left */
    {0xC2, 0xF1, 0xF8, 0x7C, 0x2E, 0x19, 0x10, 0x08}, /* bottom-right */
};

static const unsigned char explosion_frame1[4][8] = {
    {0x00, 0x20, 0x00, 0x00, 0x08, 0x00, 0x00, 0x02},
    {0x00, 0x04, 0x00, 0x00, 0x10, 0x00, 0x00, 0x40},
    {0x02, 0x00, 0x00, 0x08, 0x00, 0x00, 0x20, 0x00},
    {0x40, 0x00, 0x00, 0x10, 0x00, 0x00, 0x04, 0x00},
};

/* Blank glyph: once the character generator points at our custom table,
 * PETSCII code $20 no longer means "space" -- it would read whatever
 * happens to sit at $1400+$20*8, uninitialized. CHAR_BLANK is an explicit
 * all-zero code used to clear screen cells instead. */
static const unsigned char blank[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void sprites_load(void) {
    /* Character base register ($9005/VICCR5): top nibble keeps the screen
     * matrix at $1000 (bits 1100, this project's fixed config -- see
     * CLAUDE.md "Graphics and colors"), low nibble moves the character
     * generator to $1400 RAM (bits 1101). */
    VIC.addr = 0xCD;

    load_quad(CHAR_APPLE_TL, apple_frame0);
    load_quad(CHAR_CARROT_TL, carrot_frame0);
    load_quad(CHAR_GRAPES_TL, grapes_frame0);
    load_quad(CHAR_PEPPER_TL, pepper_frame0);
    load_quad(CHAR_SHIP_TL, ship);
    load_char(CHAR_LIFE_SHIP, life_ship);
    load_char(CHAR_SHOT_L, shot_l);
    load_char(CHAR_SHOT_R, shot_r);
    load_char(CHAR_ENEMY_SHOT_L, enemy_shot_l);
    load_char(CHAR_ENEMY_SHOT_R, enemy_shot_r);
    load_char(CHAR_BLANK, blank);
}

void sprites_set_apple_frame(unsigned char frame) {
    load_quad(CHAR_APPLE_TL, frame ? apple_frame1 : apple_frame0);
}

void sprites_set_carrot_frame(unsigned char frame) {
    load_quad(CHAR_CARROT_TL, frame ? carrot_frame1 : carrot_frame0);
}

void sprites_set_grapes_frame(unsigned char frame) {
    load_quad(CHAR_GRAPES_TL, frame ? grapes_frame1 : grapes_frame0);
}

void sprites_set_pepper_frame(unsigned char frame) {
    load_quad(CHAR_PEPPER_TL, frame ? pepper_frame1 : pepper_frame0);
}

void sprites_set_countdown_digit(unsigned char digit) {
    switch (digit) {
    case 1:  load_quad(CHAR_COUNTDOWN_TL, countdown_1); break;
    case 2:  load_quad(CHAR_COUNTDOWN_TL, countdown_2); break;
    default: load_quad(CHAR_COUNTDOWN_TL, countdown_3); break;
    }
}

void sprites_set_explosion_frame(unsigned char frame) {
    load_quad(CHAR_EXPLOSION_TL, frame ? explosion_frame1 : explosion_frame0);
}
