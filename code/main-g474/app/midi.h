#ifndef MIDI_H_
#define MIDI_H_

/* MIDI helpers for the main board: a periodic Active Sensing task and a
 * console / microrl command layer for sending messages by hand. Kept separate
 * from usb_app.cc's USB/TinyUSB glue so these helpers can print usage/errors
 * via printf (\r\n line endings) and parse argv tokens. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* The console and sysex-receive layer below is C++ only (gsl::span); the push
 * notifications at the end are C-callable, for the C modules that raise
 * them. */
#ifdef __cplusplus
#include <gsl/span>

/* Call from the main loop on every iteration. When midi_active_sensing_enable
 * is set, sends an Active Sensing byte (0xFE) every midi_active_sensing_period
 * milliseconds so a receiver can detect a dropped link. */
void midi_poll(void);

/* argv_span is the token array with argv[0] = the command word. If argv[0] is
 * one of the MIDI commands below it is executed (sending one message on cable 0)
 * and true is returned; otherwise false is returned so the caller can keep
 * dispatching. A recognised-but-malformed command prints usage and still
 * returns true. Channel is 1-16; note/controller/value/velocity 0-127.
 *
 *   send_note_on  <channel> <note> [velocity]   (velocity defaults to 64)
 *   send_note_off <channel> <note>
 *   send_cc       <channel> <controller> <value>
 */
bool midi_console_execute(gsl::span<const char* const> argv_span);

/* Lists the MIDI command usage. */
void midi_console_help(void);

/* Fill out[] with up to cap MIDI command names starting with prefix (prefix may
 * be NULL/"" to match all); returns the count. Backs a completion callback. */
size_t midi_console_complete(const char *prefix, const char **out, size_t cap);

/* Handles an incoming System Exclusive (sysex) message: data is the message
 * identifier and body. Called from usb_app.cc once a complete frame
 * (0xF0...0xF7) has been received and its checksum verified. */
void midi_sysex_received(gsl::span<const uint8_t> data);

extern "C" {
#endif /* __cplusplus */

/* The push notifications below are sent only while the configuration
 * tool holds the monitor lease (SYSEX_MSG_MONITOR_LEASE in midi.cc, renewed by
 * its live poll); otherwise they do nothing. */

/* Sends a Bellows Direction sysex push notification (SYSEX_MSG_BELLOWS_DIRECTION
 * in midi.cc): direction is bellows_t's raw encoding (0=BELLOWS_PULL,
 * 1=BELLOWS_PUSH, 2=BELLOWS_NEUTRAL). Called by keyboard.cpp exactly when the
 * effective bellows direction changes, before the resulting NOTE ON/OFF
 * events it explains - so a client can always resolve which push/pull note a
 * key is currently sounding without guessing from the note number alone.
 *
 * C linkage: keyboard.cpp calls this from inside its own file-wide
 * extern "C" block (its own functions need C linkage to be callable from
 * main.c), so this declaration and midi.cc's definition of it must agree. */
void midi_send_bellows_direction(uint8_t direction);

/* Sends a Property Changed sysex push notification (SYSEX_MSG_PROPERTY_CHANGED
 * in midi.cc) with the property's live value and saved state, for a change
 * the device made on its own and saved to flash (the bellows center
 * calibrated by a long press of FN1), so the configuration tool shows it
 * without polling. Changes the tool already polls (table_mode,
 * keyboard_tuning, bellow_program in MONITOR_LEASE) don't need it. */
void midi_send_property_changed(size_t index);

/* Sends a Key Strike sysex push notification (SYSEX_MSG_KEY_STRIKE in
 * midi.cc) right after each NOTE ON: the measured strike speed (ADC counts per
 * 10 ms; 0 when no clean transit was seen), the velocity actually sent, and
 * whether the note was deferred (the key was already down when the bellows
 * started moving, so the speed does not describe it). Sent whatever the
 * velocity_source, so the configuration tool can plot strikes on the response
 * curve while it is being tuned. */
void midi_send_key_strike(uint16_t speed, uint8_t velocity, bool deferred);

/* Sends a Key Release sysex push notification (SYSEX_MSG_KEY_RELEASE in
 * midi.cc) right after the NOTE OFF of a key: the measured release speed
 * (counts per 10 ms; 0 when no clean transit was seen), the velocity actually
 * sent, and whether the note was silenced by the bellows reaching neutral
 * rather than by the key (the speed then does not describe it). Not sent for
 * a keyboard link lost, which releases no key. */
void midi_send_key_release(uint16_t speed, uint8_t velocity, bool neutral);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_H_ */
