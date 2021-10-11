// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
//
//
//                Thread   Level  File      Line
//                   |       |     |          |
//                   |       |     |          |
//                   |       |     |          |
//               (1234567) TRACE main.c:main:123 hello, world!
//                                       |       \___________/
//                                       |             |
//                                       |             |
//                                      Func         Message
//
// Xerror is a suite of tools. For compiler errors, xerror provide a logging
// mechanism, error codes, and exceptions. For user errors, xerror provides
// formatting capabilities for error messages.

#pragma once

#include <stddef.h>

#include "../extern/cexception/CException.h"
#include "lib/str.h"

//xerror is guaranteed to always alias int for glibc and backwards compatibility
typedef int xerror;

//this function enqueues a new error message to an internal thread-safe buffer.
//the buffer will automatically flush when full or when the level is XFATAL. All
//messages are guaranteed to be newline terminated.
__attribute__((__format__(__printf__, 5, 6)))
void XerrorLog
(
	const char *file,
	const char *func,
	const int line,
	const int level,
	const char *msg,
	...
);

#define XFATAL 0

#define xerror_fatal(msg, ...) \
XerrorLog(__FILE__, __func__, __LINE__, XFATAL, msg, ##__VA_ARGS__)

#define XERROR 1

#define xerror_issue(msg, ...) \
XerrorLog(__FILE__, __func__, __LINE__, XERROR, msg, ##__VA_ARGS__)

#define XTRACE 2

#define xerror_trace(msg, ...) \
XerrorLog(__FILE__, __func__, __LINE__, XTRACE, msg, ##__VA_ARGS__)

//manual full flush
void XerrorFlush(void);

//error codes
#define XESUCCESS     0 //function returned successfully
#define XENOMEM	      1 //dynamic allocation failed
#define XEOPTION      2 //options parsing failed
#define XEFULL        3 //data structure is at capacity
#define XEFILE        4 //IO failure
#define XEBUSY        5 //thread is waiting on a condition
#define XECLOSED      6 //communication channel is closed
#define XETHREAD      7 //multithreading issue has occured
#define XESHELL	      8 //shell error
#define XEPARSE	      9 //parsing to AST failed
#define XEUNDEFINED  10 //unspecified error

//code map for lib/channel.h
#define CHANNEL_ESUCCESS XESUCCESS
#define CHANNEL_EBUSY	 XEBUSY
#define CHANNEL_ECLOSED  XECLOSED

const char *XerrorDescription(const xerror err);

//exceptions
#define XXPARSE ((CEXCEPTION_T) 1) // cannot parse the current token

//display an error message to stderr in red font; does not log to xerror buffer
//if line is > 0 then the message is prefixed with "line #:"
void XerrorUser(const size_t line, const string msg, ...); 

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
