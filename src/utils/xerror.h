// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
//
// Xerror is a suite of tools. For compiler errors, xerror provide a logging
// mechanism, error codes, and exceptions. For user errors, xerror provides
// formatting capabilities for error messages.

#pragma once

#include <stddef.h>

#include "CException.h"

typedef char cstring;

//------------------------------------------------------------------------------
//XerrorLog enqueues a new error message to an internal thread-safe buffer. The
//buffer will automatically flush to stderr when full, when the level is XFATAL,
//or if XERROR_DEBUG is defined. All messages are newline terminated.

__attribute__((__format__(__printf__, 3, 4)))
void XerrorLog(const cstring *func, const int level, const cstring *msg, ...);

#define XFATAL 0
#define XERROR 1
#define XTRACE 2

#define xerror_fatal(msg, ...) \
XerrorLog(__func__, XFATAL, msg, ##__VA_ARGS__)

#define xerror_issue(msg, ...) \
XerrorLog(__func__, XERROR, msg, ##__VA_ARGS__)

#define xerror_trace(msg, ...) \
XerrorLog(__func__, XTRACE, msg, ##__VA_ARGS__)

//manual stderr flush
void XerrorFlush(void);

//------------------------------------------------------------------------------
//exceptions

#define XXPARSE  ((CEXCEPTION_T) 1) // raised when grammar is ill-formed
#define XXGRAPH  ((CEXCEPTION_T) 2) // raised when generic graphing issue found
#define XXSYMBOL ((CEXCEPTION_T) 3) // raised when symbol resolution fails

//------------------------------------------------------------------------------
//source code error messages

//print a stderr message in red font; does not log to the internal buffer. 
//Prefix a line number when ln > 0 and a file name when fname != NULL.
void XerrorUser(const cstring *fname, const size_t ln, const cstring *msg, ...);

//same actions as XerrorUser but prints in yellow font.
void XwarnUser(const cstring *fname, const size_t ln, const cstring *msg, ...);

//same actions as XerrorUser but prints in green font
void XhelpUser(const cstring *fname, const size_t ln, const cstring *msg, ...);

//colours provided by @gon1332 at stackoverflow.com/questions/2616906/
#ifdef COLOURS
	#define COLOUR_RESET	"\x1B[0m"
	#define ANSI_RED  	"\x1B[31m"
	#define ANSI_GREEN  	"\x1B[32m"
	#define ANSI_YELLOW 	"\x1B[33m"
	#define ANSI_BLUE  	"\x1B[34m"
	#define ANSI_MAGENTA	"\x1B[35m"
	#define ANSI_CYAN  	"\x1B[36m"
	#define ANSI_WHITE      "\x1B[37m"

	#define RED(str)	(ANSI_RED str COLOUR_RESET)
	#define GREEN(str)	(ANSI_GREEN str COLOUR_RESET)
	#define YELLOW(str)	(ANSI_YELLOW str COLOUR_RESET)
	#define BLUE(str)	(ANSI_BLUE str COLOUR_RESET)
	#define MAGENTA(str)	(ANSI_MAGENTA str COLOUR_RESET)
	#define CYAN(str)	(ANSI_CYAN str COLOUR_RESET)
	#define WHITE(str)	(ANSI_WHITE str COLOUR_RESET)

	#define BOLD(str) 	"\x1B[1m" str COLOUR_RESET
	#define UNDERLINE(str) 	"\x1B[4m" str COLOUR_RESET
#else
	#define COLOUR_RESET
	#define ANSI_RED
	#define ANSI_GREEN
	#define ANSI_YELLOW
	#define ANSI_BLUE
	#define ANSI_MAGENTA
	#define ANSI_CYAN
	#define ANSI_WHITE

	#define RED(str) str
	#define GREEN(str) str
	#define YELLOW(str) str
	#define BLUE(str) str
	#define MAGENTA(str) str
	#define CYAN(str) str
	#define WHITE(str) str

	#define BOLD(str) str
	#define UNDERLINE(str) str

#endif
