#include "usb_app.h"

#include "tusb.h"
#include "stm32g4xx_hal.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <gsl/span>
#include "midi.h"
#include "properties.h"

constexpr uint8_t MIDI_SYSEX_START = 0xF0;
constexpr uint8_t MIDI_SYSEX_END = 0xF7;

/* Max decoded sysex body (checksum + payload); also caps the incoming raw
 * frame midi_input_process accumulates. */
constexpr size_t MAX_SYSEX = 256;

/* The body between 0xF0/0xF7 carries arbitrary 16-bit values, but ALSA/Web
 * MIDI read any byte with bit 7 set as a new status byte and corrupt the
 * frame (escaping only 0xF0/0xF7 was tried and failed). So, as in standard
 * MIDI sysex, each run of up to 7 bytes becomes 8: a leading byte whose
 * bit j is input byte j's bit 7, then the bytes with bit 7 cleared.
 * CFG_TUD_MIDI_TX_BUFSIZE is sized from MAX_ENCODED_SYSEX. */
constexpr size_t sysex7_encoded_size(size_t decoded_len) { return ((decoded_len + 6) / 7) * 8; }
constexpr size_t MAX_ENCODED_SYSEX = sysex7_encoded_size(MAX_SYSEX);

/* [seconds.millis] prefix on SysEx console lines (errors, and the
 * log_midi_sysex trace), to line them up against host-side captures. */
static void print_timestamp(void)
{
  uint32_t ms = HAL_GetTick();
  printf("[%6lu.%03lu] ", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
}

void usb_app_init(void)
{
  /* USB clocks (HSI48 + CRS) come from CubeMX init code, checked by
   * code/tests/test_usb_config.py. */

  /* Below the console UART, so a CDC burst can't starve reception (HAL tick
   * stays at 0). */
  NVIC_SetPriority(USB_HP_IRQn, 6);
  NVIC_SetPriority(USB_LP_IRQn, 6);
  NVIC_SetPriority(USBWakeUp_IRQn, 6);

  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_FULL,
  };
  tusb_init(0, &dev_init);
}

bool usb_app_mounted(void)
{
  return tud_mounted();
}

/* XOR of 16-bit little-endian words (odd trailing byte folded in), for both
 * directions. */
static uint16_t sysex_checksum(const uint8_t *data, size_t len)
{
  uint16_t cs = 0;
  size_t i = 0;
  for (; i + 1 < len; i += 2)
    cs ^= (uint16_t)(data[i] | (uint16_t)(data[i + 1] << 8));
  if (i < len) cs ^= data[i];
  return cs;
}

/* Returns bytes written, or SIZE_MAX if out_cap is too small. */
static size_t sysex_encode7(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap)
{
  size_t n = 0, i = 0;
  while (i < len) {
    size_t group = (len - i < 7) ? (len - i) : 7;
    if (n + 1 + group > out_cap) return SIZE_MAX;
    uint8_t msb = 0;
    for (size_t j = 0; j < group; j++)
      if (in[i + j] & 0x80) msb |= (uint8_t)(1u << j);
    out[n++] = msb;
    for (size_t j = 0; j < group; j++) out[n++] = (uint8_t)(in[i + j] & 0x7F);
    i += group;
  }
  return n;
}

/* Returns bytes written, or SIZE_MAX if out_cap is too small. */
static size_t sysex_decode7(const uint8_t *in, size_t len, uint8_t *out, size_t out_cap)
{
  size_t n = 0, i = 0;
  while (i < len) {
    uint8_t msb = in[i++];
    size_t group = (len - i < 7) ? (len - i) : 7;
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

/* Raw 0xF0..0xF7 frame -> checksum-verified payload (message id + body).
 * Returns nullptr on success, else a reason to log. */
static const char *unpack_sysex(gsl::span<const uint8_t> frame, gsl::span<const uint8_t> *payload)
{
  if (frame.size() < 5) return "frame too short";
  if (frame.front() != MIDI_SYSEX_START || frame.back() != MIDI_SYSEX_END) return "missing 0xF0/0xF7 markers";

  static std::array<uint8_t, MAX_SYSEX> decoded;
  gsl::span<const uint8_t> encoded_body = frame.subspan(1, frame.size() - 2);
  size_t decoded_len = sysex_decode7(encoded_body.data(), encoded_body.size(), decoded.data(), decoded.size());
  if (decoded_len == SIZE_MAX) return "malformed 7-bit encoding";
  if (decoded_len < 3) return "frame too short";

  uint16_t received_checksum = (uint16_t)(decoded[0] | (uint16_t)(decoded[1] << 8));
  gsl::span<const uint8_t> covered(decoded.data() + 2, decoded_len - 2);
  if (sysex_checksum(covered.data(), covered.size()) != received_checksum) return "checksum mismatch";

  *payload = covered;
  return nullptr;
}

static void midi_input_process(gsl::span<const uint8_t, 4> packet)
{
  static std::array<uint8_t, MAX_SYSEX> sysex_buf;
  static size_t sysex_len = 0;

  /* The Code Index Number (low nibble of byte 0) gives the payload length;
   * 0x00 is valid sysex data, so it can't mark the end. */
  uint8_t valid_bytes;
  switch (packet[0] & 0x0F) {
    case 0x4: case 0x7: valid_bytes = 3; break;  /* sysex starts/continues, or ends with 3 bytes */
    case 0x6:           valid_bytes = 2; break;  /* sysex ends with 2 bytes */
    case 0x5:           valid_bytes = 1; break;  /* sysex ends with 1 byte */
    default: return;                             /* not a sysex-carrying packet */
  }

  for (size_t i = 1; i <= valid_bytes; i++) {
    uint8_t byte = packet[i];

    if (byte == MIDI_SYSEX_START) {
      if (sysex_len >= MAX_SYSEX) {
        print_timestamp();
        printf("sysex: frame discarded (buffer overflow)\r\n");
      }
      sysex_len = 1;
      sysex_buf[0] = byte;
    } else if (byte == MIDI_SYSEX_END && sysex_len > 0) {
      if (sysex_len < MAX_SYSEX) {
        sysex_buf[sysex_len++] = byte;
        gsl::span<const uint8_t> frame(sysex_buf.data(), sysex_len);
        if (g_properties->log_midi_sysex) {
          print_timestamp();
          printf("<- ");
          for (size_t j = 0; j < frame.size(); j++) printf("%02X ", frame[j]);
          printf("\r\n");
        }

        gsl::span<const uint8_t> payload;
        const char *discard_reason = unpack_sysex(frame, &payload);
        if (!discard_reason) midi_sysex_received(payload);
        else { print_timestamp(); printf("sysex: frame discarded (%s)\r\n", discard_reason); }
      } else {
        print_timestamp();
        printf("sysex: frame discarded (buffer overflow)\r\n");
      }
      sysex_len = 0;
    } else if (sysex_len > 0 && sysex_len < MAX_SYSEX) {
      sysex_buf[sysex_len++] = byte;
    }
  }
}

void usb_app_task(void)
{
  tud_task();

  std::array<uint8_t, 4> packet;
  while (tud_midi_available()) {
    tud_midi_packet_read(packet.data());
    midi_input_process(packet);
  }
}

void usb_app_midi_test_note(uint8_t note)
{
  usb_app_midi_note_on(0, note, 100);
  usb_app_midi_note_off(0, note);
}

void usb_app_midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
  uint8_t const cable = 0;
  uint8_t msg[3] = { 0x90 | (channel & 0x0F), note, velocity };
  tud_midi_stream_write(cable, msg, 3);
}

void usb_app_midi_note_off(uint8_t channel, uint8_t note)
{
  uint8_t const cable = 0;
  uint8_t msg[3] = { 0x80 | (channel & 0x0F), note, 0 };
  tud_midi_stream_write(cable, msg, 3);
}

void usb_app_midi_control_change(uint8_t channel, uint8_t controller, uint8_t value)
{
  uint8_t const cable = 0;
  uint8_t msg[3] = { 0xB0 | (channel & 0x0F), controller, value };
  tud_midi_stream_write(cable, msg, 3);
}

void usb_app_midi_control_change_14bit(uint8_t channel, uint8_t cc_msb, uint16_t value14)
{
  value14 &= 0x3FFF;
  usb_app_midi_control_change(channel, cc_msb, (uint8_t)(value14 >> 7));
  usb_app_midi_control_change(channel, (uint8_t)(cc_msb + 32), (uint8_t)(value14 & 0x7F));
}

void usb_app_midi_active_sensing(void)
{
  uint8_t const cable = 0;
  uint8_t msg = 0xFE;
  tud_midi_stream_write(cable, &msg, 1);
}

void usb_app_midi_send_sysex(const uint8_t *data, size_t len)
{
  uint8_t const cable = 0;

  uint16_t checksum = sysex_checksum(data, len);

  static std::array<uint8_t, MAX_SYSEX> body;
  if (len + 2 > body.size()) {
    print_timestamp();
    printf("sysex: outgoing frame too large (%zu body bytes), not sent\r\n", len);
    return;
  }
  body[0] = (uint8_t)(checksum & 0xff);
  body[1] = (uint8_t)(checksum >> 8);
  std::copy(data, data + len, body.begin() + 2);

  static std::array<uint8_t, MAX_ENCODED_SYSEX> encoded;
  size_t n = sysex_encode7(body.data(), len + 2, encoded.data(), encoded.size());
  if (n == SIZE_MAX) {
    print_timestamp();
    printf("sysex: outgoing frame too large to encode (%zu body bytes), not sent\r\n", len);
    return;
  }

  if (g_properties->log_midi_sysex) {
    print_timestamp();
    printf("-> %02X ", MIDI_SYSEX_START);
    for (size_t i = 0; i < n; i++) printf("%02X ", encoded[i]);
    printf("%02X \r\n", MIDI_SYSEX_END);
  }

  uint8_t const header = MIDI_SYSEX_START;
  tud_midi_stream_write(cable, &header, 1);

  tud_midi_stream_write(cable, encoded.data(), n);

  uint8_t const footer = MIDI_SYSEX_END;
  tud_midi_stream_write(cable, &footer, 1);
}

/* extern "C" so these override the startup file's weak handlers; mangled
 * names would be silently dropped, leaving the vectors on Default_Handler. */
extern "C" {

void USB_HP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USB_LP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USBWakeUp_IRQHandler(void)
{
  tud_int_handler(0);
}

}  /* extern "C" */
