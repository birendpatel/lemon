// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The options translation unit provides command line options parsing via GNU
// argp and maintains an opaque and thread-safe options state.

#pragma once

//on failure this function will abort the program. On success the elements of 
//argv are reordered and argc is modified such that **argv points to the first
//unparsed element. If argc and argv are the main() arguments, (*argv)[argc]
//will still be NULL.
void OptionsParse(int *argc, char ***argv);

//print the entire options state to stderr
void OptionsPrint(void);


