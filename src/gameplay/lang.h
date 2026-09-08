/* UI language: English (default) or French, toggled by L on the title
 * screen only (see title.c) -- the choice then also applies to the game
 * screen's own text (HUD, lose screen), which never changes it itself. The
 * game's name ("FRUIT INVADERS", drawn with bigfont.c) is never translated
 * -- only the plain-font strings below are. Every accessor below returns
 * the fixed string for whichever language is current; callers never branch
 * on the language themselves.
 */

#ifndef LANG_H
#define LANG_H

enum { LANG_EN = 0, LANG_FR = 1 };

/* Switches to the other language. Call only from the title screen (see
 * title.c's 'L' handling) -- the game screen has no control that calls
 * this. */
void lang_toggle(void);

/* Returns the current language (LANG_EN or LANG_FR). Starts as LANG_EN
 * (BSS-zeroed) every time the program starts -- there is no persistence
 * across runs, per CLAUDE.md-requested behavior ("the game begins in
 * English"). */
unsigned char lang_current(void);

const char *lang_mute_hint(void);     /* "M MUTE MUSIC" */
const char *lang_music_off(void);     /* "MUSIC OFF" */
const char *lang_start_prompt(void);  /* "PRESS SPACE TO START" */
const char *lang_level_label(void);   /* "LEVEL" -- game.c appends ":NN" itself */
const char *lang_lose_title(void);    /* "YOU LOSE" */
const char *lang_lose_prompt(void);   /* "PRESS SPACE" */

/* Help screen strings (see help.c) -- lang_help_hint() is also used by the
 * title screen itself, which no longer spells out each control directly
 * (see title.c) and instead points at the help screen for that. The rest
 * are only ever drawn by help.c: one line per control, plus a section
 * header marking which screen it's available on (see help.c's layout) and
 * a blinking prompt to leave the screen (F1 again, mirroring the title
 * screen's Space prompt). */
const char *lang_help_hint(void);            /* "F1: HELP" */
const char *lang_help_section_title(void);   /* "TITLE SCREEN" */
const char *lang_help_section_game(void);    /* "IN GAME" */
const char *lang_help_line_language(void);   /* "L      LANGUAGE" */
const char *lang_help_line_mute(void);       /* "M      MUTE MUSIC" */
const char *lang_help_line_help(void);       /* "F1     HELP" */
const char *lang_help_line_move(void);       /* "S/D    MOVE" */
const char *lang_help_line_fire(void);       /* "SPACE  FIRE" */
const char *lang_help_line_pause(void);      /* "P      PAUSE" */
const char *lang_help_line_title_screen(void); /* "H      TITLE SCREEN" */
const char *lang_help_back_prompt(void);     /* "F1: BACK" */

#endif
