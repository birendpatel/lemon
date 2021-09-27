// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
// API for the command line options handler

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "xerror.h"

//all options are represented as bit flags within some category
typedef struct options {
	uint8_t diagnostic;
	uint8_t ir;
	uint8_t machine;
	uint8_t user;
} options;

//bit flag positions within the options struct
#define DIAGNOSTIC_ALL 		1 << 0
#define DIAGNOSTIC_OPT		1 << 1
#define DIAGNOSTIC_PASS		1 << 2
#define DIAGNOSTIC_TOKENS	1 << 3
#define DIAGNOSTIC_THREAD	1 << 4
#define IR_DISASSEMBLE		1 << 1
#define MACHINE_NORUN		1 << 1
#define USER_INTERACTIVE	1 << 0

options options_init(void);

//must be invoked before reading flags from the options struct.
//
//on success, argi is the index of the first element in argv that was not parsed
//by the options handler. Argv is reordered so that all elements in argv from
//[argi, argc) are elements that were not parsed.
//
//returns XENOMEM, XEOPTION, or XESUCCESS.
xerror options_parse(options *self, int argc, char **argv, int *argi);

void options_fprintf(options *self, FILE *stream);
