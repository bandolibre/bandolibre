/*
 * microrl configuration for the Bandolibre project.
 *
 * This file overrides third_party/microrl/src/config.h. It is force-included
 * into microrl.c by CMake (see target's COMPILE_FLAGS), and its include guard
 * matches the one in the upstream config so that the upstream file is skipped
 * when microrl.h pulls in "config.h".
 */
#ifndef _MICRORL_CONFIG_H_
#define _MICRORL_CONFIG_H_

#include "ansi.h"

#define _COMMAND_LINE_LEN 101 /* 100 chars + NUL */
#define _COMMAND_TOKEN_NMB 8
#define _PROMPT_DEFAULT ANSI_FG_GREEN "IRin >" ANSI_RESET " "
#define _PROMPT_LEN 7
#define _USE_COMPLETE
#define _USE_HISTORY
#define _RING_HISTORY_LEN 256
#define _USE_ESC_SEQ
#define _USE_LIBC_STDIO
#define _USE_CTLR_C
#define _ENABLE_INIT_PROMPT
/* The terminal sends CR on Enter (_ENDL_CR), but output must end lines with
 * CR LF: with a bare CR the Tab-completion list is printed on the prompt row
 * and the prompt redraw erases it. */
#define _ENDL_CR
#define ENDL "\r\n"

#endif
