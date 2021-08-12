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

#ifndef __GNUC__
    #error "Lemon requires a GNU C compiler"
#endif

//all source files under strict purview of CLemon (e.g., not /src/lib or /extern)
//use lemon_error and its associated codes.
typedef int lemon_error;

//error codes
#define LEMON_ESUCCESS 0 /**< @brief Function terminated successfully. */
#define LEMON_ENONMEM  1 /**< @brief Dynamic allocation failed. */
