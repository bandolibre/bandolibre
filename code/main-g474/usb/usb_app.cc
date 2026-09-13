#include "usb_app.h"

#include "tusb.h"
#include "stm32g4xx_hal.h"
#include <array>
#include <gsl/span>

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

static void midi_input_process(gsl::span<const uint8_t, 4> packet)
{
  constexpr uint8_t MIDI_SYSEX_START = 0xF0;
  constexpr uint8_t MIDI_SYSEX_END = 0xF7;
  constexpr size_t MAX_SYSEX = 256;

  static std::array<uint8_t, MAX_SYSEX> sysex_buf;
  static size_t sysex_len = 0;

  for (size_t i = 1; i < packet.size(); i++) {
    uint8_t byte = packet[i];
    if (byte == 0) break;

    if (byte == MIDI_SYSEX_START) {
      sysex_len = 1;
      sysex_buf[0] = byte;
    } else if (byte == MIDI_SYSEX_END && sysex_len > 0) {
      if (sysex_len < MAX_SYSEX) {
        sysex_buf[sysex_len++] = byte;
        midi_sysex_received(sysex_buf.data(), sysex_len);
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
  tud_midi_stream_write(cable, data, len);
}

/* USB interrupt handlers (override the weak defaults from the startup file) */
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
