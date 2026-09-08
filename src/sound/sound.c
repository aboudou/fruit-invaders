#include "sound.h"

#include <vic20.h> /* VIC struct (voice1/2/3, noise, volume_color -- $900A-$900E) */

/* KERNAL jiffy clock, low byte -- same use as game.c/title.c (paces the
 * blocking effects below without needing a timer IRQ). */
#define JIFFY_LOW ((volatile unsigned char *)0x00A2)

/* $900E low nibble (0-15) -- see VQRH12.pdf p.61, "VICCRE: bit 3-0 volume".
 * The high nibble is the multicolor auxiliary color, unused anywhere in
 * this project (see CLAUDE.md, "Graphics and colors") but preserved below
 * rather than assumed zero, in case that changes later. */
#define MASTER_VOLUME 15

/* Volume while the title tune plays -- quieter than MASTER_VOLUME so the
 * background music doesn't compete with the (louder) one-shot effects.
 * There's only the one volume register ($900E), shared by every voice (see
 * VQRH12.pdf p.61 -- no per-voice volume exists on this hardware), so this
 * is swapped in by sound_music_start() and back out by sound_music_stop()
 * rather than being a separate, simultaneous level. */
#define MUSIC_VOLUME 6

/* Noise generator ($900D) byte for the explosion burst: bit 7 enables it,
 * bits 6-0 shape the noise's character -- no note table applies here (see
 * VQRH12.pdf p.61, "VICCRD: bit 6-0 oscillator range"), unlike the three
 * tone voices below, which do follow one. A low range value reads as a
 * deeper, "boomier" crunch than a high one (confirmed by ear in VICE). */
#define EXPLOSION_NOISE 0x90

/* Chromatic note values for the three tone voices ($900A-$900C), taken
 * directly from VQRH12.pdf's "SOUND AND MUSIC" chapter (p.45): bit 7 (the
 * enable switch) is already included in each value below, so these can be
 * written to a voice register as-is. All three voices share this same
 * byte encoding (see the register map, p.61) -- only their physical octave
 * range differs by convention, not the format -- so any of them can play
 * any of these notes. */
#define NOTE_E1  153
#define NOTE_F1  159
#define NOTE_G1  170
#define NOTE_A1  179
#define NOTE_C2  191
#define NOTE_D2  198
#define NOTE_DS2 201
#define NOTE_E2  204
#define NOTE_F2  207
#define NOTE_G2  212
#define NOTE_A2  217
#define NOTE_B2  221
#define NOTE_C3  223
#define NOTE_D3  226
#define NOTE_E3  230

#define TONE_JIFFIES 8 /* ~160ms at 50Hz -- per-note pace shared by both tunes below */

/* Title-screen tune, voice2 (alto) -- three 16-step phrases (a bouncy C-F-G
 * "question", a higher, descending "answer", then a lower, syncopated
 * "bridge" that dips down an octave for contrast before climbing back up
 * to where the question started), each ending on a 0 (rest) so every
 * phrase join -- mid-loop or end-of-loop -- reads as a phrase end rather
 * than a note just being cut off. Three short phrases instead of one long
 * one so the tune has some shape over its now-longer loop rather than just
 * repeating -- see sound_music_tick() for how it steps through and loops.
 */
static const unsigned char music_notes[] = {
    /* "Question": rising bounce through C, F, G */
    NOTE_C2, NOTE_E2, NOTE_G2, NOTE_E2,
    NOTE_F2, NOTE_A2, NOTE_G2, NOTE_E2,
    NOTE_D2, NOTE_F2, NOTE_A2, NOTE_F2,
    NOTE_G2, NOTE_E2, NOTE_C2, 0,
    /* "Answer": higher, descending back down to loop into the question again */
    NOTE_C3, NOTE_B2, NOTE_A2, NOTE_G2,
    NOTE_E3, NOTE_D3, NOTE_C3, NOTE_G2,
    NOTE_F2, NOTE_E2, NOTE_D2, NOTE_C2,
    NOTE_E2, NOTE_G2, NOTE_C3, 0,
    /* "Bridge": lower octave, syncopated, climbing back up to loop into the
     * question again */
    NOTE_G1, NOTE_A1, NOTE_C2, NOTE_A1,
    NOTE_F1, NOTE_G1, NOTE_A1, NOTE_F1,
    NOTE_E1, NOTE_G1, NOTE_C2, NOTE_G1,
    NOTE_D2, NOTE_C2, NOTE_G1, 0,
};
#define MUSIC_STEP_COUNT (sizeof(music_notes) / sizeof(music_notes[0]))
#define MUSIC_STEP_JIFFIES 12 /* ~240ms at 50Hz -- per-step tempo, ~11.5s per loop (48 steps) */

static unsigned char music_index;
static unsigned char music_last_jiffy;
static unsigned char music_muted;

static void wait_jiffies(unsigned char n) {
    unsigned char start = *JIFFY_LOW;
    while ((unsigned char)(*JIFFY_LOW - start) < n) {
        /* busy-wait */
    }
}

void sound_init(void) {
    VIC.volume_color = (VIC.volume_color & 0xF0) | MASTER_VOLUME;
    sound_stop();
}

void sound_stop(void) {
    VIC.voice1 = 0;
    VIC.voice2 = 0;
    VIC.voice3 = 0;
    VIC.noise = 0;
}

void sound_explosion(void) {
    VIC.noise = EXPLOSION_NOISE;
}

/* Rising C-E-G-C major arpeggio on voice3 (soprano) -- a bright "success"
 * chime for clearing a level. */
void sound_level_up(void) {
    VIC.voice3 = NOTE_C2;
    wait_jiffies(TONE_JIFFIES);
    VIC.voice3 = NOTE_E2;
    wait_jiffies(TONE_JIFFIES);
    VIC.voice3 = NOTE_G2;
    wait_jiffies(TONE_JIFFIES);
    VIC.voice3 = NOTE_C3;
    wait_jiffies(TONE_JIFFIES);
    VIC.voice3 = 0;
}

/* Falling G-D#-C-G motif on voice1 (bass) -- a somber "game over" tone,
 * each note held a little longer than the last, for losing the final life. */
void sound_game_over(void) {
    VIC.voice1 = NOTE_G2;
    wait_jiffies(TONE_JIFFIES * 2);
    VIC.voice1 = NOTE_DS2;
    wait_jiffies(TONE_JIFFIES * 2);
    VIC.voice1 = NOTE_C2;
    wait_jiffies(TONE_JIFFIES * 2);
    VIC.voice1 = NOTE_G1;
    wait_jiffies(TONE_JIFFIES * 3);
    VIC.voice1 = 0;
}

/* Resets the title tune to its first step and starts it playing, at
 * MUSIC_VOLUME or silent depending on the current mute state. Call once
 * when the title screen is (re-)entered -- mute is a persistent player
 * preference (see sound_music_toggle_mute()), not reset here, so a mute set
 * on an earlier visit (title or help screen) stays in effect across a lost
 * game or `H` back to the title screen. */
void sound_music_start(void) {
    VIC.volume_color = (VIC.volume_color & 0xF0) | (music_muted ? 0 : MUSIC_VOLUME);
    music_index = 0;
    VIC.voice2 = music_notes[0];
    music_last_jiffy = *JIFFY_LOW;
}

/* Advances the title tune by one step once MUSIC_STEP_JIFFIES have elapsed
 * since the last step, looping back to the start past the last one -- a
 * no-op otherwise. Non-blocking by design: meant to be called from inside
 * whatever polling loop is already checking for input, so the tune plays
 * without ever delaying that loop's own responsiveness (see title.c). */
void sound_music_tick(void) {
    if ((unsigned char)(*JIFFY_LOW - music_last_jiffy) < MUSIC_STEP_JIFFIES) {
        return;
    }
    music_last_jiffy = *JIFFY_LOW;
    music_index++;
    if (music_index >= MUSIC_STEP_COUNT) {
        music_index = 0;
    }
    VIC.voice2 = music_notes[music_index];
}

/* Toggles whether the title tune is audible, purely via volume (0 when
 * muted, MUSIC_VOLUME when not) -- sound_music_tick() keeps writing notes
 * to voice2 either way, so the sequence keeps advancing while muted and
 * un-muting resumes exactly in sync rather than restarting the tune. Call
 * from title.c's or help.c's `M` key handler -- the resulting music_muted
 * state persists across screens (see sound_music_start()). */
void sound_music_toggle_mute(void) {
    music_muted = !music_muted;
    VIC.volume_color = (VIC.volume_color & 0xF0) | (music_muted ? 0 : MUSIC_VOLUME);
}

/* Whether the title tune is currently muted (see sound_music_toggle_mute())
 * -- title.c uses this to draw/hide its "MUSIC OFF" indicator. */
unsigned char sound_music_is_muted(void) {
    return music_muted;
}

/* Silences voice2 and restores MASTER_VOLUME (see sound_music_start()) --
 * called once Space is pressed, so gameplay's own effects play back at
 * their normal, louder level rather than the music's quieter one. */
void sound_music_stop(void) {
    VIC.voice2 = 0;
    VIC.volume_color = (VIC.volume_color & 0xF0) | MASTER_VOLUME;
}
