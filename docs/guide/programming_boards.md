---
---

## Connecting the debugger probe

Plug the ST-Link into a USB port and verify the device is detected:

```
$ lsusb | grep STLINK
Bus 001 Device 070: ID 0483:3754 STMicroelectronics STLINK-V3
```

Install [stlink-tools](https://github.com/stlink-org/stlink) from source, in a
directory next to your `bandolibre` checkout:

```
sudo apt remove stlink-tools
sudo apt install build-essential cmake libusb-1.0-0-dev
git clone --depth 1 --branch testing https://github.com/stlink-org/stlink
cd stlink
git apply ../bandolibre/docs/guide/0001-fix-st-trace-fix-SWO-trace-on-STLINK-V3-HS-bulk-endp.patch
make release && sudo make install && sudo ldconfig
```

Once the ST-Link is plugged into the board:

```
$ st-info --probe
Found 1 stlink programmers
  version:    V3J15
  serial:     002F00413235510637333439
  flash:      131072 (pagesize: 2048)
  sram:       131072
  chipid:     0x469
  dev-type:   STM32G47x_G48x
```

## Install build prerequisites

The ARM toolchain and the build backend:

```
sudo apt install gcc-arm-none-eabi ninja-build
```

I use [`just`](https://just.systems/) to run commands from a configuration file
such as `code/main-g474/justfile`:

```
curl --proto '=https' --tlsv1.2 -sSf https://just.systems/install.sh | bash -s -- --to DEST
```

or

```
cargo install just
```

## Checkout the source code

```
git clone --recurse-submodules https://github.com/bandolibre/bandolibre.git
```

## Build the firmware

Generate the build system once, then build:

```
cd code/main-g474
just init_build_release build_release
```

## Flash the firmware

Flashing wing firmware onto a main board — or the reverse — drives pins against
the connected hardware and can damage the boards, so a chip must be registered
before it can be flashed. `code/tool/boards.csv` maps each STM32 unique device
ID to its board type, and is checked before every flash.

Register a board once, before its first flash:

```
cd code/main-g474
just registry_add
```

Then flash with:

```
cd code/main-g474
just flash_release
```

`just flash_release` programs the board on the connected ST-Link, and refuses
if its chip is not registered as that board type. Connect one ST-Link at a
time.

The same recipe apply in `code/wing-g474`.

## Main board: bootloader and application

The main board carries a bootloader at `0x08000000` so it can be updated from
the browser or by copying a file onto a USB drive — see
[firmware_update.md](firmware_update.md) and
[code/boot-g474/README.md](https://github.com/bandolibre/bandolibre/blob/main/code/boot-g474/README.md).
Both builds of the application (Debug and Release, both `-Os`) link above it,
at `0x08008000`, in the 92 KB below the saved settings:

| Recipe | Builds | Writes at | Bootloader after |
|---|---|---|---|
| `just flash` | Debug, `-Os -g2` | `0x08008000` | untouched |
| `just flash_release` | bootloader + Release, `-Os -flto` | `0x08000000`, `0x08008000` | installed |
| `just dfu` | Release, packaged as `.uf2` | `0x08008000` | (no flashing; update over USB) |
| `cd ../boot-g474 && just flash` | the bootloader only | `0x08000000` | installed |

Install the bootloader once per board (`just flash_release`, or the
bootloader-only recipe); `just flash` then never touches it, and a Debug board
can still be updated from the browser.

`just dfu` is the one to use for a release: it builds the Release
configuration and writes `build/Release/main-g474.uf2`.

None of this affects `code/wing-g474`, which has a single memory map.


## Read UART debug console

Characters written with `printf` (via `_write` retargeted to `HAL_UART_Transmit`) are sent over USART to the STLink VCP bridge, which forwards them to a host `/dev/ttyACMx` device.

Use `tio` to read it — it auto-reconnects across resets, unlike `screen` or `cat`:

```bash
tio /dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*
```

The glob resolves to the correct `/dev/ttyACMx` regardless of what other USB serial devices are present. Or via the justfile recipe:

```bash
just console
```

Press `ctrl-t q` to quit.

