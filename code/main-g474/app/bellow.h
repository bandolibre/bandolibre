#ifndef APP_BELLOW_H
#define APP_BELLOW_H

#include <stdbool.h>
#include <stdint.h>
#include "keyboard_layout.h"

/* The bellows drives expression on every tuning, and on a bisonoric one it also
 * selects which of a key's two notes sounds (see keyboard_layout.h).
 * This module reads the two hall sensors, tracks the bellows direction and how
 * hard it is being pushed or pulled (both derived from the combined hall
 * reading), and emits CC#11 (Expression) from the intensity — except in table
 * mode, where the bellows rests and CC#11 is instead pinned to a constant. */

/* Current bellows direction (BELLOWS_NEUTRAL/PUSH/PULL). */
bellows_t bellow_direction(void);

/* How hard the bellows is currently being pushed or pulled, as a fraction of
 * full travel: 0.0..1.0 (0.0 in BELLOWS_NEUTRAL). Consumers scale it to
 * whatever discrete range they need (CC#11's 14-bit pair, MIDI velocity's
 * 7-bit range, ...). */
float bellow_intensity(void);

/* Sensitivity multiplier for the level FN1 currently selects, as a Q8 fixed
 * point value (256 = x1.0). Already applied to bellow_intensity(). Table mode
 * has no bellows signal to scale, so the levels do not affect it. */
uint16_t bellow_sens_scale_q8(void);

/* Samples both hall sensors, updates the direction/intensity, and emits the
 * expression CC. Call once per main loop iteration. Read the result via
 * bellow_direction(); consumers track changes themselves. */
void bellow_poll(void);

/* Raw ADC readings from the last bellow_poll(), before centering or
 * classification: hall0/hall1 correspond to hadc3/hadc4 in bellow.c. */
void bellow_get_raw(uint16_t *hall0, uint16_t *hall1);

/* Diagnostic sweep over a range of the bellow_settle_us property: for each
 * value, repeatedly samples both hall sensors and prints a table of their mean
 * and standard deviation, to pick the smallest settling delay that reads
 * stably. Blocks the main loop for a few seconds and restores bellow_settle_us
 * before returning. Intended for the console 'bellow_tune' command. */
void bellow_tune(void);

#endif /* APP_BELLOW_H */
