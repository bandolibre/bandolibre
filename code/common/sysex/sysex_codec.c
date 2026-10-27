#include "sysex_codec.h"

uint16_t sysex_checksum(const uint8_t *data, size_t len)
{
  uint16_t cs = 0;
  size_t i = 0;
  for (; i + 1 < len; i += 2)
    cs ^= (uint16_t)(data[i] | (uint16_t)(data[i + 1] << 8));
  if (i < len) cs ^= data[i];
  return cs;
}

/* Encodes the concatenation of two segments, so a frame can be packed from a
 * checksum and a payload without copying them together first. */
static size_t encode7_2(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len,
                        uint8_t *out, size_t out_cap)
{
  size_t const len = a_len + b_len;
  size_t n = 0, i = 0;
  while (i < len) {
    size_t const group = (len - i < 7) ? (len - i) : 7;
    if (n + 1 + group > out_cap) return SIZE_MAX;
    uint8_t *msb = &out[n++];
    *msb = 0;
    for (size_t j = 0; j < group; j++) {
      size_t const k = i + j;
      uint8_t const byte = (k < a_len) ? a[k] : b[k - a_len];
      if (byte & 0x80) *msb |= (uint8_t)(1u << j);
      out[n++] = (uint8_t)(byte & 0x7F);
    }
    i += group;
  }
  return n;
}

size_t sysex_encode7(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap)
{
  return encode7_2(in, len, NULL, 0, out, out_cap);
}

size_t sysex_decode7(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap)
{
  size_t n = 0, i = 0;
  while (i < len) {
    uint8_t const msb = in[i++];
    size_t const group = (len - i < 7) ? (len - i) : 7;
    if (n + group > out_cap) return SIZE_MAX;
    for (size_t j = 0; j < group; j++) {
      uint8_t b = in[i + j];
      if (msb & (1u << j)) b |= 0x80;
      out[n++] = b;
    }
    i += group;
  }
  return n;
}

size_t sysex_pack(const uint8_t *payload, size_t len, uint8_t *out, size_t out_cap)
{
  if (out_cap < 2) return 0;
  uint16_t const cs = sysex_checksum(payload, len);
  uint8_t const head[2] = { (uint8_t)(cs & 0xFF), (uint8_t)(cs >> 8) };

  out[0] = SYSEX_START;
  size_t const n = encode7_2(head, sizeof(head), payload, len, out + 1, out_cap - 2);
  if (n == SIZE_MAX) return 0;
  out[1 + n] = SYSEX_END;
  return n + 2;
}

const char *sysex_unpack(const uint8_t *frame, size_t frame_len,
                         uint8_t *decoded, size_t decoded_cap,
                         const uint8_t **payload, size_t *payload_len)
{
  if (frame_len < 5) return "frame too short";
  if (frame[0] != SYSEX_START || frame[frame_len - 1] != SYSEX_END) return "missing 0xF0/0xF7 markers";

  size_t const n = sysex_decode7(frame + 1, frame_len - 2, decoded, decoded_cap);
  if (n == SIZE_MAX) return "malformed 7-bit encoding";
  if (n < 3) return "frame too short";

  uint16_t const received = (uint16_t)(decoded[0] | (uint16_t)(decoded[1] << 8));
  if (sysex_checksum(decoded + 2, n - 2) != received) return "checksum mismatch";

  *payload = decoded + 2;
  *payload_len = n - 2;
  return NULL;
}

uint8_t sysex_usb_packet_len(uint8_t header)
{
  switch (header & 0x0F) {
    case 0x4: case 0x7: return 3;  /* sysex starts/continues, or ends with 3 bytes */
    case 0x6:           return 2;  /* sysex ends with 2 bytes */
    case 0x5:           return 1;  /* sysex ends with 1 byte */
    default:            return 0;  /* not a sysex-carrying packet */
  }
}

bool midi_usb_packet(const uint8_t *msg, size_t len, size_t *pos, uint8_t packet[4])
{
  if (*pos >= len) return false;
  size_t const left = len - *pos;
  size_t const n = (left < 3) ? left : 3;

  if (msg[0] == SYSEX_START)
    packet[0] = (left > 3) ? 0x4 : (uint8_t)(0x4 + n);   /* ends with n = 1, 2, 3 bytes */
  else
    packet[0] = (msg[0] >= 0xF0) ? 0xF : (uint8_t)(msg[0] >> 4);   /* single byte, or channel message */

  for (size_t i = 0; i < 3; i++) packet[1 + i] = (i < n) ? msg[*pos + i] : 0;
  *pos += n;
  return true;
}

sysex_rx_result_t sysex_rx_byte(sysex_rx_t *rx, uint8_t byte, size_t *frame_len)
{
  if (byte == SYSEX_START) {
    sysex_rx_result_t const r = (rx->len >= rx->cap) ? SYSEX_RX_OVERFLOW : SYSEX_RX_NONE;
    rx->buf[0] = byte;
    rx->len = 1;
    return r;
  }
  if (rx->len == 0) return SYSEX_RX_NONE;

  if (byte == SYSEX_END) {
    size_t const n = rx->len;
    rx->len = 0;
    if (n >= rx->cap) return SYSEX_RX_OVERFLOW;
    rx->buf[n] = byte;
    *frame_len = n + 1;
    return SYSEX_RX_FRAME;
  }

  if (rx->len < rx->cap) rx->buf[rx->len++] = byte;
  return SYSEX_RX_NONE;
}
