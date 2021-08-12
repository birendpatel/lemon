/**
 * @file options.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options implementation.
 */

#include <argp.h>

#include "options.h"

enum options {
	DIAGNOSTIC_PASS = 0,
	DIAGNOSTIC_HEAP = 1,
};

//argp global parameters
const char *argp_program_version = "1.0.0.1 alpha";
const char *argp_program_bug_address = "https://github.com/birendpatel/lemon";
static const char *args_doc = "lemon [options] [filename]";
