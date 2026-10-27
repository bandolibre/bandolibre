#ifndef BOOT_USB_DESCRIPTORS_H
#define BOOT_USB_DESCRIPTORS_H

#include <stdbool.h>

/* Chooses the USB configuration before USB starts: MIDI only, or MIDI plus
 * the BANDOLIBRE drive. See usb_descriptors.c. */
void usb_descriptors_set_drive(bool drive);

/* Whether the configuration chosen above includes the drive. */
bool usb_descriptors_has_drive(void);

#endif /* BOOT_USB_DESCRIPTORS_H */
