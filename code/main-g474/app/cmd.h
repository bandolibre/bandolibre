#ifndef APP_CONSOLE_H_
#define APP_CONSOLE_H_

/* Console command dispatcher and tab-completion for application commands.
 * Handles: hello, midi, dfu, bellow_tune, help, properties (show/get/set/reset). */

/* Executes a console command given argc/argv. Returns 0. */
int console_execute(int argc, const char *const *argv);

/* Tab-completion callback for microrl. Returns a NULL-terminated array of
 * matching command/argument completions up to CONSOLE_COMPL_MAX. */
char **console_complete(int argc, const char *const *argv);

#endif /* APP_CONSOLE_H_ */
