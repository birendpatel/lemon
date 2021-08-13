/**
 * @file options.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options API.
 */

#pragma once

#include "lemon.h"

//options handle
typedef struct options options;

//No Doxygen, see options.c info array.
typedef enum options_code {
	DIAGNOSTIC_ALL,
	DIAGNOSTIC_FLAGS,
	DIAGNOSTIC_PASS,
	DIAGNOSTIC_TOKENS,
	MACHINE_BYTECODE,
	OPTIONS_COUNT
} options_code;

/*******************************************************************************
 * @fn options_init
 * @brief GNU argp option parser.
 * @details Options will be heap allocated. Call options_free when done.
 * @returns Possibly LEMON_ENOMEM.
 ******************************************************************************/
lemon_error options_init(options **self, int argc, char **argv);

/*******************************************************************************
 * @fn options_read
 * @brief Return the status of an option.
 * @returns LEMON_ERANGE if invalid option.
 ******************************************************************************/
bool options_read(options *self, options_code optcode);

/*******************************************************************************
 * @fn options_free
 ******************************************************************************/
void options_free(options *self);
