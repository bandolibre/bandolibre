#include "midi_tx.h"

#include "stm32g4xx_hal.h"   /* HAL_GetTick */
#include "sysex_codec.h"
#include "tusb.h"

static bool host_stalled;   /* the last wait timed out */
static bool owe_sysex_end;  /* a frame was cut: close it before anything else */

/* Queues one packet, waiting for room unless the host looks stalled. */
static bool put(const uint8_t packet[4])
{
  if (tud_midi_packet_write(packet)) {
    host_stalled = false;
    return true;
  }
  if (host_stalled || !tud_midi_mounted()) return false;

  uint32_t const start = HAL_GetTick();
  while (HAL_GetTick() - start < MIDI_TX_WAIT_MS) {
    tud_task();
    if (tud_midi_packet_write(packet)) return true;
  }
  host_stalled = true;
  return false;
}

bool midi_tx_send(const uint8_t *msg, size_t len)
{
  if (owe_sysex_end) {
    static uint8_t const end[4] = { 0x05, SYSEX_END, 0, 0 };   /* CIN 0x5: sysex ends with 1 byte */
    if (!put(end)) return false;
    owe_sysex_end = false;
  }

  size_t pos = 0;
  uint8_t packet[4];
  while (midi_usb_packet(msg, len, &pos, packet)) {
    if (put(packet)) continue;
    /* Dropped. Past the first packet of a frame the host holds an open
     * sysex, which the next message must not run into. */
    if (msg[0] == SYSEX_START && pos > 3) owe_sysex_end = true;
    return false;
  }
  return true;
}
