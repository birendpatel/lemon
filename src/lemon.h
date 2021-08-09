/*
 * @file lemon.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Commons header for Lemon source code.
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef __GNUC__
    #error "Lemon requires a GNU C compiler"
#endif

//error codes
#define LEMON_ESUCCESS	0	/** Function terminated successfully. */
#define LEMON_ENOMEM	1	/** Cannot allocate memory. */
#define LEMON_ENULLPTR	2	/** Null pointer received but not allowed. */
#define LEMON_UNDEFINED	999	/** Internal compiler bug, please report. */
