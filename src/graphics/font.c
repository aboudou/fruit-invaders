#include "font.h"

#include "charmem.h"
#include "screen.h"
/* CHAR_FONT_* enum and CHAR_BLANK (the fallback glyph) now come from font.h/
 * sprites.h -- see font.h for why the enum moved there. */

/* 8x8 glyphs, hand-authored as blocky 5x7 letterforms: 5 pixels wide
 * (packed into bits 7-3, leaving a blank margin in bits 2-0 for inter-letter
 * spacing) by 7 pixels tall (row 7 left blank for inter-line spacing).
 * Authored "1 = the glyph's own body" like the sprite bitmaps in sprites.c
 * -- charmem_load() applies the same hardware inversion. Only the letters
 * used by title.c's and game.c's strings are defined. */
static const unsigned char glyph_A[8]     = {0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00};
static const unsigned char glyph_C[8]     = {0x78, 0x80, 0x80, 0x80, 0x80, 0x80, 0x78, 0x00};
static const unsigned char glyph_D[8]     = {0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0x00};
static const unsigned char glyph_E[8]     = {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0x00};
static const unsigned char glyph_F[8]     = {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80, 0x00};
static const unsigned char glyph_G[8]     = {0x78, 0x80, 0x80, 0xB8, 0x88, 0x88, 0x78, 0x00};
static const unsigned char glyph_H[8]     = {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00};
static const unsigned char glyph_I[8]     = {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0xF8, 0x00};
static const unsigned char glyph_K[8]     = {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88, 0x00};
static const unsigned char glyph_L[8]     = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8, 0x00};
static const unsigned char glyph_M[8]     = {0x88, 0xD8, 0xA8, 0x88, 0x88, 0x88, 0x88, 0x00};
static const unsigned char glyph_N[8]     = {0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x88, 0x00};
static const unsigned char glyph_O[8]     = {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00};
static const unsigned char glyph_P[8]     = {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80, 0x00};
static const unsigned char glyph_R[8]     = {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88, 0x00};
static const unsigned char glyph_S[8]     = {0x78, 0x80, 0x80, 0x70, 0x08, 0x08, 0xF0, 0x00};
static const unsigned char glyph_T[8]     = {0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00};
static const unsigned char glyph_U[8]     = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00};
static const unsigned char glyph_V[8]     = {0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20, 0x00};
static const unsigned char glyph_W[8]     = {0x88, 0x88, 0x88, 0xA8, 0xA8, 0xD8, 0x88, 0x00};
static const unsigned char glyph_Y[8]     = {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20, 0x00};
static const unsigned char glyph_SLASH[8] = {0x08, 0x10, 0x20, 0x20, 0x20, 0x40, 0x80, 0x00};
static const unsigned char glyph_COLON[8] = {0x00, 0x20, 0x20, 0x00, 0x20, 0x20, 0x00, 0x00};

/* Digits, same blocky 5x7 style as the letters above -- needed for the HUD
 * level counter (see game.c, "LEVEL:01"). */
static const unsigned char glyph_0[8] = {0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70, 0x00};
static const unsigned char glyph_1[8] = {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70, 0x00};
static const unsigned char glyph_2[8] = {0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xF8, 0x00};
static const unsigned char glyph_3[8] = {0xF0, 0x08, 0x10, 0x30, 0x08, 0x08, 0xF0, 0x00};
static const unsigned char glyph_4[8] = {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10, 0x00};
static const unsigned char glyph_5[8] = {0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70, 0x00};
static const unsigned char glyph_6[8] = {0x30, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70, 0x00};
static const unsigned char glyph_7[8] = {0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40, 0x00};
static const unsigned char glyph_8[8] = {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00};
static const unsigned char glyph_9[8] = {0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60, 0x00};

void font_load(void) {
    charmem_load(CHAR_FONT_A, glyph_A, 8);
    charmem_load(CHAR_FONT_C, glyph_C, 8);
    charmem_load(CHAR_FONT_D, glyph_D, 8);
    charmem_load(CHAR_FONT_E, glyph_E, 8);
    charmem_load(CHAR_FONT_F, glyph_F, 8);
    charmem_load(CHAR_FONT_G, glyph_G, 8);
    charmem_load(CHAR_FONT_H, glyph_H, 8);
    charmem_load(CHAR_FONT_I, glyph_I, 8);
    charmem_load(CHAR_FONT_K, glyph_K, 8);
    charmem_load(CHAR_FONT_L, glyph_L, 8);
    charmem_load(CHAR_FONT_M, glyph_M, 8);
    charmem_load(CHAR_FONT_N, glyph_N, 8);
    charmem_load(CHAR_FONT_O, glyph_O, 8);
    charmem_load(CHAR_FONT_P, glyph_P, 8);
    charmem_load(CHAR_FONT_R, glyph_R, 8);
    charmem_load(CHAR_FONT_S, glyph_S, 8);
    charmem_load(CHAR_FONT_T, glyph_T, 8);
    charmem_load(CHAR_FONT_U, glyph_U, 8);
    charmem_load(CHAR_FONT_V, glyph_V, 8);
    charmem_load(CHAR_FONT_W, glyph_W, 8);
    charmem_load(CHAR_FONT_Y, glyph_Y, 8);
    charmem_load(CHAR_FONT_SLASH, glyph_SLASH, 8);
    charmem_load(CHAR_FONT_COLON, glyph_COLON, 8);
    charmem_load(CHAR_FONT_0, glyph_0, 8);
    charmem_load(CHAR_FONT_1, glyph_1, 8);
    charmem_load(CHAR_FONT_2, glyph_2, 8);
    charmem_load(CHAR_FONT_3, glyph_3, 8);
    charmem_load(CHAR_FONT_4, glyph_4, 8);
    charmem_load(CHAR_FONT_5, glyph_5, 8);
    charmem_load(CHAR_FONT_6, glyph_6, 8);
    charmem_load(CHAR_FONT_7, glyph_7, 8);
    charmem_load(CHAR_FONT_8, glyph_8, 8);
    charmem_load(CHAR_FONT_9, glyph_9, 8);
}

static unsigned char code_for(char c) {
    switch (c) {
    case 'A': return CHAR_FONT_A;
    case 'C': return CHAR_FONT_C;
    case 'D': return CHAR_FONT_D;
    case 'E': return CHAR_FONT_E;
    case 'F': return CHAR_FONT_F;
    case 'G': return CHAR_FONT_G;
    case 'H': return CHAR_FONT_H;
    case 'I': return CHAR_FONT_I;
    case 'K': return CHAR_FONT_K;
    case 'L': return CHAR_FONT_L;
    case 'M': return CHAR_FONT_M;
    case 'N': return CHAR_FONT_N;
    case 'O': return CHAR_FONT_O;
    case 'P': return CHAR_FONT_P;
    case 'R': return CHAR_FONT_R;
    case 'S': return CHAR_FONT_S;
    case 'T': return CHAR_FONT_T;
    case 'U': return CHAR_FONT_U;
    case 'V': return CHAR_FONT_V;
    case 'W': return CHAR_FONT_W;
    case 'Y': return CHAR_FONT_Y;
    case '/': return CHAR_FONT_SLASH;
    case ':': return CHAR_FONT_COLON;
    case '0': return CHAR_FONT_0;
    case '1': return CHAR_FONT_1;
    case '2': return CHAR_FONT_2;
    case '3': return CHAR_FONT_3;
    case '4': return CHAR_FONT_4;
    case '5': return CHAR_FONT_5;
    case '6': return CHAR_FONT_6;
    case '7': return CHAR_FONT_7;
    case '8': return CHAR_FONT_8;
    case '9': return CHAR_FONT_9;
    default:  return CHAR_BLANK; /* space, and anything else not covered */
    }
}

void font_print(unsigned char row, unsigned char col, const char *text, unsigned char color) {
    while (*text) {
        screen_put(row, col, code_for(*text), color);
        col++;
        text++;
    }
}
