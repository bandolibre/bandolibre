#include "properties.h"

#include <string.h>

/* Mutable live values. The default is a named initializer in the table, so the
 * preprocessor can't build a static initializer for this struct; it is filled
 * from the descriptor defaults by property__load_defaults() (a constructor that
 * runs before main()). Published read-only through g_properties. */
static properties_t s_props;
const properties_t *const g_properties = &s_props;

/* Descriptor table, generated from the single source of truth. Sequential
 * array: the array index is the public "index" (definition order). The named
 * .factory_value/.min/.max/.description initializers ride in via __VA_ARGS__. */
static const property_desc_t g_prop_desc[] = {
/* Parameter names must avoid the designators .type/.name/.tag below, or the
 * preprocessor would rewrite e.g. ".type" into ".uint16_t". */
#define PROPERTY(ptag, ctype, pname, ...)                                      \
  { .type = PROPERTY_TYPE_ENUM(ctype), .name = #pname, .tag = (ptag),          \
    .offset = (uint16_t)offsetof(properties_t, pname),                         \
    PROPERTY_DEFAULTS(ctype) __VA_ARGS__ },
#include "property_table.def"
#undef PROPERTY
};

#define PROPERTY_COUNT (sizeof(g_prop_desc) / sizeof(g_prop_desc[0]))

/* Compile-time uniqueness of nonzero tags. Duplicate case labels are a
 * constraint violation, so two properties sharing a nonzero tag fail to
 * compile. A zero tag (transient) maps to a unique negative placeholder
 * (-1 - __COUNTER__) so multiple transient properties never collide. This
 * function is never called; it exists only to be compiled. */
__attribute__((unused))
static void property__check_unique_tags(int v)
{
  switch (v)
  {
#define PROPERTY(tg, ct, nm, ...) case ((tg) ? (int)(tg) : (-1 - __COUNTER__)):
#include "property_table.def"
#undef PROPERTY
    default: break;
  }
}

/* 0xFFFF is what an erased flash slot reads as (property_store.h). */
#define PROPERTY(tg, ct, nm, ...) \
  _Static_assert((tg) < 0xFFFFu, "tag 0xFFFF is reserved for erased flash: " #nm);
#include "property_table.def"
#undef PROPERTY

/* ---- generic field access -------------------------------------------------*/

static uint16_t read_value(const property_desc_t *d)
{
  void *field = (char *)&s_props + d->offset;
  if (d->type == PROPERTY_TYPE_BOOL) return *(bool *)field ? 1u : 0u;
  return *(uint16_t *)field;
}

static void write_value(const property_desc_t *d, uint16_t v)
{
  void *field = (char *)&s_props + d->offset;
  if (d->type == PROPERTY_TYPE_BOOL) *(bool *)field = (v != 0);
  else *(uint16_t *)field = v;
}

static uint16_t clamp(const property_desc_t *d, uint16_t v)
{
  if (v < d->min) return d->min;
  if (v > d->max) return d->max;
  return v;
}

/* ---- lifecycle ------------------------------------------------------------*/

void property_reset_all(void)
{
  for (size_t i = 0; i < PROPERTY_COUNT; i++)
    write_value(&g_prop_desc[i], property_default(i));
}

/* Populate the live values before main(): the reset handler calls
 * __libc_init_array ahead of main, which runs constructors, so g_properties is
 * valid with no explicit init call. Nothing is saved yet at that point (the
 * saved values are read by property_load_from_flash() from main_init), so
 * these are the factory values. */
__attribute__((constructor))
static void property__load_defaults(void)
{
  for (size_t i = 0; i < PROPERTY_COUNT; i++)
    write_value(&g_prop_desc[i], g_prop_desc[i].factory_value);
}

/* ---- introspection --------------------------------------------------------*/

size_t property_count(void)
{
  return PROPERTY_COUNT;
}

const property_desc_t *property_at(size_t index)
{
  return index < PROPERTY_COUNT ? &g_prop_desc[index] : NULL;
}

bool property_by_name(const char *name, size_t *out_index)
{
  if (!name) return false;
  for (size_t i = 0; i < PROPERTY_COUNT; i++)
  {
    if (strcmp(g_prop_desc[i].name, name) == 0)
    {
      if (out_index) *out_index = i;
      return true;
    }
  }
  return false;
}

/* ---- typed get / set ------------------------------------------------------*/

bool property_get_u16(size_t index, uint16_t *out)
{
  if (index >= PROPERTY_COUNT || g_prop_desc[index].type != PROPERTY_TYPE_U16) return false;
  if (out) *out = read_value(&g_prop_desc[index]);
  return true;
}

bool property_set_u16(size_t index, uint16_t value)
{
  if (index >= PROPERTY_COUNT || g_prop_desc[index].type != PROPERTY_TYPE_U16) return false;
  write_value(&g_prop_desc[index], clamp(&g_prop_desc[index], value));
  return true;
}

bool property_get_bool(size_t index, bool *out)
{
  if (index >= PROPERTY_COUNT || g_prop_desc[index].type != PROPERTY_TYPE_BOOL) return false;
  if (out) *out = read_value(&g_prop_desc[index]) != 0;
  return true;
}

bool property_set_bool(size_t index, bool value)
{
  if (index >= PROPERTY_COUNT || g_prop_desc[index].type != PROPERTY_TYPE_BOOL) return false;
  write_value(&g_prop_desc[index], value ? 1u : 0u);
  return true;
}

bool property_reset(size_t index)
{
  if (index >= PROPERTY_COUNT) return false;
  write_value(&g_prop_desc[index], property_default(index));
  return true;
}

uint16_t property_value(size_t index)
{
  return index < PROPERTY_COUNT ? read_value(&g_prop_desc[index]) : 0u;
}
