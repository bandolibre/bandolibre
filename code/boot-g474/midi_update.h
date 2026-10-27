#ifndef BOOT_MIDI_UPDATE_H
#define BOOT_MIDI_UPDATE_H

/* Firmware update over MIDI SysEx, the bootloader's side of the web update
 * tool (docs/update/index.html). Protocol in common/sysex/sysex_ids.h.
 *
 * Safety rests on two orderings:
 *   - UPDATE_BEGIN erases the application's first page before anything else,
 *     so from then on app_valid() fails and an interrupted update (tab
 *     closed, cable pulled) boots back into the bootloader;
 *   - the image's first double word (initial SP and reset PC) is held in RAM
 *     and programmed only by UPDATE_COMMIT, once the CRC-32 of the whole image
 *     in flash matches the one announced at BEGIN.
 *
 * Writes are idempotent (rewriting identical bytes succeeds), so the tool can
 * retry any request whose reply it lost. Every request carries a session
 * number from BEGIN, so a second client that starts its own update makes the
 * first one fail cleanly instead of interleaving pages. Requests the
 * bootloader does not know (the configuration tool's, bandopm's) are ignored
 * without a reply.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Reads and answers the pending MIDI packets. Call from the DFU loop. Resets
 * the chip on its own 100 ms after a successful UPDATE_COMMIT. */
void midi_update_task(void);

/* True while an update session is open, for the LED. */
bool midi_update_active(void);

/* Forgets the open session: the UF2 drive is about to rewrite the
 * application, and the MIDI client must not carry on over it. */
void midi_update_cancel(void);

/* Provided by main.c. */

/* The application's vector table looks valid. */
bool app_valid(void);

/* Set by the NMI handler when a flash read hit a double ECC error: a double
 * word torn by a power cut while it was programmed. */
extern volatile bool flash_ecc_fault;

/* Copies `len` bytes of flash; false if any of them were torn. */
bool flash_read(uint32_t addr, void *dst, size_t len);

#endif /* BOOT_MIDI_UPDATE_H */
