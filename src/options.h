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

//this function will abort the program on failure, but on return argv and argc
//are modified so that **argv and (*argv)[*argc] point to the first unparsed 
//option.
void OptionsParse(options *opt, int *argc, char ***argv);

void OptionsPrint(options *opt, FILE *stream);
