#ifndef APP_KEYBOARD_H
#define APP_KEYBOARD_H

#include <stdint.h>

#include "keyboard_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 0-based (on-wire) MIDI channels for the two keyboards: the left wing (SPI1)
 * and the right (SPI2) play on separate channels so a host can route/voice them
 * independently. The single bellows drives both, so its expression CC is sent
 * on both (see bellow.c). */
#define L_MIDI_CH 0
#define R_MIDI_CH 1

/* ===== Wing keyboard link (SPI slave) ===================================== *
 * Each wing streams a fixed SPI_LINK_FRAME_WORDS frame (see spi_link.h): word
 * 0 is the wing id, words 1.. are the raw hall measurement per key. SPI1 is
 * wired to the left wing, SPI2 to the right (L_SPI_ / R_SPI_ pins), but the
 * wing id in the frame is authoritative for note mapping. Reception is
 * DMA-driven (interrupt fallback), re-armed from the main loop on the NSS idle
 * gap (the only point that word-aligns); bad (CRC/overrun/misaligned) frames
 * are dropped and counted.
 *
 * The HAL_SPI Rx/Error callbacks are implemented in keyboard.cc and route to
 * the matching bus; the rest of the link is driven from the main loop through
 * the calls below. */

/* Binds the two buses to SPI1/SPI2 and their NSS lines and arms reception.
 * Call once after MX_SPIx_Init(). */
void keyboard_init(void);

/* Decodes any frames received since the last call into NOTE ON/OFF, recovers
 * any bus that lost word alignment, and logs link errors (throttled). Call
 * once per main loop iteration. */
void keyboard_poll(void);

/* Prints the 1 Hz per-bus reception rates (good/misaligned/crc/bus per second),
 * dt_ms being the elapsed time since the last call. */
void keyboard_print_rates(uint32_t dt_ms);

/* Count of keys currently held across both wing keyboards, regardless of bellows
 * direction (open pallets, including in NEUTRAL). The bellows inertia model uses
 * it to pick how fast chamber pressure bleeds: air escapes through an open pallet
 * whether or not its reed is sounding. */
unsigned keyboard_keys_pressed(void);

/* Wing id last seen on the given side's bus, or 0 if that wing hasn't sent a
 * good frame yet (e.g. not connected). */
uint8_t keyboard_wing_id(side_t side);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEYBOARD_H */
