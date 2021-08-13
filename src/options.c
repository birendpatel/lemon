/**
 * @file options.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon command line options implementation.
 */

#include <argp.h>
#include <errno.h>
#include <stdio.h>

#include "options.h"

//argp options keys; options without a short name begin at 256
#define DIAGNOSTIC_ALL_KEY	256
#define DIAGNOSTIC_OPT_KEY	257
#define DIAGNOSTIC_PASS_KEY	258
#define DIAGNOSTIC_TOKENS_KEY	259
#define IR_DISASSEMBLE_KEY	'S'
#define MACHINE_NORUN_KEY	'k'

//argp global parameters and docs
const char *argp_program_version = "1.0.0.6 alpha";
const char *argp_program_bug_address = "https://github.com/birendpatel/lemon/issues";
static const char *args_doc = "lemon [options] [filename]";
static const char *doc = "This is the CLemon interpreter for the Lemon language.";

//argp options descriptions
static const struct argp_option options_info[] = {
	{
		.name = "Dall",
		.key  = DIAGNOSTIC_ALL_KEY,
		.doc  = "Enable all diagnostics."
	},
	{
		.name = "Dopt",
		.key  = DIAGNOSTIC_OPT_KEY,
		.doc  = "Display the options state on stderr."
	},
	{
		.name = "Dpass",
		.key  = DIAGNOSTIC_PASS_KEY,
		.doc  = "Notify on stderr all entry and exit points for all compiler passes."
	},
	{
		.name = "Dtokens",
		.key  = DIAGNOSTIC_TOKENS_KEY,
		.doc  = "Display tokens on stderr once lexical analysis is complete."
	},
	{
		.name = "Iasm",
		.key  = IR_DISASSEMBLE_KEY,
		.doc  = "Output disassembled bytecode from the final pass to stderr."
	},
	{
		.name = "Mkill",
		.key = MACHINE_NORUN_KEY,
		.doc = "Compile to bytecode but do not run the virtual machine."
	}
};

//argp parser actions
error_t parser(int key, __attribute__((unused)) char *arg, struct argp_state *state)
{
	options *opt = (options *) state->input;

	switch (key) {
	case DIAGNOSTIC_ALL_KEY:
		opt->diagnostic = 0xFF;
		break;

	case DIAGNOSTIC_OPT_KEY:
		opt->diagnostic |= DIAGNOSTIC_OPT;
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

options options_init(void) {
	options opt = {
		.diagnostic = 0,
		.ir = 0,
		.machine = 0
	};

	return opt;
}

lemon_error options_parse(options *self, int argc, char **argv)
{
	assert(self);
	assert(argc > 0);
	assert(argv);

	struct argp args_data = {
		.options = options_info,
		.parser = parser,
		.args_doc = args_doc,
		.doc = doc
	};

	error_t err = argp_parse(&args_data, argc, argv, 0, 0, &self);

	if (err == ENOMEM) {
		return LEMON_ENOMEM;
	} else if (err == EINVAL) {
		return LEMON_EOPTION;
	} else {
		assert(1 == 0); //catch unknown errno
	}

	return LEMON_ESUCCESS;
}

void options_diplay(options *self)
{
	fprintf(stderr, "OPTIONS\n\n");

	fprintf(stderr, "diagnostic\n");
	fprintf(stderr, "\tall: %d\n", self->diagnostic & DIAGNOSTIC_ALL);
	fprintf(stderr, "\topt: %d\n", self->diagnostic & DIAGNOSTIC_OPT);
	fprintf(stderr, "\tpass: %d\n", self->diagnostic & DIAGNOSTIC_PASS);
	fprintf(stderr, "\ttokens: %d\n", self->diagnostic & DIAGNOSTIC_TOKENS);
	
	fprintf(stderr, "intermediate representation\n");
	fprintf(stderr, "\tdisassemble: %d\n", self->ir & IR_DISASSEMBLE);
	
	fprintf(stderr, "virtual machine\n");
	fprintf(stderr, "norun: %d\n", self->machine & MACHINE_NORUN);

	fprintf(stderr, "END OPTIONS\n\n");
}
