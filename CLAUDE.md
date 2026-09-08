# Fruit Invaders

*Space Invaders* clone for the Commodore VIC-20, where the invaders are
pixelated fruits and vegetables. Still under active development — see
"Current project state" below for what's implemented so far and what's
still open.

## Target hardware

- **Machine**: Commodore VIC-20 **PAL** (affects video/raster timing and VIA
  frequencies — do not plan for NTSC-variant code).
- **Required memory configuration (current code): expansion blocks 1, 2 and
  3 active.**
  - Block 1: `$2000–$3FFF` (+8K)
  - Block 2: `$4000–$5FFF` (+8K)
  - Block 3: `$6000–$7FFF` (+8K)
  - Block 4 (`$8000–$9FFF`) stays **inactive** (character ROM / extended I/O
    area).
  - **Blocks 0 (`$0400–$0FFF`, +3K) and 5 (`$A000–$BFFF`, +8K, normally
    reserved for cartridges) are provisioned in the linker script for future
    growth but are not required by the code today.** Confirmed empirically
    (2026-09-06, VICE `vice` MCP server): the linker's `.block0`/`.block5`
    sections are both zero-sized in the current build
    (`__block0_start == __block0_end`, `__block5_start == __block5_end`, no
    source file references either section), and a "stack-painting" test
    (sentinel-fill `$4FB4`-`$7FFF` with `0xAA`, then play through several
    title/countdown/gameplay/explosion/level-transition cycles and check how
    far the sentinel got overwritten) showed the software stack never
    descends more than ~90 bytes below `__stack` (`$8000`) and the heap is
    never touched — static data (`.text`/`.rodata`/`.data`/`.bss`) and the
    runtime stack both stay comfortably inside blocks 1-3. If a future
    feature (more fruit types, gameplay music, larger sprite/level data)
    needs more RAM, block 0 and/or block 5 are the next ones to activate —
    update this section and re-run the same VICE verification when that
    happens, rather than assuming.
  - **Important**: the SDK's default link script
    (`mos-platform/vic20/lib/link.ld`) only knows the standard contiguous
    `0/3/8/16/24` KB combinations starting at block 1. Blocks 1+2+3 (24 KB)
    is one of those standard combos, but this project still **needs a
    custom linker script** regardless (see
    [link/vic20-fruit-invaders.ld](link/vic20-fruit-invaders.ld)): it fixes
    an unrelated issue where the default script could place ordinary
    code/data inside the `$1400-$1FFF` custom character-memory window (see
    "Sprite storage format"), and it keeps blocks 0/5 available as ready-to
    use (currently empty, zero-sized) `.block0`/`.block5` sections for the
    day they're actually needed (see the Toolchain section and
    [link/README.md](link/README.md)).

## Language and coding conventions

- **C or C++ required** for all game logic — the goal is to avoid a source
  base written entirely in assembly, not to exclude C++.
- Inline assembly or separate `.s` files are allowed, but only for routines
  where it is genuinely justified (cycle-accurate hardware access, raster
  IRQ, performance-critical routines). **Every assembly routine must be
  documented**: a header comment explaining its purpose, its
  input/output parameters/registers, which registers/flags it modifies, and
  why C/C++ was not sufficient.
- No unnecessary abstraction or genericity: this targets an 8-bit machine
  with very constrained RAM and CPU (6502 at ~1 MHz), the code must stay
  direct and readable.

## Source code organization

Clearly separate responsibilities into distinct modules rather than mixing
everything into a single file:

- **Gameplay**: game loop, entity state (player, invaders, shots),
  collisions, level progression, title screen / game over.
- **Graphics**: screen/character display, software sprites, colors,
  scrolling — see "Graphics and colors" below for the approach.
- **Sound**: sound effects and music via the VIA / the VIC's sound
  generator.
- **Input**: keyboard (and optionally joystick) reading, translated into
  abstract game actions (left/right/fire) before reaching gameplay.

## Graphics and colors

**Text mode only** — no hardware sprites, no bitmap graphics mode. All
visuals (ship, invaders, shots, background) are pseudo-graphics built from
a **custom character set redefined in RAM**, at a dedicated address set via
the character base register (`$9005`/36869, bits 0–3).

- **Screen**: 22 columns x 23 rows. With this project's memory
  configuration (block 1 active → "8K+" expansion), screen memory and color
  RAM sit at their expanded-map addresses: screen matrix at `$1000`/4096,
  color RAM at `$9400`/37888 (not the unexpanded `$1E00`/`$9600` pair).
- **Live redraw**: the VIC-I reads each character's bitmap from character
  memory every frame, so overwriting a character's bitmap bytes in RAM is
  reflected on screen instantly, with no explicit redraw call. This is the
  basis for animating invaders/explosions: cycle the bitmap data of the
  character code(s) they use rather than moving screen cells.
- **Character size**: 8x8 pixels by default (1 byte per row, 8 bytes per
  character). A 16-row mode also exists (`$9003`/36867, bit 0: 0 = 8x8,
  1 = 8x16), doubling character memory per glyph (16 bytes) and changing
  how the "number of rows" field in the same register is read — default to
  8x8; only switch if a concrete need (taller sprite) justifies the extra
  memory cost, and re-verify the exact row-count interaction in the PDF
  reference before relying on it.
- **Colors**:
  - **Border color**: global, `$900F`/36879 bits 0–2.
  - **Screen background color**: global, `$900F`/36879 bits 4–7 — shared by
    every character cell, not settable per character.
  - **Per-character foreground color**: one color RAM byte per screen cell
    (bits 0–2, default 1); this is the only per-character color in
    standard mode.
  - **Multicolor mode**: set bit 3 of a character's color RAM byte to 1
    (no separate global enable switch — it's purely per-character). This
    halves horizontal resolution to 4x8-pixel blocks (2 bits per block
    instead of 1) so each block picks one of **4** colors instead of 2:
    `00` screen background, `01` border color, `10` that character's
    foreground color, `11` auxiliary color (`$900E`/36878 bits 4–7). Only
    the foreground color and the multicolor bit vary per character — the
    other three colors in the palette are shared screen-wide.

## Sprite storage format

Sprites (fruits/vegetables, ship, shots, explosions) are composed from a
small grid of adjacent 8x8 character cells rather than one cell each — a
single cell is too small to read as a fruit shape.

- **Sprite sizes**: fruits/vegetables and the ship are **2x2 characters**
  (16x16 px, 4 character codes each) — same size for both so they share the
  explosion effect below. Shots are **2x1 characters** (16x8 px, 2 codes
  side by side) — as wide as the ship/fruits rather than a single 8x8 cell:
  a 1-char-wide shot has no cell that's actually centered under a 2-char
  sprite (the true center falls exactly on the boundary between two cells,
  never inside either one), so matching the full 2-column footprint instead
  removes the misalignment entirely and keeps a future shot/fruit collision
  check a plain same-two-columns comparison (see the comment in
  [src/graphics/sprites.c](src/graphics/sprites.c)). Explosion effect: **2x2
  characters** (4 codes), a single shared effect reused by every dying
  entity (see constraint below).
- **Per-code data**: 8 bytes per character code (1 byte per pixel row, bit 7
  = leftmost pixel). Source arrays are authored as "1 = the sprite's own
  body, 0 = empty", the intuitive way — but the VIC-I's actual standard-mode
  convention is the opposite of the naive assumption: a "1" bit shows the
  *screen background* color and "0" shows the color-RAM (per-character
  foreground) color. Confirmed empirically in VICE (uncorrected, every
  sprite rendered as the exact negative of its intended shape). Rather than
  author the arrays inverted, [charmem.c](src/graphics/charmem.c)'s
  `charmem_load()` inverts each byte once, centrally, while copying into
  character memory — keep authoring new sprite data the intuitive
  "1 = body" way; the load path handles the hardware quirk. A full sprite
  pose is stored as a flat `unsigned char[32]` (4 sub-chars x 8 rows), one
  array per pose/frame (idle, leg-wobble frame(s), explosion frame(s)).
- **Movement vs. animation are two different operations**:
  - *Moving* a sprite = writing its (already-defined) character codes into
    new screen-matrix cells — ordinary screen POKEs, cheap, does not touch
    character memory.
  - *Animating/exploding* a sprite = `memcpy`-ing a different 32-byte pose
    into the character-memory bytes for the code(s) currently on screen —
    the live-redraw property from "Graphics and colors" makes this appear
    instantly, no screen POKE needed.
- **Key constraint**: every screen cell using a given character code shows
  the same bitmap — redefining a code's bitmap changes *all* its on-screen
  occurrences at once. This is why invaders share codes per fruit type
  (they animate in lockstep, which matches the genre) and why a dying
  invader's cells are switched to the *shared* explosion code rather than
  getting a private one — a private per-invader explosion would need a
  private set of codes per grid slot instead of per fruit type, and isn't
  needed since simultaneous explosions can look identical.
- **Character code budget & placement**: the character-base register
  (`$9005`/36869, low nibble) only offers four fixed RAM windows —
  `$1000`, `$1400`, `$1800`, `$1C00` (Programmer's Reference Guide,
  VICCR5) — no arbitrary address. `$1000-$11FF` is already the relocated
  text screen (see "Graphics and colors"), leaving **`$1400`-`$1FFF` (3 KB,
  384 possible 8x8 codes)** for the custom set. Budgeting a handful of
  fruit types (4 codes each) + ship (4) + explosion (4) + shots (2) +
  a minimal custom font subset for title/score text (letters/digits
  actually used — the character generator can only point at one table at a
  time, so ROM font and custom graphics can't be mixed) stays well under
  that limit.
- **No linker script change needed**: `$1400-$1FFF` falls inside the
  generic program RAM region (`ram`, $1201-$7FFF) already declared in
  [link/vic20-fruit-invaders.ld](link/vic20-fruit-invaders.ld) — unlike
  `.block0`/`.block5` (physically disjoint from `ram`, hence their
  load-elsewhere-then-`memcpy` mechanism), `$1400` is *inside* the single
  contiguous blob the `.prg` loader already places at `$1201`. So the
  sprite arrays can be ordinary `const` data anywhere in `ram`, copied to
  `$1400` by a small runtime loop (implemented in
  [src/graphics/sprites.c](src/graphics/sprites.c)) instead of relying on a
  linker-placed section. No `-fno-lto` is needed either for this data:
  that flag is only required when a section is referenced solely through
  linker-generated symbols (as `.block0`/`.block5` are) — here the arrays
  are referenced by name from real C code, which LTO won't discard. The
  destination pointers (character memory, screen matrix, color RAM) are
  still declared `volatile`, since the VIC-I reads them as a side channel
  the optimizer can't see — a plain `memcpy` would silently drop that
  qualifier, so a hand-written volatile-aware copy loop is used instead.

## Controls

- `S`: move left
- `D`: move right
- `Space`: fire
- `M`: toggle background music mute (title screen only)
- `H`: return to the title screen (game screen only, at any point — a
  debug/testing shortcut, not an intended player-facing control)

## Title screen

Implemented in [src/gameplay/title.c](src/gameplay/title.c), shown before
every game and returning to `main()` when Space is pressed. Deliberately
graphical rather than a flat black background — new pseudo-bitmap tiles for
this (chasing-light bulb, star, left/right arrow) live in
[src/graphics/decor.c](src/graphics/decor.c)/`decor.h`, a small module
separate from sprites.c/font.c/bigfont.c since none of it is gameplay- or
text-rendering-related, title.c is its only caller:
- the game's name, **FRUIT INVADERS**, in a dedicated 1x2-cell "big" font
  (see [src/graphics/bigfont.c](src/graphics/bigfont.c), distinct from the
  regular 8x8 font used for body text) — deliberately not the ship or a
  shot sprite (see "Sprite storage format": both are reused as-is in the
  gameplay screen, nothing title-specific to show there);
- a chasing-light marquee border across the top and bottom rows (arcade
  "running lights" look): a repeating bulb tile, color-cycled through the
  same four fruit colors used elsewhere on this screen, with the top and
  bottom rows chasing in opposite directions off one shared phase counter
  advanced every animation tick — pure color-RAM rewrites, no extra timer;
- a twinkling starfield filling the otherwise-empty background rows (fixed
  scatter of star tiles, picked to avoid every row the title/ticker/text
  content already occupies) — each star swaps between white and cyan
  (never goes fully dark, so the field doesn't look like it's vanishing)
  on the same tick as the start prompt's blink, XORed with the star's own
  index so only about half the field swaps on any given tick instead of
  the whole thing flipping in lockstep;
- the four fruit/vegetable sprites as an infinite horizontal ticker
  (scrolling across the full screen width and wrapping back in from the
  left), wobble-animated in place independently of the scroll;
- a reminder of the controls (`S`/`D` to move, `Space` to fire, `M` to
  toggle music) — a first pass replaced this with icon rows (reduced-ship/
  shot/note tiles next to the key names) but was reverted at the user's
  request: the plain sentences read more clearly than the pictograms did,
  so this stays text while the border/starfield above stayed graphical;
- a looping background tune (see `sound_music_start()` /
  `sound_music_tick()` / `sound_music_stop()` in
  [src/sound/sound.c](src/sound/sound.c)), mutable with `M` (a
  "MUSIC OFF" indicator reflects the state) and always stopped the moment
  Space is pressed;
- a blinking "PRESS SPACE TO START" prompt, framed by two inward-pointing
  arrow tiles that blink in lockstep with it.

## Levels

Decided and implemented (approach "a single looping level" below), in
[src/gameplay/game.c](src/gameplay/game.c)'s `advance_level()`: the fruit
formation (layout and fruit types) is identical every level and loops
forever — there is no level cap and no per-level formation/background
change. Difficulty instead ramps per level:
- **Marching pace**: the formation's starting speed for the level is
  shaved down by `GRID_MOVE_SPEEDUP` jiffies from the previous level's
  starting speed (floored at `GRID_MOVE_JIFFIES_MIN`), on top of the
  existing in-level speedup already applied on every step-down — so each
  level both starts faster than the last *and* keeps accelerating as it
  progresses.
- **Enemy fire**: one additional fruit per level is designated a
  "shooter" that fires straight down at the ship at a steady interval
  (level 1 = 1 shooter, level 2 = 2, ..., capped at `ENEMY_MAX_SHOOTERS`
  from level 5 on) — see `shooters_select()`.
- **Extra lives**: every `LIFE_BONUS_LEVELS` levels reached, one life is
  awarded (capped at `MAX_LIVES`); otherwise lives carry over unchanged
  from the previous level.

## Toolchain — llvm-mos

Compiler available locally:

```
/Users/aboudou/Developer/VIC-20/llvm-mos
```

- VIC-20 target compiler: `bin/mos-vic20-clang` (C) and `bin/mos-vic20-clang++`
  (C++), depending on the language used per file.
- Platform files: `mos-platform/vic20/` (`include/`, `asminc/`, `lib/`).
  - `include/vic20.h`, `include/_vic.h`, `include/_6522.h`: VIC and VIA
    registers, colors, keys. `VIC` (`_vic.h`'s `__vic` struct at `$9000`)
    covers most registers directly (e.g. `VIC.addr` for `$9005`); the
    `COLOR_BLACK`..`COLOR_YELLOW`/`COLOR_ORANGE`.. macros in `vic20.h` match
    the VIC-I palette exactly and are safe to reuse. **Gotcha**: `vic20.h`
    also defines `COLOR_RAM` at `$9600`, which is only correct for the
    unexpanded/+3K memory config — this project's 8K+ config puts color RAM
    at `$9400` (see "Graphics and colors"), so that macro must not be used
    as-is (see the local `SCREEN_COLOR_RAM` in
    [src/graphics/screen.c](src/graphics/screen.c)).
  - `lib/link.ld`: default link script (see limitation above,
    `__memory_expansion` only accepts 0/3/8/16/24).
  - `lib/basic-header.o`, `lib/libcrt0.a`, `lib/libc.a`: startup/CRT and
    minimal libc.
- Typical compilation of a `.prg` executable:

```bash
/Users/aboudou/Developer/VIC-20/llvm-mos/bin/mos-vic20-clang -Os -o game.prg main.c
```

- For the 0+1+2+3+5 memory configuration, a custom linker script must be
  passed (`-T custom.ld` or `-Wl,-T,custom.ld`) instead of relying on
  `__memory_expansion`.

## Debugging / testing — VICE MCP server

The `vice` MCP server (configured in `.mcp.json` as an HTTP endpoint,
`http://127.0.0.1:6510/mcp`) talks to a build of the VICE emulator itself
with native MCP support compiled in (`--enable-mcp-server`; the local
build lives at `/Users/aboudou/Developer/VIC-20/vice-mcp/`, from
[github.com/barryw/vice-mcp](https://github.com/barryw/vice-mcp) — a VICE
fork, **not** the same project as that author's older standalone
`ViceMCP` .NET wrapper, which this project no longer uses). Because it's
an HTTP server rather than `stdio`, Claude Code does not launch it
automatically — the emulator has to already be running with `-mcpserver`
before the `vice` tools work:

```bash
/Users/aboudou/Developer/VIC-20/vice-mcp/VICE.app/Contents/Resources/bin/xvic -mcpserver
```

(default host/port `127.0.0.1:6510`; see `xvic -help` for
`-mcpserverport`/`-mcpserverhost`/`-mcpservertoken` to change that). It
exposes memory/register inspection, execution control (step/run/
breakpoints/checkpoints), disassembly, keyboard/joystick injection,
display screenshots, disk access, and VIC-II/CIA/SID/sprite state — use
it to validate each development step against real emulator state instead
of guessing at runtime behavior.

## VIC-20 documentation

Official hardware reference:

```
/Users/aboudou/kDrive Arnaud et Magali/Arnaud/VIC-20/VQRH12.pdf
```

(VIC-20 Programmer's Reference Guide — VIC-I, VIA1/VIA2, memory, character
set, BASIC/KERNAL). Consult it before implementing any low-level hardware
access (software sprites, scrolling, sound, keyboard/joystick reading,
raster IRQ) instead of guessing addresses or register behavior.

## Current project state

Repository not yet initialized in Git. The custom linker script for the
0+1+2+3+5 memory configuration is ready
([link/vic20-fruit-invaders.ld](link/vic20-fruit-invaders.ld), see
[link/README.md](link/README.md) for the `.block0`/`.block5` banks).
Graphics approach and sprite storage format are decided and implemented
(see "Graphics and colors" and "Sprite storage format"):
[src/graphics/charmem.c](src/graphics/charmem.c) is the shared
volatile-aware, inverting loader into character memory at `$1400`;
[src/graphics/sprites.c](src/graphics/sprites.c) holds the apple/carrot/
grapes/pepper (2 animation frames each)/ship/shot bitmaps on top of it;
[src/graphics/font.c](src/graphics/font.c) is a minimal hand-authored
uppercase font (only the letters/`/` actually used in title/game screen
text) sharing the same character set; [src/graphics/bigfont.c](src/graphics/bigfont.c)
is the separate 1x2-cell "big" font used only for the title screen's game
name (also sharing that character set); [src/graphics/decor.c](src/graphics/decor.c)
holds the title screen's marquee/star/arrow tiles on top of it (also
sharing that character set); [src/graphics/screen.c](src/graphics/screen.c)
writes the screen matrix and color RAM. [src/gameplay/title.c](src/gameplay/title.c)
is the title screen (see "Title screen" for the full breakdown), plus a looping tune (see
`sound_music_start()`/`sound_music_tick()`/`sound_music_stop()` in
[src/sound/sound.c](src/sound/sound.c)) that starts as the screen is drawn
and stops the moment Space is pressed; it returns when Space is pressed
(polled via the KERNAL `GETIN` call, `cbm_k_getin()`, with no dedicated
input module yet — see "Still to define/do" below).
[src/gameplay/game.c](src/gameplay/game.c) is the real gameplay screen: HUD
(lives icons left, "LEVEL:NN" right), ship movement (`S`/`D`, clamped to the
screen) and single-shot firing (`Space`, travels straight up and vanishes at
the top of the game zone), a pre-game 3-2-1 countdown, and a fruit grid
(6 cols x 5 rows, one fruit type per row, cycling apple/carrot/grapes/pepper)
that marches sideways one character per step and steps down a row at each
screen edge, speeding up slightly on every step-down -- "edge" is recomputed
every tick from whichever columns still have a live fruit (`grid_alive_col_bounds()`),
not the original formation's fixed span, so bouncing correctly happens
sooner once an outer column has been fully cleared out. When the grid's bottom
row reaches the ship's lane, that row disappears, a life is lost (HUD
updated), the ship plays the shared explosion effect (see "Sprite storage
format" -- the ship is its first user), and the border flashes twice before
play resumes from where it was. On the last life, the screen shows
"YOU LOSE"/"PRESS SPACE" and returns to the title screen on `Space`. Still
returns to the title screen on `H` at any point (a debug/testing shortcut).
The shot kills whichever individual fruit it overlaps (per-fruit alive
state, independent of the whole-row loss above -- see `fruit_alive[][]` and
`grid_check_hit()`), playing the same shared explosion effect at that
fruit's position; when every fruit is gone (by shot or by row-loss, however
the formation empties out), the level counter advances and a fresh full
formation spawns at the starting position, one notch faster and with one
more enemy shooter than the level before, lives carried over except for
the periodic bonus life (see `advance_level()` and "Levels").
[src/sound/sound.c](src/sound/sound.c) is the sound
module: three one-shot effects via the VIC-I's own sound generator
(`$900A`-`$900E`, no VIA timers/IRQ needed -- see "Source code
organization", "Sound"). Register format and the chromatic note table its
two tone-based effects use come straight from VQRH12.pdf's "SOUND AND
MUSIC" chapter (p.45-46, note values already include the enable bit) and
the memory map (p.61, confirms the `struct __vic` field layout in
`<vic20.h>`'s `voice1/2/3`/`noise`/`volume_color`). `sound_explosion()` (a
noise burst) is non-blocking and paired with a `sound_stop()` call, so it
plays underneath the *existing* two-frame wait in `explode_ship()`/
`explode_fruit()` rather than adding its own sequential delay -- both
already share the one explosion effect visually (see "Sprite storage
format"), and now share this one sound too. `sound_level_up()` (rising
C-E-G-C on voice3) and `sound_game_over()` (falling G-D#-C-G on voice1) are
self-contained blocking calls, played from `advance_level()` (right after
its green border flash) and `show_lose_screen()` (right after drawing the
"YOU LOSE" text) respectively. `sound_init()` sets the master volume
(`$900E` low nibble) once at startup, called from
[src/main.c](src/main.c) alongside `sprites_load()`/`font_load()`. Verified
in VICE via the `vice` MCP server: `sound_init()`'s volume write confirmed by reading
`$900E` directly; `sound_explosion()`/`sound_stop()`'s on/off pair confirmed
by setting an execution breakpoint on `explode_ship()` (via a `-Wl,-Map=`
build to get its address, since LTO inlines the sound_* calls themselves
and leaves no symbol of their own) and reading `$900D` before/after --
`90` during the burst, `00` once `explode_ship()` returns; `sound_game_over()`
confirmed the same way against `show_lose_screen()`, reading `$900A` back
at `00` (self-silenced) once execution reached the post-text KERNAL
`GETIN` wait, with "YOU LOSE" already visible in screen memory at that
point. `sound_level_up()` wasn't separately hardware-verified (same
`wait_jiffies`-paced write-then-clear pattern as `sound_game_over()`, just
a different voice/notes) -- low-risk enough to accept on code review alone
given the other two confirmed the pattern end-to-end. The title screen also
has a fourth, structurally different piece: a looping tune on voice2
(alto, left free by the three effects above), needed to *not* block Space
the way the blocking effects safely can (they only ever run after the
player has already lost control for a moment -- an explosion, a level
transition, game over). It's a small tick-driven step sequencer instead:
`sound_music_start()` resets a 16-step note array (`music_notes[]`, a short
bouncy C-F-G riff ending on a one-step rest so the loop point reads as a
phrase end, not a cut-off note) to its first step; `sound_music_tick()`,
called on every pass of title.c's existing `wait_jiffies_or_space()` busy
loop (the same loop already polling `cbm_k_getin()` for Space every pass,
not just once per jiffy), advances to the next step once
`MUSIC_STEP_JIFFIES` have elapsed and is a cheap no-op otherwise, so Space
detection is never delayed; `sound_music_stop()` silences voice2 once Space
is seen, before returning to `game_screen_run()` (which never touches
voice2, but leaving a note ringing into the countdown would still be
wrong). Verified in VICE via the `vice` MCP server: repeated `$900B` reads while idle
on the title screen showed it changing value over time (the tune
progressing) without any checkpoint pausing execution; a Space press was
still answered immediately (screen advanced to the countdown on the very
next screenshot); `$900B` read back `00` right after, confirming
`sound_music_stop()`. [src/main.c](src/main.c) loads resources once, then loops forever
alternating `title_screen_run()` and `game_screen_run()`. Verified visually
in VICE via the `vice` MCP server (`vice_keyboard_type`, not
`vice_keyboard_key_press`, to inject keys — see gotcha below). Build with:

```bash
/Users/aboudou/Developer/VIC-20/llvm-mos/bin/mos-vic20-clang \
    -Os -T link/vic20-fruit-invaders.ld -o game.prg \
    src/main.c src/gameplay/title.c src/gameplay/game.c \
    src/graphics/sprites.c src/graphics/screen.c src/graphics/font.c \
    src/graphics/bigfont.c src/graphics/decor.c src/graphics/charmem.c \
    src/sound/sound.c
```

**VICE MCP server gotcha**: `vice_keyboard_key_press` (matrix-level key emulation)
was observed to leave the KERNAL keyboard buffer (`$C6`/`KEY_COUNT`)
permanently at 0 on this VIC-20 target, even holding the key for multiple
seconds — confirmed by pausing execution mid-hold and reading `$C6`
directly. `vice_keyboard_type` (buffer injection) works reliably instead;
prefer it for any future scripted-input testing here.

Still to define/do: a real input module (title/game screens poll
`cbm_k_getin()` directly for now — see "Input" in "Source code
organization"), gameplay-screen music (only the title screen has one so
far, plus the three one-shot effects — see
[src/sound/sound.c](src/sound/sound.c)), more fruit types, and a proper
`.gitignore`/Git init.

**VICE MCP server gotcha 2 (2026-09-06)**: this project's `vice`-MCP-driven VICE
instance can run dramatically slower than real time even with
`WarpMode`/`Speed` resources reporting normal (0/100) — measured at one
point running at ~8% of real PAL speed (34 jiffies of KERNAL clock advance,
read directly from `$A2`, over 8.9 real seconds bracketed by `date`, versus
the ~445 a real 50Hz PAL machine would produce). Likely a host resource
constraint on this specific sandboxed instance, not a config issue. Two
consequences for testing: (1) wall-clock `sleep`-then-screenshot testing of
any jiffy-paced behavior (movement pacing, animation, a timed event) is
unreliable here — a screenshot can land far later in game-time than the
`sleep` duration suggests, since real time keeps passing during every tool
round-trip regardless of what's `sleep`d; (2) to sanity-check the visual
result of a rare/brief event (an explosion frame, a step-down) without
waiting out the real pacing, build a throwaway copy of the source with its
jiffy constants (e.g. `GRID_MOVE_JIFFIES_BASE/MIN`, `COUNTDOWN_JIFFIES`)
temporarily slashed to 1-3, compile and run *that* `.prg` instead of
editing the real timing constants, and/or read the relevant character
memory directly (`vice_memory_read` at `$1400 + code*8`) to confirm a
sprite's bitmap loaded correctly instead of trying to catch it on screen.
