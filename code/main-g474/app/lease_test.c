/* Host unit tests for the renewable lease. Pure C, no HAL -- compile and run
 * natively (see `just test`). Framework-free: a tiny assert macro that counts
 * failures and reports a final summary. */

#include "lease.h"

#include <stdio.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                               \
  do {                                                            \
    g_checks++;                                                   \
    if (!(cond)) {                                                \
      g_failures++;                                               \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
    }                                                             \
  } while (0)

static void test_never_renewed(void)
{
  lease_t l = {0};
  CHECK(!lease_held(&l, 0, 2000));
  CHECK(!lease_held(&l, 100, 2000));
}

static void test_expires(void)
{
  lease_t l = {0};
  lease_renew(&l, 1000);
  CHECK(lease_held(&l, 1000, 2000));
  CHECK(lease_held(&l, 2999, 2000));
  CHECK(!lease_held(&l, 3000, 2000));
  /* A renewal extends it. */
  lease_renew(&l, 2500);
  CHECK(lease_held(&l, 4000, 2000));
  CHECK(!lease_held(&l, 4500, 2000));
}

static void test_tick_wraps(void)
{
  lease_t l = {0};
  lease_renew(&l, 0xFFFFFF00u);
  CHECK(lease_held(&l, 0x00000100u, 2000));   /* 512 ms later, across the wrap */
  CHECK(!lease_held(&l, 0x00000800u, 2000));  /* 2304 ms later */
}

int main(void)
{
  test_never_renewed();
  test_expires();
  test_tick_wraps();
  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
