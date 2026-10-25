#include "key_strike.h"

#include <math.h>

#include "bellow_curve.h"

void key_strike_reset(key_strike_t *s)
{
  s->have_prev = false;
  s->phase = KEY_STRIKE_IDLE;
}

/* Time at which the reading crossed `thr` on its way from prev_v to v (either
 * direction, thr between the two), assuming a straight line between the two
 * samples. */
static uint32_t crossing_us(const key_strike_t *s, uint16_t v, uint32_t t_us, uint16_t thr)
{
  uint32_t dt = t_us - s->prev_t_us;
  int32_t move = (int32_t)v - (int32_t)s->prev_v;
  int32_t part = (int32_t)thr - (int32_t)s->prev_v;
  return s->prev_t_us + (uint32_t)((float)dt * (float)part / (float)move);
}

/* Speed of a transit of `span` counts that began at s->t0_us and ended at
 * t1_us, in counts per 10 ms. */
static int32_t transit_speed(const key_strike_t *s, uint32_t t1_us, uint16_t span)
{
  uint32_t dt = t1_us - s->t0_us;
  if (dt < 1) dt = 1;
  if (dt > KEY_STRIKE_MAX_TRANSIT_US) dt = KEY_STRIKE_MAX_TRANSIT_US;
  float per10ms = (float)span * 10000.0f / (float)dt;
  return per10ms > 65535.0f ? 65535 : (int32_t)(per10ms + 0.5f);
}

key_edge_t key_strike_update(key_strike_t *s, uint16_t v, uint32_t t_us,
                             uint16_t start, uint16_t press, uint16_t release,
                             int32_t *speed)
{
  key_edge_t edge = KEY_EDGE_NONE;
  if (start <= press) start = (uint16_t)(press + 1);
  if (release <= press) release = (uint16_t)(press + 1);

  if (s->have_prev)
  {
    /* Strike timing starts where the reading falls across start. Back above
     * start, the stroke was abandoned (or hasn't begun), so an earlier
     * crossing no longer describes this press. */
    if (v > start)
    {
      if (s->phase == KEY_STRIKE_FALLING) s->phase = KEY_STRIKE_IDLE;
    }
    else if (s->prev_v > start)
    {
      s->t0_us = crossing_us(s, v, t_us, start);
      s->phase = KEY_STRIKE_FALLING;
    }

    /* Release timing starts where the reading rises across press, and is
     * abandoned the same way if it falls back. If start is set below
     * release, a key lifted above start and pushed back down before reaching
     * release starts a strike timing over this one: that release then
     * reports no speed. */
    if (v <= press)
    {
      if (s->phase == KEY_STRIKE_RISING) s->phase = KEY_STRIKE_IDLE;
    }
    else if (s->prev_v <= press)
    {
      s->t0_us = crossing_us(s, v, t_us, press);
      s->phase = KEY_STRIKE_RISING;
    }

    if (s->prev_v > press && v <= press)
    {
      edge = KEY_EDGE_PRESS;
      *speed = s->phase == KEY_STRIKE_FALLING
             ? transit_speed(s, crossing_us(s, v, t_us, press), (uint16_t)(start - press))
             : KEY_STRIKE_NO_SPEED;
      s->phase = KEY_STRIKE_IDLE;
    }
    else if (s->prev_v < release && v >= release)
    {
      edge = KEY_EDGE_RELEASE;
      *speed = s->phase == KEY_STRIKE_RISING
             ? transit_speed(s, crossing_us(s, v, t_us, release), (uint16_t)(release - press))
             : KEY_STRIKE_NO_SPEED;
      s->phase = KEY_STRIKE_IDLE;
    }
  }

  s->prev_v = v;
  s->prev_t_us = t_us;
  s->have_prev = true;
  return edge;
}

float key_strike_normalize(const key_strike_map_t *m, int32_t speed)
{
  float lo = (float)m->speed_min;
  float hi = (float)m->speed_max;
  float x = (float)speed;
  float t;
  if (m->log_scale)
  {
    /* ln needs positive arguments; below 1 count per 10 ms is "no speed". */
    if (lo < 1.0f) lo = 1.0f;
    if (hi <= lo) return x >= hi ? 1.0f : 0.0f;
    if (x < 1.0f) x = 1.0f;
    t = (logf(x) - logf(lo)) / (logf(hi) - logf(lo));
  }
  else
  {
    if (hi <= lo) return x >= hi ? 1.0f : 0.0f;
    t = (x - lo) / (hi - lo);
  }
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return bellow_curve_apply(t, m->cx1, m->cy1, m->cx2, m->cy2);
}

uint8_t key_strike_velocity(const key_strike_map_t *m, int32_t speed)
{
  float lo = (float)m->vel_min;
  float hi = (float)m->vel_max;
  float vel = lo + key_strike_normalize(m, speed) * (hi - lo);
  int32_t r = (int32_t)(vel + 0.5f);
  if (r < 1) r = 1;
  if (r > 127) r = 127;
  return (uint8_t)r;
}
