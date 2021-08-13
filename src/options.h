/*
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
#define DIAGNOSTIC_OPT		1 << 1
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
 * @param argi Index in argv of the first element not parsed by this function.
 * The argv argument is reordered so that all elements in argv from [argi, argc)
 * are elements that were not parsed.
 * @returns Possibly LEMON_ENOMEM or LEMON_EOPTION.
 ******************************************************************************/
lemon_error options_parse(options *self, int argc, char **argv, int *argi);

/*******************************************************************************
 * @fn options_display
 * @brief Print all options on stderr.
 ******************************************************************************/
void options_display(options *self);
