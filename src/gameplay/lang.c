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

const char *lang_help_hint(void) {
    return current_lang == LANG_FR ? "F1: AIDE" : "F1: HELP";
}

const char *lang_help_section_title(void) {
    return current_lang == LANG_FR ? "ECRAN TITRE" : "TITLE SCREEN";
}

const char *lang_help_section_game(void) {
    return current_lang == LANG_FR ? "EN PARTIE" : "IN GAME";
}

const char *lang_help_line_language(void) {
    return current_lang == LANG_FR ? "L      LANGUE" : "L      LANGUAGE";
}

const char *lang_help_line_mute(void) {
    return current_lang == LANG_FR ? "M      COUPER MUSIQUE" : "M      MUTE MUSIC";
}

const char *lang_help_line_help(void) {
    return current_lang == LANG_FR ? "F1     AIDE" : "F1     HELP";
}

const char *lang_help_line_move(void) {
    return current_lang == LANG_FR ? "S/D    BOUGER" : "S/D    MOVE";
}

const char *lang_help_line_fire(void) {
    return current_lang == LANG_FR ? "ESPACE TIR" : "SPACE  FIRE";
}

const char *lang_help_line_pause(void) {
    return current_lang == LANG_FR ? "P      PAUSE" : "P      PAUSE";
}

const char *lang_help_line_title_screen(void) {
    return current_lang == LANG_FR ? "H      ECRAN TITRE" : "H      TITLE SCREEN";
}

const char *lang_help_back_prompt(void) {
    return current_lang == LANG_FR ? "F1: RETOUR" : "F1: BACK";
}
