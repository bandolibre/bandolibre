#ifndef APP_PEDALS_H
#define APP_PEDALS_H

#include <stdbool.h>
#include <stdint.h>

/* Two external pedals plug into the main board: an expression pedal and a
 * sustain pedal. Each has a presence-detect line (EXP/SUS_PEDAL_INT, high when a
 * pedal is plugged in) and an analog wiper read by ADC1 (expression) / ADC2
 * (sustain). This module polls both and logs presence changes and wiper
 * movement. */

/* Reads both pedals' presence lines and wiper ADCs, and logs presence changes
 * and wiper movement past a small threshold. Call once per main loop
 * iteration. */
void pedals_poll(void);

/* Raw wiper ADC readings and presence flags from the last pedals_poll().
 * A sample is meaningless (floating) while its *_connected is false. */
void pedals_get_raw(uint16_t *pedal1_sample, bool *pedal1_connected,
                    uint16_t *pedal2_sample, bool *pedal2_connected);

#endif /* APP_PEDALS_H */
