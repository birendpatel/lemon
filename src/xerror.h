// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
// Error handing API for all compiler and user errors.
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
//
// The xerror logger stores messages in an internal buffer and flushes to stderr
// when full or when fatal messages are passed. If the macro XERROR_DEBUG is
// defined, then all log levels on all log calls will trigger an immediate
// flush.

#pragma once

#include "../extern/cexception/CException.h"

//the API guarantees for backwards and C library compatibility the xerror type
//will always be an alias for the int type.
typedef int xerror;

//This function will automatically flush its internal buffer to stderr whenever
//it is full or when the level is XFATAL. All messages are newline terminated.
__attribute__((__format__(__printf__, 5, 6)))
void __xerror_log
(
	const char *file,
	const char *func,
	const int line,
	const int level,
	const char *msg,
	...
);

#define XFATAL 0

#define xerror_fatal(msg, ...) 				               \
__xerror_log(__FILE__, __func__, __LINE__, XFATAL, msg, ##__VA_ARGS__)

#define XERROR 1

#define xerror_issue(msg, ...) 				               \
__xerror_log(__FILE__, __func__, __LINE__, XERROR, msg, ##__VA_ARGS__)

#define XTRACE 2

#define xerror_trace(msg, ...) 				               \
__xerror_log(__FILE__, __func__, __LINE__, XTRACE, msg, ##__VA_ARGS__)

//manually flush the internal xerror buffer.
void xerror_flush(void);

//error codes
#define XESUCCESS     0 //function returned successfully
#define XENOMEM	      1 //allocation in 3rd party library failed
#define XEOPTION      2 //options parsing failed
#define XEFULL        3 //container is at capacity
#define XEFILE        4 //IO failure
#define XEBUSY        5 //a thread is waiting on a condition
#define XECLOSED      6 //attempted to use a closed channel
#define XETHREAD      7 //multithreading issue has occured
#define XEUNDEFINED   8 //generic unspecified error has occured

//mapping between channel codes and xerror codes
#define CHANNEL_ESUCCESS XESUCCESS
#define CHANNEL_EBUSY	 XEBUSY
#define CHANNEL_ECLOSED  XECLOSED

const char *xerror_str(const xerror err);

//exceptions
#define XXPARSE ((CEXCEPTION_T) 1) // cannot parse the current token

//colour macros provided by @gon1332 at stackoverflow.com/questions/2616906/
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

	#define BOLD(srt) 	"\x1B[1m" str COLOUR_RESET
	#define UNDERLINE(x) 	"\x1B[4m" str COLOUR_RESET
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
