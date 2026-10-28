#include <gsl/span>

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "bootloader.h"
#include "system_dfu.h"
#include "usb_app.h"
#include "bellow.h"
#include "properties.h"
#include "properties_console.h"
#include "console.h"
}

#include "midi.h"

#define CONSOLE_COMPL_MAX 32

namespace {
bool midi_exec_wrapper(int argc, const char *const *argv)
{
  return midi_console_execute(gsl::span<const char* const>(argv, argc));
}

void midi_help_wrapper()
{
  midi_console_help();
}

size_t midi_complete_wrapper(const char *prefix, const char **out, size_t cap)
{
  return midi_console_complete(prefix, out, cap);
}
}

extern "C" {

int console_execute(int argc, const char *const *argv)
{
  if (argc == 0)
    return 0;
  if (strcmp(argv[0], "hello") == 0)
  {
    printf("Hello, Bandolibre!\r\n");
  }
  else if (strcmp(argv[0], "midi") == 0)
  {
    uint8_t note = (argc > 1) ? (uint8_t)atoi(argv[1]) : 69;
    usb_app_midi_test_note(note);
    printf("Sent MIDI note %u on/off\r\n", note);
  }
  else if (strcmp(argv[0], "dfu") == 0)
  {
    printf("Rebooting into the bootloader; the BANDOLIBRE drive will appear.\r\n");
    reboot_to_bootloader(true);
  }
  else if (strcmp(argv[0], "romdfu") == 0)
  {
    printf("Restarting into the STM32 DFU loader in ROM (USB 0483:df11).\r\n");
    HAL_Delay(20);   // let the line out of the console UART
    system_dfu_enter();
  }
  else if (strcmp(argv[0], "bellow_tune") == 0) bellow_tune();
  else if (strcmp(argv[0], "help") == 0)
  {
    properties_help();
    midi_help_wrapper();
  }
  else if (properties_execute(argc, argv)) { }
  else if (midi_exec_wrapper(argc, argv)) { }
  else
    printf("Unknown command: %s (try 'help')\r\n", argv[0]);
  return 0;
}

char **console_complete(int argc, const char *const *argv)
{
  static const char *commands[] = { "help", "show", "get", "set", "reset", "save", "factory_reset", "store",
                                    "hello", "midi", "bellow_tune", "dfu", "romdfu" };
  static char *out[CONSOLE_COMPL_MAX + 1];
  const char *partial = (argc > 0) ? argv[argc - 1] : "";
  size_t n = 0;

  if (argc <= 1)
  {
    size_t plen = strlen(partial);
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
      if (strncmp(commands[i], partial, plen) == 0)
        out[n++] = (char *)commands[i];
    const char *midi[CONSOLE_COMPL_MAX];
    size_t m = midi_complete_wrapper(partial, midi, CONSOLE_COMPL_MAX - n);
    for (size_t i = 0; i < m; i++)
      out[n++] = (char *)midi[i];
  }
  else if (argc == 2 && (strcmp(argv[0], "get") == 0 ||
                         strcmp(argv[0], "set") == 0 ||
                         strcmp(argv[0], "reset") == 0))
  {
    const char *names[CONSOLE_COMPL_MAX];
    size_t m = properties_complete(partial, names, CONSOLE_COMPL_MAX);
    for (size_t i = 0; i < m; i++)
      out[n++] = (char *)names[i];
  }
  out[n] = NULL;
  return out;
}

}  /* extern "C" */
