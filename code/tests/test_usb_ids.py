"""Verify the USB identity the two firmware images declare.

The vendor id is STMicroelectronics' and the product id is sublicensed from ST
for this project: valid only for a product built on ST silicon, and only for
Bandolibre. Neither may drift back to TinyUSB's development ids (0xCafe /
0x4000 | class bitmap), which are fine on a bench and not fine on an instrument
that leaves one.

One product id covers the whole product, so the application (MIDI) and the
bootloader (mass storage) share VID:PID and are told apart by bcdDevice. Hosts
cache a driver per VID/PID/revision, so if the two revisions ever collide, a
host that has seen one device can bind the wrong driver to the other. Nothing
in the build notices; it shows up as a board that enumerates but does not work,
on one machine, after a firmware update.
"""

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

APP_DESCRIPTORS = REPO_ROOT / "code" / "main-g474" / "usb" / "usb_descriptors.c"
BOOT_DESCRIPTORS = REPO_ROOT / "code" / "boot-g474" / "usb_descriptors.c"

USB_VID = 0x0483  # STMicroelectronics
USB_PID = 0xA5B4  # sublicensed to Bandolibre


def defined_value(path: Path, name: str) -> int:
    """Read a `#define <name> <hex>` from a descriptor file."""
    match = re.search(rf"^#define\s+{name}\s+(0[xX][0-9a-fA-F]+)", path.read_text(), re.M)
    if match is None:
        raise AssertionError(f"no #define {name} in {path}")
    return int(match.group(1), 16)


def field_value(path: Path, name: str) -> int:
    """Read a `.<name> = <hex>,` initializer from the device descriptor."""
    match = re.search(rf"^\s*\.{name}\s*=\s*(0[xX][0-9a-fA-F]+),", path.read_text(), re.M)
    if match is None:
        raise AssertionError(f"no .{name} initializer in {path}")
    return int(match.group(1), 16)


class TestLicensedIds(unittest.TestCase):
    def test_application_uses_licensed_ids(self):
        self.assertEqual(defined_value(APP_DESCRIPTORS, "USB_VID"), USB_VID)
        self.assertEqual(defined_value(APP_DESCRIPTORS, "USB_PID"), USB_PID)

    def test_bootloader_uses_licensed_ids(self):
        self.assertEqual(defined_value(BOOT_DESCRIPTORS, "USB_VID"), USB_VID)
        self.assertEqual(defined_value(BOOT_DESCRIPTORS, "USB_PID"), USB_PID)

    def test_descriptors_use_the_defines(self):
        """A literal in the descriptor would pass the checks above and still ship
        the wrong id."""
        for path in (APP_DESCRIPTORS, BOOT_DESCRIPTORS):
            with self.subTest(path=path.name):
                text = path.read_text()
                for field, macro in (("idVendor", "USB_VID"), ("idProduct", "USB_PID")):
                    self.assertIsNotNone(
                        re.search(rf"^\s*\.{field}\s*=\s*{macro},", text, re.M),
                        f".{field} in {path.name} does not use {macro}",
                    )


class TestDeviceRevision(unittest.TestCase):
    def test_revisions_differ(self):
        """The bootloader has two configurations (MIDI only, MIDI + drive),
        each with its own revision, and the application a third."""
        revisions = {
            "application": field_value(APP_DESCRIPTORS, "bcdDevice"),
            "bootloader MIDI only": defined_value(BOOT_DESCRIPTORS, "BCD_DEVICE_MIDI"),
            "bootloader MIDI + drive": defined_value(BOOT_DESCRIPTORS, "BCD_DEVICE_DRIVE"),
        }
        self.assertEqual(
            len(set(revisions.values())), len(revisions),
            f"bcdDevice values collide: {revisions}. Application and bootloader "
            "share VID:PID, so bcdDevice is all that keeps a host from caching "
            "one interface layout's drivers for another",
        )


def device_name_strings(path: Path) -> list[str]:
    """The manufacturer and product strings (indices 1 and 2) of a descriptor
    file, with a #define'd name resolved."""
    text = path.read_text()
    array = re.search(r"string_desc_arr\[\]\s*=\s*\{(.*?)\};", text, re.S)
    assert array, f"no string_desc_arr in {path}"
    entries = [line.split("//")[0].strip().rstrip(",")
               for line in array.group(1).splitlines() if line.strip()]
    defines = dict(re.findall(r'^#define\s+(\w+)\s+"([^"]*)"', text, re.M))
    names = []
    for entry in entries[1:3]:
        literal = re.fullmatch(r'"([^"]*)"', entry)
        names.append(literal.group(1) if literal else defines[entry])
    return names


class TestDeviceNames(unittest.TestCase):
    """The web tools pick the instrument by its MIDI port name and leave the
    bootloader alone: /bandolibre/i and not /firmware update/i. Windows names
    the port after the manufacturer, Linux and macOS after the product, so
    both strings must follow the rule (see the midi-autoconnect skill)."""

    def test_application_is_plain_bandolibre(self):
        for name in device_name_strings(APP_DESCRIPTORS):
            self.assertRegex(name, r"(?i)bandolibre")
            self.assertNotRegex(name, r"(?i)firmware update")

    def test_bootloader_says_firmware_update(self):
        for name in device_name_strings(BOOT_DESCRIPTORS):
            self.assertRegex(name, r"(?i)bandolibre")
            self.assertRegex(name, r"(?i)firmware update")


if __name__ == "__main__":
    unittest.main()
