/*
 * TinyUSB application glue for the Bandolibre main board.
 * Composite device: MIDI + CDC mirror of the microrl console.
 */

#ifndef USB_APP_H_
#define USB_APP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the TinyUSB device stack. Call after MX_USB_PCD_Init() so the USB
 * peripheral clock (48 MHz from PLLQ) is already configured. */
void usb_app_init(void);

/* True once the host has enumerated and configured the device, i.e. whether a
 * host is actually attached and has mounted us. */
bool usb_app_mounted(void);

/* Runs the TinyUSB device task and the CDC console bridge.
 * Call from the main loop on every iteration. */
void usb_app_task(void);

/* Sends a short note on/off pair on MIDI channel 0, for end-to-end testing. */
void usb_app_midi_test_note(uint8_t note);

/* Sends a Note On / Note Off on the given 0-based MIDI channel (cable 0). */
void usb_app_midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void usb_app_midi_note_off(uint8_t channel, uint8_t note);

/* Sends a Control Change on the given 0-based MIDI channel (cable 0). */
void usb_app_midi_control_change(uint8_t channel, uint8_t controller, uint8_t value);

/* Sends a single Active Sensing real-time byte (0xFE) on cable 0. */
void usb_app_midi_active_sensing(void);

/* Sends a System Exclusive (sysex) message on cable 0: data is the message
 * identifier and body (matching what midi_sysex_received expects on the way
 * in), wrapped with a leading 0xF0 + 2-byte checksum and a trailing 0xF7. */
void usb_app_midi_send_sysex(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* USB_APP_H_ */
