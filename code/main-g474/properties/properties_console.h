#ifndef PROPERTIES_CONSOLE_H_
#define PROPERTIES_CONSOLE_H_

/* Console / microrl command layer over the property system. Kept separate from
 * properties.c so that core stays printf/stdlib-free; these helpers print via
 * printf (\r\n line endings) and parse argv tokens. */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* argv is the usual token array with argv[0] = the command word. If argv[0] is
 * one of the property commands below it is executed and true is returned;
 * otherwise false is returned so the caller can keep dispatching. A
 * recognised-but-malformed command prints usage and still returns true.
 * Names may glob ('*'/'?'), e.g. `get log_*` addresses a whole family.
 *
 *   show                 list all properties with current value
 *   get   <name>         show value, min, max and default
 *   set   <name> <value> set a property, clamped to [min,max]
 *   reset <name>         restore default(s)
 */
bool   properties_execute(int argc, const char *const *argv);

/* With pattern NULL, lists the property command usage only (the full property
 * table is not printed here; use 'show' or 'help <pattern>' for that). With a
 * pattern, lists the properties whose name matches it instead (pattern may
 * glob, '*'/'?'). */
void   properties_help(const char *pattern);

/* Fill out[] with up to cap property names starting with prefix (prefix may be
 * NULL/"" to match all); returns the count. Backs a completion callback. */
size_t properties_complete(const char *prefix, const char **out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* PROPERTIES_CONSOLE_H_ */
