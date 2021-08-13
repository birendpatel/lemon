/**
 * @file options.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options implementation.
 */

#include <argp.h>

#include "options.h"

//argp options keys; options without a short name begin at 256
#define DIAGNOSTIC_ALL_KEY	256
#define DIAGNOSTIC_FLAGS_KEY	257
#define DIAGNOSTIC_PASS_KEY	258
#define DIAGNOSTIC_TOKENS_KEY	259
#define IR_DISASSEMBLE_KEY	'S'
#define MACHINE_NORUN_KEY	'k'

//argp global parameters and docs
const char *argp_program_version = "1.0.0.5 alpha";
const char *argp_program_bug_address = "https://github.com/birendpatel/lemon/issues";
static const char *args_doc = "lemon [options] [filename]";
static const char *doc = "This is the CLemon interpreter for the Lemon language."

//argp options descriptions
static const struct argp_options options_info[] = {
	{
		.name = "Dall",
		.key  = DIAGNOSTIC_ALL_KEY
		.doc  = "Enable all diagnostics."
	},
	{
		.name = "Dflags",
		.key  = DIAGNOSTIC_FLAGS_KEY
		.doc  = "Display flags on stderr, including those not set by options."
	},
	{
		.name = "Dpass",
		.key  = DIAGNOSTIC_PASS_KEY
		.doc  = "Notify on stderr all entry and exit points for all compiler passes."
	},
	{
		.name = "Dtokens",
		.key  = DIAGNOSTIC_TOKENS_KEY
		.doc  = "Display tokens on stderr once lexical analysis is complete."
	},
	{
		.name = "Iasm",
		.key  = IR_DISASSEMBLE_KEY,
		.doc  = "Output disassembled bytecode from the final pass to stderr."
	}
	{
		.name = "Mkill"
		.key = MACHINE_NORUN_KEY,
		.doc = "Compile to bytecode but do not run the virtual machine."
	}
};

//argp parser actions
error_t parser(int key, char *arg, struct argp_state *state)
{
	options *opt = (opt*) state->input;

	switch (key) {
	case DIAGNOSTIC_ALL_KEY:
		opt->diagnostic = 0xFF;
		break;

	case DIAGNOSTIC_FLAGS_KEY:
		opt->diagnostic |= DIAGNOSTIC_FLAGS;
		break;

	case DIAGNOSTIC_PASS_KEY:
		opt->diagnostic |= DIAGNOSTIC_PASS;
		break;

	case DIAGNOSTIC_TOKENS_KEY:
		opt->diagnostic |= DIAGNOSTIC_TOKENS;
		break;

	case IR_DISASSEMBLE_KEY:
		opt->ir |= IR_DISASSEMBLE;
		break;

	case MACHINE_NORUN_KEY:
		opt->machine |= MACHINE_NORUN;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

//The initializer places CLemon into a default state. Currently all options
//default to off, or false. Meaning, CLemon does not do any optimisations or
//diagnostics or anything whatsovever that may be remotely interesting. It
//just performs the 3 basic compiler passes: parser -> ast -> bytecode. And
//then executes the VM and exits.
options options_init(void) {
	options opt = {
		.diagnostic = 0,
		.ir = 0,
		.machine = 0
	};

	return opt;
}
