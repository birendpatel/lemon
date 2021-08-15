/**
 * @file lemon.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Common header for Lemon source code.
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LEMON_VERSION "3.1.0.2 (alpha)"

#ifndef __GNUC__
    #error "Lemon requires a GNU C compiler"
#endif

//all source files beloning to  CLemon (e.g., not /src/lib or /extern)
//use lemon_error and its associated codes. lemon_error is for errors
//not caused by the user, e.g., syscalls.
typedef int lemon_error;

//error codes
#define LEMON_ESUCCESS 0 /**< @brief Function terminated successfully. */
#define LEMON_ENOMEM   1 /**< @brief Dynamic allocation failed. */
#define LEMON_EOPTION  2 /**< @brief Options parsing failed. */
#define LEMON_EFULL    3 /**< @brief A dynamic container is at capacity */
#define LEMON_EFILE    4 /**< @brief Stdio file error */
#define LEMON_EUNDEF   5 /**< @brief Undefined error code for initialization */

//vector error codes mapped to lemon error codes.
//source files are responsible for including ./lib/vector.h
#define VECTOR_ESUCCESS LEMON_ESUCCESS
#define VECTOR_ENOMEM LEMON_ENOMEM
#define VECTOR_EFULL LEMON_EFULL

//error code verbose fetch
const char *lemon_describe(lemon_error err);

//formatted stderr reporting.
//source files are responsible for includes stdio.h
#define FMTSTR_STDERR "error: %s: %s: %d: %s\n"
#define INFO_STDERR __FILE__, __func__, __LINE__
#define REPORT_STDERR(msg) fprintf(stderr, FMTSTR_STDERR, INFO_STDERR, (msg))

//assess an error code, report it, and return a new code.
//only applicable where cleanup is not required (or RAII is in effect).
#define KILL 1
#define NOKILL 0

#define REPORT_ERROR(expr, msg, kill, ret) 				       \
	do {								       \
		if ((expr)) {						       \
			REPORT_STDERR(msg);				       \
									       \
			if (kill) {					       \
				exit(ret);				       \
			} else {					       \
				return ret;				       \
			}						       \
		}							       \
	} while (0)

//allocation helpers
#define KILOBYTE ((size_t) 1024)
