#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

/* The application's side of the bootloader hand-off (boot-g474, memmap.h):
 * the version record the bootloader reads, and the reboot into update mode. */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reboots into the bootloader's update mode, after 50 ms of USB service so a
 * reply or a console line already queued gets out. With `drive`, the bootloader
 * also shows the BANDOLIBRE drive for a .uf2 copy (console `dfu`); without,
 * MIDI only (the web update tool). Does not return. */
void reboot_to_bootloader(bool drive);

#ifdef __cplusplus
}
#endif

#endif /* BOOTLOADER_H_ */
