# Bootloader (main board)

Lets the main board be reflashed over the USB cable already carrying MIDI, with
no ST-Link and no toolchain, two ways:

- **From the browser.** The update tool (`docs/update/`) asks the application
  to restart into update mode, where this bootloader shows up as the MIDI
  device "Bandolibre firmware update" and takes the firmware page by page over
  SysEx ([`midi_update.c`](midi_update.c)).
- **By copying a file.** Hold **FN2** (the leftmost function button) while
  powering the instrument on, and a drive called `BANDOLIBRE` appears next to
  the MIDI device. Drop `main-g474.uf2` on it; when the copy finishes the drive
  disappears and the instrument restarts on the new firmware.

End-user instructions are in
[docs/guide/firmware_update.md](../../docs/guide/firmware_update.md), which
also summarises the SysEx protocol.

## Flash layout

The STM32G474CBT6 has 128 KB of flash, shared:

| Address | Size | Contents |
|---|---|---|
| `0x08000000` | 32 KB | this bootloader (currently ~28 KB used) |
| `0x08008000` | 92 KB | application, Debug or Release build (~85 / ~78 KB used) |
| `0x0801F000` | 2 KB | saved properties, partition A |
| `0x0801F800` | 2 KB | saved properties, partition B |

The bootloader refuses images larger than the application region, so a
firmware update never erases the saved properties.

[`memmap.h`](memmap.h) is the single source of truth for these numbers; four
other files repeat them and [`code/tests/test_dfu_config.py`](../tests/test_dfu_config.py)
fails if any of them drifts.

Both builds of the application are `-Os` and link above the bootloader with
`STM32G474XX_FLASH_APP.ld`, so `just flash` in `../main-g474` (Debug) leaves
the bootloader in place, and a Debug board updates from the browser like any
other. The linker script also places the application's version record
(`app_info_t`, `.app_info`) at `APP_BASE + 0x200`, right after the vector
table, so update mode can report what is installed.

## Boot sequence

1. Reset lands here, always.
2. Clocks come up exactly as in the application's `SystemClock_Config()`:
   96 MHz core from the HSI16 PLL, USB on HSI48 trimmed by the CRS against USB
   start-of-frame. The board is crystal-less, so the CRS is not optional.
3. `HAL_MspInit()` clears the UCPD dead-battery pull-down — see the warning
   below.
4. Update mode is entered if **any** of:
   - the word at `BOOT_FLAG_ADDR` holds `BOOT_FLAG_MAGIC_MIDI`, written by the
     application when the update tool sends `ENTER_UPDATE_MODE`: MIDI only;
   - it holds `BOOT_FLAG_MAGIC`, written by the application's `dfu` console
     command: MIDI and the drive;
   - PB4 (`SW_FN2`) reads low, i.e. the button is held: MIDI and the drive;
   - the application's vector table fails a sanity check, so a board with
     missing or half-written firmware recovers on its own: MIDI and the drive.
5. Otherwise it de-initialises everything, points `SCB->VTOR` at `APP_BASE`,
   loads the application's stack pointer and jumps to its reset handler.

The flag is cleared as soon as it is read, so a reset or power cycle before an
update starts returns to the application.

## Update over MIDI

[`midi_update.c`](midi_update.c) answers HELLO (with `mode` = update) and the
update commands of [`../common/sysex/sysex_ids.h`](../common/sysex/sysex_ids.h);
the framing is shared with the application through
[`../common/sysex/sysex_codec.h`](../common/sysex/sysex_codec.h). Two
orderings make it safe to interrupt at any point:

- `UPDATE_BEGIN` erases page 0 before anything else, so from then on the
  vector table check fails and the board boots back into update mode;
- the image's first double word (initial SP and reset PC) is held in RAM and
  written only by `UPDATE_COMMIT`, after the CRC-32 of the whole image in
  flash matches the one given to `UPDATE_BEGIN`.

Writes are idempotent, so the tool simply retries any request whose reply it
lost. Every request carries a sequence number echoed by its reply and every
write the session number from `UPDATE_BEGIN`, so the configuration tool or a
second update tab on the same port cannot take a reply meant for another
client, nor interleave pages. Anything else (the configuration tool's and
BandoPM's requests) is ignored without a reply.

A double ECC error on a flash read (a double word torn by a power cut while it
was programmed) raises an NMI; the bootloader's `NMI_Handler` clears it and
`flash_read()` reports it, so a torn vector table reads as invalid instead of
hanging the boot.

## Why UF2 rather than copying a `.bin`

A UF2 file is a flat sequence of 512-byte blocks, each carrying the address it
belongs at, a family id identifying the chip, and its position in the file.
[`msc_disk.c`](msc_disk.c) inspects every sector the host writes to the volume,
keeps the ones that are UF2 blocks for an STM32G4 inside the application
region, and throws away everything else — directory entries, FAT updates, and
the `.Spotlight-V100` and `System Volume Information` debris that macOS and
Windows leave on any removable drive.

That means no filesystem code at all: no directory to parse, no cluster chain
to follow, and no assumption about the order in which a host flushes its
writes. Recognising a plain `.bin` would need all three, and each is a place
for a host-specific quirk to corrupt an image. It is also why every
drag-and-drop bootloader (Adafruit, RP2040, micro:bit) uses UF2.

Blocks are staged in RAM and only written to flash once all of them have
arrived, so an interrupted or truncated copy never reaches the chip. The
bootloader has the RAM to spare: the application uses 8 KB of the 128 KB.
`flash_write_app()` then programs the page holding the vector table **last**,
and its first double word last of all, so a power cut during the ~1 s write
leaves an image that fails the validity check and the board comes back up in
update mode rather than jumping into something half-written.

## ⚠ PB4 needs the UCPD dead-battery pull-down cleared

`SW_FN2` sits on PB4, which is also **UCPD1_CC2**. Out of reset that pin can
carry a 5.1 kΩ pull-down from the UCPD peripheral, gated by the level on PA10
(STM32G474CB datasheet, pin table note 6). PA10 is this board's `USART1_RX`,
which the attached ST-Link VCP holds high while idle — so the pull-down is
active, it beats the ~40 kΩ internal pull-up, and the button reads as pressed
forever. `HAL_PWREx_DisableUCPDDeadBattery()` in `HAL_MspInit()` removes it,
and `test_dfu_config.py` checks the call is still there.

PB4 is also `NJTRST` and comes out of reset in its JTAG alternate function
(note 5), so it has to be claimed as a plain input. The board is debugged over
SWD, which does not use that pin.

FN0 was **not** used for this: it sits on PB8/BOOT0 and drives BOOT0 *high*
when pressed, which would collide with ST's own boot-mode selection if the
`nBOOT_SEL` option byte were ever cleared.

## USB identity

| | Application | Bootloader, web update | Bootloader, otherwise |
|---|---|---|---|
| VID:PID | `0483:a5b4` | `0483:a5b4` | `0483:a5b4` |
| bcdDevice | `0x0100` | `0x0201` | `0x0202` |
| Interfaces | MIDI | MIDI | MIDI + mass storage |
| Manufacturer = product | Bandolibre | Bandolibre firmware update | Bandolibre firmware update |
| Serial | 96-bit chip UID, 24 hex digits | same | same |

The VID/PID is ST's vendor id with a product id sublicensed to this project
(see [`../main-g474/usb/README.md`](../main-g474/usb/README.md)). One product id
covers the whole product, so application and bootloader share it and differ by
`bcdDevice` instead: hosts cache a driver set per VID/PID/revision, and the
three expose different interfaces.

Manufacturer and product are the same string because the MIDI port name comes
from the product on Linux and macOS and from the manufacturer on Windows. With
"firmware update" in both, the configuration tool and BandoPM, which only
auto-connect to a port matching `/bandolibre/i` and not `/firmware update/i`,
leave the bootloader to the update tool on every OS. The drive is shown only
when the bootloader may be a recovery (FN2, console `dfu`, no valid
application): a web update gets MIDI alone, so no drive pops up and macOS has
no unejected disk to complain about at the restart.

## Build

Not a CubeMX project — there is no `.ioc` and no generated `Core/`. It
configures its handful of peripherals directly, so there is no second generated
project to keep in sync with the hardware. It does reuse the main board's
vendored HAL and startup file rather than carrying a second copy, the same way
both boards reuse `../common`.

```sh
just build          # Debug (also -Os and LTO; the bootloader must fit either way)
just build_release  # what ships
just size           # how much of the 32 KB region is used
just flash          # install at 0x08000000, leaving the application intact
just uf2            # build/Release/boot-g474.uf2, for the update tool in STM32 DFU mode
```

`just flash` goes through `../tool/flash.py` with the board type `main-g474`,
so the chip-UID registry still refuses to write main board firmware to a wing.

Without an ST-Link, the bootloader installs from the update tool
(`docs/update/`) with the board in the chip's own DFU mode (FN0 held at power-on,
left keyboard unplugged). The release file from `just dfu` in `../main-g474`
carries it next to the firmware, and the tool rewrites it only when it differs;
`boot-g474.uf2` holds it alone and switches the page to **Install bootloader**.
The bootloader's own drive ignores the bootloader blocks of either file, since
they lie outside the application region.

## Testing

```sh
lsusb -v -d 0483:a5b4 | grep bcdDevice   # 0201 web update, 0202 with the drive
amidi -l                        # "Bandolibre firmware update"
cat /media/*/BANDOLIBRE/INFO_UF2.TXT
```

The bootloader prints to the same console as the application (USART1 on PA9,
921600 baud, TX only), which is the fastest way to see why an update did not
take:

```
boot-g474: web=0 console=0 button=1 app_valid=1
boot-g474: entering update mode with the drive
uf2: all 270 blocks received (image_len=69116)
flash: writing 69116 bytes, page=2048 bytes, 34 pages, DBANK=1, FLASHSIZE=128 KB
```

Every page is read back after erasing and the commit is abandoned if one did
not clear, so a failure names the address rather than silently writing a
corrupt image.

Things worth exercising after a change, all of which must leave a working
board: unplug mid-copy; copy an unrelated file; copy a `.uf2` built for
`0x08000000`; delete a file from the volume; close the update tool mid-write
and power-cycle (the board must come back in update mode, and Resume update
must finish); install a `.uf2` with one corrupted byte through the tool (the
commit must be refused); select the bootloader by hand in the configuration
tool during an update (the update must still finish).
