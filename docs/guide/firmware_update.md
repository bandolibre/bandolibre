---
---
# Updating the firmware

The main board can be updated over the same USB cable it uses for MIDI. No
programmer, no cable other than the one you already have, and nothing to
install. There are two ways: from the browser (the usual one), and by copying a
file onto a USB drive the instrument shows (no browser needed).

## From the browser

1. Connect the instrument and open the
   [update tool](https://bandolibre.github.io/bandolibre/update/) in Chrome,
   Edge or Firefox (Safari and iOS have no Web MIDI).
2. The tool finds the instrument and shows its firmware and the latest release.
   **Use a file…** (or a drop onto the page) installs a `.uf2` of your own.
3. Click **Update firmware**. The instrument restarts into its firmware update
   mode, where it appears as the MIDI device "Bandolibre firmware update". The
   grid fills in as each page of the firmware is written, then checked.
4. Once the whole firmware checks out, the instrument restarts on it, and the
   tool shows the new version.

Saved settings are kept. Live changes that were not saved are lost at the
restart. The configuration tool and BandoPM can stay open: they never connect
to the update mode on their own, show that an update is running, and reconnect
to the instrument afterwards.

## With the BANDOLIBRE drive

1. Unplug the instrument's USB cable.
2. Hold down **FN2**, the leftmost of the three function buttons.
3. Plug the USB cable back in, keeping FN2 held until a drive appears.
4. A drive called **BANDOLIBRE** shows up, the way a USB stick would. Its LED
   blinks slowly while it waits.
5. Copy `main-g474.uf2` onto that drive.
6. The LED blinks faster while the file transfers, then stays lit for about a
   second while the firmware is written. The drive disappears and the
   instrument restarts on the new firmware.

The drive is not real storage — it exists only to receive the firmware file,
so do not expect files copied there to stay. The update tool also works while
the drive is shown.

## If something goes wrong

**The browser update stopped: tab closed, cable pulled, power lost.** Nothing
is broken. The first thing an update does is erase the start of the old
firmware, and the last thing it does, once the whole new firmware has been
checked, is write the few bytes that make it bootable. In between, the
instrument always starts in update mode. Open the update tool again and click
**Resume update**.

**The copy onto the drive was interrupted.** Nothing is broken either. The new
firmware is only written after the whole file has arrived, so an interrupted
copy is simply discarded. Start again from step 1.

**The instrument does not start, and shows the BANDOLIBRE drive on its own
without FN2 held.** This is the recovery path working as intended: the
bootloader checks the firmware before starting it, and stays in update mode if
it is missing or damaged. Use the update tool, or copy the `.uf2` again.

**The update tool says the instrument cannot be updated from the browser, or
a drive appears instead of the update mode.** The instrument's firmware or
bootloader predates the browser update. Update once with the drive; a
bootloader that predates it needs the ST-Link session below.

**Nothing happens when you copy the file.** The file has to be a `.uf2` built
for this board. A `.bin`, a `.hex`, or a `.uf2` for another project is ignored —
that is deliberate, so an accidental drag cannot damage the instrument.

**The drive does not appear at all.** Make sure FN2 is held *before* the cable
goes in and stays held for a second or two after. If the instrument is running
normally, you can also reach the drive from the debug console (below).

## Rebooting into the drive without unplugging

If you have the console open (see
[programming_boards.md](programming_boards.md)), the `dfu` command does the
same thing:

```
> dfu
Rebooting into the bootloader; the BANDOLIBRE drive will appear.
```

## For developers

### Producing the .uf2

```sh
cd code/main-g474
just dfu            # -> build/Release/main-g474.uf2
```

The update tool offers the copy in `docs/update/firmware/` (`latest.json` and
the `.uf2`), which the release process publishes with each release. Debug
builds link above the bootloader too, so `build/Debug/main-g474.elf` converted
with `../tool/uf2.py` can be installed the same way.

A board that has never had the bootloader installed, or has the older
drive-only one, needs one ST-Link session first:

```sh
cd code/boot-g474
just flash          # the bootloader only, over SWD; the application is untouched
```

After that it can be updated over USB for good, and `just flash` in
`code/main-g474` installs the Debug image above the bootloader without
touching it.

### How the browser update works

The bootloader shows up as a USB-MIDI device and speaks the same SysEx framing
as the application (`code/common/sysex/`). Its ids are in `sysex_ids.h`:

- **HELLO** (0x00): both modes answer, with a `mode` byte that tells the
  instrument from the update mode. Port names are only a hint.
- **ENTER_UPDATE_MODE** (0x20, application): reboots into the bootloader, MIDI
  only (no drive).
- **UPDATE_INFO** (0x21): page size, application area, largest chunk, whether
  the installed application is valid or an update was interrupted, its version
  (read from `.app_info` at `APP_BASE + 0x200`), any open session, and whether
  the BANDOLIBRE drive is exposed too.
- **UPDATE_BEGIN** (0x22): image size and CRC-32; erases page 0 first and
  returns a session number.
- **UPDATE_WRITE** (0x23), **UPDATE_CRC** (0x24), **UPDATE_ERASE** (0x25):
  write a chunk (idempotent, so lost replies are simply retried), read back the
  CRC-32 of a range, erase a page to rewrite it.
- **UPDATE_COMMIT** (0x26): checks the CRC-32 of the whole image, then writes
  its first double word (initial SP and reset PC), which was held in RAM until
  now, and reboots.
- **UPDATE_LEAVE** (0x27): reboots into the installed application without
  updating, only while it is valid (nothing erased yet). The tool sends it when
  it gives up before BEGIN, and for its **Return to playing** button.

Each request carries a sequence number the reply echoes, and every write
carries the session, so another page on the same port can neither take a reply
for its own nor interleave its pages: a second `UPDATE_BEGIN` makes the first
client fail with "bad session". The configuration tool and BandoPM read HELLO's
mode, stop talking, and wait.

The full picture, including the flash layout and why the drive uses UF2, is in
[code/boot-g474/README.md](https://github.com/bandolibre/bandolibre/blob/main/code/boot-g474/README.md).
