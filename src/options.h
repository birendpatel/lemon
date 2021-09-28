 // Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 // API for the Command line options handler

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "xerror.h"

//every command line option is mapped to a bit flag.
typedef struct options {
	uint8_t diagnostic;
	uint8_t ir;
	uint8_t machine;
	uint8_t user;
} options;

//flags
#define DIAGNOSTIC_ALL 		1 << 0
#define DIAGNOSTIC_OPT		1 << 1
#define DIAGNOSTIC_PASS		1 << 2
#define DIAGNOSTIC_TOKENS	1 << 3
#define DIAGNOSTIC_THREAD	1 << 4
#define IR_DISASSEMBLE		1 << 1
#define MACHINE_NORUN		1 << 1
#define USER_INTERACTIVE	1 << 0

options options_init(void);

//returns XEOPTION on failure or may abort within function.
//
//on success, argi is the index in argv of the first element not parsed by the
//options handler. The elements of argv are reordered so that all elements
//from [argi, argc) are unparsed elements.
xerror options_parse(options *self, int argc, char **argv, int *argi);

void options_fprintf(options *self, FILE *stream);
