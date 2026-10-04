#ifndef PROPERTIES_H_
#define PROPERTIES_H_

/* Global property system.
 *
 * Every tunable value is declared once in property_table.def. From that single
 * list we generate a struct of live values (read directly via g_properties) and
 * an enumerable descriptor table that drives bound-checked writes and, later, a
 * console / MIDI 2.0 Property Exchange editor.
 *
 * Reads: g_properties->kb_press        (direct, no function call, read-only)
 * Writes / introspection: the index-keyed API below.
 *
 * The live values are loaded from their factory values by a constructor before
 * main() runs, so they are valid with no explicit init call. Live values are
 * RAM only. A persistent property (nonzero tag) can also have a saved value in
 * flash, set from the web tool; property_load_from_flash() applies the saved
 * values at boot. A property's default is its saved value if it has one, else
 * its factory value: that is what reset restores. The flash format is in
 * property_store.h; the saved-value API is implemented in property_persist.cc. */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROPERTY_TAG_NONE 0u   /* tag value marking a transient (non-persistent) property */

typedef enum {
  PROPERTY_TYPE_BOOL = 0,
  PROPERTY_TYPE_U16  = 1,
} property_type_t;

/* Map the C type token used in property_table.def to its property_type_t. The
 * `bool` token expands to `_Bool` (stdbool.h) before it reaches the paste, so
 * the bool mapping is keyed on `_Bool`. */
#define PROPERTY_TYPE_ENUM_uint16_t PROPERTY_TYPE_U16
#define PROPERTY_TYPE_ENUM__Bool    PROPERTY_TYPE_BOOL
#define PROPERTY_TYPE_ENUM(type) PROPERTY_TYPE_ENUM_##type

/* Per-type metadata defaults, expanded ahead of the table's named initializers
 * so a property can omit them. bool has a fixed 0..1 range, so bool properties
 * leave out .min/.max; uint16_t supplies its own. Keyed on the post-expansion
 * type token (`bool` -> `_Bool`). */
#define PROPERTY_DEFAULTS_uint16_t
#define PROPERTY_DEFAULTS__Bool      .min = 0, .max = 1,
#define PROPERTY_DEFAULTS(type) PROPERTY_DEFAULTS_##type

/* Live values. The type token in the table IS the C type, so each field
 * declares directly. Read through g_properties (the read-only view below). */
typedef struct {
#define PROPERTY(tag, type, name, ...) type name;
#include "property_table.def"
#undef PROPERTY
} properties_t;

typedef struct {
  property_type_t  type;
  const char      *name;          /* == field name (stringized) */
  const char      *description;
  uint16_t         tag;           /* 0 = transient; else unique permanent flash id */
  uint16_t         factory_value; /* the firmware's value, before any saved value */
  uint16_t         min;           /* inclusive */
  uint16_t         max;           /* inclusive */
  uint16_t         offset;        /* offsetof(properties_t, field) */
} property_desc_t;

/* Read-only view of the live values: g_properties->kb_press compiles, but
 * assigning through it does not. Writes go through the API below. */
extern const properties_t *const g_properties;

void property_reset_all(void);  /* restore every property to its default (saved ?? factory) */

/* Reflexive / editor API, addressed by a dense index in [0, property_count()).
 * The index is NOT stable across software versions (removing a property
 * reindexes the rest): obtain it via property_at() or property_by_name(),
 * never hardcode or persist it. */
size_t                 property_count(void);
const property_desc_t *property_at(size_t index);          /* NULL if out of range */
bool                   property_by_name(const char *name, size_t *out_index);

bool property_get_u16(size_t index, uint16_t *out);        /* false on bad index/type */
bool property_set_u16(size_t index, uint16_t value);       /* clamps to [min,max] */
bool property_get_bool(size_t index, bool *out);
bool property_set_bool(size_t index, bool value);
bool property_reset(size_t index);                         /* one -> default */
uint16_t property_value(size_t index);                     /* live value of any type (bool 0/1);
                                                            * 0 on bad index */

/* ---- saved values (property_persist.cc) ---------------------------------- */

typedef enum {
  PROPERTY_SAVE_OK = 0,
  PROPERTY_SAVE_BAD_INDEX,
  PROPERTY_SAVE_TRANSIENT,     /* tag 0: this property is never saved */
  PROPERTY_SAVE_FLASH_ERROR,
  PROPERTY_SAVE_DISABLED,      /* flash not in 2 KB pages (DBANK=0): saving is off */
} property_save_result_t;

typedef struct {
  bool     enabled;
  int8_t   partition;          /* active partition 0 (A) or 1 (B), -1 if nothing saved */
  uint16_t generation;         /* 0 if nothing saved */
  uint16_t used;               /* record slots used in the active partition */
  uint16_t capacity;           /* record slots per partition */
  uint16_t saved_count;        /* properties that have a saved value */
} property_store_status_t;

/* Apply the saved values at boot (once, from main_init) and print one console
 * line with the store's state. */
void property_load_from_flash(void);

bool     property_get_saved(size_t index, uint16_t *out);  /* false if none (or bad index) */
uint16_t property_default(size_t index);                   /* saved ?? factory; 0 on bad index */

/* Write the saved value, clamped to [min,max]. A value equal to the factory
 * value clears the saved value instead; an unchanged value writes nothing.
 * The live value is not touched. */
property_save_result_t property_set_saved(size_t index, uint16_t value);
property_save_result_t property_clear_saved(size_t index);

/* Save the live value of every persistent property whose live value differs
 * from its default. *changed (may be NULL) counts the properties written. */
property_save_result_t property_save_all(size_t *changed);

/* Forget every saved value (erase the store) and restore every property,
 * transient ones included, to its factory value. */
property_save_result_t property_factory_reset(void);

property_store_status_t property_store_status(void);

#ifdef __cplusplus
}
#endif

#endif /* PROPERTIES_H_ */
