#ifndef APP_BELLOW_CLASSIFY_H
#define APP_BELLOW_CLASSIFY_H

#include <stdint.h>
#include "keyboard_layout.h"

/* Result of classifying a signed reading into a bellows direction and an
 * intensity on 0..BELLOW_INTENSITY_MAX (0 always when NEUTRAL). */
typedef struct {
  bellows_t direction;
  uint16_t  intensity;  /* 0..BELLOW_INTENSITY_MAX, always 0 when NEUTRAL */
} bellow_classify_result_t;

/* Classifies a (signed) reading into PUSH/NEUTRAL/PULL around a center, with a
 * deadzone and hysteresis so a resting reading never chatters between states.
 *
 * Transitions (using push side as example, pull is symmetric):
 *   From NEUTRAL: enter PUSH when value < push_edge = center - dead/2 - hyst/2
 *   From PUSH:    return to NEUTRAL when value >= push_edge + hyst
 *
 * So "dead" sets the half-span of the zone between the two return-to-NEUTRAL
 * thresholds, and "hyst" is the extra travel required to leave NEUTRAL in the
 * first place (entry threshold = dead/2 + hyst/2 from center).
 *
 * Intensity is 0 at the entry edge (push_edge) and BELLOW_INTENSITY_MAX at
 * full_push/full_pull.
 * Pass full_push = full_pull = 0 to skip intensity (direction-only use). */
bellow_classify_result_t bellow_classify(bellows_t prev, int32_t value,
                                         int32_t center, int32_t dead, int32_t hyst,
                                         int32_t full_push, int32_t full_pull);

#endif /* APP_BELLOW_CLASSIFY_H */
