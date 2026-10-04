#ifndef PROPERTY_FLASH_H_
#define PROPERTY_FLASH_H_

/* The Flash the property store runs on. On the board it is the two pages at
 * PROPS_BASE (property_flash.cc); host tests link a FakeFlash instead
 * (property_flash_fake.cc), which is the only reason this is a seam. */

#include "property_store.h"

Flash &property_flash();

#endif /* PROPERTY_FLASH_H_ */
