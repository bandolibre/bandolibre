#ifndef COMMON_SYSEX_CODEC_H
#define COMMON_SYSEX_CODEC_H

/* SysEx framing shared by the application (main-g474/usb/usb_app.cc) and the
 * bootloader (boot-g474/midi_update.c), so the instrument and its firmware
 * update mode speak the same frames to the web tools.
 *
 * A frame is F0 <enc7(cs_lo cs_hi id body...)> F7, with no manufacturer id.
 * cs is the XOR of the 16-bit little-endian words of id+body (an odd trailing
 * byte folded in alone).
 *
 * The body carries arbitrary bytes, but ALSA/Web MIDI read any byte with bit 7
 * set as a new status byte and corrupt the frame (escaping only 0xF0/0xF7 was
 * tried and failed). So, as in standard MIDI sysex, each run of up to 7 bytes
 * becomes 8: a leading byte whose bit j is input byte j's bit 7, then the bytes
 * with bit 7 cleared.
 *
 * The web tools (docs/connect/index.html, docs/update/index.html,
 * bandopm/docs/app.js) implement the other end; change them together.
 *
 * HAL-free, host-tested by sysex_codec_test.c.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSEX_START 0xF0u
#define SYSEX_END   0xF7u

/* Encoded size of `n` decoded bytes: 8 per full group of 7, and a last
 * partial group of r bytes takes r + 1. */
#define SYSEX_ENCODED_SIZE(n) \
  (((n) / 7u) * 8u + (((n) % 7u) ? ((n) % 7u) + 1u : 0u))

/* Size of a whole frame (markers included) carrying `payload_len` bytes of
 * id+body. */
#define SYSEX_FRAME_SIZE(payload_len) (SYSEX_ENCODED_SIZE((payload_len) + 2u) + 2u)

uint16_t sysex_checksum(const uint8_t *data, size_t len);

/* Return the bytes written, or SIZE_MAX if out_cap is too small. */
size_t sysex_encode7(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap);
size_t sysex_decode7(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap);

/* Payload (id + body) -> whole frame, F0..F7 included. Returns the frame
 * length, or 0 if out_cap is too small. */
size_t sysex_pack(const uint8_t *payload, size_t len, uint8_t *out, size_t out_cap);

/* Whole frame -> checksum-verified payload (id + body), decoded into
 * `decoded`. On success returns NULL and sets *payload / *payload_len (inside
 * `decoded`); otherwise returns a reason to log. */
const char *sysex_unpack(const uint8_t *frame, size_t frame_len,
                         uint8_t *decoded, size_t decoded_cap,
                         const uint8_t **payload, size_t *payload_len);

/* Number of MIDI bytes a USB-MIDI event packet carries when it is part of a
 * sysex (from the Code Index Number in its first byte), or 0 otherwise. 0x00
 * is valid sysex data, so the length can't come from the bytes. */
uint8_t sysex_usb_packet_len(uint8_t header);

/* The reverse, for sending: the next USB-MIDI event packet (cable 0) of
 * `msg`, from *pos, which it advances. `msg` is one whole message: a sysex
 * frame F0..F7, a channel message (note, CC...), or a single real-time byte
 * (0xF8..0xFF). Returns false once the message is used up. A sysex frame
 * becomes CIN 0x4 packets of 3 bytes and a last one of CIN 0x5/0x6/0x7 that
 * holds its end, so the frame never shares a packet with another message. */
bool midi_usb_packet(const uint8_t *msg, size_t len, size_t *pos, uint8_t packet[4]);

/* Reassembles frames from a byte stream, into a caller-owned buffer. */
typedef struct {
  uint8_t *buf;
  size_t cap;
  size_t len;   /* 0 = outside a frame; cap = overflowed, dropping bytes */
} sysex_rx_t;

typedef enum {
  SYSEX_RX_NONE,      /* nothing complete yet */
  SYSEX_RX_FRAME,     /* buf[0..*frame_len) holds a whole frame, F0..F7 */
  SYSEX_RX_OVERFLOW,  /* a frame longer than the buffer was dropped */
} sysex_rx_result_t;

/* Feed one byte. A new F0 restarts the frame, so a frame cut short by another
 * one is dropped rather than spliced. */
sysex_rx_result_t sysex_rx_byte(sysex_rx_t *rx, uint8_t byte, size_t *frame_len);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_SYSEX_CODEC_H */
