/**
 * @file options.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options implementation.
 */

#include <argp.h>

#include "options.h"

//argp global parameters and docs
const char *argp_program_version = "1.0.0.22 alpha";
const char *argp_program_bug_address = "https://github.com/birendpatel/lemon";
static const char *args_doc = "lemon [options] [filename]";
static const char *doc = "This is the CLemon interpreter for the Lemon language."

//options array indices
enum {
	DIAGNOSTIC_ALL,
	DIAGNOSTIC_PASS,
	DIAGNOSTIC_TOKENS,
};

//argp options descriptions
static const struct argp_options options[] = {
	[DIAGNOSTIC_ALL] = {
		.name = "Dall",
		.doc  = "Enable all diagnostics."
	},
	[DIAGNOSTIC_PASS] = {
		.name = "Dpass",
		.doc  = "Notify on stderr all entry and exit points for all compiler passes."
	},
	[DIAGNOSTIC_LEXOUT] = {
		.name = "Dtokens",
		.doc  = "Display tokens on stderr when lexical analysis is complete."
	}
}

