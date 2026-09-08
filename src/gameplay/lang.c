#include "lang.h"

/* French strings are hand-picked to fit font.c's uppercase-only, unaccented
 * glyph set (see font.c -- adding B/Q/Z there was needed for BOUGER/
 * MUSIQUE/APPUYEZ below, the only three letters no English string here used)
 * and to stay within whatever screen width each string's caller already
 * budgets for it (see title.c's *_FIELD_WIDTH constants and game.c's
 * centering, which account for French usually running longer than
 * English). */
static unsigned char current_lang = LANG_EN;

void lang_toggle(void) {
    current_lang = current_lang == LANG_EN ? LANG_FR : LANG_EN;
}

unsigned char lang_current(void) {
    return current_lang;
}

const char *lang_controls_hint(void) {
    return current_lang == LANG_FR ? "S/D BOUGER ESPACE TIR" : "S/D MOVE  SPACE FIRE";
}

const char *lang_mute_hint(void) {
    return current_lang == LANG_FR ? "M COUPER MUSIQUE" : "M MUTE MUSIC";
}

const char *lang_music_off(void) {
    return current_lang == LANG_FR ? "MUSIQUE OFF" : "MUSIC OFF";
}

const char *lang_start_prompt(void) {
    return current_lang == LANG_FR ? "APPUYEZ SUR ESPACE" : "PRESS SPACE TO START";
}

const char *lang_level_label(void) {
    return current_lang == LANG_FR ? "NIVEAU" : "LEVEL";
}

const char *lang_lose_title(void) {
    return current_lang == LANG_FR ? "PARTIE FINIE" : "YOU LOSE";
}

const char *lang_lose_prompt(void) {
    return current_lang == LANG_FR ? "APPUYEZ SUR ESPACE" : "PRESS SPACE";
}
