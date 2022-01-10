// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The options module provides command line options parsing via GNU argp and
// maintains an opaque read-only options state.

#pragma once

#include <stdbool.h>
#include <stddef.h>

//------------------------------------------------------------------------------
//on success returns true and modifies argv/argc such that (*argv)[0] thru
//(*argv)[argc - 1] are the unparsed elements (if any) and (*argv)[argc] is
//NULL. This function must be executed and return before any multithreading
//in application code may commence.

bool OptionsParse(int *argc, char ***argv);

//------------------------------------------------------------------------------
//execute 'lemon --help' on the terminal to see all the available options;
//these calls are thread-safe provided the OptionsParse contract is upheld.

void OptionsDstate(void); //prints the options state to stderr if --Dstate

bool OptionsDtokens(void); //true if --Dtokens

bool OptionsDdeps(void); //true if --Ddeps

bool OptionsDsym(void); //true if --Dsym

size_t OptionsArena(void); //returns a default size if --Arena not specified

