/**
 * @file options.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options API.
 */

#pragma once

#include "lemon.h"

typedef struct options {
	uint8_t diagnostic;
	uint8_t ir;
	uint8_t machine;
} options;

//bitwise operations for reading the options struct
#define DIAGNOSTIC_ALL 		1 << 0
#define DIAGNOSTIC_FLAGS	1 << 1
#define DIAGNOSTIC_PASS		1 << 2
#define DIAGNOSTIC_TOKENS	1 << 3
#define IR_DISASSEMBLE		1 << 1
#define MACHINE_NORUN		1 << 1

/*******************************************************************************
 * @fn options_init
 * @brief Initialize an options struct to the default state.
 *******************************************************************************/
options options_init(void);

/*******************************************************************************
 * @fn options_parse
 * @brief GNU argp option parser. Must be invoked before reading from options.
 * @returns Possibly LEMON_ENOMEM.
 ******************************************************************************/
lemon_error options_parse(options *self, int argc, char **argv);

