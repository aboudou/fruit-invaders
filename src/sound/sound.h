/* Sound effects via the VIC-I's built-in sound generator: three square-wave
 * tone voices at $900A-$900C, a noise generator at $900D, and the master
 * volume in the low nibble of $900E (see CLAUDE.md, "Sound" in "Source code
 * organization" -- the VIC's own generator is enough for these simple
 * one-shot effects, no VIA timers/IRQ needed). Register format and the
 * chromatic note table sound_level_up()/sound_game_over() use their pitch
 * values from are both taken from VQRH12.pdf's "SOUND AND MUSIC" chapter
 * (p.45-46) and the memory map (p.61) -- see sound.c.
 */

#ifndef SOUND_H
#define SOUND_H

/* Sets the master volume ($900E low nibble) to its playing level and
 * silences every voice. Call once at startup, before any other sound_*
 * call (same convention as sprites_load()/font_load() in main.c). */
void sound_init(void);

/* Silences every voice (the three tone oscillators and the noise
 * generator) -- the counterpart to sound_explosion() below. */
void sound_stop(void);

/* Starts a noise burst for the shared explosion effect -- non-blocking, so
 * it plays underneath whatever visual timing the caller is already doing
 * (see explode_ship()/explode_fruit() in game.c, which bracket their
 * two-frame explosion animation with this and a matching sound_stop()).
 * Reused by both the ship's and a fruit's explosion, same "one shared
 * effect" approach as the visual explosion itself (see CLAUDE.md, "Sprite
 * storage format"). */
void sound_explosion(void);

/* Blocking ascending fanfare (~640ms) for clearing a level -- see
 * advance_level() in game.c, played alongside its green border flash. */
void sound_level_up(void);

/* Blocking descending tone (~880ms) for losing the last life -- see
 * show_lose_screen() in game.c. */
void sound_game_over(void);

/* Looping title-screen tune on voice2 (alto) -- unlike the effects above,
 * this is a non-blocking, tick-driven player so it never delays a Space
 * press: sound_music_start() resets it and plays the first note,
 * sound_music_tick() advances to the next note once its duration has
 * elapsed (a no-op otherwise, cheap enough to call from inside a tight
 * input-polling loop -- see title.c, which calls it on every pass of its
 * own wait/poll loop), and sound_music_stop() silences voice2 -- called
 * once Space is pressed, so the tune doesn't bleed into the game screen. */
void sound_music_start(void);
void sound_music_tick(void);
void sound_music_stop(void);

/* Toggles the title tune between audible and muted (volume only -- the
 * note sequence keeps advancing either way, so un-muting resumes exactly
 * in sync rather than restarting). Meant to be wired to the `M` key on the
 * title screen only (see title.c); has no effect on the one-shot gameplay
 * effects, which don't run at the same time as the title tune anyway. */
void sound_music_toggle_mute(void);

/* Whether the title tune is currently muted -- title.c uses this to
 * draw/hide its "MUSIC OFF" indicator. */
unsigned char sound_music_is_muted(void);

#endif
