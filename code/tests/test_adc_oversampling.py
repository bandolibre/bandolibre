"""Verify the bellows' two Hall-sensor ADCs (ADC3/ADC4 on main-g474) use matched
oversampling in the .ioc.

A prior regression (commit 7b5c2ad "Oversample the bellow") bumped both
channels' Oversampling.Ratio from 64 to 256 but only bumped ADC3's
RightBitShift, leaving ADC4 one bit behind -- silently reporting the two
channels on different effective scales (13-bit vs 14-bit) before bellow.c sums
them unweighted as hall_total = hall0 + hall1. This test would have caught
that: RightBitShift (and Ratio, which sets how many extra bits the shift is
allowed to trade for) must match on both channels so hall0/hall1 stay on the
same scale.
"""

import unittest
from pathlib import Path

from ioc_parser import parse_ioc

REPO_ROOT = Path(__file__).resolve().parents[2]


def main_g474_settings() -> dict[str, str]:
    return parse_ioc(REPO_ROOT / "code" / "main-g474" / "main-g474.ioc")


class TestBellowsAdcOversampling(unittest.TestCase):
    """ADC3 (hall0) and ADC4 (hall1) must report the bellows' two Hall sensors
    on the same scale, or bellow.c's unweighted hall0+hall1 sum silently
    double-weights whichever channel has the smaller RightBitShift."""

    def test_oversampling_enabled_on_both_channels(self):
        settings = main_g474_settings()
        for adc in ("ADC3", "ADC4"):
            with self.subTest(adc=adc):
                self.assertEqual(settings.get(f"{adc}.OversamplingMode"), "ENABLE")

    def test_ratio_matches(self):
        settings = main_g474_settings()
        self.assertEqual(settings.get("ADC3.Ratio"), settings.get("ADC4.Ratio"))

    def test_right_bit_shift_matches(self):
        settings = main_g474_settings()
        self.assertEqual(settings.get("ADC3.RightBitShift"), settings.get("ADC4.RightBitShift"))


if __name__ == "__main__":
    unittest.main()
