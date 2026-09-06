#include "bigfont.h"

#include "charmem.h"
#include "font.h"    /* CHAR_FONT_END: base for this enum, same disjoint-range scheme */
#include "screen.h"
#include "sprites.h" /* CHAR_BLANK, APPLE_COLOR/CARROT_COLOR/GRAPES_COLOR/PEPPER_COLOR */

/* Big-font character codes, placed right after the small font's codes
 * (disjoint ranges in the same $1400 character set -- see charmem.h and
 * font.h's CHAR_FONT_END). Two codes per letter: top half then bottom half
 * (see bigfont.h for the vertical-doubling scheme). Only the letters used
 * by "FRUIT INVADERS" are defined. */
enum {
    CHAR_BIG_F_TOP = CHAR_FONT_END,
    CHAR_BIG_F_BOT,
    CHAR_BIG_R_TOP,
    CHAR_BIG_R_BOT,
    CHAR_BIG_U_TOP,
    CHAR_BIG_U_BOT,
    CHAR_BIG_I_TOP,
    CHAR_BIG_I_BOT,
    CHAR_BIG_T_TOP,
    CHAR_BIG_T_BOT,
    CHAR_BIG_N_TOP,
    CHAR_BIG_N_BOT,
    CHAR_BIG_V_TOP,
    CHAR_BIG_V_BOT,
    CHAR_BIG_A_TOP,
    CHAR_BIG_A_BOT,
    CHAR_BIG_D_TOP,
    CHAR_BIG_D_BOT,
    CHAR_BIG_E_TOP,
    CHAR_BIG_E_BOT,
    CHAR_BIG_S_TOP,
    CHAR_BIG_S_BOT,
};

/* Each pair below stretches font.c's small glyph_X[8] (5x7 -- row 7 is
 * always a blank inter-line spacer in that font, see font.c) over the full
 * 16-row-tall cell: only the 7 meaningful source rows (0-6) are used, each
 * shown at target row t = the source row s where s = (t*7)/16 for
 * t = 0..15 -- an even nearest-neighbor stretch (some source rows appear
 * twice, one three times, so 7 rows exactly fill 16) rather than a plain
 * 2x doubling that would leave the source's blank row 7 doubled into two
 * dead rows at the bottom of every letter. This is deliberate: an earlier
 * revision plain-doubled (including that blank spacer) for every letter
 * except S, which made S alone visibly taller/bolder than the rest --
 * rather than shrink S to match, every letter now fills the same full
 * height S accidentally had. Split into a top half (target rows 0-7) and
 * bottom half (rows 8-15). Authored "1 = the glyph's own body" like every
 * other sprite/font source array; charmem_load() applies the same
 * hardware inversion (see CLAUDE.md, "Sprite storage format"). */
static const unsigned char big_F_top[8] = {0xF8, 0xF8, 0xF8, 0x80, 0x80, 0x80, 0x80, 0xF0};
static const unsigned char big_F_bot[8] = {0xF0, 0xF0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};

static const unsigned char big_R_top[8] = {0xF0, 0xF0, 0xF0, 0x88, 0x88, 0x88, 0x88, 0xF0};
static const unsigned char big_R_bot[8] = {0xF0, 0xF0, 0xA0, 0xA0, 0x90, 0x90, 0x88, 0x88};

static const unsigned char big_U_top[8] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
static const unsigned char big_U_bot[8] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x70};

static const unsigned char big_I_top[8] = {0xF8, 0xF8, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20};
static const unsigned char big_I_bot[8] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xF8, 0xF8};

static const unsigned char big_T_top[8] = {0xF8, 0xF8, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20};
static const unsigned char big_T_bot[8] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};

static const unsigned char big_N_top[8] = {0x88, 0x88, 0x88, 0xC8, 0xC8, 0xA8, 0xA8, 0x98};
static const unsigned char big_N_bot[8] = {0x98, 0x98, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};

static const unsigned char big_V_top[8] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
static const unsigned char big_V_bot[8] = {0x88, 0x88, 0x88, 0x88, 0x50, 0x50, 0x20, 0x20};

static const unsigned char big_A_top[8] = {0x70, 0x70, 0x70, 0x88, 0x88, 0x88, 0x88, 0xF8};
static const unsigned char big_A_bot[8] = {0xF8, 0xF8, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};

static const unsigned char big_D_top[8] = {0xF0, 0xF0, 0xF0, 0x88, 0x88, 0x88, 0x88, 0x88};
static const unsigned char big_D_bot[8] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0xF0};

static const unsigned char big_E_top[8] = {0xF8, 0xF8, 0xF8, 0x80, 0x80, 0x80, 0x80, 0xF0};
static const unsigned char big_E_bot[8] = {0xF0, 0xF0, 0x80, 0x80, 0x80, 0x80, 0xF8, 0xF8};

static const unsigned char big_S_top[8] = {0x78, 0x78, 0x78, 0x80, 0x80, 0x80, 0x80, 0x70};
static const unsigned char big_S_bot[8] = {0x70, 0x70, 0x08, 0x08, 0x08, 0x08, 0xF0, 0xF0};

void bigfont_load(void) {
    charmem_load(CHAR_BIG_F_TOP, big_F_top, 8);
    charmem_load(CHAR_BIG_F_BOT, big_F_bot, 8);
    charmem_load(CHAR_BIG_R_TOP, big_R_top, 8);
    charmem_load(CHAR_BIG_R_BOT, big_R_bot, 8);
    charmem_load(CHAR_BIG_U_TOP, big_U_top, 8);
    charmem_load(CHAR_BIG_U_BOT, big_U_bot, 8);
    charmem_load(CHAR_BIG_I_TOP, big_I_top, 8);
    charmem_load(CHAR_BIG_I_BOT, big_I_bot, 8);
    charmem_load(CHAR_BIG_T_TOP, big_T_top, 8);
    charmem_load(CHAR_BIG_T_BOT, big_T_bot, 8);
    charmem_load(CHAR_BIG_N_TOP, big_N_top, 8);
    charmem_load(CHAR_BIG_N_BOT, big_N_bot, 8);
    charmem_load(CHAR_BIG_V_TOP, big_V_top, 8);
    charmem_load(CHAR_BIG_V_BOT, big_V_bot, 8);
    charmem_load(CHAR_BIG_A_TOP, big_A_top, 8);
    charmem_load(CHAR_BIG_A_BOT, big_A_bot, 8);
    charmem_load(CHAR_BIG_D_TOP, big_D_top, 8);
    charmem_load(CHAR_BIG_D_BOT, big_D_bot, 8);
    charmem_load(CHAR_BIG_E_TOP, big_E_top, 8);
    charmem_load(CHAR_BIG_E_BOT, big_E_bot, 8);
    charmem_load(CHAR_BIG_S_TOP, big_S_top, 8);
    charmem_load(CHAR_BIG_S_BOT, big_S_bot, 8);
}

static void code_for(char c, unsigned char *top, unsigned char *bot) {
    switch (c) {
    case 'F': *top = CHAR_BIG_F_TOP; *bot = CHAR_BIG_F_BOT; return;
    case 'R': *top = CHAR_BIG_R_TOP; *bot = CHAR_BIG_R_BOT; return;
    case 'U': *top = CHAR_BIG_U_TOP; *bot = CHAR_BIG_U_BOT; return;
    case 'I': *top = CHAR_BIG_I_TOP; *bot = CHAR_BIG_I_BOT; return;
    case 'T': *top = CHAR_BIG_T_TOP; *bot = CHAR_BIG_T_BOT; return;
    case 'N': *top = CHAR_BIG_N_TOP; *bot = CHAR_BIG_N_BOT; return;
    case 'V': *top = CHAR_BIG_V_TOP; *bot = CHAR_BIG_V_BOT; return;
    case 'A': *top = CHAR_BIG_A_TOP; *bot = CHAR_BIG_A_BOT; return;
    case 'D': *top = CHAR_BIG_D_TOP; *bot = CHAR_BIG_D_BOT; return;
    case 'E': *top = CHAR_BIG_E_TOP; *bot = CHAR_BIG_E_BOT; return;
    case 'S': *top = CHAR_BIG_S_TOP; *bot = CHAR_BIG_S_BOT; return;
    default:  *top = CHAR_BLANK;     *bot = CHAR_BLANK;     return; /* space, and anything else */
    }
}

/* Per-letter color cycle -- the same four colors already used for the
 * apple/carrot/grapes/pepper sprites (see sprites.h), so the title's
 * palette visibly ties back to the fruits animated below it rather than
 * introducing an unrelated one. */
static const unsigned char PALETTE[] = {APPLE_COLOR, CARROT_COLOR, GRAPES_COLOR, PEPPER_COLOR};
#define PALETTE_LEN (sizeof(PALETTE) / sizeof(PALETTE[0]))

void bigfont_print(unsigned char row, unsigned char col, const char *text) {
    unsigned char i = 0;
    unsigned char top, bot, color;

    while (*text) {
        code_for(*text, &top, &bot);
        color = PALETTE[i % PALETTE_LEN];
        screen_put(row, col, top, color);
        screen_put(row + 1, col, bot, color);
        col++;
        text++;
        i++;
    }
}
