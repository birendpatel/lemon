/**
 * @file options.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options implementation.
 */

#include <argp.h>

#include "options.h"

struct options {
	uint8_t diagnostics;
	uint8_t machine;
};

//Since the options struct is dense and requires bitwise operations, the values
//in enum options_code cannot be used to access information in the struct. So
//for every API user-facing option code, there are two corresponding macros
//listed here. The first is for the struct access. The second is for the keygen
//used by argp_options.
#define DALL 1 << 0
#define DALL_KEY 256

#define DFLAGS 1 << 1
#define DFLAGS_KEY 257

#define DPASS 1 << 2
#define DPASS_KEY 258

#define DTOKENS 1 << 3
#define DTOKENS_KEY 259

#define MBYTECODE 1 << 0
#define MBYTECODE_KEY 'S'

//argp global parameters and docs
const char *argp_program_version = "1.0.0.4 alpha";
const char *argp_program_bug_address = "https://github.com/birendpatel/lemon/issues";
static const char *args_doc = "lemon [options] [filename]";
static const char *doc = "This is the CLemon interpreter for the Lemon language."

//argp options descriptions
//options without a short name begin their keys at 256 (see the above macro list).
static const struct argp_options options_info[OPTIONS_COUNT] = {
	[DIAGNOSTIC_ALL] = {
		.name = "Dall",
		.key  = DALL_KEY
		.doc  = "Enable all diagnostics."
	},
	[DIAGNOSTIC_FLAGS] = {
		.name = "Dflags",
		.key  = DFLAGS_KEY
		.doc  = "Display flags on stderr, including those not set by options."
	},
	[DIAGNOSTIC_PASS] = {
		.name = "Dpass",
		.key  = DPASS_KEY
		.doc  = "Notify on stderr all entry and exit points for all compiler passes."
	},
	[DIAGNOSTIC_TOKENS] = {
		.name = "Dtokens",
		.key  = DTOKENS_KEY
		.doc  = "Display tokens on stderr when lexical analysis is complete."
	},
	[MACHINE_BYTECODE] = {
		.key = MBYTECODE_KEY,
		.doc = "Generate bytecode but do not run the virtual machine."
	}
};

//argp parser actions
error_t parser(int key, char *arg, struct argp_state *state)
{
	options *opt = (opt*) state->input;

	switch (key) {
	case DALL_KEY:
		opt->diagnostics |= DALL;
		break;

	case DFLAGS_KEY:
		opt->diagnostics |= DFLAGS;
		break;

	case DPASS_KEY:
		opt->diagnostics |= DPASS;
		break;

	case DTOKENS_KEY:
		opt->diagnostics |= DTOKENS;
		break;

	case BYTECODE:
		opt->machine |= MBYTECODE;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}
