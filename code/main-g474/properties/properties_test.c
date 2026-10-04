/* Host unit tests for the property system. Pure C, no HAL — compile and run
 * natively (see `just test`). Framework-free: a tiny assert macro that counts
 * failures and reports a final summary. */

#include "properties.h"
#include "properties_console.h"

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                     \
  do {                                                                  \
    g_checks++;                                                         \
    if (!(cond)) {                                                      \
      g_failures++;                                                     \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
    }                                                                   \
  } while (0)

/* Resolve a property index by name or abort the test if it is missing. */
static size_t idx(const char *name)
{
  size_t i = (size_t)-1;
  if (!property_by_name(name, &i)) {
    printf("FATAL: unknown property '%s'\n", name);
    g_failures++;
  }
  return i;
}

static void test_defaults(void)
{
  /* No reset first: this verifies the static initialization of the live
   * struct (values are valid before any call). Runs first in main(). */
  CHECK(property_count() == 71);

  /* Direct reads match defaults from property_table.def. */
  CHECK(g_properties->key_press == 1900);
  CHECK(g_properties->key_release == 2100);
  CHECK(g_properties->bellow_center == 28972);
  CHECK(g_properties->bellow_cc_period_ms == 5);

  /* Same value via the index API. */
  uint16_t v = 0;
  CHECK(property_get_u16(idx("key_press"), &v) && v == 1900);
}

static void test_lookup(void)
{
  size_t i;
  CHECK(property_by_name("bellow_center", &i));
  CHECK(property_at(i)->tag == 64);
  CHECK(strcmp(property_at(i)->name, "bellow_center") == 0);

  CHECK(!property_by_name("does_not_exist", &i));
  CHECK(property_at(property_count()) == NULL);
}

/* Guards against a hand-edited description silently overflowing
 * handle_get_property_description()'s sysex response buffer (midi.cc:
 * std::array<uint8_t, 200> payload) -- past that, DataWriter::write() just
 * stops writing without erroring, truncating the description with no
 * terminating NUL, which then reads back as null on the client and crashes
 * (property enumeration failed: Cannot read properties of null). Mirrors the
 * exact byte layout that handler builds: msg_type(1) + index(2) + type(1) +
 * factory_value(2) + name + NUL + description + NUL. */
static void test_description_payload_fits(void)
{
  enum { SYSEX_PAYLOAD_MAX = 200 };
  for (size_t i = 0; i < property_count(); i++)
  {
    const property_desc_t *d = property_at(i);
    size_t total = 1 + 2 + 1 + 2 + strlen(d->name) + 1 + strlen(d->description) + 1;
    if (total > SYSEX_PAYLOAD_MAX)
    {
      printf("FAIL %s:%d: property '%s' description response is %zu bytes, over the %d-byte sysex buffer\n",
             __FILE__, __LINE__, d->name, total, SYSEX_PAYLOAD_MAX);
      g_failures++;
    }
    g_checks++;
  }
}

static void test_set_clamp(void)
{
  property_reset_all();
  size_t i = idx("key_press"); /* range [0,4095] */

  CHECK(property_set_u16(i, 1234));
  CHECK(g_properties->key_press == 1234);

  CHECK(property_set_u16(i, 60000)); /* above max -> clamps to 4095 */
  CHECK(g_properties->key_press == 4095);

  size_t j = idx("bellow_cc_period_ms"); /* range [1,1000] */
  CHECK(property_set_u16(j, 0));  /* below min -> clamps to 1 */
  CHECK(g_properties->bellow_cc_period_ms == 1);

  CHECK(!property_set_u16(property_count(), 5)); /* bad index */
}

static void test_type_guards(void)
{
  property_reset_all();
  size_t i = idx("key_press"); /* a U16 property */
  bool b;
  CHECK(!property_get_bool(i, &b));      /* wrong type */
  CHECK(!property_set_bool(i, true));    /* wrong type */
}

static void test_reset(void)
{
  property_reset_all();
  size_t i = idx("key_press");
  CHECK(property_set_u16(i, 42));
  CHECK(g_properties->key_press == 42);
  CHECK(property_reset(i));
  CHECK(g_properties->key_press == 1900);

  CHECK(property_set_u16(idx("bellow_center"), 100));
  property_reset_all();
  CHECK(g_properties->bellow_center == 28972);
}

/* Host builds link property_flash_fake.cc: the store runs on RAM, and a
 * "reboot" is property_load_from_flash() again over the same fake flash. */
bool fake_flash_append(uint16_t tag, uint16_t value);

static void test_saved_value(void)
{
  /* Boot: until the store has been loaded it reports saving as disabled. */
  CHECK(property_set_saved(idx("key_press"), 1) == PROPERTY_SAVE_DISABLED);
  property_load_from_flash();
  CHECK(property_store_status().enabled);
  CHECK(property_factory_reset() == PROPERTY_SAVE_OK);
  size_t i = idx("key_press"); /* factory 1900, range [0,4095] */
  uint16_t v = 0;
  CHECK(!property_get_saved(i, &v));
  CHECK(property_default(i) == 1900);

  CHECK(property_set_saved(i, 1500) == PROPERTY_SAVE_OK);
  CHECK(property_get_saved(i, &v) && v == 1500);
  CHECK(property_default(i) == 1500);
  CHECK(g_properties->key_press == 1900); /* the live value is not touched */
  CHECK(property_reset(i));
  CHECK(g_properties->key_press == 1500); /* reset restores the saved value */

  CHECK(property_set_saved(i, 60000) == PROPERTY_SAVE_OK); /* clamped */
  CHECK(property_get_saved(i, &v) && v == 4095);

  /* Saving the factory value clears the saved value. */
  CHECK(property_set_saved(i, 1900) == PROPERTY_SAVE_OK);
  CHECK(!property_get_saved(i, &v));
  CHECK(property_default(i) == 1900);

  CHECK(property_set_saved(idx("log_bellow"), 1) == PROPERTY_SAVE_TRANSIENT);
  CHECK(property_clear_saved(idx("log_bellow")) == PROPERTY_SAVE_TRANSIENT);
  CHECK(property_set_saved(property_count(), 1) == PROPERTY_SAVE_BAD_INDEX);
}

static void test_saved_survives_reboot(void)
{
  CHECK(property_factory_reset() == PROPERTY_SAVE_OK);
  CHECK(property_set_saved(idx("bellow_center"), 25000) == PROPERTY_SAVE_OK);
  CHECK(property_set_saved(idx("midi_active_sensing_enable"), 0) == PROPERTY_SAVE_OK);
  CHECK(property_set_u16(idx("bellow_center"), 100));
  CHECK(property_set_bool(idx("log_bellow"), true));

  property_load_from_flash(); /* reboot */
  CHECK(g_properties->bellow_center == 25000);
  CHECK(g_properties->midi_active_sensing_enable == false);
  CHECK(g_properties->log_bellow == false); /* transient: factory at boot */
  CHECK(g_properties->key_press == 1900);   /* not saved: factory */

  property_store_status_t st = property_store_status();
  CHECK(st.enabled);
  CHECK(st.partition == 0);
  CHECK(st.generation == 1);
  CHECK(st.used == 2);
  CHECK(st.saved_count == 2);
}

static void test_save_all(void)
{
  CHECK(property_factory_reset() == PROPERTY_SAVE_OK);
  CHECK(property_set_u16(idx("key_press"), 1500));
  CHECK(property_set_u16(idx("bellow_center"), 20000));
  CHECK(property_set_bool(idx("log_bellow"), true)); /* transient: never saved */

  size_t n = 99;
  CHECK(property_save_all(&n) == PROPERTY_SAVE_OK);
  CHECK(n == 2);
  CHECK(property_save_all(&n) == PROPERTY_SAVE_OK);
  CHECK(n == 0); /* nothing differs from the defaults any more */
  CHECK(property_store_status().used == 2);

  /* Back to the factory value: saving clears it. */
  CHECK(property_set_u16(idx("key_press"), 1900));
  CHECK(property_save_all(&n) == PROPERTY_SAVE_OK);
  CHECK(n == 1);
  uint16_t v;
  CHECK(!property_get_saved(idx("key_press"), &v));

  property_load_from_flash();
  CHECK(g_properties->key_press == 1900);
  CHECK(g_properties->bellow_center == 20000);
  CHECK(property_store_status().saved_count == 1);
}

static void test_factory_reset(void)
{
  CHECK(property_set_saved(idx("key_release"), 2500) == PROPERTY_SAVE_OK);
  CHECK(property_set_u16(idx("key_release"), 2500));
  CHECK(property_set_bool(idx("log_bellow"), true));

  CHECK(property_factory_reset() == PROPERTY_SAVE_OK);
  CHECK(g_properties->key_release == 2100);
  CHECK(g_properties->log_bellow == false);
  CHECK(property_store_status().partition == -1);
  CHECK(property_store_status().saved_count == 0);

  property_load_from_flash();
  CHECK(g_properties->key_release == 2100);
}

static void test_load_ignores_bad_records(void)
{
  CHECK(property_factory_reset() == PROPERTY_SAVE_OK);
  CHECK(fake_flash_append(1, 1500));    /* key_press */
  CHECK(fake_flash_append(1, 9999));    /* key_press above its max: ignored */
  CHECK(fake_flash_append(1000, 1234)); /* a tag this firmware does not have */
  CHECK(fake_flash_append(64, 25000));  /* bellow_center */

  property_load_from_flash();
  CHECK(g_properties->key_press == 1500); /* the last in-range value */
  CHECK(g_properties->bellow_center == 25000);
  CHECK(property_store_status().saved_count == 2);
}

static void test_complete(void)
{
  const char *out[128];
  /* "bellow_" matches the bellow_* properties (shared center, 3 programs of
   * 11, the program selector, hysteresis, CC, settle, sample rate, 1-euro
   * filter) */
  CHECK(properties_complete("bellow_", out, 128) == 41);
  /* "key_" matches the two key_* properties, not keyboard_tuning */
  CHECK(properties_complete("key_", out, 128) == 2);
  /* empty prefix matches all (buffer is sized above property_count()) */
  CHECK(properties_complete("", out, 128) == property_count());
  /* cap is honored */
  CHECK(properties_complete("", out, 3) == 3);
  /* no match */
  CHECK(properties_complete("nope", out, 128) == 0);
}

int main(void)
{
  test_defaults();
  test_lookup();
  test_description_payload_fits();
  test_set_clamp();
  test_type_guards();
  test_reset();
  test_saved_value();
  test_saved_survives_reboot();
  test_save_all();
  test_factory_reset();
  test_load_ignores_bad_records();
  test_complete();

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
