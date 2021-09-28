 // Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <argp.h>
#include <assert.h>
#include <errno.h>

#include "defs.h"
#include "options.h"

//argp options keys; options without a short name begin at 256
#define DIAGNOSTIC_ALL_KEY	256
#define DIAGNOSTIC_OPT_KEY	257
#define DIAGNOSTIC_PASS_KEY	258
#define DIAGNOSTIC_TOKENS_KEY	259
#define DIAGNOSTIC_THREAD_KEY	260
#define IR_DISASSEMBLE_KEY	'S'
#define MACHINE_NORUN_KEY	'k'
#define USER_INTERACTIVE_KEY	'i'

//argp global parameters and docs
const char *argp_program_version = LEMON_VERSION;
const char *argp_program_bug_address = "https://github.com/birendpatel/lemon/issues";
static char args_doc[] = "<file 1> ... <file n>";
static char doc[] = "\nThis is the C Lemon interpreter for the Lemon language.";

//argp options descriptions
static struct argp_option options_info[] = {
	{
		.name = "Dall",
		.key  = DIAGNOSTIC_ALL_KEY,
		.doc  = "Enable all diagnostics."
	},
	{
		.name = "Dopt",
		.key  = DIAGNOSTIC_OPT_KEY,
		.doc  = "Display the options state before compilation begins."
	},
	{
		.name = "Dpass",
		.key  = DIAGNOSTIC_PASS_KEY,
		.doc  = "Notify on all entry and exit points for all compiler passes."
	},
	{
		.name = "Dtokens",
		.key  = DIAGNOSTIC_TOKENS_KEY,
		.doc  = "Display tokens on stderr once lexical analysis is complete."
	},
	{
		.name = "Dthread",
		.key  = DIAGNOSTIC_THREAD_KEY,
		.doc  = "Notify on all thread creation, detachment, exits, and joins."
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
	},
	{
		.key = USER_INTERACTIVE_KEY,
		.doc = "Launch the REPL after the input files have executed."
	},
	{0} //terminator required by argp, otherwise nonprintables will appear in -?
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

	case DIAGNOSTIC_THREAD_KEY:
		opt->diagnostic |= DIAGNOSTIC_THREAD;
		break;

	case IR_DISASSEMBLE_KEY:
		opt->ir |= IR_DISASSEMBLE;
		break;

	case MACHINE_NORUN_KEY:
		opt->machine |= MACHINE_NORUN;
		break;

	case USER_INTERACTIVE_KEY:
		opt->user |= USER_INTERACTIVE;
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
		.machine = 0,
		.user = 0
	};

	return opt;
}

xerror options_parse(options *self, int argc, char **argv, int *argi)
{
	assert(self);
	assert(argc > 0);
	assert(argv);
	assert(argi);

	struct argp args_data = {
		.options = options_info,
		.parser = parser,
		.args_doc = args_doc,
		.doc = doc
	};

	error_t err = argp_parse(&args_data, argc, argv, 0, argi, self);

	if (err == ENOMEM) {
		return XENOMEM;
	} else if (err == EINVAL) {
		return XEOPTION;
	} else {
		//catch unknown errnos in debug mode.
		//argp documentation only lists ENOMEM and EINVAL
		//but suggests other error codes could appear.
		assert(err == 0);
	}

	return XESUCCESS;
}

#define TO_BOOL(x) ((x) ? 1 : 0)

void options_fprintf(options *self, FILE *stream)
{
	fprintf(stream, "OPTIONS\n\n");

	fprintf(stream, "diagnostic\n");
	fprintf(stream, "\tall: %d\n", TO_BOOL(self->diagnostic & DIAGNOSTIC_ALL));
	fprintf(stream, "\topt: %d\n", TO_BOOL(self->diagnostic & DIAGNOSTIC_OPT));
	fprintf(stream, "\tpass: %d\n", TO_BOOL(self->diagnostic & DIAGNOSTIC_PASS));
	fprintf(stream, "\ttokens: %d\n", TO_BOOL(self->diagnostic & DIAGNOSTIC_TOKENS));
	fprintf(stream, "\tthread: %d\n", TO_BOOL(self->diagnostic & DIAGNOSTIC_THREAD));

	fprintf(stream, "\nintermediate representation\n");
	fprintf(stream, "\tdisassemble: %d\n", TO_BOOL(self->ir & IR_DISASSEMBLE));

	fprintf(stream, "\nvirtual machine\n");
	fprintf(stream, "\tnorun: %d\n", TO_BOOL(self->machine & MACHINE_NORUN));

	fprintf(stream, "\nuser preferences\n");
	fprintf(stream, "\tinteractive: %d\n", TO_BOOL(self->user & USER_INTERACTIVE));

	fprintf(stream, "\nEND OPTIONS\n\n");
}
