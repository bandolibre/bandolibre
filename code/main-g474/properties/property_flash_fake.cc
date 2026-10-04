/* Host builds: the property store runs on a RAM-backed FakeFlash. */
#include "fake_flash.h"
#include "property_flash.h"

FakeFlash g_fake_flash;

Flash &property_flash()
{
  return g_fake_flash;
}

/* For properties_test.c (C): append a raw SET record behind the cache's back,
 * as an older firmware with other tags or bounds could have left it. */
extern "C" bool fake_flash_append(uint16_t tag, uint16_t value)
{
  PropertyStore s(g_fake_flash, gsl::span<const uint16_t>());
  s.init();
  return s.append(StoreRecord::make(tag, StoreKind::Set, value)) == PropertyStore::Result::Ok;
}
