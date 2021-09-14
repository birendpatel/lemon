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
#define XENOMEM       1 /**< @brief Dynamic allocation failed. */
#define XEOPTION      2 /**< @brief Options parsing failed. */
#define XEFULL        3 /**< @brief A container is at capacity. */
#define XEFILE        4 /**< @brief IO failure. */
#define XEBUSY        5 /**< @brief A thread is waiting on a condition. */
#define XECLOSED      6 /**< @brief Attempted to use a closed channel. */
#define XETHREAD      7 /**< @brief A Multithreading issue has occured. */
#define XEPARSE	      8 /**< @brief AST parse error */
#define XEUNDEFINED   9 /**< @brief A generic unspecified error has occured. */

//mapping between channel codes and xerror codes
#define CHANNEL_ESUCCESS XESUCCESS
#define CHANNEL_ENOMEM	 XENOMEM
#define CHANNEL_EBUSY	 XEBUSY
#define CHANNEL_ECLOSED  XECLOSED
