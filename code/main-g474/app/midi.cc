#include "midi.h"
#include "main.h"        /* HAL_GetTick */
#include "properties.h"
#include "usb_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern "C" {

#define MIDI_SYSEX_START 0xF0
#define MIDI_SYSEX_END 0xF7

#define MIDI_DEFAULT_VELOCITY 64

void midi_poll(void)
{
  static uint32_t last_tick = 0;
  if (!g_properties->midi_active_sensing_enable) return;
  uint32_t now = HAL_GetTick();
  if ((now - last_tick) < g_properties->midi_active_sensing_period) return;
  last_tick = now;
  usb_app_midi_active_sensing();
}

/* Parse argv[idx] as an integer in [lo,hi]. Prints an error and returns false
 * if missing, not a number, or out of range. */
static bool parse_arg(int argc, const char *const *argv, int idx,
                      long lo, long hi, const char *what, long *out)
{
  if (idx >= argc) { printf("missing %s\r\n", what); return false; }
  char *end;
  long v = strtol(argv[idx], &end, 0);
  if (argv[idx][0] == '\0' || *end != '\0') { printf("not a number: %s\r\n", argv[idx]); return false; }
  if (v < lo || v > hi) { printf("%s out of range [%ld,%ld]: %ld\r\n", what, lo, hi, v); return false; }
  *out = v;
  return true;
}

static void cmd_note_on(int argc, const char *const *argv)
{
  long channel, note, velocity = MIDI_DEFAULT_VELOCITY;
  if (!parse_arg(argc, argv, 1, 1, 16, "channel", &channel) ||
      !parse_arg(argc, argv, 2, 0, 127, "note", &note))
  {
    printf("usage: send_note_on <channel 1-16> <note 0-127> [velocity 0-127]\r\n");
    return;
  }
  if (argc > 3 && !parse_arg(argc, argv, 3, 0, 127, "velocity", &velocity)) return;
  usb_app_midi_note_on((uint8_t)(channel - 1), (uint8_t)note, (uint8_t)velocity);
  printf("note on  ch %ld note %ld vel %ld\r\n", channel, note, velocity);
}

static void cmd_note_off(int argc, const char *const *argv)
{
  long channel, note;
  if (!parse_arg(argc, argv, 1, 1, 16, "channel", &channel) ||
      !parse_arg(argc, argv, 2, 0, 127, "note", &note))
  {
    printf("usage: send_note_off <channel 1-16> <note 0-127>\r\n");
    return;
  }
  usb_app_midi_note_off((uint8_t)(channel - 1), (uint8_t)note);
  printf("note off ch %ld note %ld\r\n", channel, note);
}

static void cmd_cc(int argc, const char *const *argv)
{
  long channel, controller, value;
  if (!parse_arg(argc, argv, 1, 1, 16, "channel", &channel) ||
      !parse_arg(argc, argv, 2, 0, 127, "controller", &controller) ||
      !parse_arg(argc, argv, 3, 0, 127, "value", &value))
  {
    printf("usage: send_cc <channel 1-16> <controller 0-127> <value 0-127>\r\n");
    return;
  }
  usb_app_midi_control_change((uint8_t)(channel - 1), (uint8_t)controller, (uint8_t)value);
  printf("cc       ch %ld ctrl %ld val %ld\r\n", channel, controller, value);
}

static void cmd_send_sysex(int argc, const char *const *argv);

bool midi_console_execute(int argc, const char *const *argv)
{
  if (argc == 0) return false;
  if (strcmp(argv[0], "send_note_on") == 0)       cmd_note_on(argc, argv);
  else if (strcmp(argv[0], "send_note_off") == 0) cmd_note_off(argc, argv);
  else if (strcmp(argv[0], "send_cc") == 0)       cmd_cc(argc, argv);
  else if (strcmp(argv[0], "send_sysex") == 0)    cmd_send_sysex(argc, argv);
  else return false;
  return true;
}

void midi_console_help(void)
{
  printf("\r\nMIDI commands (cable 0, channel 1-16, data 0-127):\r\n");
  printf("  send_note_on  <channel> <note> [velocity]   velocity defaults to %d\r\n", MIDI_DEFAULT_VELOCITY);
  printf("  send_note_off <channel> <note>\r\n");
  printf("  send_cc       <channel> <controller> <value>\r\n");
  printf("  send_sysex    <byte0> [byte1] ...           sends raw sysex (hex/decimal)\r\n");
}

size_t midi_console_complete(const char *prefix, const char **out, size_t cap)
{
  static const char *const names[] = { "send_note_on", "send_note_off", "send_cc", "send_sysex" };
  size_t n = 0;
  size_t plen = prefix ? strlen(prefix) : 0;
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]) && n < cap; i++)
    if (strncmp(names[i], prefix ? prefix : "", plen) == 0) out[n++] = names[i];
  return n;
}

static void cmd_send_sysex(int argc, const char *const *argv)
{
  if (argc < 2) {
    printf("usage: send_sysex <byte0> [byte1] ...\r\n");
    printf("  sends sysex message with given data bytes (in hex, decimal, or 0x prefix)\r\n");
    printf("  example: send_sysex 0xF0 0x7E 0x00 0x09 0x01 0xF7\r\n");
    return;
  }

  uint8_t msg[256];
  size_t len = 0;

  for (int i = 1; i < argc && len < sizeof(msg); i++) {
    char *end;
    long val = strtol(argv[i], &end, 0);
    if (argv[i][0] == '\0' || *end != '\0') {
      printf("not a number: %s\r\n", argv[i]);
      return;
    }
    if (val < 0 || val > 255) {
      printf("byte out of range [0,255]: %ld\r\n", val);
      return;
    }
    msg[len++] = (uint8_t)val;
  }

  usb_app_midi_send_sysex(msg, len);
  printf("sysex sent: ");
  for (size_t i = 0; i < len; i++) printf("%02X ", msg[i]);
  printf("\r\n");
}

void midi_sysex_received(const uint8_t *data, size_t len)
{
  printf("sysex received (%zu bytes): ", len);
  for (size_t i = 0; i < len; i++) printf("%02X ", data[i]);
  printf("\r\n");

  // The sysex frames have the following format.
  // 0-1  : 16 bit checksum
  // 2    :
}

}  /* extern "C" */
