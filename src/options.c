 // Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <argp.h>
#include <assert.h>
#include <errno.h>

#include "defs.h"
#include "options.h"

#define NO_SHORT_OPTION(key) (256 + key)
#define SHORT_OPTION(key) key

#define DIAGNOSTIC_ALL_KEY	NO_SHORT_OPTION(0)
#define DIAGNOSTIC_OPT_KEY	NO_SHORT_OPTION(1)
#define DIAGNOSTIC_PASS_KEY	NO_SHORT_OPTION(2)
#define DIAGNOSTIC_TOKENS_KEY	NO_SHORT_OPTION(3)
#define DIAGNOSTIC_THREAD_KEY	NO_SHORT_OPTION(4)
#define IR_DISASSEMBLE_KEY	SHORT_OPTION('S')
#define MACHINE_NORUN_KEY	SHORT_OPTION('k')
#define USER_INTERACTIVE_KEY	SHORT_OPTION('i')

const char *argp_program_version = LEMON_VERSION;
const char *argp_program_bug_address = "github.com/birendpatel/lemon/issues";
static char args_doc[] = "<file 1> ... <file n>";
static char doc[] = "\nThis is the C Lemon interpreter for the Lemon language.";

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
		.doc  = "Notify entry and exit points for all compiler passes."
	},
	{
		.name = "Dtokens",
		.key  = DIAGNOSTIC_TOKENS_KEY,
		.doc  = "Display tokens found during lexical analysis."
	},
	{
		.name = "Dthread",
		.key  = DIAGNOSTIC_THREAD_KEY,
		.doc  = "Notify on thread creation and joins."
	},
	{
		.name = "Iasm",
		.key  = IR_DISASSEMBLE_KEY,
		.doc  = "Output disassembled bytecode."
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
	
	//terminator required by argp
	{0} 
};

error_t parser(int key, unused char *arg, struct argp_state *state)
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
		break;
	}

	return 0;
}

options OptionsInit(void) 
{
	options opt = {
		.diagnostic = 0,
		.ir = 0,
		.machine = 0,
		.user = 0
	};

	return opt;
}

void OptionsParse(options *opt, int *argc, char ***argv)
{
	assert(opt);
	assert(argc);
	assert(argv);
	assert(*argc);
	assert(*argv);

	struct argp args_data = {
		.options = options_info,
		.parser = parser,
		.args_doc = args_doc,
		.doc = doc
	};

	int unparsed_index = 0;
	error_t err = 0;

	err = argp_parse(&args_data, *argc, *argv, 0, &unparsed_index, opt);

	switch (err) {
	case 0:
		*argc = *argc - unparsed_index;
		*argv = *argv + unparsed_index;
		return;

	case ENOMEM:
		xerror_fatal("dynamic allocation failed");
		abort();

	case EINVAL:
		XerrorUser(0, "invalid option or option argument");
		abort();

	default:
		//argp docs only list ENOMEM and EINVAL but suggest that other
		//error codes could appear.
		xerror_fatal("unknown argp error");
		abort();
	}
}

static int ToIndicator(int value)
{
	return value ? 1 : 0;
}

void OptionsPrint(options *opt, FILE *stream)
{
	const char *msg = "diagnostic\n"
			  "\tall: %d\n"
			  "\topt: %d\n"
			  "\tpass: %d\n"
			  "\ttokens: %d\n"
			  "\tthread: %d\n"
			  "\nintermediate representation\n"
			  "\tdisassemble: %d\n"
			  "\nvirtual machine\n"
			  "\tnorun: %d\n"
			  "\nuser preferences\n"
			  "\tinteractive: %d\n"
			  "\n";

	int dall = ToIndicator(self->diagnostic & DIAGNOSTIC_ALL);
	int dopt = ToIndicator(self->diagnostic & DIAGNOSTIC_OPT);
	int dpass = ToIndicator(self->diagnostic & DIAGNOSTIC_PASS);
	int dtokens = ToIndicator(self->diagnostic & DIAGNOSTIC_TOKENS);
	int dthread = ToIndicator(self->diagnostic & DIAGNOSTIC_THREAD);
	int irdasm = ToIndicator(self->ir & IR_DISASSEMBLE);
	int mnorun = ToIndicator(self->machine & MACHINE_NORUN);
	int uinteractive = ToIndicator(self->user & USER_INTERACTIVE);

	fprintf(stream, msg, dall, dopt, dpass, dtokens, dthread, irdasm,
		mnorun, uinteractive);
}
