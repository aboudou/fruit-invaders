# Custom link script — 0+1+2+3+5 memory configuration

`vic20-fruit-invaders.ld` replaces the llvm-mos SDK's default `link.ld` to
express the project's memory configuration (expansion blocks 0, 1, 2, 3 and
5 active, block 4 inactive — see [CLAUDE.md](../CLAUDE.md)).

## Build

```bash
/Users/aboudou/Developer/VIC-20/llvm-mos/bin/mos-vic20-clang \
    -Os -fno-lto \
    -T link/vic20-fruit-invaders.ld \
    -o game.prg \
    src/*.c
```

Two important points to follow:

- **`-T` as a direct clang driver flag**, not `-Wl,-T,...`: the llvm-mos
  driver always appends `-Tlink.ld` (the default script) to the linker
  command line. A `-T` passed directly to the clang driver replaces that
  default script; a `-Wl,-T,...` is added on top of it, and `ld.lld` then
  processes both scripts cumulatively, causing a `region 'ram' already
  defined` error.
- **`-fno-lto` is mandatory** as soon as the program places data in the
  `.block0` / `.block5` banks (see below). With the full LTO enabled by
  default by the SDK, a `const` array fully read with constant indices can
  be replaced by its literal values and its section removed before the
  linker even sees the script's `KEEP()`. Without `-fno-lto`, memory
  placement is no longer reliable for this kind of data (typically content
  read directly by hardware — custom character set, sprites — without going
  through a C indexed access visible to the optimizer).

## Memory regions produced

| Region        | Addresses         | Size    | Content                                   |
|---------------|-------------------|---------|--------------------------------------------|
| `ram`         | `$1201`–`$7FFF`   | ~27.5 KB | Code, rodata, data, bss, heap, software stack (identical to the SDK's standard "24 KB" configuration) |
| `ram_block0`  | `$0400`–`$0FFF`   | 3 KB    | `.block0` section (orphan zone, ignored by the KERNAL as soon as an 8K+ block is active) |
| `ram_block5`  | `$A000`–`$BFFF`   | 8 KB    | `.block5` section (cartridge block used as RAM) |

`$1000`–`$11FF` serves as the text screen (automatically relocated by the
KERNAL as soon as a block 1/2/3 is active); `$8000`–`$9FFF` (block 4) stays
character ROM / I/O, unused.

A `.prg` file only carries a single block of bytes loaded at a single
address: `ram_block0` and `ram_block5` therefore cannot be preloaded
directly. Their initial content is stored (LMA) in `ram` — so it is indeed
present in the `.prg` — and must be explicitly copied to its final address
(VMA) at startup.

## Using the `.block0` / `.block5` banks

```c
__attribute__((section(".block0")))
const unsigned char custom_charset[3072] = { /* ... */ };

extern unsigned char __block0_load_start[];
extern unsigned char __block0_start[];
extern unsigned int  __block0_size;

void relocate_banks(void) {
    memcpy(__block0_start, __block0_load_start, (unsigned)&__block0_size);
    /* same for __block5_load_start / __block5_start / __block5_size */
}
```

Call `relocate_banks()` exactly once at startup, before any use of data
placed in these banks. The symbols
`__block0_start`/`__block0_end`/`__block5_start`/`__block5_end` delimit each
zone; `ASSERT`s in the script make the link fail if the placed content
exceeds the available size (3 KB / 8 KB).
