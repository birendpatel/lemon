// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The options translation unit provides command line options parsing via GNU
// argp and maintains an opaque, thread-safe, and read-only options state.

#pragma once

#include <stdbool.h>

//on failure this function will abort the application program. On success argv
//and argc are modified such that (*argv)[0] thru (*argv)[argc - 1] are the
//unparsed elements. (*argv)[argc] will remain NULL by C99 section 5.2.1.1.2.
//if the --Dopt option is set, the options state will be printed to stderr.
void OptionsParse(int *argc, char ***argv);

typedef enum options_flag {
	DIAGNOSTIC_OPTIONS_STATE,
	DIAGNOSTIC_COMPILER_PASSES,
	DIAGNOSTIC_LEXICAL_TOKENS,
	DIAGNOSTIC_MULTITHREADING,
	IR_DISASSEMBLE,
	VM_NO_RUN,
	USER_INTERACTIVE,
} options_flag;

//an invalid flag argument returns false
bool OptionsGetFlag(const options_flag flag);
