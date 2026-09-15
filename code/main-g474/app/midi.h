#ifndef MIDI_H_
#define MIDI_H_

/* MIDI helpers for the main board: a periodic Active Sensing task and a
 * console / microrl command layer for sending messages by hand. Kept separate
 * from usb_app.cc's USB/TinyUSB glue so these helpers can print usage/errors
 * via printf (\r\n line endings) and parse argv tokens. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
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

/* Sends a Bellows Direction sysex push notification (SYSEX_MSG_BELLOWS_DIRECTION
 * in midi.cc): direction is bellows_t's raw encoding (0=BELLOWS_PULL,
 * 1=BELLOWS_PUSH, 2=BELLOWS_NEUTRAL). Called by keyboard.cpp exactly when the
 * effective bellows direction changes, before the resulting NOTE ON/OFF
 * events it explains - so a client can always resolve which push/pull note a
 * key is currently sounding without guessing from the note number alone.
 *
 * extern "C": keyboard.cpp calls this from inside its own file-wide
 * extern "C" block (its own functions need C linkage to be callable from
 * main.c), so this declaration and midi.cc's definition of it must agree on
 * C linkage too, unlike the rest of this gsl::span-based, C++-only header. */
extern "C" void midi_send_bellows_direction(uint8_t direction);

#endif /* MIDI_H_ */
