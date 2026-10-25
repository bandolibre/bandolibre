#!/usr/bin/env python3
"""Flash firmware to the board on the connected ST-Link, after checking that its
chip is registered as the target board type.

Examples:
  flash.py wing-g474 build/Debug/wing-g474.bin
  flash.py main-g474 build/Release/main-g474.bin --address 0x08008000

--address exists because the main board can carry two images: the UF2
bootloader at 0x08000000 and the Release application above it. See
code/boot-g474/memmap.h.
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from memmap import props_base
from registry import BOARDS, load_registry, read_uid, st_flash


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("board", choices=BOARDS, help="Board type to flash")
    parser.add_argument("binary", type=Path, help="Binary (.bin) file to flash")
    parser.add_argument("--address", type=lambda v: int(v, 0), default=0x08000000,
                        help="Flash address to write at (default 0x08000000)")
    args = parser.parse_args()

    # st-flash erases only the pages it writes, so an image that stops below
    # PROPS_BASE leaves the main board's saved properties intact.
    if args.board == "main-g474":
        end = args.address + args.binary.stat().st_size
        if end > props_base():
            print(f"Error: {args.binary} would end at 0x{end:08x}, past the saved "
                  f"properties at 0x{props_base():08x}; flashing it would erase them.",
                  file=sys.stderr)
            sys.exit(1)

    uid = read_uid()
    board = load_registry().get(uid)
    if board is None:
        print(f"Error: chip {uid} is not registered. Run 'just registry_add' first.",
              file=sys.stderr)
        sys.exit(1)
    if board != args.board:
        print(f"Error: the connected chip {uid} is a {board}, not a {args.board}.",
              file=sys.stderr)
        sys.exit(1)

    print(f"Flashing {args.board} (chip {uid}) at 0x{args.address:08x}…")
    result = st_flash(None, ["--reset", "write", str(args.binary), f"0x{args.address:08x}"])
    if result.returncode != 0:
        print("Error: flash failed.", file=sys.stderr)
        sys.exit(1)
    print("  Done.")


if __name__ == "__main__":
    main()
