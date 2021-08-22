/**
 * @file xerror.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Error handling API.
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
 * @fn __xerror_report
 * @brief Log a verbose error description.
 * @details This function logs the description to an in-memory buffer but it
 * does not automatically flush the description to an IO channel unless the
 * buffer is full. The buffer size, its storage, and its mechanism are opaque.
 ******************************************************************************/
__attribute__((__format__(__printf__, 5, 6)))
void __xerror_report
(
	const char *file,
	const char *func,
	const int line,
	const int level,
	const char *msg,
	...
);

/*******************************************************************************
 * @def xerror_report
 * @brief Convenience wrapper over __xerror_report.
 ******************************************************************************/
#define xerror_report(level, msg, ...) 				               \
__xerror_report(__FILE__, __func__, __LINE__, level, msg, ##__VA_ARGS__)

/*******************************************************************************
 * @fn xerror_flush
 * @brief Flush the xerror buffer to stderr.
 * @details The xerror buffer will automatically flush when full. Invoke this
 * function when the errors must be delivered and they must be delivered NOW.
 ******************************************************************************/
void xerror_flush(void);

/*******************************************************************************
 * @fn xerror_str
 * @brief Provides an error description given a suitable API error code.
 * @details This function is constructivist. If the user provides an invalid
 * error code, a dummy description will be returned.
 ******************************************************************************/
const char *xerror_str(xerror err);

//level codes
#define XFATAL 0
#define XERROR 1
#define XINFO  2

//error codes
#define XESUCCESS     0 /**< @brief Function returned successfully. */
#define XENOMEM       1 /**< @brief Dynamic allocation failed. */
#define XEOPTION      2 /**< @brief Options parsing failed. */
#define XEFULL        3 /**< @brief A container is at capacity. */
#define XEFILE        4 /**< @brief IO failure. */
#define XEBUSY        5 /**< @brief A thread is waiting on a condition. */
#define XECLOSED      6 /**< @brief Attempted to use a closed channel. */
#define XETHREAD      7 /**< @brief A Multithreading issue has occured. */
#define XEUNDEFINED 999 /**< @brief A generic unspecified error has occured. */

//mapping between vector codes and xerror codes
#define VECTOR_ESUCCESS XESUCCESS
#define VECTOR_ENOMEM	XENOMEM
#define VECTOR_EFULL	XEFULL

//mapping between channel codes and xerror codes
#define CHANNEL_ESUCCESS XESUCCESS
#define CHANNEL_ENOMEM	 XENOMEM
#define CHANNEL_EBUSY	 XEBUSY
#define CHANNEL_ECLOSED  XECLOSED
