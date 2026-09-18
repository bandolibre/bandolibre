#ifndef APP_BELLOW_CLASSIFY_H
#define APP_BELLOW_CLASSIFY_H

#include <stdint.h>
#include "keyboard_layout.h"

/* Result of classifying a signed reading into a bellows direction and an
 * intensity fraction (0 always when NEUTRAL). */
typedef struct {
  bellows_t direction;
  float     intensity;  /* 0.0..1.0, always 0 when NEUTRAL -- a fraction of
                        * full travel */
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
 * Intensity is 0.0 at the entry edge (push_edge) and 1.0 at full_push/full_pull.
 * Pass full_push = full_pull = 0 to skip intensity (direction-only use).
 */
bellow_classify_result_t bellow_classify(bellows_t prev, float value_f,
                                         int32_t center, int32_t dead, int32_t hyst,
                                         int32_t full_push, int32_t full_pull);

#endif /* APP_BELLOW_CLASSIFY_H */
