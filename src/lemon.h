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

#define LEMON_VERSION "3.0.0.2 (alpha)"

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
#define LEMON_EUNDEF   3 /**< @brief Undefined error code for initialization */

//error code verbose fetch
const char *lemon_describe(lemon_error err);

//allocation helpers
#define KILOBYTE ((size_t) 1024)
