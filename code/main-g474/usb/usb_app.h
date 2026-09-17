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

/* Sends a 14-bit Control Change pair on the given 0-based MIDI channel (cable
 * 0): the MSB (value14>>7) on cc_msb, immediately followed by the LSB
 * (value14&0x7F) on cc_msb+32 -- the standard MIDI convention for a coarse/fine
 * CC pair (e.g. CC#11 Expression paired with CC#43). value14 is masked to 14
 * bits. Implemented as two back-to-back usb_app_midi_control_change() calls:
 * every MIDI-sending call site in this firmware runs from the single
 * cooperative main-loop thread, so the two writes can't be torn apart by
 * another producer. */
void usb_app_midi_control_change_14bit(uint8_t channel, uint8_t cc_msb, uint16_t value14);

/* Sends a single Active Sensing real-time byte (0xFE) on cable 0. */
void usb_app_midi_active_sensing(void);

/* Sends a System Exclusive (sysex) message on cable 0: data is the message
 * identifier and body (matching what midi_sysex_received expects on the way
 * in), wrapped with a leading 0xF0 + 2-byte checksum and a trailing 0xF7.
 * The checksum and data bytes are repacked into 7-bit groups in between (see
 * sysex_encode7 in usb_app.cc) so every byte between the markers has its
 * high bit clear - required for the frame to survive transports that
 * reconstruct a standard MIDI byte stream (the Linux kernel's own USB-MIDI
 * driver feeding ALSA/Web MIDI does this and corrupts any 8-bit-clean byte
 * with bit 7 set, mistaking it for a new status byte). */
void usb_app_midi_send_sysex(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* USB_APP_H_ */
