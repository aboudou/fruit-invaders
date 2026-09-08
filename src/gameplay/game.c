#include "game.h"

#include "../graphics/decor.h" /* CHAR_STAR */
#include "../graphics/font.h"
#include "../graphics/screen.h"
#include "../graphics/sprites.h"
#include "../graphics/starfield.h"
#include "../sound/sound.h"
#include "lang.h"

#include <cbm.h>    /* cbm_k_getin(): KERNAL keyboard-buffer read, also pulls in vic20.h's COLOR_* */
#include <stdlib.h> /* rand()/srand(): shooter selection and fire-timing stagger, see shooters_select() */
#include <string.h> /* strlen(): centers/right-aligns lang.h's language-dependent strings below */

/* Screen layout (22x23, see screen.h): row 0 is the HUD (lives left, level
 * right); the last two rows are the ship's horizontal movement lane (see
 * SHIP_ROW below -- the 2x2 ship sprite is exactly 2 rows tall); everything
 * in between (rows 1-20) is the game zone where fruit invaders will live. */
#define HUD_ROW 0

/* Lives counter: one CHAR_LIFE_SHIP icon per remaining life, left-aligned
 * from col 0 -- an icon reads at a glance faster than a "LIVES: n" label
 * would on a 22-column screen. MAX_LIVES reserves enough cells to blank a
 * lost life's icon by index rather than having to shift the remaining ones
 * left. Also the cap on the extra-life bonus awarded every LIFE_BONUS_LEVELS
 * levels (see advance_level()). */
#define MAX_LIVES         5
#define STARTING_LIVES    3
#define LIFE_BONUS_LEVELS 5

/* Level counter: right-aligned "LEVEL:NN"/"NIVEAU:NN" (see lang.h) ending
 * at the last column, zero-padded to 2 digits per the spec (starts at 01).
 * Advances when every fruit is cleared (see advance_level()) -- what else
 * should change between levels (harder pace, a different formation, ...) is
 * left for later (see CLAUDE.md, "Levels"). */
#define STARTING_LEVEL 1

/* Starfield background: a fixed scatter of twinkling stars behind the fruit
 * grid/ship/shots (see starfield.c/h for the shared position table, also
 * used by the title screen -- see CLAUDE.md, "Title screen"). Unlike the
 * title screen, this screen's sprites roam over the whole field, so every
 * place a sprite's trail is erased has to restore whichever star (if any)
 * was underneath instead of blanking to CHAR_BLANK --
 * bg_clear_cell()/bg_clear_quad()/bg_clear_pair() below stand in for a plain
 * screen_clear_quad()/screen_clear_pair() call everywhere that would
 * otherwise permanently erase a star. star_blink is this screen's own
 * twinkle phase (reset to 0 each time game_screen_run() starts, see below),
 * advanced on the same GRID_ANIM_JIFFIES tick that already drives the fruit
 * wobble (see game_screen_run()'s main loop) -- star_twinkle() only
 * recolors a cell that's still actually showing CHAR_STAR right now
 * (screen_get(), see screen.h), so a star currently covered by the grid/
 * ship/a shot is correctly left alone instead of being painted over
 * whatever's really there. */
static unsigned char star_blink;

static void bg_clear_cell(unsigned char row, unsigned char col) {
    unsigned char color;

    if (starfield_lookup(row, col, star_blink, &color)) {
        screen_put(row, col, CHAR_STAR, color);
    } else {
        screen_put(row, col, CHAR_BLANK, COLOR_BLACK);
    }
}

static void bg_clear_quad(unsigned char row, unsigned char col) {
    bg_clear_cell(row, col);
    bg_clear_cell(row, (unsigned char)(col + 1));
    bg_clear_cell((unsigned char)(row + 1), col);
    bg_clear_cell((unsigned char)(row + 1), (unsigned char)(col + 1));
}

static void bg_clear_pair(unsigned char row, unsigned char col) {
    bg_clear_cell(row, col);
    bg_clear_cell(row, (unsigned char)(col + 1));
}

static void star_twinkle(void) {
    unsigned char i, color;

    star_blink ^= 1;
    for (i = 0; i < STARFIELD_COUNT; i++) {
        if (screen_get(STARFIELD_ROW[i], STARFIELD_COL[i]) == CHAR_STAR) {
            color = starfield_twinkle_color(i, star_blink);
            screen_put(STARFIELD_ROW[i], STARFIELD_COL[i], CHAR_STAR, color);
        }
    }
}

/* Fruit grid: fills the top of the game zone with a formation of 2x2 fruit
 * sprites, top/left-aligned, each one character apart from its neighbours
 * in both directions. GRID_COLS/GRID_ROWS are the largest formation that
 * still leaves at least 5 free columns to the right (across the full
 * 22-column screen width) and at least 5 free rows below the formation
 * (within the 20-row game zone, rows 1-20 -- see "Screen layout" above), so
 * the whole grid can later shift sideways and step downward without
 * leaving the game zone or reaching the ship's lane. Movement itself isn't
 * implemented yet -- this only places the initial formation.
 *
 * Each fruit's footprint including its trailing gap is 3 cells (2 for the
 * 2x2 sprite + 1 blank gap), except the last fruit along a row/column,
 * which doesn't need a trailing gap -- so N fruits along an axis span
 * (3*N - 1) cells.
 *   Columns (22 wide):  3*N - 1 + 5 <= 22  =>  N <= 6  (span 17, margin 5)
 *   Rows (20-row zone):  3*M - 1 + 5 <= 20  =>  M <= 5  (span 14, margin 6)
 */
#define GRID_COLS      6
#define GRID_ROWS      5
#define GRID_COL_START 0
#define GRID_ROW_START (HUD_ROW + 1)
#define GRID_STEP      3 /* 2-cell sprite + 1-cell gap, same on both axes */

/* Grid movement: the whole formation shifts sideways one character per step
 * until its leading edge would run off the screen, then steps down one
 * character row and reverses direction -- classic invaders marching. Only
 * col_offset/row_offset (added to every fruit's base position below) and
 * the marching direction change; GRID_COLS/GRID_ROWS/the base positions
 * above stay fixed.
 *
 * The bounce points are computed fresh every tick from whichever columns
 * still have a live fruit in them (see grid_alive_col_bounds() below), not
 * from the original GRID_COLS-1..0 span -- if the player has cleared out an
 * edge column, the formation should bounce off the screen edge as soon as
 * the new outermost surviving column reaches it, not wait for where the
 * (now empty) original edge column would have. col_offset is signed
 * because that surviving span can sit anywhere within the original
 * formation's footprint, including left of where column 0 started (e.g.
 * column 0 itself dead, column 2 now the leftmost survivor: the formation
 * can shift further left than col_offset==0 used to allow, since column 0's
 * position no longer needs to stay on-screen). There's no equivalent row
 * cap: the formation is meant to keep stepping down (and shedding its
 * bottom row, see game_screen_run()) for as long as the game runs. */
#define GRID_DIR_RIGHT 1
#define GRID_DIR_LEFT  0

/* Marching pace: starts slow, shaved down by GRID_MOVE_SPEEDUP jiffies each
 * time the formation steps down a row (per CLAUDE.md-requested difficulty
 * ramp), floored at GRID_MOVE_JIFFIES_MIN so it never becomes unplayably
 * fast (or reaches 0, which would stall the unsigned-char countdown in
 * game_screen_run() below). */
#define GRID_MOVE_JIFFIES_BASE 30 /* ~600ms at 50Hz per step, starting pace */
#define GRID_MOVE_JIFFIES_MIN   5 /* ~100ms at 50Hz, fastest allowed pace */
#define GRID_MOVE_SPEEDUP       2 /* shaved off the pace on each step-down */

/* One fruit type per grid row, cycling through the four available types --
 * every cell in a row shares that type's character codes and therefore
 * animates in lockstep with the rest of that type on screen (see
 * CLAUDE.md, "Sprite storage format"). Rows are consumed from the bottom up
 * as the formation reaches the ship's lane (see game_screen_run()), so
 * active_rows below always keeps rows 0..active_rows-1 -- row index alone
 * (not a separate alive flag per row) is enough to know what's still live. */
static const unsigned char fruit_row_code[4] = {
    CHAR_APPLE_TL, CHAR_CARROT_TL, CHAR_GRAPES_TL, CHAR_PEPPER_TL,
};
static const unsigned char fruit_row_color[4] = {
    APPLE_COLOR, CARROT_COLOR, GRAPES_COLOR, PEPPER_COLOR,
};

/* Per-fruit alive state, independent of active_rows: active_rows drops a
 * whole row at once (it reached the ship's lane, see game_screen_run()),
 * while a single fruit dies when the shot hits it (see grid_check_hit()
 * below) -- a row can therefore still be drawn (r < active_rows) with some
 * of its fruits already gone. Reset to all-alive at the start of every game
 * and every new level (see grid_reset_alive()). */
static unsigned char fruit_alive[GRID_ROWS][GRID_COLS];

static void grid_reset_alive(void) {
    unsigned char r, c;

    for (r = 0; r < GRID_ROWS; r++) {
        for (c = 0; c < GRID_COLS; c++) {
            fruit_alive[r][c] = 1;
        }
    }
}

/* Draws the still-alive fruits among the active rows (0..active_rows-1) at
 * the formation's current position (col_offset/row_offset added to every
 * fruit's base grid position). */
static void grid_draw(signed char col_offset, unsigned char row_offset,
                       unsigned char active_rows) {
    unsigned char r, c;

    for (r = 0; r < active_rows; r++) {
        for (c = 0; c < GRID_COLS; c++) {
            if (fruit_alive[r][c]) {
                screen_put_quad(GRID_ROW_START + r * GRID_STEP + row_offset,
                                 GRID_COL_START + c * GRID_STEP + col_offset,
                                 fruit_row_code[r % 4], fruit_row_color[r % 4]);
            }
        }
    }
}

/* Blanks the still-alive fruits among the active rows at their current
 * position -- the counterpart to grid_draw(), called before the formation
 * moves so its old position doesn't linger on screen. Dead fruits are
 * already blank (grid_check_hit()'s caller clears them individually), so
 * skipping them here isn't required for correctness, just avoids a
 * redundant clear. */
static void grid_erase(signed char col_offset, unsigned char row_offset,
                        unsigned char active_rows) {
    unsigned char r, c;

    for (r = 0; r < active_rows; r++) {
        for (c = 0; c < GRID_COLS; c++) {
            if (fruit_alive[r][c]) {
                bg_clear_quad(GRID_ROW_START + r * GRID_STEP + row_offset,
                               GRID_COL_START + c * GRID_STEP + col_offset);
            }
        }
    }
}

/* Checks whether the shot at (shot_row, shot_col) overlaps an alive fruit
 * in the formation at its current position, and if so, returns 1 with
 * *out_r and *out_c set to that fruit's grid indices (0-based). A fruit's
 * screen footprint is (fruit_row, fruit_row+1) x (fruit_col, fruit_col+1);
 * the shot's is (shot_row) x (shot_col, shot_col+1) -- same width as a
 * fruit (see CLAUDE.md, "Sprite storage format"), but *not* aligned to the
 * same column grid: shot_col tracks the ship, which the player moves one
 * column at a time, while fruit_col moves in GRID_STEP-column jumps as the
 * formation marches, so the two 2-column spans can land partially
 * overlapping rather than only exactly coincident or exactly disjoint (a
 * plain fruit_col == shot_col equality check missed those partial-overlap
 * cases -- confirmed by hand-testing in VICE). Two 2-wide integer spans
 * [a, a+1] and [b, b+1] overlap iff a <= b+1 and b <= a+1 -- true even when
 * they only share one column. Only 30 cells at most, so a plain scan is
 * cheap enough not to need anything cleverer. */
static unsigned char grid_check_hit(unsigned char shot_row, unsigned char shot_col,
                                     signed char col_offset, unsigned char row_offset,
                                     unsigned char active_rows,
                                     unsigned char *out_r, unsigned char *out_c) {
    unsigned char r, c, fruit_row, fruit_col;

    for (r = 0; r < active_rows; r++) {
        fruit_row = GRID_ROW_START + r * GRID_STEP + row_offset;
        if (shot_row != fruit_row && shot_row != (unsigned char)(fruit_row + 1)) {
            continue;
        }
        for (c = 0; c < GRID_COLS; c++) {
            if (!fruit_alive[r][c]) {
                continue;
            }
            fruit_col = GRID_COL_START + c * GRID_STEP + col_offset;
            if (fruit_col <= (unsigned char)(shot_col + 1) &&
                shot_col <= (unsigned char)(fruit_col + 1)) {
                *out_r = r;
                *out_c = c;
                return 1;
            }
        }
    }
    return 0;
}

/* Finds the leftmost and rightmost grid columns (0..GRID_COLS-1) that still
 * have at least one live fruit in any of the active rows, and writes them
 * to *out_leftmost and *out_rightmost -- used every movement tick to compute
 * where the formation should actually bounce (see GRID_DIR_RIGHT/LEFT
 * handling in game_screen_run()), instead of the fixed 0..GRID_COLS-1 span,
 * which stays wrong once an edge column is fully cleared out (see comment
 * above GRID_DIR_RIGHT). Always finds something as long as fruits_remaining
 * is nonzero (every remaining fruit is, by construction, in some active row
 * -- see game_screen_run()), so callers don't need to handle "not found". */
static void grid_alive_col_bounds(unsigned char active_rows,
                                   unsigned char *out_leftmost, unsigned char *out_rightmost) {
    unsigned char r, c;
    unsigned char leftmost = 0;
    unsigned char rightmost = 0;
    unsigned char found = 0;

    /* c runs low to high, so the first column with a live fruit is the
     * leftmost (latched once via `found`) and the last one seen is the
     * rightmost (kept updated). */
    for (c = 0; c < GRID_COLS; c++) {
        for (r = 0; r < active_rows; r++) {
            if (fruit_alive[r][c]) {
                if (!found) {
                    leftmost = c;
                    found = 1;
                }
                rightmost = c;
                break;
            }
        }
    }
    *out_leftmost = leftmost;
    *out_rightmost = rightmost;
}

/* Ship position: bottom row of the 2x2 sprite touches the last screen row
 * (see CLAUDE.md, "Sprite storage format" -- ship is 2x2 chars), centered
 * horizontally, with S/D clamped so it can't move off either edge (no
 * wraparound). */
#define SHIP_ROW        (SCREEN_ROWS - 2)
#define SHIP_COL_MIN     0
#define SHIP_COL_MAX     (SCREEN_COLS - 2)
#define SHIP_COL_CENTER  ((SCREEN_COLS - 2) / 2)

/* Shot: 2x1 chars (see sprites.c), fired from directly over the ship --
 * shot_col tracks ship_col itself, not an offset cell, since the shot's
 * sprite is exactly as wide as the ship it launches from. One row above the
 * ship, it travels straight up one row at a time until it reaches the top
 * of the game zone, where it vanishes -- SHOT_TOP_ROW stops at row 1, not
 * row 0, so it never overwrites the HUD (see "Screen layout" above). Only
 * one shot is ever in flight -- Space is a no-op while shot_active is set.
 * Its trail is blanked as it moves, which eats into the placeholder text it
 * flies over -- fine since that text is only temporary (see CLAUDE.md,
 * "Current project state"). */
#define SHOT_START_ROW  (SHIP_ROW - 1)
#define SHOT_TOP_ROW     (HUD_ROW + 1)
#define SHOT_STEP_JIFFIES 2 /* ~40ms at 50Hz -- fast but visible per-row travel */

/* Fruit wobble: same per-type animation as the title screen (see title.c),
 * kept going here too -- paced independently of the grid's marching so the
 * two don't drift into lockstep as the marching speeds up. */
#define GRID_ANIM_JIFFIES 25 /* ~0.5s at 50Hz, same pace as title.c */

/* KERNAL jiffy clock, low byte -- see title.c for the same use (paces
 * animation without blocking on key input). */
#define JIFFY_LOW ((volatile unsigned char *)0x00A2)

/* Enemy fire: every level, one more randomly chosen fruit (capped at
 * ENEMY_MAX_SHOOTERS) is designated a "shooter" that fires a shot straight
 * down at a regular interval -- CLAUDE.md-adjacent difficulty ramp
 * requested alongside the grid's own marching speedup (level 1 = 1
 * shooter, level 2 = 2, ... capped at ENEMY_MAX_SHOOTERS from level 5 on).
 * A shooter's shot is 2 chars wide like the player's (see sprites.c, same
 * centering rationale) but travels down instead of up, is independent of
 * its shooter's own position once fired (same "spawn column, then straight
 * line" behavior as the player's shot -- see SHOT_START_ROW above), and
 * passes through any fruit cell it crosses with no visual or gameplay
 * effect (see enemy_shots_update() -- it reuses grid_check_hit() purely as
 * an occlusion test there, never acting on a "hit"). ENEMY_MAX_SHOOTERS
 * bounds the per-shooter state arrays below (shooter_r/c index into
 * fruit_alive[][] the same way grid_check_hit() does). */
#define ENEMY_MAX_SHOOTERS        5
#define ENEMY_FIRE_PERIOD_JIFFIES 150 /* ~3s at 50Hz between one shooter's shots */

/* Per-row travel pace, in jiffies between steps (so *larger* = *slower*,
 * same convention as GRID_MOVE_JIFFIES_* above) -- originally 2 (same pace
 * as the player's own shot, see SHOT_STEP_JIFFIES), found too fast in
 * playtesting; briefly tried at 75% (3 jiffies, only ~67% in practice due
 * to integer rounding -- see git history/CLAUDE.md tuning notes). Current
 * value is 50% of the original: new_jiffies = old_jiffies / 0.5 = 2 * 2 = 4
 * -- exact this time, no rounding needed. Keep 2 in mind as the
 * pre-adjustment baseline if this needs tuning again. */
#define ENEMY_SHOT_STEP_JIFFIES   4

/* Per-shooter state, indexed 0..shooter_count-1 (see shooters_select()):
 * shooter_r/c is the fruit cell chosen as a shooter; shooter_shot_active is
 * whether it currently has a shot in flight; shooter_shot_row/col is that
 * shot's position (col fixed at spawn, row advancing); shooter_shot_drawn
 * tracks whether the shot is actually rendered at shooter_shot_row right
 * now (cleared while passing through a fruit cell, so enemy_shots_update()
 * knows not to erase a cell it never drew into -- see "Enemy fire" above);
 * shooter_last_fire_jiffy/shooter_last_step_jiffy are this shooter's own
 * jiffy-clock checkpoints, paced independently per shooter so their shots
 * visibly desynchronize instead of marching in lockstep. */
static unsigned char shooter_r[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_c[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_count;
static unsigned char shooter_shot_active[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_shot_drawn[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_shot_row[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_shot_col[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_last_fire_jiffy[ENEMY_MAX_SHOOTERS];
static unsigned char shooter_last_step_jiffy[ENEMY_MAX_SHOOTERS];

/* Picks shooter_count = min(level, ENEMY_MAX_SHOOTERS) distinct fruit cells
 * to be this level's shooters, via a partial Fisher-Yates shuffle of all
 * GRID_ROWS*GRID_COLS cells (cheap in bounded time at this scale -- at most
 * 30 cells) -- called once at game start and again at the top of every
 * level (see advance_level()), always right after grid_reset_alive() so
 * every cell is still a valid pick. Each picked shooter's fire timer is
 * seeded with an initial phase evenly spread across one
 * ENEMY_FIRE_PERIOD_JIFFIES window (shooter i gets i*period/shooter_count),
 * so shooters visibly desync instead of opening fire together (CLAUDE.md-
 * requested "not all at once") -- deliberately *not* randomized: with only
 * up to ENEMY_MAX_SHOOTERS=5 shooters, this platform's rand() (a small
 * embedded PRNG, low-bit quality not guaranteed) called back-to-back in
 * this same loop produced visibly clustered offsets in practice, which read
 * on screen as near-simultaneous fire -- exactly the bug reported. A fixed
 * even split has no such failure mode and needs no entropy source. rand()
 * is still used below for *which* cells become shooters (position variety
 * matters there; timing precision doesn't). cell_r/cell_c are
 * function-static (not stack-local) to match this file's existing
 * preference for fixed module-level buffers over relying on call-frame
 * stack space (see fruit_alive[][] above). */
static void shooters_select(unsigned char level) {
    static unsigned char cell_r[GRID_ROWS * GRID_COLS];
    static unsigned char cell_c[GRID_ROWS * GRID_COLS];
    unsigned char total = (unsigned char)GRID_ROWS * GRID_COLS;
    unsigned char i, j, r, c, tmp;
    unsigned char now = *JIFFY_LOW;

    /* Erase any shot still visibly in flight from the previous level (the
     * fresh grid drawn right after this call only touches its own cells, so
     * a stray shot elsewhere on screen would otherwise be left as permanent
     * visual garbage) -- uses the *old* shooter_count, still in scope until
     * overwritten below. No-op on the very first call (shooter_count starts
     * at 0, BSS-zeroed). */
    for (i = 0; i < shooter_count; i++) {
        if (shooter_shot_active[i] && shooter_shot_drawn[i]) {
            bg_clear_pair(shooter_shot_row[i], shooter_shot_col[i]);
        }
    }

    for (r = 0, i = 0; r < GRID_ROWS; r++) {
        for (c = 0; c < GRID_COLS; c++, i++) {
            cell_r[i] = r;
            cell_c[i] = c;
        }
    }

    shooter_count = level;
    if (shooter_count > ENEMY_MAX_SHOOTERS) {
        shooter_count = ENEMY_MAX_SHOOTERS;
    }

    srand(now);
    for (i = 0; i < shooter_count; i++) {
        unsigned char phase = (unsigned char)(((unsigned int)i * ENEMY_FIRE_PERIOD_JIFFIES) /
                                               shooter_count);

        j = (unsigned char)(i + (unsigned char)rand() % (total - i));
        tmp = cell_r[i]; cell_r[i] = cell_r[j]; cell_r[j] = tmp;
        tmp = cell_c[i]; cell_c[i] = cell_c[j]; cell_c[j] = tmp;

        shooter_r[i] = cell_r[i];
        shooter_c[i] = cell_c[i];
        shooter_shot_active[i] = 0;
        shooter_shot_drawn[i] = 0;
        shooter_last_fire_jiffy[i] = (unsigned char)(now - phase);
    }
}

/* Countdown shown before play starts: "3", "2", "1", one per second, as the
 * 16x16 digit sprite (see sprites.c) centered on screen -- reusing the ship's
 * horizontal centering column since both are 2x2 sprites. Unlike title.c's
 * blink loop, cbm_k_getin() is deliberately never polled while it runs, so
 * S/D/Space have no effect during it (see CLAUDE.md-requested behavior:
 * only the ship is on screen and input is blocked until the countdown
 * ends). */
#define COUNTDOWN_ROW     (SCREEN_ROWS / 2 - 1)
#define COUNTDOWN_COL     SHIP_COL_CENTER
#define COUNTDOWN_START   3
#define COUNTDOWN_JIFFIES 50 /* ~1s at 50Hz per digit */

/* Busy-waits for n jiffies (input intentionally not polled -- callers that
 * need responsive input, like title.c's blink loop, do their own waiting
 * instead of using this). Shared by run_countdown() and flash_border(). */
static void wait_jiffies(unsigned char n) {
    unsigned char start = *JIFFY_LOW;
    while ((unsigned char)(*JIFFY_LOW - start) < n) {
        /* busy-wait */
    }
}

/* Displays the 3/2/1 countdown and blocks until it's done. Any key presses
 * that queued up in the KERNAL buffer during the wait are drained
 * afterwards so they don't leak into the game loop that follows -- without
 * this, a key held down during the countdown would still act on the ship
 * the instant input unblocks, which isn't "blocked" input. */
static void run_countdown(void) {
    unsigned char count;

    /* Digit set before the screen cells are pointed at its codes, so nothing
     * uninitialized is ever briefly visible (see sprites_set_countdown_digit()
     * -- these codes aren't preloaded by sprites_load()). */
    sprites_set_countdown_digit(COUNTDOWN_START);
    screen_put_quad(COUNTDOWN_ROW, COUNTDOWN_COL, CHAR_COUNTDOWN_TL, COUNTDOWN_COLOR);
    for (count = COUNTDOWN_START; count >= 1; count--) {
        wait_jiffies(COUNTDOWN_JIFFIES);
        if (count > 1) {
            sprites_set_countdown_digit(count - 1);
        }
    }
    bg_clear_quad(COUNTDOWN_ROW, COUNTDOWN_COL);
    while (cbm_k_getin() != 0) {
        /* discard anything buffered during the countdown */
    }
}

#define EXPLOSION_JIFFIES 12 /* ~240ms at 50Hz per explosion frame */

/* Replaces the ship's 2x2 cells with the shared explosion effect (see
 * CLAUDE.md, "Sprite storage format") for its two animation frames, then
 * restores the ship sprite -- the visual cue, at the ship's own position,
 * that a life was just lost. Only the bitmap needs redefining between the
 * two frames (live redraw, see CLAUDE.md, "Graphics and colors") -- the
 * screen cells themselves only need poking once at the start (to switch
 * from ship codes to explosion codes) and once at the end (back to ship
 * codes). Called before flash_border() from game_screen_run(). */
static void explode_ship(unsigned char ship_col) {
    sound_explosion();
    sprites_set_explosion_frame(0);
    screen_put_quad(SHIP_ROW, ship_col, CHAR_EXPLOSION_TL, EXPLOSION_COLOR);
    wait_jiffies(EXPLOSION_JIFFIES);
    sprites_set_explosion_frame(1);
    wait_jiffies(EXPLOSION_JIFFIES);
    sound_stop();
    screen_put_quad(SHIP_ROW, ship_col, CHAR_SHIP_TL, SHIP_COLOR);
}

/* Same shared explosion effect as explode_ship(), played at a fruit's
 * current screen position instead of the ship's -- the visual cue that the
 * shot just killed it. Unlike the ship (which survives and is redrawn),
 * the fruit is gone for good, so this ends by clearing the cell rather than
 * restoring a sprite there. Caller (game_screen_run()) is responsible for
 * marking the fruit dead in fruit_alive[][] -- this only handles the
 * visual. */
static void explode_fruit(unsigned char r, unsigned char c,
                           signed char col_offset, unsigned char row_offset) {
    unsigned char row = GRID_ROW_START + r * GRID_STEP + row_offset;
    unsigned char col = GRID_COL_START + c * GRID_STEP + col_offset;

    sound_explosion();
    sprites_set_explosion_frame(0);
    screen_put_quad(row, col, CHAR_EXPLOSION_TL, EXPLOSION_COLOR);
    wait_jiffies(EXPLOSION_JIFFIES);
    sprites_set_explosion_frame(1);
    wait_jiffies(EXPLOSION_JIFFIES);
    sound_stop();
    bg_clear_quad(row, col);
}

#define FLASH_JIFFIES 8 /* ~160ms at 50Hz per flash phase */

/* Flashes the border `color`/black twice -- the visual cue for a notable
 * game event (see call sites: red for a life lost, green for a level
 * cleared). Blocks for its whole duration, which is what pauses the grid's
 * marching and the player's controls together: nothing in the caller's
 * loop runs again until this returns. Only the border (low nibble of
 * $900F) changes; the screen background stays black (see CLAUDE.md,
 * "Graphics and colors"). */
static void flash_border(unsigned char color) {
    unsigned char i;

    for (i = 0; i < 2; i++) {
        VIC.bg_border_color = (COLOR_BLACK << 4) | color;
        wait_jiffies(FLASH_JIFFIES);
        VIC.bg_border_color = (COLOR_BLACK << 4) | COLOR_BLACK;
        wait_jiffies(FLASH_JIFFIES);
    }
}

#define LOSE_ROW        11
#define LOSE_PROMPT_ROW 13

/* Clears the screen and shows the game-over message, blocking until Space
 * is pressed. Caller returns to the title screen right after (see
 * game_screen_run()). Both strings come from lang.h (see game_screen_run()'s
 * comment on this screen rendering whichever language was last selected on
 * the title screen), so their length isn't a fixed constant the way it was
 * before translation -- centered here from strlen() instead of a
 * hand-computed column, same rounding (odd leftover width favors the left)
 * either way. */
static void show_lose_screen(void) {
    const char *title = lang_lose_title();
    const char *prompt = lang_lose_prompt();
    unsigned char title_len = (unsigned char)strlen(title);
    unsigned char prompt_len = (unsigned char)strlen(prompt);

    screen_clear();
    font_print(LOSE_ROW, (SCREEN_COLS - title_len) / 2, title, COLOR_WHITE);
    font_print(LOSE_PROMPT_ROW, (SCREEN_COLS - prompt_len) / 2, prompt, COLOR_CYAN);
    sound_game_over();
    while (cbm_k_getin() != ' ') {
        /* wait for acknowledgement */
    }
}

/* Draws one CHAR_LIFE_SHIP icon per remaining life, cols 0..lives-1, and
 * blanks the rest of the MAX_LIVES slots -- redrawing the whole row each
 * time keeps this correct however lives changes (no separate "erase the
 * lost one" call needed). */
static void hud_draw_lives(unsigned char lives) {
    unsigned char col;

    for (col = 0; col < MAX_LIVES; col++) {
        screen_put(HUD_ROW, col, col < lives ? CHAR_LIFE_SHIP : CHAR_BLANK, SHIP_COLOR);
    }
}

/* Draws "LEVEL:NN"/"NIVEAU:NN" (see lang.h) right-aligned against the last
 * screen column. The label's length depends on the current language, so
 * unlike before translation this can't be a fixed 8-char buffer/column --
 * built from lang_level_label() plus ":NN" instead, with the starting
 * column computed from the actual resulting length. */
static void hud_draw_level(unsigned char level) {
    char text[10]; /* longest case: FR "NIVEAU:NN" (9 chars) + NUL */
    const char *label = lang_level_label();
    unsigned char len = (unsigned char)strlen(label);
    unsigned char i;

    for (i = 0; i < len; i++) {
        text[i] = label[i];
    }
    text[i++] = ':';
    text[i++] = '0' + (level / 10);
    text[i++] = '0' + (level % 10);
    text[i] = '\0';

    font_print(HUD_ROW, SCREEN_COLS - i, text, COLOR_WHITE);
}

/* Advances every shooter's fire timer and any shot it has in flight by one
 * tick (see "Enemy fire" above). A shooter with no shot in flight fires a
 * fresh one once its own ENEMY_FIRE_PERIOD_JIFFIES timer elapses, as long as
 * its fruit is still alive and there's still room to spawn above the ship
 * (see the `row <= SHIP_ROW` guard -- a shooter whose fruit has marched
 * dangerously close to the ship simply skips this attempt and retries after
 * another full period, which in practice never matters since the row-loss
 * handling in game_screen_run() will have already retired that row by
 * then). A shot in flight steps down one row every ENEMY_SHOT_STEP_JIFFIES:
 * grid_check_hit() is reused purely as an occlusion test at the new row
 * (its return value is never acted on) so a shot crossing a still-alive
 * fruit's cell simply isn't drawn there -- passing through with no visual
 * or gameplay effect -- and shooter_shot_drawn tracks that so the *next*
 * erase knows whether there is anything to erase. Once a shot's row reaches
 * SHIP_ROW, it's checked against ship_col with the same two-span overlap
 * test grid_check_hit() uses internally (both the shot and the ship are
 * 2 cells wide); a hit plays the exact same consequence as a fruit row
 * reaching the ship in game_screen_run() (life lost, ship explosion, border
 * flash, game over on the last life) -- kept here rather than duplicated at
 * the call site since every input it needs (ship_col, the grid offsets,
 * *lives) is already available to pass in. Returns 1 if the caller should
 * return from game_screen_run() immediately (the lose screen was already
 * shown), 0 otherwise. */
static unsigned char enemy_shots_update(unsigned char *lives, unsigned char ship_col,
                                         signed char grid_col_offset,
                                         unsigned char grid_row_offset,
                                         unsigned char active_rows) {
    unsigned char i;
    unsigned char now = *JIFFY_LOW;
    unsigned char dummy_r, dummy_c;

    for (i = 0; i < shooter_count; i++) {
        if (!shooter_shot_active[i]) {
            if (shooter_r[i] < active_rows && fruit_alive[shooter_r[i]][shooter_c[i]] &&
                (unsigned char)(now - shooter_last_fire_jiffy[i]) >= ENEMY_FIRE_PERIOD_JIFFIES) {
                unsigned char row = GRID_ROW_START + shooter_r[i] * GRID_STEP +
                                     grid_row_offset + 2;
                unsigned char col = GRID_COL_START + shooter_c[i] * GRID_STEP +
                                     grid_col_offset;

                shooter_last_fire_jiffy[i] = now;
                if (row <= SHIP_ROW) {
                    shooter_shot_active[i] = 1;
                    shooter_shot_drawn[i] = 1;
                    shooter_shot_row[i] = row;
                    shooter_shot_col[i] = col;
                    shooter_last_step_jiffy[i] = now;
                    screen_put_pair(row, col, CHAR_ENEMY_SHOT_L, ENEMY_SHOT_COLOR);
                }
            }
            continue;
        }

        if ((unsigned char)(now - shooter_last_step_jiffy[i]) < ENEMY_SHOT_STEP_JIFFIES) {
            continue;
        }
        shooter_last_step_jiffy[i] = now;

        if (shooter_shot_drawn[i]) {
            bg_clear_pair(shooter_shot_row[i], shooter_shot_col[i]);
        }

        if (shooter_shot_row[i] >= SHIP_ROW) {
            unsigned char hit = ship_col <= (unsigned char)(shooter_shot_col[i] + 1) &&
                                 shooter_shot_col[i] <= (unsigned char)(ship_col + 1);

            shooter_shot_active[i] = 0;
            if (hit) {
                (*lives)--;
                hud_draw_lives(*lives);
                explode_ship(ship_col);
                flash_border(COLOR_RED);
                if (*lives == 0) {
                    show_lose_screen();
                    return 1;
                }
            }
            continue;
        }

        shooter_shot_row[i]++;
        if (grid_check_hit(shooter_shot_row[i], shooter_shot_col[i], grid_col_offset,
                            grid_row_offset, active_rows, &dummy_r, &dummy_c)) {
            shooter_shot_drawn[i] = 0; /* passes through the fruit -- no visual, no effect */
        } else {
            shooter_shot_drawn[i] = 1;
            screen_put_pair(shooter_shot_row[i], shooter_shot_col[i], CHAR_ENEMY_SHOT_L,
                             ENEMY_SHOT_COLOR);
        }
    }
    return 0;
}

/* Called when the player has cleared every fruit (fruits_remaining reaches
 * 0 in game_screen_run()): bumps the level counter and resets the
 * formation to a fresh, fully-alive grid at the starting position. Every
 * LIFE_BONUS_LEVELS levels reached, an extra life is awarded (capped at
 * MAX_LIVES) -- otherwise lives carry over unchanged. The formation itself
 * is unchanged level to level (see CLAUDE.md, "Levels") except for its
 * starting pace: *level_start_jiffies is shaved down by GRID_MOVE_SPEEDUP
 * (the same amount a row step-down shaves off within a level, see
 * GRID_MOVE_* above) every time a level begins, floored at
 * GRID_MOVE_JIFFIES_MIN, so each level starts slightly faster than the
 * last on top of its own in-level ramp-up. Also reselects the level's
 * shooters (see shooters_select()) against the fresh grid -- must run after
 * grid_reset_alive() so every cell is a valid pick, same ordering
 * game_screen_run() uses for the very first level. Plays the same
 * flash_border() cue used for a lost life (see game_screen_run()), but
 * green instead of red -- the visual confirmation that the level just
 * cleared -- *before* the new formation is drawn, not after. */
static void advance_level(unsigned char *level, unsigned char *lives,
                           signed char *grid_col_offset,
                           unsigned char *grid_row_offset, unsigned char *grid_dir,
                           unsigned char *active_rows, unsigned char *move_jiffies,
                           unsigned char *level_start_jiffies,
                           unsigned char *fruits_remaining) {
    (*level)++;
    hud_draw_level(*level);
    if (*level % LIFE_BONUS_LEVELS == 0 && *lives < MAX_LIVES) {
        (*lives)++;
        hud_draw_lives(*lives);
    }
    grid_reset_alive();
    shooters_select(*level);
    *grid_col_offset = 0;
    *grid_row_offset = 0;
    *grid_dir = GRID_DIR_RIGHT;
    *active_rows = GRID_ROWS;
    *level_start_jiffies = *level_start_jiffies > GRID_MOVE_JIFFIES_MIN + GRID_MOVE_SPEEDUP
                               ? *level_start_jiffies - GRID_MOVE_SPEEDUP
                               : GRID_MOVE_JIFFIES_MIN;
    *move_jiffies = *level_start_jiffies;
    *fruits_remaining = (unsigned char)GRID_ROWS * GRID_COLS;
    flash_border(COLOR_GREEN);
    sound_level_up();
    grid_draw(*grid_col_offset, *grid_row_offset, *active_rows);
}

/* Freezes the game until P is pressed again -- a gameplay-only toggle, not
 * mentioned on the title screen's controls reminder (user-requested; H's
 * "debug shortcut" caveat in CLAUDE.md is a separate thing) and shown with
 * no on-screen indicator (also user-requested) -- just a silent freeze. H
 * still works here too (same "at any point" shortcut as the rest of the game
 * screen); the caller checks the return value and returns from
 * game_screen_run() immediately if so.
 *
 * The jiffy clock keeps advancing during the freeze (it's KERNAL-driven, not
 * stoppable from here), so every paced timer's own "last_*_jiffy" checkpoint
 * -- the three passed in by pointer, plus the module-level per-shooter
 * arrays -- is nudged forward by the paused duration before returning;
 * otherwise every timer would see a huge elapsed gap the instant play
 * resumes (the shot leaping several rows at once, every shooter firing back
 * to back, the marching grid jumping ahead). This is exact even for a pause
 * lasting longer than 256 jiffies (~5s): shifting both "now" (by the KERNAL
 * clock actually running) and "last" (by the same `elapsed` byte) by the
 * real elapsed time leaves their unsigned-char difference -- what every
 * caller's "(now - last) >= period" check actually reads -- exactly what it
 * was the instant pause_screen_run() was entered, regardless of how long the
 * pause lasted or how many times the 1-byte clock wrapped during it. */
static unsigned char pause_screen_run(unsigned char *last_jiffy, unsigned char *last_grid_jiffy,
                                       unsigned char *last_anim_jiffy) {
    unsigned char pause_start = *JIFFY_LOW;
    unsigned char elapsed;
    unsigned char key;
    unsigned char i;

    for (;;) {
        key = cbm_k_getin();
        if (key == 'H') {
            return 1;
        }
        if (key == 'P') {
            break;
        }
    }

    elapsed = (unsigned char)(*JIFFY_LOW - pause_start);
    *last_jiffy += elapsed;
    *last_grid_jiffy += elapsed;
    *last_anim_jiffy += elapsed;
    for (i = 0; i < shooter_count; i++) {
        shooter_last_fire_jiffy[i] += elapsed;
        shooter_last_step_jiffy[i] += elapsed;
    }
    return 0;
}

void game_screen_run(void) {
    unsigned char ship_col = SHIP_COL_CENTER;
    unsigned char shot_active = 0;
    unsigned char shot_row = 0;
    unsigned char shot_col = 0;
    unsigned char last_jiffy = 0;
    unsigned char key;
    unsigned char lives = STARTING_LIVES;
    unsigned char level = STARTING_LEVEL;

    /* Grid marching state: col_offset/row_offset are added to every fruit's
     * base position by grid_draw()/grid_erase(); grid_dir is which way
     * col_offset is currently heading; active_rows shrinks from GRID_ROWS
     * as rows are lost (see the step-down handling below); move_jiffies is
     * the current pace, sped up on every step-down (see GRID_MOVE_* above);
     * level_start_jiffies is the pace move_jiffies gets reset to at the
     * start of each level -- GRID_MOVE_JIFFIES_BASE for level 1, then
     * shaved down by advance_level() on every level after that (see its
     * comment); fruits_remaining is every still-alive fruit across the
     * whole formation (not just the active rows), independent of
     * active_rows -- it reaches 0 either by shots or by rows reaching the
     * ship's lane, whichever finishes the formation off, and that's what
     * triggers advance_level(). */
    signed char grid_col_offset = 0;
    unsigned char grid_row_offset = 0;
    unsigned char grid_dir = GRID_DIR_RIGHT;
    unsigned char active_rows = GRID_ROWS;
    unsigned char move_jiffies = GRID_MOVE_JIFFIES_BASE;
    unsigned char level_start_jiffies = GRID_MOVE_JIFFIES_BASE;
    unsigned char fruits_remaining = (unsigned char)GRID_ROWS * GRID_COLS;
    unsigned char last_grid_jiffy;
    unsigned char anim_frame = 0;
    unsigned char last_anim_jiffy;

    screen_clear();
    star_blink = 0;
    starfield_draw_all(star_blink);
    hud_draw_lives(lives);
    hud_draw_level(level);
    grid_reset_alive();
    shooters_select(level);
    screen_put_quad(SHIP_ROW, ship_col, CHAR_SHIP_TL, SHIP_COLOR);
    run_countdown();
    grid_draw(grid_col_offset, grid_row_offset, active_rows);
    last_grid_jiffy = *JIFFY_LOW;
    last_anim_jiffy = *JIFFY_LOW;

    /* cbm_k_getin() drains the KERNAL keyboard buffer one PETSCII code at a
     * time, returning 0 when it's empty -- checked every pass so a shot in
     * flight (paced by the jiffy clock below) doesn't delay key response. */
    for (;;) {
        key = cbm_k_getin();
        if (key == 'H') {
            return;
        }
        if (key == 'P') {
            if (pause_screen_run(&last_jiffy, &last_grid_jiffy, &last_anim_jiffy)) {
                return;
            }
            continue;
        }
        if (key == 'S' && ship_col > SHIP_COL_MIN) {
            bg_clear_quad(SHIP_ROW, ship_col);
            ship_col--;
            screen_put_quad(SHIP_ROW, ship_col, CHAR_SHIP_TL, SHIP_COLOR);
        } else if (key == 'D' && ship_col < SHIP_COL_MAX) {
            bg_clear_quad(SHIP_ROW, ship_col);
            ship_col++;
            screen_put_quad(SHIP_ROW, ship_col, CHAR_SHIP_TL, SHIP_COLOR);
        } else if (key == ' ' && !shot_active) {
            unsigned char hit_r, hit_c;

            shot_row = SHOT_START_ROW;
            shot_col = ship_col;
            last_jiffy = *JIFFY_LOW;
            /* Checked even at the spawn position -- by late in a level the
             * formation may already be low enough for a fruit to be sitting
             * right there when the player fires. */
            if (grid_check_hit(shot_row, shot_col, grid_col_offset, grid_row_offset,
                                active_rows, &hit_r, &hit_c)) {
                fruit_alive[hit_r][hit_c] = 0;
                fruits_remaining--;
                explode_fruit(hit_r, hit_c, grid_col_offset, grid_row_offset);
                if (fruits_remaining == 0) {
                    advance_level(&level, &lives, &grid_col_offset, &grid_row_offset, &grid_dir,
                                  &active_rows, &move_jiffies, &level_start_jiffies,
                                  &fruits_remaining);
                }
            } else {
                shot_active = 1;
                screen_put_pair(shot_row, shot_col, CHAR_SHOT_L, SHOT_COLOR);
            }
        }

        if (shot_active && (unsigned char)(*JIFFY_LOW - last_jiffy) >= SHOT_STEP_JIFFIES) {
            last_jiffy = *JIFFY_LOW;
            bg_clear_pair(shot_row, shot_col);
            if (shot_row == SHOT_TOP_ROW) {
                shot_active = 0;
            } else {
                unsigned char hit_r, hit_c;

                shot_row--;
                if (grid_check_hit(shot_row, shot_col, grid_col_offset, grid_row_offset,
                                    active_rows, &hit_r, &hit_c)) {
                    shot_active = 0;
                    fruit_alive[hit_r][hit_c] = 0;
                    fruits_remaining--;
                    explode_fruit(hit_r, hit_c, grid_col_offset, grid_row_offset);
                    if (fruits_remaining == 0) {
                        advance_level(&level, &lives, &grid_col_offset, &grid_row_offset, &grid_dir,
                                      &active_rows, &move_jiffies, &level_start_jiffies,
                                      &fruits_remaining);
                    }
                } else {
                    screen_put_pair(shot_row, shot_col, CHAR_SHOT_L, SHOT_COLOR);
                }
            }
        }

        if ((unsigned char)(*JIFFY_LOW - last_anim_jiffy) >= GRID_ANIM_JIFFIES) {
            last_anim_jiffy = *JIFFY_LOW;
            anim_frame ^= 1;
            sprites_set_apple_frame(anim_frame);
            sprites_set_carrot_frame(anim_frame);
            sprites_set_grapes_frame(anim_frame);
            sprites_set_pepper_frame(anim_frame);
            star_twinkle();
        }

        if (enemy_shots_update(&lives, ship_col, grid_col_offset, grid_row_offset,
                                active_rows)) {
            return;
        }

        if (active_rows > 0 &&
            (unsigned char)(*JIFFY_LOW - last_grid_jiffy) >= move_jiffies) {
            unsigned char row_lost = 0;

            last_grid_jiffy = *JIFFY_LOW;
            grid_erase(grid_col_offset, grid_row_offset, active_rows);

            /* March one character sideways, or -- at the edge -- step down
             * one row, reverse direction and speed up (see GRID_MOVE_*
             * above). The edge is wherever the *surviving* leftmost/
             * rightmost column currently is (see grid_alive_col_bounds()
             * and the comment above GRID_DIR_RIGHT/LEFT), recomputed every
             * tick since which columns are still alive can change between
             * ticks (a shot landing, or a row reaching the ship's lane). */
            {
                unsigned char leftmost_c, rightmost_c;
                signed char col_offset_min, col_offset_max;

                grid_alive_col_bounds(active_rows, &leftmost_c, &rightmost_c);
                col_offset_min = -(signed char)(GRID_COL_START + leftmost_c * GRID_STEP);
                col_offset_max = (signed char)(SCREEN_COLS - 2 -
                                                (GRID_COL_START + rightmost_c * GRID_STEP));

                if (grid_dir == GRID_DIR_RIGHT) {
                    if (grid_col_offset < col_offset_max) {
                        grid_col_offset++;
                    } else {
                        grid_row_offset++;
                        grid_dir = GRID_DIR_LEFT;
                        move_jiffies = move_jiffies > GRID_MOVE_JIFFIES_MIN + GRID_MOVE_SPEEDUP
                                           ? move_jiffies - GRID_MOVE_SPEEDUP
                                           : GRID_MOVE_JIFFIES_MIN;
                    }
                } else {
                    if (grid_col_offset > col_offset_min) {
                        grid_col_offset--;
                    } else {
                        grid_row_offset++;
                        grid_dir = GRID_DIR_RIGHT;
                        move_jiffies = move_jiffies > GRID_MOVE_JIFFIES_MIN + GRID_MOVE_SPEEDUP
                                           ? move_jiffies - GRID_MOVE_SPEEDUP
                                           : GRID_MOVE_JIFFIES_MIN;
                    }
                }
            }

            /* Has the (possibly just-stepped-down) bottom active row's
             * bottom edge reached the ship's lane? Only the bottom row can
             * ever be at risk -- the whole formation moves as one rigid
             * block, so it's always the closest to the ship. */
            if (GRID_ROW_START + (active_rows - 1) * GRID_STEP + grid_row_offset + 1 >=
                SHIP_ROW) {
                unsigned char c;
                unsigned char row_had_fruit = 0;

                /* Whatever was still alive in this row is gone too --
                 * counts toward fruits_remaining the same as a shot kill
                 * would (see advance_level() below), just without the
                 * per-fruit explosion (the row already vanishes as a
                 * whole; see grid_draw() only covering r < active_rows).
                 * The row itself always passes out of play once it reaches
                 * here (active_rows-- unconditionally, so the next row up
                 * becomes the one at risk) -- but the ship is only actually
                 * destroyed (life lost, explosion, flash) if this row still
                 * had a fruit left to reach it. A row the player already
                 * cleared out by shooting should reach the ship's lane
                 * harmlessly (this was previously triggering a false
                 * life-loss on an empty row). */
                for (c = 0; c < GRID_COLS; c++) {
                    if (fruit_alive[active_rows - 1][c]) {
                        fruits_remaining--;
                        row_had_fruit = 1;
                    }
                }
                active_rows--;
                if (row_had_fruit) {
                    lives--;
                    hud_draw_lives(lives);
                    row_lost = 1;
                }
            }

            grid_draw(grid_col_offset, grid_row_offset, active_rows);

            /* Flash first, then check for game over -- the flash is the
             * player's feedback that they just lost a life, and that holds
             * true for the last one too (see CLAUDE.md-requested order). */
            if (row_lost) {
                explode_ship(ship_col);
                flash_border(COLOR_RED);
            }
            if (lives == 0) {
                show_lose_screen();
                return;
            }
            if (fruits_remaining == 0) {
                advance_level(&level, &lives, &grid_col_offset, &grid_row_offset, &grid_dir,
                              &active_rows, &move_jiffies, &level_start_jiffies,
                              &fruits_remaining);
            }
        }
    }
}
