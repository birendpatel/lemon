// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "options.h"
#include "xerror.h"

typedef struct options options;

error_t UnsafeParser(int, __attribute__((unused)) char *, struct argp_state *);
static const options *AcquireReadOnly(void);
static void ReleaseReadOnly(void);
static void UnsafePrint(void);
//------------------------------------------------------------------------------

struct options {
	pthread_mutex_t mutex;
	struct {
		unsigned int options_state : 1;
		unsigned int compiler_passes : 1;
		unsigned int lexical_tokens : 1;
		unsigned int multithreading : 1;
	} diagnostic;
	struct {
		unsigned int disassemble : 1;
	} ir;
	struct {
		unsigned int no_run : 1;
	} vm;
	struct {
		unsigned int interactive : 1;
	} user;
};

static options opt = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.diagnostic = {
		.options_state = 0,
		.compiler_passes = 0,
		.lexical_tokens = 0,
		.multithreading = 0
	},
	.ir = {
		.disassemble = 0
	},
	.vm = {
		.no_run = 0
	},
	.user = {
		.interactive = 0
	}
};

//------------------------------------------------------------------------------
//gnu argp

enum argp_keys {
	diagnostic_all_key = 256,
	diagnostic_options_state_key = 257,
	diagnostic_compiler_passes_key = 258,
	diagnostic_lexical_tokens_key = 259,
	diagnostic_multithreading_key = 260,
	ir_disassemble_key = 'S',
	vm_no_run_key = 'k',
	user_interactive_key = 'i',
};

const cstring *argp_program_version = LEMON_VERSION;
const cstring *argp_program_bug_address = "github.com/birendpatel/lemon/issues";
static char args_doc[] = "<file 1> ... <file n>";
static char doc[] = "\nThis is the C Lemon interpreter for the Lemon language.";

static struct argp_option options_info[] = {
	{
		.name = "Dall",
		.key  = diagnostic_all_key,
		.doc  = "Enable all diagnostics."
	},
	{
		.name = "Dopt",
		.key  = diagnostic_options_state_key,
		.doc  = "Print the options state before compilation begins."
	},
	{
		.name = "Dpass",
		.key  = diagnostic_compiler_passes_key,
		.doc  = "Signal the entry/exit points for each compiler pass."
	},
	{
		.name = "Dtokens",
		.key  = diagnostic_lexical_tokens_key,
		.doc  = "Print tokens as they are found during tokenization."
	},
	{
		.name = "Dthread",
		.key  = diagnostic_multithreading_key,
		.doc  = "Signal when threads are created and destroyed."
	},
	{
		.name = "Iasm",
		.key  = ir_disassemble_key,
		.doc  = "Disassemble and print the bytecode."
	},
	{
		.name = "Mkill",
		.key = vm_no_run_key,
		.doc = "Do not load and execute the compiled bytecode."
	},
	{
		.key = user_interactive_key,
		.doc = "Launch the REPL after the input files have executed."
	},

	{0} //terminator required by GNU argp
};

error_t UnsafeParser
(
	int key,
	__attribute__((unused)) char *arg,
	struct argp_state *state
)
{
	options *opt = (options *) state->input;

	switch (key) {
	case diagnostic_all_key:
		opt->diagnostic.options_state = 1;
		opt->diagnostic.compiler_passes = 1;
		opt->diagnostic.lexical_tokens = 1;
		opt->diagnostic.multithreading = 1;
		break;

	case diagnostic_options_state_key:
		opt->diagnostic.options_state = 1;
		break;

	case diagnostic_compiler_passes_key:
		opt->diagnostic.compiler_passes = 1;
		break;

	case diagnostic_lexical_tokens_key:
		opt->diagnostic.lexical_tokens = 1;
		break;

	case diagnostic_multithreading_key:
		opt->diagnostic.multithreading = 1;
		break;

	case ir_disassemble_key:
		opt->ir.disassemble = 1;
		break;

	case vm_no_run_key:
		opt->vm.no_run = 1;
		break;

	case user_interactive_key:
		opt->user.interactive = 1;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
		break;
	}

	return 0;
}

void OptionsParse(int *argc, char ***argv)
{
	assert(argc);
	assert(*argc);
	assert(argv);
	assert(*argv);

	static bool called = false;

	if (called) {
		xerror_issue("attempted to parse options more than once");
		XerrorFlush();
		return;
	}

	struct argp args_data = {
		.options = options_info,
		.parser = UnsafeParser,
		.args_doc = args_doc,
		.doc = doc
	};

	int unparsed_index = 0;
	error_t err = 0;

	pthread_mutex_lock(&opt.mutex);

	err = argp_parse(&args_data, *argc, *argv, 0, &unparsed_index, &opt);

	switch (err) {
	case 0:
		*argc = *argc - unparsed_index;
		*argv = *argv + unparsed_index;
		break;

	case ENOMEM:
		xerror_fatal("dynamic allocation failed");
		break;

	case EINVAL:
		XerrorUser(NULL, 0, "invalid option or option argument");
		break;

	default:
		//argp docs only list ENOMEM and EINVAL but suggest that other
		//error codes could appear depending on the implementation.
		xerror_fatal("unknown argp error");
		break;
	}

	if (err) {
		abort();
	}

	if (opt.diagnostic.options_state) {
		UnsafePrint();
	}

	called = true;

	pthread_mutex_unlock(&opt.mutex);
}

static void UnsafePrint(void)
{
	static const cstring *fmt =
		"diagnostic\n"
		"\toptions state: %d\n"
		"\tcompiler passes: %d\n"
		"\tlexical tokens: %d\n"
		"\tmultithreading: %d\n"
		"\nintermediate representation\n"
		"\tdisassemble: %d\n"
		"\nvirtual machine\n"
		"\tno run: %d\n"
		"\nuser preferences\n"
		"\tinteractive: %d\n"
		"\n";

	const int dopt = opt.diagnostic.options_state;
	const int dpass = opt.diagnostic.compiler_passes;
	const int dtokens = opt.diagnostic.lexical_tokens;
	const int dthread = opt.diagnostic.multithreading;
	const int irdasm = opt.ir.disassemble;
	const int mnorun = opt.vm.no_run;
	const int uinteractive = opt.user.interactive;

	fprintf(stderr, fmt, dopt, dpass, dtokens, dthread, irdasm, mnorun,
		uinteractive);
}

//------------------------------------------------------------------------------
//read-only functions

//generate compiler cast warning if read-only contract isn't enforced by caller
static const options *AcquireReadOnly(void)
{
	pthread_mutex_lock(&opt.mutex);

	return (const options *) &opt;
}

static void ReleaseReadOnly(void)
{
	pthread_mutex_unlock(&opt.mutex);
}

bool OptionsGetFlag(const options_flag flag)
{
	bool status = false;

	const options *ro_opt = AcquireReadOnly();

	switch(flag) {
	case DIAGNOSTIC_OPTIONS_STATE:
		status = ro_opt->diagnostic.options_state;
		break;

	case DIAGNOSTIC_COMPILER_PASSES:
		status = ro_opt->diagnostic.compiler_passes;
		break;

	case DIAGNOSTIC_LEXICAL_TOKENS:
		status = ro_opt->diagnostic.lexical_tokens;
		break;

	case DIAGNOSTIC_MULTITHREADING:
		status = ro_opt->diagnostic.multithreading;
		break;

	case IR_DISASSEMBLE:
		status = ro_opt->ir.disassemble;
		break;

	case VM_NO_RUN:
		status = ro_opt->vm.no_run;
		break;

	case USER_INTERACTIVE:
		status = ro_opt->user.interactive;
		break;

	default:
		xerror_issue("invalid flag argument: %d", flag);
		break;
	}

	ReleaseReadOnly();

	return status;
}
