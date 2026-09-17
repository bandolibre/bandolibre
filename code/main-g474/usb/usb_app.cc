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

/* Max size of a decoded (checksum + payload) sysex body. Also bounds how
 * large an incoming encoded wire frame we're willing to accumulate (see
 * midi_input_process) - matched by CFG_TUD_MIDI_TX_BUFSIZE in
 * tusb_config.h. */
constexpr size_t MAX_SYSEX = 256;

/* 7-bit-safe encoding for the checksum+payload region between the literal
 * 0xF0/0xF7 markers above. Property values (and any future binary payload,
 * e.g. raw hall sensor readings) are arbitrary 16-bit numbers, so a data
 * byte in there can have its high bit set. An earlier version of this
 * protocol escaped only the 3 literal byte values that collide with our own
 * 0xF0/0xF7 framing (SLIP/PPP-style byte-stuffing), on the theory that this
 * is a closed protocol between this firmware and code/tool/sysex/sysex_dump.py
 * / site/midi.html that didn't need full MIDI-spec compliance. That was
 * wrong: confirmed empirically, the Linux kernel's own USB-MIDI driver
 * reconstructs a standard MIDI byte stream when feeding ALSA/Web MIDI, and
 * any byte with bit 7 set gets misread there as a new status byte,
 * corrupting the frame - even though the exact same bytes survive raw USB
 * access (sysex_dump.py) untouched, since that path decodes USB-MIDI
 * CIN-tagged packets directly and never reinterprets them as generic MIDI.
 * Real MIDI sysex avoids this entirely by repacking every byte into 7-bit
 * groups, which this now does too: each run of up to 7 input bytes becomes
 * 8 output bytes - a leading byte holding the high bit of each input byte
 * (bit j = input byte j's bit 7), followed by those bytes with bit 7
 * cleared. Every output byte is then <= 0x7F, so it can never collide with
 * 0xF0/0xF7 either - no separate escaping step is needed on top. */
constexpr size_t sysex7_encoded_size(size_t decoded_len) { return ((decoded_len + 6) / 7) * 8; }
constexpr size_t MAX_ENCODED_SYSEX = sysex7_encoded_size(MAX_SYSEX);

/* [seconds.millis] prefix matching the format the host-side sysex_dump.py
 * tool uses, so the two logs can be compared line by line. */
static void print_timestamp(void)
{
  uint32_t ms = HAL_GetTick();
  printf("[%6lu.%03lu] ", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
}

void usb_app_init(void)
{
  /* The 48 MHz USB kernel clock (HSI48 + CRS, crystal-less) and the USB
   * peripheral clock are configured by the CubeMX-generated
   * SystemClock_Config() and MX_USB_PCD_Init(); enforced by
   * code/tests/test_usb_config.py against the .ioc. */

  /* Keep USB below the console UART so a CDC write burst cannot starve
   * character reception (HAL tick stays at 0, set by HAL_Init). */
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

/* XOR-fold checksum over 16-bit little-endian words, folding in a lone
 * trailing byte if len is odd. Used to both seal outgoing sysex frames and
 * verify incoming ones. */
static uint16_t sysex_checksum(const uint8_t *data, size_t len)
{
  uint16_t cs = 0;
  size_t i = 0;
  for (; i + 1 < len; i += 2)
    cs ^= (uint16_t)(data[i] | (uint16_t)(data[i + 1] << 8));
  if (i < len) cs ^= data[i];
  return cs;
}

/* Encodes in[0..len) into 7-bit-safe groups written to out (see comment on
 * sysex7_encoded_size above). Returns the number of bytes written, or
 * SIZE_MAX if out_cap is too small. */
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

/* Reverses sysex_encode7: decodes in[0..len) into out. Returns the number
 * of bytes written, or SIZE_MAX if out_cap is too small or `in` ends mid
 * group (truncated frame). */
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

/* Strips a complete raw sysex frame (0xF0 ... 0xF7, as accumulated by
 * midi_input_process) down to its checksum-verified payload (message
 * identifier + body). On success returns nullptr and sets *payload; on
 * failure returns a short reason string to log and leaves *payload
 * untouched. */
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

  /* The low nibble of byte 0 is the USB-MIDI Code Index Number, which says
   * how many of the next three bytes are real payload versus unused packet
   * tail. Sysex payload can legitimately contain 0x00 (our checksum framing
   * does this constantly), so a zero byte can't be used to detect the end
   * of valid data. */
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

/* USB interrupt handlers (override the weak defaults from the startup file).
 * extern "C" is required here: the vector table in startup_stm32g474xx.s
 * declares these with C linkage, so without it these get C++-mangled names
 * that don't override the weak aliases, and --gc-sections silently drops
 * them as unreferenced, leaving the vectors pointing at Default_Handler's
 * infinite loop. */
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
