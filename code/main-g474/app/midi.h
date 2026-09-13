#ifndef MIDI_H_
#define MIDI_H_

/* MIDI helpers for the main board: a periodic Active Sensing task and a
 * console / microrl command layer for sending messages by hand. Kept separate
 * from usb_app.c (which stays printf-free) so these helpers can print
 * usage/errors via printf (\r\n line endings) and parse argv tokens. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call from the main loop on every iteration. When midi_active_sensing_enable
 * is set, sends an Active Sensing byte (0xFE) every midi_active_sensing_period
 * milliseconds so a receiver can detect a dropped link. */
void midi_poll(void);

/* argv is the usual token array with argv[0] = the command word. If argv[0] is
 * one of the MIDI commands below it is executed (sending one message on cable 0)
 * and true is returned; otherwise false is returned so the caller can keep
 * dispatching. A recognised-but-malformed command prints usage and still
 * returns true. Channel is 1-16; note/controller/value/velocity 0-127.
 *
 *   send_note_on  <channel> <note> [velocity]   (velocity defaults to 64)
 *   send_note_off <channel> <note>
 *   send_cc       <channel> <controller> <value>
 */
bool midi_console_execute(int argc, const char *const *argv);

/* Lists the MIDI command usage. */
void midi_console_help(void);

/* Fill out[] with up to cap MIDI command names starting with prefix (prefix may
 * be NULL/"" to match all); returns the count. Backs a completion callback. */
size_t midi_console_complete(const char *prefix, const char **out, size_t cap);

/* Handles incoming System Exclusive (sysex) MIDI messages. Called from usb_app.c
 * when a complete sysex message (0xF0...0xF7) is received. */
void midi_sysex_received(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_H_ */
