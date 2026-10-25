#ifndef LEASE_H_
#define LEASE_H_

/* A lease that a host holds by renewing it: held for `ms` after its last
 * renewal, and never before the first one. Times are millisecond ticks
 * (HAL_GetTick) and survive their wrap-around. HAL-free, for the host tests. */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool renewed;   /* renewed at least once */
  uint32_t tick;  /* tick of the last renewal */
} lease_t;

static inline void lease_renew(lease_t *l, uint32_t now)
{
  l->renewed = true;
  l->tick = now;
}

static inline bool lease_held(const lease_t *l, uint32_t now, uint32_t ms)
{
  return l->renewed && now - l->tick < ms;
}

#endif /* LEASE_H_ */
