#!/usr/bin/env python3
"""Flash firmware to the board on the connected ST-Link, after checking that its
chip is registered as the target board type.

Examples:
  flash.py wing-g474 build/Debug/wing-g474.bin
  flash.py main-g474 build/Release/main-g474.bin --address 0x08008000

--address exists because the main board carries two images: the bootloader
at 0x08000000 and the application (Debug or Release) above it. See
code/boot-g474/memmap.h.
"""

import argparse
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from memmap import defines, props_base
from registry import BOARDS, load_registry, read_uid, st_flash


def bootloader_installed() -> bool:
    """Whether the reset vector at BOOT_BASE points into the bootloader slot.

    Without a bootloader, an application written at APP_BASE never runs: the
    CPU starts from whatever image sits at BOOT_BASE, typically an old
    application linked for APP_BASE whose reset vector then lands in the middle
    of the new code."""
    m = defines()
    with tempfile.NamedTemporaryFile() as f:
        st_flash(None, ["read", f.name, hex(m["BOOT_BASE"]), "8"], capture_output=True)
        head = Path(f.name).read_bytes()
    if len(head) < 8:
        return False
    _sp, pc = struct.unpack("<II", head)
    return m["BOOT_BASE"] <= pc < m["APP_BASE"]


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

    if (args.board == "main-g474" and args.address == defines()["APP_BASE"]
            and not bootloader_installed()):
        print("Error: no bootloader at 0x08000000, so the application would never "
              "start. Install it first: cd code/boot-g474 && just flash",
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
