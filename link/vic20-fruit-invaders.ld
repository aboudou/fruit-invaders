/* Fruit Invaders - VIC-20 PAL - custom link script.
 *
 * Fixed memory configuration targeted by the project: expansion blocks
 * 0, 1, 2 and 3 active (24 KB contiguous starting at $1200) PLUS block 5
 * ($A000-$BFFF, 8 KB, normally reserved for cartridges). Block 4
 * ($8000-$9FFF, character ROM / extended I/O) stays inactive.
 *
 * The SDK's default link script (mos-platform/vic20/lib/link.ld) only
 * knows the standard contiguous 0/3/8/16/24 KB combinations starting at
 * block 1, and completely ignores block 5: it therefore cannot express
 * this configuration. This file is a derived, frozen version of it (no
 * PROVIDE/ASSERT on __memory_expansion), which adds two extra disjoint
 * RAM zones:
 *
 *   - block 0 ($0400-$0FFF, 3 KB): as soon as an 8K+ block (1, 2 or 3) is
 *     active, the KERNAL relocates the text screen to $1000-$11FF and
 *     sets the start-of-memory pointer (MEMSTR, $0281/$0282) to $1200 --
 *     it then NEVER includes block 0 in the contiguous zone, even though
 *     it is physically present (Programmer's Reference Guide, "COMMON
 *     POINTER VALUES", p.48-49 and p.59). Block 0 therefore remains a
 *     valid but orphan RAM zone, only reachable by addressing
 *     $0400-$0FFF directly.
 *
 *   - main zone ($1201-$7FFF, ~27.5 KB usable): $1000-$11FF serves as the
 *     text screen (automatically relocated), then contiguous user program
 *     up to the end of block 3 ($7FFF). This is exactly the standard
 *     "24 KB" configuration (blocks 1+2+3) of the original link.ld
 *     (__memory_expansion = 24): the code, the C library and the heap
 *     live here, without changing anything to the SDK's usual conventions.
 *
 *   - block 5 ($A000-$BFFF, 8 KB): cartridge zone normally in ROM, used
 *     here as RAM by the expansion cartridge. With block 4 ($8000-$9FFF)
 *     inactive between the two, this is a second disjoint zone, not
 *     managed by the KERNAL.
 *
 * A .prg file can only contain a single block of bytes loaded at a single
 * address (standard PRG format: 2 load-address bytes followed by the
 * data). Blocks 0 and 5 therefore cannot be preloaded directly by LOAD:
 * they are exposed via two dedicated output sections (.block0 / .block5)
 * whose initial content is stored (LMA) in the main zone -- so it is
 * indeed present in the .prg -- and must be explicitly copied to its
 * final address (VMA) at game startup, the same way the C runtime already
 * does for .data.
 * See the symbols __block0_load_start/__block0_start/__block0_size
 * (and the __block5_ equivalents) below: placing data in these banks is
 * done with
 *   __attribute__((section(".block0"))) const unsigned char sprites[] = ...;
 * followed by a simple memory copy (memcpy or loop) from
 * __block0_load_start to __block0_start over __block0_size bytes,
 * to be run once before any use of that data.
 *
 * IMPORTANT -- compile with -fno-lto: the SDK enables full LTO by
 * default, which can evaluate at compile time a const array fully read
 * with constant indices and simply remove its section (even with a
 * linker-side KEEP(), which has nothing left to keep once LTO has already
 * made the symbol disappear upstream). Since .block0/.block5 typically
 * hold data read directly by hardware (VIC, custom character set) without
 * necessarily going through a C indexed access visible to the optimizer,
 * -fno-lto makes section placement predictable and avoids this class of
 * surprises. Verified empirically (llvm-mos, mos-vic20-clang): see the
 * README in the link/ directory.
 *
 * Derived from mos-platform/vic20/lib/link.ld and
 * mos-platform/commodore/lib/commodore.ld.
 *
 * RESERVED WINDOW ($1400-$1FFF, 3 KB) -- the custom character set is read
 * by the VIC-I directly from RAM at a fixed hardware address (character
 * base register $9005/VICCR5, one of exactly four windows: $1000/$1400/
 * $1800/$1C00 -- see CLAUDE.md, "Sprite storage format"). $1000 is already
 * the relocated text screen, so this project uses $1400.
 * $1400-$1FFF sits *inside* the "ram" region below, in the middle of the
 * address range the linker would otherwise freely use for code/rodata/
 * data -- and unlike .block0/.block5 (which live outside "ram" entirely
 * and are only ever referenced by name), a normal function or const array
 * can perfectly legally be placed there by the linker's default,
 * size-driven layout. Nothing stopped this from happening: an early build
 * placed part of `put_quad`'s own compiled code at $1400-$146F, which
 * `sprites_load()` (run once at startup) then overwrote with sprite
 * bitmap bytes before the CPU ever got there -- executing sprite data as
 * if it were instructions and crashing back to BASIC. The exact function
 * affected depended on the program's total code size (LTO/layout
 * dependent), which is why adding or removing unrelated code changed
 * whether the bug showed up.
 * Fix: below, in place of a plain `INCLUDE commodore.ld`, this script
 * inlines commodore.ld's own setup (see that file) and inserts a single
 * `. = 0x2000;` between the BASIC stub and the C program's sections --
 * forcing the linker to jump straight from the end of the stub to $2000,
 * so .text/.rodata/.data/.bss (and therefore every function and const
 * array placed by the normal, automatic layout) can never start below
 * $2000. The ~480 bytes between the stub and $1400 are sacrificed as
 * padding along with the reserved window itself; that's the whole cost.
 */

__basic_zp_start = 0x0000;
__basic_zp_end = 0x0090;

MEMORY {
    /* The "ram" region MUST stay declared first: as in commodore.ld,
     * .basic_header is placed without an explicit region below, and the
     * linker then picks the first region in the MEMORY block. Putting it
     * elsewhere would send the startup BASIC stub to the wrong bank and
     * break RUN. */

    /* Main zone: screen ($1000-$11FF) + user program (blocks 1+2+3). */
    ram (rw) : ORIGIN = 0x1201, LENGTH = 0x6DFF

    /* Block 0: $0400-$0FFF (+3K). Isolated RAM zone (see comment above). */
    ram_block0 (rw) : ORIGIN = 0x0400, LENGTH = 0x0C00

    /* Block 5: $A000-$BFFF (+8K). Isolated RAM zone (cartridge used as RAM). */
    ram_block5 (rw) : ORIGIN = 0xA000, LENGTH = 0x2000
}

/* Inlined + modified mos-platform/commodore/lib/commodore.ld: identical
 * except for the `. = 0x2000;` reservation below -- see the header
 * comment for why a plain `INCLUDE commodore.ld` isn't safe here. */
__rc0 = __basic_zp_start;
INCLUDE imag-regs.ld
__basic_zp_size = __basic_zp_end - __basic_zp_start;

MEMORY { zp : ORIGIN = __rc31 + 1, LENGTH = __basic_zp_end - (__rc31 + 1) }

INPUT(basic-header.o)

REGION_ALIAS("c_readonly", ram)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
    /* BASIC program that jumps to _start -- must stay at ORIGIN(ram)
     * ($1201), matching the KERNAL's start-of-program pointer for the
     * 8K+ expansion memory map. */
    .basic_header : { *(.basic_header) } >ram

    /* Reserved: custom character-set window ($1400-$1FFF) -- see the
     * header comment. Explicit output section bound to >ram (rather than a
     * bare top-level ". = 0x2000;") because only a section actually bound
     * to a region advances *that region's* own placement pointer -- a bare
     * assignment only moves the global location counter, which the
     * region-bound sections from c.ld below (>c_readonly / >c_writeable,
     * aliased to ram) turned out not to pick up. */
    .reserved_charmem : {
        . = 0x2000;
    } >ram

    INCLUDE c.ld
}

/* Top of the C runtime's software stack, right above the main zone --
 * identical to the value used by the original link.ld for the standard
 * 24 KB configuration, which our main zone reproduces exactly. */
__stack = 0x8000;

SECTIONS {
    .block0 : {
        __block0_start = .;
        /* KEEP: without this, --gc-sections removes this data as soon as
         * no instruction explicitly references it (e.g. a const array
         * fully folded into constants at compile time). */
        KEEP(*(.block0 .block0.*))
        __block0_end = .;
    } >ram_block0 AT>ram
    __block0_load_start = LOADADDR(.block0);
    __block0_size = SIZEOF(.block0);

    .block5 : {
        __block5_start = .;
        KEEP(*(.block5 .block5.*))
        __block5_end = .;
    } >ram_block5 AT>ram
    __block5_load_start = LOADADDR(.block5);
    __block5_size = SIZEOF(.block5);
}

ASSERT(__block0_size <= 0x0C00, "The .block0 content exceeds the 3 KB available ($0400-$0FFF).")
ASSERT(__block5_size <= 0x2000, "The .block5 content exceeds the 8 KB available ($A000-$BFFF).")

OUTPUT_FORMAT {
    /* Tells the VIC-20's LOAD where to place the file's content. */
    SHORT(ORIGIN(ram))
    TRIM(ram)
}
