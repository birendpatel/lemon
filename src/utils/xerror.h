// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
// 
// Xerror is a suite of error handling tools. For internal compiler errors, it
// provides a logging mechanism, the Unity exception library, and exception
// codes. For user errors, it provides formatted stderr messages.

#pragma once

#include <stddef.h>

#include "CException.h"

typedef char cstring;

//------------------------------------------------------------------------------
//XerrorLog enqueues a new error message to an internal thread-safe buffer. The
//buffer will automatically flush to stderr when full, when the level is XFATAL,
//or if XERROR_DEBUG is defined. All messages are newline terminated.

__attribute__((__format__(__printf__, 4, 5)))
void XerrorLog
(
 	const cstring *file,
	const cstring *func,
	const int level,
	const cstring *txt,
	...
);

void XerrorFlush(void);

#define XTRACE 0
#define XERROR 1
#define XFATAL 2

#define xerror_trace(msg, ...) \
XerrorLog(__FILE__, __func__, XTRACE, msg, ##__VA_ARGS__)

#define xerror_issue(msg, ...) \
XerrorLog(__FILE__, __func__, XERROR, msg, ##__VA_ARGS__)

#define xerror_fatal(msg, ...) \
XerrorLog(__FILE__, __func__, XFATAL, msg, ##__VA_ARGS__)

//------------------------------------------------------------------------------
//exceptions

#define XXPARSE  ((CEXCEPTION_T) 1) // raised when grammar is ill-formed
#define XXGRAPH  ((CEXCEPTION_T) 2) // raised when generic graphing issue found
#define XXSYMBOL ((CEXCEPTION_T) 3) // raised when symbol resolution fails

//------------------------------------------------------------------------------
//source code error messages

//print coloured stderr message; does not log to the xerror internal buffer.
//prefix a line number when line > 0 and a file name when fname != NULL.
void XerrorUser
(
	const cstring *fname,
	const size_t line,
	const int level,
	const cstring *msg,
	...
);

#define XUSERERROR 0 //red
#define XUSERWARN  1 //yellow
#define XUSERHELP  2 //green

#define xuser_error(fname, line, msg, ...) \
XerrorUser(fname, line, XUSERERROR, msg, ##__VA_ARGS__)

#define xuser_warn(fname, line, msg, ...) \
XerrorUser(fname, line, XUSERWARN, msg, ##__VA_ARGS__)

#define xuser_help(fname, line, msg, ...) \
XerrorUser(fname, line, XUSERHELP, msg, ##__VA_ARGS__)
