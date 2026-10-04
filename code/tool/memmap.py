"""Read the main board flash split from code/boot-g474/memmap.h.

flash.py and uf2.py use it to refuse a binary that would run into the saved
properties, so the address is read from the header rather than repeated here.
"""

import re
from pathlib import Path

MEMMAP_H = Path(__file__).resolve().parents[1] / "boot-g474" / "memmap.h"


def defines() -> dict[str, int]:
    """The integer #defines of memmap.h, evaluated in order."""
    values: dict[str, int] = {}
    for name, expr in re.findall(r"^#define\s+(\w+)\s+(.+?)\s*$",
                                 MEMMAP_H.read_text(), re.MULTILINE):
        cleaned = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)U?L?L?\b", r"\1", expr)
        for other, value in values.items():
            cleaned = re.sub(rf"\b{other}\b", str(value), cleaned)
        if re.fullmatch(r"[0-9a-fA-FxX()+\-*\s]+", cleaned):
            values[name] = eval(cleaned, {"__builtins__": {}}, {})  # noqa: S307 - fixed grammar
    return values


def props_base() -> int:
    """First address of the saved properties; no image may reach it."""
    return defines()["PROPS_BASE"]
