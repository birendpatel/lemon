/**
 * @file lemon.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Common header for Lemon source code, featuring commonly used system
 * headers, internal error handling, compiler checks, and misc definitions.
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LEMON_VERSION "4.0.0.0 (alpha)"

#ifndef __GNUC__
    #error "Lemon requires a GNU C compiler"
#endif

/*******************************************************************************
 * @typedef lemon_error
 * @brief A lemon error is an integer-type code used by all C Lemon source files
 * for handling internal errors such as syscall or vector failures. This class
 * of errors is not concerned with error's in the user's input source code.
 ******************************************************************************/
typedef int lemon_error;

#define LEMON_ESUCCESS 0 /**< @brief Function terminated successfully. */
#define LEMON_ENOMEM   1 /**< @brief Dynamic allocation failed. */
#define LEMON_EOPTION  2 /**< @brief Options parsing failed. */
#define LEMON_EFULL    3 /**< @brief A dynamic container is at capacity */
#define LEMON_EFILE    4 /**< @brief Stdio file error */
#define LEMON_EUNDEF   5 /**< @brief Undefined error code for initialization */

/* mapping between ./lib/vector.h codes and C Lemon. */
#define VECTOR_ESUCCESS LEMON_ESUCCESS
#define VECTOR_ENOMEM 	LEMON_ENOMEM
#define VECTOR_EFULL 	LEMON_EFULL

/*******************************************************************************
 * @fn lemon_describe
 * @brief This function is the lemon_error equivalent to the standard library
 * strerror call for errno. 
 ******************************************************************************/
const char *lemon_describe(lemon_error err);

/* helper macros for EXIT_ERROR and RETURN_ERROR handlers */
#define FMTSTR_STDERR "error: %s: %s: %d: %s\n"

#define FMTSTR_STDERR_HEAD "error: %s: %s: %d: %s: %s\n"

#define INFO_STDERR __FILE__, __func__, __LINE__

#define REPORT_STDERR(msg) 						       \
	fprintf(stderr, FMTSTR_STDERR, INFO_STDERR, (msg))

#define REPORT_STDERR_HEAD(head, msg) 					       \
	fprintf(stderr, FMTSTR_STDERR_HEAD, INFO_STDERR, (head), (msg))

/*******************************************************************************
 * @def EXIT_ERROR
 * @brief Report an error on stderr and exit the program.
 * @param expr If expr evaluates to true, then error and exit are triggered.
 * @param msg Sent to stderr with line info.
 * @param val Value to send in exit condition.
 ******************************************************************************/
#define EXIT_ERROR(expr, msg, val) 				       	       \
	do {								       \
		if ((expr)) {						       \
			REPORT_STDERR(msg);				       \
			exit(val);					       \
		}							       \
	} while (0)

/*******************************************************************************
 * @RETURN_ERROR
 * @brief Report an error on stderr and return to the callee. See EXIT_ERROR.
 ******************************************************************************/
#define RETURN_ERROR(expr, msg, val) 				               \
	do {								       \
		if ((expr)) {						       \
			REPORT_STDERR(msg);				       \
			return val;					       \
		}							       \
	} while (0)

/*******************************************************************************
 * @def RETURN_ERROR_HEAD
 * @brief Report an error on stderr with a message header and then return to
 * the callee. See RETURN_ERROR.
 * @details This macro is implemented to help provide extra information on
 * syscall errors, such as the filename. __VA_ARGS__ and vfprinf are a  bit too
 * overboard for Lemon's present use cases.
 ******************************************************************************/
#define RETURN_ERROR_HEAD(expr, head, msg, val) 			       \
	do {								       \
		if ((expr)) {						       \
			REPORT_STDERR_HEAD(head, msg);			       \
			return val;					       \
		}							       \
	} while (0)

/* buffer and allocation definitions */
#define KiB(x) ((size_t) (x) * (size_t) 1024)
#define MiB(x) ((size_t) (x) * (size_t)  1048576)
