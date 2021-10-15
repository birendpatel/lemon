// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The options translation unit provides command line options parsing via GNU
// argp and maintains an opaque, thread-safe, and read-only options state.

#pragma once

#include <stdbool.h>

//on failure this function will abort the program. On success the elements of 
//argv are reordered and argc is modified such that **argv points to the first
//unparsed element. If argc and argv are the main() arguments, (*argv)[argc]
//will remain NULL.
void OptionsParse(int *argc, char ***argv);

//print the entire options state to stderr
void OptionsPrint(void);

typedef enum options_flag {
	DIAGNOSTIC_ALL,
	DIAGNOSTIC_OPTION_STATE,
	DIAGNOSTIC_COMPILER_PASSES,
	DIAGNOSTIC_LEXICAL_TOKENS,
	DIAGNOSTIC_MULTITHREADING,
	IR_DISASSEMBLE,
	VM_NO_RUN,
	USER_INTERACTIVE,
} options_flag;

//an invalid flag argument returns false
bool OptionsGetFlag(const options_flag flag);
