// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The options module provides command line options parsing via GNU argp and
// maintains an opaque read-only options state.

#pragma once

#include <stdbool.h>
#include <stddef.h>

//on success returns true and modifies argv/argc such that (*argv)[0] thru
//(*argv)[argc - 1] are the unparsed elements and (*argv)[argc] is NULL
bool OptionsParse(int *argc, char ***argv);

//print all options values to stderr
void OptionsPrint(void);

//get options values; execute ./lemon --help on the terminal to see all the
//available options
bool OptionsDstate(void);
bool OptionsDtokens(void);
bool OptionsDdeps(void);
size_t OptionsArena(void);

