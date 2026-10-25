#!/usr/bin/env python3
"""Board registry for STM32 chips — prevents flashing the wrong firmware."""

import argparse
import csv
import subprocess
import sys
import tempfile
from pathlib import Path

REGISTRY = Path(__file__).parent / "boards.csv"
BOARDS = ["main-g474", "wing-g474"]
UID_ADDR = 0x1FFF7590
UID_LEN = 12


def probe_serial() -> str | None:
    """The serial of the one connected ST-Link, or None if there are several
    or none. st-flash needs it: without --serial it may open another ST USB
    device, such as the main board's own USB port (0483:a5b4), and fail."""
    result = subprocess.run(["st-info", "--probe"], capture_output=True, text=True)
    serials = [line.split(":", 1)[1].strip() for line in result.stdout.splitlines()
               if line.strip().startswith("serial:")]
    return serials[0] if len(serials) == 1 else None


def st_flash(serial: str | None, args: list[str], **kwargs) -> subprocess.CompletedProcess:
    """Runs st-flash on the given (or the one connected) ST-Link. If it cannot
    connect, retries under reset: a running firmware can refuse a plain
    debugger attach."""
    cmd = ["st-flash"]
    serial = serial or probe_serial()
    if serial:
        cmd += ["--serial", serial]
    result = subprocess.run(cmd + args, **kwargs)
    if result.returncode != 0:
        result = subprocess.run(cmd + ["--connect-under-reset"] + args, **kwargs)
    return result


def read_uid(serial: str | None = None) -> str:
    with tempfile.NamedTemporaryFile() as f:
        result = st_flash(serial, ["read", f.name, hex(UID_ADDR), str(UID_LEN)],
                          capture_output=True)
        uid = Path(f.name).read_bytes().hex()
        if result.returncode != 0 or not uid or set(uid) <= {"0", "f"}:
            label = f"ST-Link {serial}" if serial else "connected board"
            print(f"Error: could not read from {label}. Is the board connected and powered?", file=sys.stderr)
            sys.exit(1)
        return uid


def load_registry() -> dict[str, str]:
    if not REGISTRY.exists():
        return {}
    rows = {}
    with REGISTRY.open() as f:
        for row in csv.reader(f):
            if row and not row[0].startswith("#"):
                rows[row[0].strip()] = row[1].strip()
    return rows


def save_registry(entries: dict[str, str]) -> None:
    with REGISTRY.open("w") as f:
        f.write("# uid,board\n")
        f.write("# uid is the 96-bit STM32 unique device ID read from 0x1FFF7590, hex-encoded, lowercase.\n")
        for uid, board in entries.items():
            f.write(f"{uid},{board}\n")


def cmd_add(args: argparse.Namespace) -> None:
    print("Probing connected board...")
    uid = read_uid()
    print(f"Chip UID: {uid}")

    entries = load_registry()
    if uid in entries:
        print(f"Warning: this chip is already registered as: {entries[uid]}")
        if input("Overwrite? [y/N] ").strip().lower() != "y":
            sys.exit(0)

    entries[uid] = args.board
    save_registry(entries)
    print(f"Registered {uid} as {args.board}.")


def cmd_check(args: argparse.Namespace) -> None:
    uid = read_uid()
    entries = load_registry()

    if uid not in entries:
        print(f"Error: chip {uid} is not registered. Run 'registry.py add' first.", file=sys.stderr)
        sys.exit(1)

    actual = entries[uid]
    if actual != args.board:
        print(f"Error: connected chip is '{actual}' but you are trying to flash '{args.board}'. Aborting.", file=sys.stderr)
        sys.exit(1)

    print(f"OK: chip {uid} confirmed as {actual}.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    p_add = sub.add_parser("add", help="Probe the connected board and record it in the registry.")
    p_add.add_argument("board", choices=BOARDS)

    p_check = sub.add_parser("check", help="Check that the connected board matches the expected type.")
    p_check.add_argument("board", choices=BOARDS)

    args = parser.parse_args()
    {"add": cmd_add, "check": cmd_check}[args.command](args)


if __name__ == "__main__":
    main()
