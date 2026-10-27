#ifndef COMMON_MIDI_TX_H
#define COMMON_MIDI_TX_H

/* Sending MIDI on cable 0 of the TinyUSB MIDI interface, for the application
 * and the bootloader alike, in place of tud_midi_stream_write().
 *
 * tud_midi_stream_write() packs bytes into USB-MIDI packets and stops at the
 * first packet that doesn't fit the TX FIFO: the rest of a sysex frame is
 * lost, the host never sees its F7, and the packet it had half built stays in
 * TinyUSB's stream state, so the next message, even a note, comes out
 * garbled. Here every message is packed by midi_usb_packet() (sysex_codec.h)
 * and written one whole packet at a time with tud_midi_packet_write(), which
 * either queues a packet or leaves everything untouched. When the FIFO is
 * full, it runs tud_task() until the host has drained room, so a long frame
 * goes out whole.
 *
 * Two limits keep the main loop alive:
 *   - each wait is capped at MIDI_TX_WAIT_MS. A frame cut by it is closed
 *     with an F7 before the next message, so the host discards it whole
 *     (bad checksum) instead of splicing it into what follows;
 *   - once a wait has timed out, the host is taken as not reading (Windows
 *     may not poll a port no application opened): later messages are
 *     dropped at once, without waiting, until a packet fits again.
 *
 * Call only from the main loop, never from an interrupt: the wait runs
 * tud_task(). That is safe even from inside a sysex handler, since tud_task()
 * only queues incoming packets and never dispatches them. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_TX_WAIT_MS 10u

/* Sends one whole message: a sysex frame F0..F7, a channel message, or a
 * single real-time byte. Returns false if it was dropped, whole or (a frame
 * cut by the wait limit) in part. */
bool midi_tx_send(const uint8_t *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_MIDI_TX_H */
