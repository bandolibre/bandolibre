/* Host unit tests for sysex_codec and crc32. Pure C, no HAL — built by
 * main-g474's `just test`. Same framework as fixed_format_test.c. */

#include "crc32.h"
#include "sysex_codec.h"

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { g_failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

/* The standard check value of CRC-32, and the same over pieces. */
static void test_crc32(void)
{
  const uint8_t *s = (const uint8_t *)"123456789";
  CHECK(crc32_update(0, s, 9) == 0xCBF43926u);
  CHECK(crc32_update(crc32_update(0, s, 4), s + 4, 5) == 0xCBF43926u);
  CHECK(crc32_update(0, s, 0) == 0);
}

/* A frame the web tools build for HELLO: F0, enc7([cs_lo, cs_hi, 0x00]), F7. */
static void test_pack_hello(void)
{
  uint8_t const payload[] = { 0x00 };
  uint8_t frame[16];
  size_t const n = sysex_pack(payload, sizeof(payload), frame, sizeof(frame));
  uint8_t const expected[] = { 0xF0, 0x00, 0x00, 0x00, 0x00, 0xF7 };
  CHECK(n == sizeof(expected));
  CHECK(memcmp(frame, expected, sizeof(expected)) == 0);
}

/* Every byte value survives, nothing between the markers has bit 7 set, and
 * the length is the documented one. */
static void test_round_trip(void)
{
  uint8_t payload[300];
  for (size_t i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)(i * 37 + 0x80);

  for (size_t len = 1; len <= sizeof(payload); len += 13) {
    uint8_t frame[SYSEX_FRAME_SIZE(300)];
    size_t const n = sysex_pack(payload, len, frame, sizeof(frame));
    CHECK(n == SYSEX_FRAME_SIZE(len));
    for (size_t i = 1; i + 1 < n; i++) CHECK((frame[i] & 0x80) == 0);

    uint8_t decoded[320];
    const uint8_t *out = NULL;
    size_t out_len = 0;
    CHECK(sysex_unpack(frame, n, decoded, sizeof(decoded), &out, &out_len) == NULL);
    CHECK(out_len == len);
    CHECK(out && memcmp(out, payload, len) == 0);
  }
}

static void test_unpack_rejects(void)
{
  uint8_t const payload[] = { 0x23, 0x01, 0xFF, 0x10 };
  uint8_t frame[32];
  size_t const n = sysex_pack(payload, sizeof(payload), frame, sizeof(frame));
  uint8_t decoded[32];
  const uint8_t *out;
  size_t out_len;

  frame[3] ^= 0x01;   /* one flipped bit */
  CHECK(sysex_unpack(frame, n, decoded, sizeof(decoded), &out, &out_len) != NULL);
  frame[3] ^= 0x01;
  CHECK(sysex_unpack(frame, n - 1, decoded, sizeof(decoded), &out, &out_len) != NULL);
  CHECK(sysex_unpack(frame, n, decoded, 2, &out, &out_len) != NULL);   /* too big */
  CHECK(sysex_pack(payload, sizeof(payload), frame, 4) == 0);
}

static void feed(sysex_rx_t *rx, const uint8_t *bytes, size_t len,
                 int *frames, int *overflows, size_t *last_len)
{
  for (size_t i = 0; i < len; i++) {
    size_t n;
    sysex_rx_result_t const r = sysex_rx_byte(rx, bytes[i], &n);
    if (r == SYSEX_RX_FRAME) { (*frames)++; *last_len = n; }
    if (r == SYSEX_RX_OVERFLOW) (*overflows)++;
  }
}

/* Reassembly: bytes outside a frame are ignored, a new F0 restarts, and an
 * overlong frame is reported once and dropped. */
static void test_reassembly(void)
{
  uint8_t buf[8];
  sysex_rx_t rx = { buf, sizeof(buf), 0 };
  int frames = 0, overflows = 0;
  size_t last = 0;

  uint8_t const a[] = { 0x01, 0xF0, 0x10, 0x11, 0xF7 };
  feed(&rx, a, sizeof(a), &frames, &overflows, &last);
  CHECK(frames == 1 && overflows == 0 && last == 4);
  CHECK(buf[0] == 0xF0 && buf[3] == 0xF7);

  uint8_t const cut[] = { 0xF0, 0x10, 0xF0, 0x20, 0x21, 0xF7 };
  feed(&rx, cut, sizeof(cut), &frames, &overflows, &last);
  CHECK(frames == 2 && overflows == 0 && last == 4 && buf[1] == 0x20);

  uint8_t const big[] = { 0xF0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xF7 };
  feed(&rx, big, sizeof(big), &frames, &overflows, &last);
  CHECK(frames == 2 && overflows == 1);

  uint8_t const fits[] = { 0xF0, 1, 2, 3, 4, 5, 6, 0xF7 };   /* exactly 8 */
  feed(&rx, fits, sizeof(fits), &frames, &overflows, &last);
  CHECK(frames == 3 && last == 8);
}

static void test_usb_packet_len(void)
{
  CHECK(sysex_usb_packet_len(0x04) == 3);
  CHECK(sysex_usb_packet_len(0x07) == 3);
  CHECK(sysex_usb_packet_len(0x06) == 2);
  CHECK(sysex_usb_packet_len(0x05) == 1);
  CHECK(sysex_usb_packet_len(0x09) == 0);   /* note on */
}

/* Packs `msg` and checks the packets against `expected` (n_packets x 4). */
static void check_packets(int line, const uint8_t *msg, size_t len, const uint8_t *expected, size_t n_packets)
{
  size_t pos = 0, count = 0;
  uint8_t packet[4];
  while (midi_usb_packet(msg, len, &pos, packet)) {
    g_checks++;
    if (count >= n_packets || memcmp(packet, expected + 4 * count, 4) != 0) {
      g_failures++;
      printf("FAIL %s:%d: packet %zu = %02X %02X %02X %02X\n", __FILE__, line, count,
             packet[0], packet[1], packet[2], packet[3]);
    }
    count++;
  }
  g_checks++;
  if (count != n_packets) {
    g_failures++;
    printf("FAIL %s:%d: %zu packets, expected %zu\n", __FILE__, line, count, n_packets);
  }
}
#define CHECK_PACKETS(msg, expected) \
  check_packets(__LINE__, (msg), sizeof(msg), (expected), sizeof(expected) / 4)

/* USB-MIDI 1.0 table 4-1: the CIN of each packet, and a sysex frame's end in
 * a packet of its own kind for every length. Round trip through
 * sysex_usb_packet_len, as the receiving side reads them. */
static void test_usb_packets(void)
{
  uint8_t const note[] = { 0x91, 60, 100 };
  uint8_t const note_p[] = { 0x09, 0x91, 60, 100 };
  CHECK_PACKETS(note, note_p);

  uint8_t const program[] = { 0xC0, 5 };
  uint8_t const program_p[] = { 0x0C, 0xC0, 5, 0 };
  CHECK_PACKETS(program, program_p);

  uint8_t const sensing[] = { 0xFE };
  uint8_t const sensing_p[] = { 0x0F, 0xFE, 0, 0 };
  CHECK_PACKETS(sensing, sensing_p);

  uint8_t const sx4[] = { 0xF0, 1, 2, 0xF7 };   /* end with 1 byte */
  uint8_t const sx4_p[] = { 0x04, 0xF0, 1, 2, 0x05, 0xF7, 0, 0 };
  CHECK_PACKETS(sx4, sx4_p);

  uint8_t const sx5[] = { 0xF0, 1, 2, 3, 0xF7 };   /* end with 2 bytes */
  uint8_t const sx5_p[] = { 0x04, 0xF0, 1, 2, 0x06, 3, 0xF7, 0 };
  CHECK_PACKETS(sx5, sx5_p);

  uint8_t const sx6[] = { 0xF0, 1, 2, 3, 4, 0xF7 };   /* end with 3 bytes */
  uint8_t const sx6_p[] = { 0x04, 0xF0, 1, 2, 0x07, 3, 4, 0xF7 };
  CHECK_PACKETS(sx6, sx6_p);

  uint8_t const sx3[] = { 0xF0, 1, 0xF7 };   /* a whole frame in one packet */
  uint8_t const sx3_p[] = { 0x07, 0xF0, 1, 0xF7 };
  CHECK_PACKETS(sx3, sx3_p);

  /* A real frame survives packing, then reassembly from the packets. */
  uint8_t const payload[] = { 0x21, 0x07, 0x00, 0x08, 0xFF, 0x80 };
  uint8_t frame[32];
  size_t const n = sysex_pack(payload, sizeof(payload), frame, sizeof(frame));
  uint8_t buf[32], decoded[32];
  sysex_rx_t rx = { buf, sizeof(buf), 0 };
  size_t pos = 0, frame_len = 0;
  int frames = 0;
  uint8_t packet[4];
  while (midi_usb_packet(frame, n, &pos, packet)) {
    for (uint8_t i = 1; i <= sysex_usb_packet_len(packet[0]); i++)
      if (sysex_rx_byte(&rx, packet[i], &frame_len) == SYSEX_RX_FRAME) frames++;
  }
  const uint8_t *out;
  size_t out_len;
  CHECK(frames == 1 && frame_len == n);
  CHECK(sysex_unpack(buf, frame_len, decoded, sizeof(decoded), &out, &out_len) == NULL);
  CHECK(out_len == sizeof(payload) && memcmp(out, payload, out_len) == 0);
}

int main(void)
{
  test_usb_packets();
  test_crc32();
  test_pack_hello();
  test_round_trip();
  test_unpack_rejects();
  test_reassembly();
  test_usb_packet_len();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures != 0;
}
