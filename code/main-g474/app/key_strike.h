#ifndef APP_KEY_STRIKE_H
#define APP_KEY_STRIKE_H

#include <stdbool.h>
#include <stdint.h>

/* Key strike and release speeds from the raw hall readings, and their mapping
 * to a MIDI velocity. HAL-free so it is host-tested.
 *
 * Why not a derivative: a reading is one 12-bit conversion with no
 * oversampling, and frames reach the main board at an irregular rate (the
 * triple buffer keeps only the latest). Differencing neighbouring samples
 * would mostly amplify noise. The standard keyboard method is used instead:
 * the time the reading takes to cross a fixed span, with both crossing times
 * linearly interpolated inside their frame interval. Noise of a few counts is
 * irrelevant against a span of hundreds, and the result is known in the very
 * frame that fires the note on or off, so it adds no latency.
 *
 * - Strike: falling from a "start" threshold (just below the rest position)
 *   to the key_press threshold.
 * - Release: rising from key_press back to the key_release threshold, the
 *   press/release hysteresis band.
 *
 * A key is either on its way down or on its way back up, never both, so one
 * state per key times both edges: `phase` says which transit t0_us belongs to.
 *
 * Time is a free-running microsecond counter (wraps after ~71 minutes; only
 * unsigned differences are used). A reading falls as the key is pressed. */

enum {
  KEY_STRIKE_IDLE,     /* no transit being timed */
  KEY_STRIKE_FALLING,  /* below start since t0_us, press threshold not yet reached */
  KEY_STRIKE_RISING,   /* above press since t0_us, release threshold not yet reached */
};

typedef struct {
  uint16_t prev_v;
  uint32_t prev_t_us;
  uint32_t t0_us;       /* interpolated time the transit in progress began */
  bool     have_prev;
  uint8_t  phase;       /* KEY_STRIKE_IDLE, _FALLING or _RISING */
} key_strike_t;

/* Which threshold a frame took the reading across, if any. */
typedef enum {
  KEY_EDGE_NONE,
  KEY_EDGE_PRESS,       /* fell across press */
  KEY_EDGE_RELEASE,     /* rose across release */
} key_edge_t;

/* Speed reported when a key crosses press (or release) without a clean
 * transit before it (first frame after boot, a press that began below the
 * start threshold, a release that began above press): the slowest the
 * mapping can express. */
#define KEY_STRIKE_NO_SPEED 0

/* A slower transit than this counts as "very slow"; it only bounds the
 * arithmetic (the mapping clamps to its own minimum speed far above it). */
#define KEY_STRIKE_MAX_TRANSIT_US 500000u

void key_strike_reset(key_strike_t *s);

/* Feeds one frame's reading for one key, `t_us` being when the frame was
 * received. If this frame took the reading down across `press` or up across
 * `release`, returns that edge and sets *speed to the transit speed in counts
 * per 10 ms; otherwise returns KEY_EDGE_NONE and leaves *speed alone. `start`
 * and `release` must be above `press`; if not, they are taken as press + 1. */
key_edge_t key_strike_update(key_strike_t *s, uint16_t v, uint32_t t_us,
                             uint16_t start, uint16_t press, uint16_t release,
                             int32_t *speed);

/* Mapping parameters, mirroring the strike_* (or release_*) properties. */
typedef struct {
  uint16_t speed_min, speed_max;      /* counts per 10 ms */
  uint16_t vel_min, vel_max;          /* 1..127 */
  uint16_t cx1, cy1, cx2, cy2;        /* Q8 Bezier control points, as bellow_curve */
  bool     log_scale;                 /* normalise ln(speed) instead of speed */
} key_strike_map_t;

/* Speed (counts per 10 ms) to a 0.0..1.0 position along the response curve,
 * before and after the S-shaping. Exposed so the tool's graph can be checked
 * against the firmware. */
float key_strike_normalize(const key_strike_map_t *m, int32_t speed);

/* Speed to MIDI velocity, 1..127. */
uint8_t key_strike_velocity(const key_strike_map_t *m, int32_t speed);

#endif /* APP_KEY_STRIKE_H */
