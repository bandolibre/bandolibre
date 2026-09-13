#include "usb_app.h"

#include "tusb.h"
#include "stm32g4xx_hal.h"
#include <array>
#include <cstdio>
#include <gsl/span>
#include "midi.h"

constexpr uint8_t MIDI_SYSEX_START = 0xF0;
constexpr uint8_t MIDI_SYSEX_END = 0xF7;

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

/* Strips a complete raw sysex frame (0xF0 ... 0xF7, as accumulated by
 * midi_input_process) down to its checksum-verified payload (message
 * identifier + body). On success returns nullptr and sets *payload; on
 * failure returns a short reason string to log and leaves *payload
 * untouched. */
static const char *unpack_sysex(gsl::span<const uint8_t> frame, gsl::span<const uint8_t> *payload)
{
  if (frame.size() < 5) return "frame too short";
  if (frame.front() != MIDI_SYSEX_START || frame.back() != MIDI_SYSEX_END) return "missing 0xF0/0xF7 markers";

  uint16_t received_checksum = (uint16_t)(frame[1] | (uint16_t)(frame[2] << 8));
  gsl::span<const uint8_t> covered = frame.subspan(3, frame.size() - 4);
  if (sysex_checksum(covered.data(), covered.size()) != received_checksum) return "checksum mismatch";

  *payload = covered;
  return nullptr;
}

static void midi_input_process(gsl::span<const uint8_t, 4> packet)
{
  constexpr size_t MAX_SYSEX = 256;

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
        print_timestamp();
        printf("<- ");
        for (size_t j = 0; j < frame.size(); j++) printf("%02X ", frame[j]);
        printf("\r\n");

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
  uint8_t header[3] = { MIDI_SYSEX_START, (uint8_t)(checksum & 0xff), (uint8_t)(checksum >> 8) };

  print_timestamp();
  printf("-> ");
  for (size_t i = 0; i < sizeof(header); i++) printf("%02X ", header[i]);
  for (size_t i = 0; i < len; i++) printf("%02X ", data[i]);
  printf("%02X \r\n", MIDI_SYSEX_END);

  tud_midi_stream_write(cable, header, sizeof(header));

  tud_midi_stream_write(cable, data, len);

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
