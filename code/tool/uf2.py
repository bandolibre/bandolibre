#!/usr/bin/env python3
"""Package a firmware binary as a UF2 image for the Bandolibre USB bootloader.

A UF2 file is a plain sequence of 512-byte blocks, each one carrying the
address it belongs at, a family id identifying the chip, and its position in
the file. That is what lets the bootloader accept a drag-and-drop copy: it
inspects every sector the host writes to the volume, keeps the ones that are
UF2 blocks for this chip, and ignores the filesystem bookkeeping around them.

Format reference: https://github.com/microsoft/uf2

Examples:
  uf2.py build/Release/main-g474.bin build/Release/main-g474.uf2
  uf2.py build/Release/main-g474.bin build/Release/main-v1_2.uf2 \\
      --with-bootloader ../boot-g474/build/Release/boot-g474.bin
  uf2.py --bootloader build/Release/boot-g474.bin build/Release/main-bootloader-v1_2.uf2

--with-bootloader makes the release file: the bootloader and the firmware as
two UF2 files back to back, each with its own block count (the format allows
concatenation). The BANDOLIBRE drive and the update tool's MIDI update take the
firmware only: the drive drops blocks outside the application region before
counting them, so the firmware part completes on its own. The update tool in
STM32 DFU mode writes both.

A --bootloader image is the bootloader alone, for DFU mode only: copied onto the
drive it does nothing.
"""

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from memmap import defines, props_base

# Must match APP_BASE and UF2_FAMILY_ID in code/boot-g474/memmap.h.
# code/tests/test_dfu_config.py fails if they drift apart.
DEFAULT_BASE = 0x08008000
STM32G4_FAMILY_ID = 0x4C71240A

UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30

UF2_FLAG_FAMILY_ID = 0x00002000

BLOCK_SIZE = 512
PAYLOAD_SIZE = 256


# Reimplemented rather than imported from upstream's utils/uf2conv.py:
#   + no new submodule just to reach one function
#   + upstream's convert_to_uf2() takes no args — it reads two module
#     globals the caller must set first; depending on that is depending on
#     unstable internal state, not a documented API
#   + our safety check (reject a binary linked at the bootloader's own
#     address) has no upstream equivalent, so a wrapper is needed regardless
#   - upstream is battle-tested by a much larger ecosystem; ours is only
#     checked against our own build (see test_dfu_config.py)
def to_uf2(data: bytes, base: int, family: int) -> bytes:
    """Convert a flat binary to UF2 blocks, one per PAYLOAD_SIZE of input."""
    num_blocks = (len(data) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE
    if num_blocks == 0:
        print("Error: input binary is empty.", file=sys.stderr)
        sys.exit(1)

    out = bytearray()
    for block_no in range(num_blocks):
        chunk = data[block_no * PAYLOAD_SIZE : (block_no + 1) * PAYLOAD_SIZE]
        # The header is eight 32-bit words; the last one holds the family id
        # rather than a file size because UF2_FLAG_FAMILY_ID is set.
        out += struct.pack(
            "<8I",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID,
            base + block_no * PAYLOAD_SIZE,
            len(chunk),
            block_no,
            num_blocks,
            family,
        )
        out += chunk.ljust(476, b"\x00")
        out += struct.pack("<I", UF2_MAGIC_END)

    assert len(out) == num_blocks * BLOCK_SIZE
    return bytes(out)


def auto_int(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("binary", type=Path, help="Flat firmware binary (.bin) to convert")
    parser.add_argument("output", type=Path, help="UF2 file to write")
    parser.add_argument("--base", type=auto_int, default=DEFAULT_BASE,
                        help="Flash address the binary is linked at "
                             f"(default 0x{DEFAULT_BASE:08X}, the application region)")
    parser.add_argument("--family", type=auto_int, default=STM32G4_FAMILY_ID,
                        help=f"UF2 family id (default 0x{STM32G4_FAMILY_ID:08X}, STM32G4)")
    parser.add_argument("--bootloader", action="store_true",
                        help="package the bootloader, at BOOT_BASE, for the update tool's DFU mode")
    parser.add_argument("--with-bootloader", type=Path, metavar="BOOT_BIN",
                        help="put this bootloader binary in the file too, ahead of the firmware")
    args = parser.parse_args()

    data = args.binary.read_bytes()

    if args.bootloader:
        check_bootloader_size(data)
        args.base = defines()["BOOT_BASE"]
    # A binary linked at 0x08000000 is a Debug image or another project's
    # firmware; the bootloader would reject every block of it, leaving the user
    # staring at a drive that silently does nothing.
    elif args.base == 0x08000000:
        print("Error: --base 0x08000000 is the bootloader's own region. "
              "Package the Release build, which links at "
              f"0x{DEFAULT_BASE:08X}.", file=sys.stderr)
        sys.exit(1)

    # The bootloader refuses such an image too, but older bootloaders allowed
    # a larger application region; stop it here before it reaches a board.
    if args.base + len(data) > props_base():
        print(f"Error: {len(data)} bytes at 0x{args.base:08X} would run into the "
              f"saved properties at 0x{props_base():08X}.", file=sys.stderr)
        sys.exit(1)

    uf2 = to_uf2(data, args.base, args.family)
    summary = f"{len(data)} bytes at 0x{args.base:08X}"
    if args.with_bootloader:
        boot = args.with_bootloader.read_bytes()
        check_bootloader_size(boot)
        uf2 = to_uf2(boot, defines()["BOOT_BASE"], args.family) + uf2
        summary = f"bootloader {len(boot)} bytes + firmware {summary}"
    args.output.write_bytes(uf2)

    blocks = len(uf2) // BLOCK_SIZE
    print(f"{args.binary} -> {args.output}: {summary}, {blocks} blocks, {len(uf2)} bytes")


def check_bootloader_size(data: bytes) -> None:
    size = defines()["BOOT_SIZE"]
    if len(data) > size:
        print(f"Error: {len(data)} bytes do not fit the {size}-byte bootloader region.",
              file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
