// ANSI/VT100 escape sequences emitted by the console and its live dashboard.
// CSI is the Control Sequence Introducer; only the subset the firmware actually
// uses is defined here. _FMT macros are printf format fragments whose argument
// is a row, column or count.
#ifndef CONSOLE_ANSI_H
#define CONSOLE_ANSI_H

#define ANSI_ESC  "\033"
#define ANSI_CSI  ANSI_ESC "["

// Erasing
#define ANSI_CLEAR_LINE_END   ANSI_CSI "K"        // erase from cursor to end of line

// Cursor visibility and save/restore
#define ANSI_CURSOR_HIDE      ANSI_CSI "?25l"
#define ANSI_CURSOR_SHOW      ANSI_CSI "?25h"
#define ANSI_CURSOR_SAVE      ANSI_CSI "s"
#define ANSI_CURSOR_RESTORE   ANSI_CSI "u"

// Cursor movement
#define ANSI_CURSOR_HOME      ANSI_CSI "H"         // move to row 1, column 1
#define ANSI_CURSOR_LEFT_FMT  ANSI_CSI "%dD"       // move cursor left N columns
#define ANSI_CURSOR_ROW_FMT   ANSI_CSI "%d;1H"     // move to row N, column 1
#define ANSI_CURSOR_BOTTOM    ANSI_CSI "999;1H"    // clamp to last row, column 1

// DECSTBM scroll region (omitted bottom param => terminal's last line)
#define ANSI_SCROLL_TOP_FMT   ANSI_CSI "%d;r"      // region = rows N..bottom
#define ANSI_SCROLL_FULL      ANSI_CSI "r"         // release region to full screen

// Colors (SGR)
#define ANSI_RESET            ANSI_CSI "0m"
#define ANSI_FG_GREEN         ANSI_CSI "32m"
#define ANSI_BOLD             ANSI_CSI "1m"
#define ANSI_FG_GREY          ANSI_CSI "90m"       // bright black, for de-emphasized text
#define ANSI_BG_GREY236       ANSI_CSI "48;5;236m" // 256-color palette, dark grey
#define ANSI_BG_RGB_FMT       ANSI_CSI "48;2;%d;%d;%dm" // 24-bit background; args are r, g, b

#endif
