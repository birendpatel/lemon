 // Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 // API for the Command line options handler

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "xerror.h"

//command line options are represented as bit flags
typedef struct options {
	uint8_t diagnostic;
	uint8_t ir;
	uint8_t machine;
	uint8_t user;
} options;

#define DIAGNOSTIC_ALL 		1 << 0
#define DIAGNOSTIC_OPT		1 << 1
#define DIAGNOSTIC_PASS		1 << 2
#define DIAGNOSTIC_TOKENS	1 << 3
#define DIAGNOSTIC_THREAD	1 << 4
#define IR_DISASSEMBLE		1 << 1
#define MACHINE_NORUN		1 << 1
#define USER_INTERACTIVE	1 << 0

options OptionsInit(void);

//This function may abort the program. Returns XENOMEM, XEOPTION, or XESUCCESS.
//
//on success, argi is the index in argv of the first element not parsed by the
//options handler. The elements of argv are reordered so that all elements
//from [argi, argc) are unparsed elements.
xerror OptionsParse(options *self, int argc, char **argv, int *argi);

void OptionsPrint(options *self, FILE *stream);
