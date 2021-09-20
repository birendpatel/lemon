/**
 * @file xerror.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Error handling API.
 * @details
 *
 *
 *                Thread   Level  File      Line
 *                   |       |     |          |
 *                   |       |     |          |
 *                   |       |     |          |
 *               (1234567) TRACE main.c:main:123 hello, world!
 *                                       |       \___________/
 *                                       |             |
 *                                       |             |
 *                                      Func         Message
 *
 *
 * The xerror logger stores messages in an internal buffer and flushes to stderr
 * when full or when fatal messages are passed. If the macro XERROR_DEBUG is
 * defined, then all log levels on all log calls will trigger an immediate
 * flush.
 */

#pragma once

#include "../extern/cexception/CException.h"

/*******************************************************************************
 * @typedef xerror
 * @brief Error codes used in C Lemon for handling errors such as syscalls. Note
 * that the API guarantees for backwards and C library compatibility that the
 * error type will always be an alias for the int type.
 ******************************************************************************/
typedef int xerror;

/*******************************************************************************
 * @fn __xerror_log
 * @brief Log a message to an in-memory buffer.
 * @details This function will automatically flush its buffer to stderr when
 * full or when the level is XFATAL. All messages are guaranteed to be newline
 * terminated.
 ******************************************************************************/
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

//level codes
#define XFATAL 0
#define XERROR 1
#define XTRACE 2

/*******************************************************************************
 * @def xerror_fatal
 * @brief Convenience wrapper over __xerror_log for level XFATAL. This triggers
 * a buffer IO flush.
 ******************************************************************************/
#define xerror_fatal(msg, ...) 				               \
__xerror_log(__FILE__, __func__, __LINE__, XFATAL, msg, ##__VA_ARGS__)

/*******************************************************************************
* @def xerror_issue
* @brief Convenience wrapper over __xerror_log for level XERROR
******************************************************************************/
#define xerror_issue(msg, ...) 				               \
__xerror_log(__FILE__, __func__, __LINE__, XERROR, msg, ##__VA_ARGS__)

/*******************************************************************************
 * @def xerror_trace
 * @brief Convenience wrapper over __xerror_log for level XTRACE
 ******************************************************************************/
#define xerror_trace(msg, ...) 				               \
__xerror_log(__FILE__, __func__, __LINE__, XTRACE, msg, ##__VA_ARGS__)

/*******************************************************************************
 * @fn xerror_flush
 * @brief Manually flush the xerror buffer to stderr.
 * @details The xerror buffer will automatically flush when full or when the
 * __xerror_report function is invoked with a XFATAL level. You might want to
 * manually flush before running a tight loop or calling time-sensitive code.
 ******************************************************************************/
void xerror_flush(void);

/*******************************************************************************
 * @fn xerror_str
 * @brief Provides an error description given a suitable API error code.
 * @details This function is constructivist. If the user provides an invalid
 * error code, a dummy description will be returned.
 ******************************************************************************/
const char *xerror_str(const xerror err);

//error codes
#define XESUCCESS     0 /**< @brief Function returned successfully. */
#define XENOMEM	      1 /**< @brief Allocation in 3rd party library failed. */
#define XEOPTION      2 /**< @brief Options parsing failed. */
#define XEFULL        3 /**< @brief A container is at capacity. */
#define XEFILE        4 /**< @brief IO failure. */
#define XEBUSY        5 /**< @brief A thread is waiting on a condition. */
#define XECLOSED      6 /**< @brief Attempted to use a closed channel. */
#define XETHREAD      7 /**< @brief A Multithreading issue has occured. */
#define XEUNDEFINED   8 /**< @brief A generic unspecified error has occured. */

//mapping between channel codes and xerror codes
#define CHANNEL_ESUCCESS XESUCCESS
#define CHANNEL_EBUSY	 XEBUSY
#define CHANNEL_ECLOSED  XECLOSED

//exceptions
#define XXPARSE ((CEXCEPTION_T) 1) /**< @brief Cannot parse current token */

//colour macros provided by @gon1332 at stackoverflow.com/questions/2616906/
//note several changes: 1) macro names are expanded 2) no-op wrapper where
//the macro must be defined on the makefile (so that we don't have to check
//the terminfo database on behalf of the user).
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
