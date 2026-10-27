#include "usb_app.h"

#include "tusb.h"
#include "stm32g4xx_hal.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <gsl/span>
#include "midi.h"
#include "properties.h"
#include "midi_tx.h"
#include "sysex_codec.h"

/* Max decoded sysex body (checksum + payload); also caps the incoming raw
 * frame midi_input_process accumulates. The framing itself is in
 * common/sysex/sysex_codec.h, shared with the bootloader. */
constexpr size_t MAX_SYSEX = 256;
constexpr size_t MAX_FRAME = SYSEX_ENCODED_SIZE(MAX_SYSEX) + 2;

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

  /* Below the console UART, so a USB burst can't starve reception (HAL tick
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

static void midi_input_process(gsl::span<const uint8_t, 4> packet)
{
  static std::array<uint8_t, MAX_SYSEX> sysex_buf;
  static sysex_rx_t rx = { sysex_buf.data(), sysex_buf.size(), 0 };

  uint8_t const valid_bytes = sysex_usb_packet_len(packet[0]);
  for (size_t i = 1; i <= valid_bytes; i++) {
    size_t frame_len;
    sysex_rx_result_t const r = sysex_rx_byte(&rx, packet[i], &frame_len);
    if (r == SYSEX_RX_OVERFLOW) {
      print_timestamp();
      printf("sysex: frame discarded (buffer overflow)\r\n");
    }
    if (r != SYSEX_RX_FRAME) continue;

    if (g_properties->log_midi_sysex) {
      print_timestamp();
      printf("<- ");
      for (size_t j = 0; j < frame_len; j++) printf("%02X ", sysex_buf[j]);
      printf("\r\n");
    }

    static std::array<uint8_t, MAX_SYSEX> decoded;
    const uint8_t *payload;
    size_t payload_len;
    const char *discard_reason = sysex_unpack(sysex_buf.data(), frame_len, decoded.data(), decoded.size(),
                                              &payload, &payload_len);
    if (!discard_reason) midi_sysex_received(gsl::span<const uint8_t>(payload, payload_len));
    else { print_timestamp(); printf("sysex: frame discarded (%s)\r\n", discard_reason); }
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
  usb_app_midi_note_off(0, note, MIDI_NOTE_OFF_DEFAULT_VELOCITY);
}

/* Every message goes through midi_tx_send() (common/sysex/midi_tx.h), whole
 * packets only: a full FIFO waits or drops whole messages, never half of one
 * that would garble the next. */

void usb_app_midi_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
  uint8_t msg[3] = { static_cast<uint8_t>(0x90 | (channel & 0x0F)), note, velocity };
  midi_tx_send(msg, sizeof(msg));
}

void usb_app_midi_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
  uint8_t msg[3] = { static_cast<uint8_t>(0x80 | (channel & 0x0F)), note, velocity };
  midi_tx_send(msg, sizeof(msg));
}

void usb_app_midi_control_change(uint8_t channel, uint8_t controller, uint8_t value)
{
  uint8_t msg[3] = { static_cast<uint8_t>(0xB0 | (channel & 0x0F)), controller, value };
  midi_tx_send(msg, sizeof(msg));
}

void usb_app_midi_control_change_14bit(uint8_t channel, uint8_t cc_msb, uint16_t value14)
{
  value14 &= 0x3FFF;
  usb_app_midi_control_change(channel, cc_msb, (uint8_t)(value14 >> 7));
  usb_app_midi_control_change(channel, (uint8_t)(cc_msb + 32), (uint8_t)(value14 & 0x7F));
}

void usb_app_midi_active_sensing(void)
{
  uint8_t const msg = 0xFE;
  midi_tx_send(&msg, 1);
}

void usb_app_midi_send_sysex(const uint8_t *data, size_t len)
{
  static std::array<uint8_t, MAX_FRAME> frame;
  size_t const n = (len + 2 <= MAX_SYSEX) ? sysex_pack(data, len, frame.data(), frame.size()) : 0;
  if (n == 0) {
    print_timestamp();
    printf("sysex: outgoing frame too large (%zu body bytes), not sent\r\n", len);
    return;
  }

  if (g_properties->log_midi_sysex) {
    print_timestamp();
    printf("-> ");
    for (size_t i = 0; i < n; i++) printf("%02X ", frame[i]);
    printf("\r\n");
  }

  if (!midi_tx_send(frame.data(), n)) {
    print_timestamp();
    printf("sysex: host not reading, frame dropped\r\n");
  }
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
