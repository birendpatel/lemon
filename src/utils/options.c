// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.  
#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "options.h"
#include "version.h"
#include "xerror.h"

typedef struct options options;
typedef struct argp_option argp_option;
typedef struct argp_state argp_state;
typedef struct argp argp;

#define unused __attribute__((unused)) 

static error_t Parser(int, char *, unused argp_state *);

//------------------------------------------------------------------------------

struct options {
	struct {
		unsigned int state: 1;
		unsigned int tokens : 1;
		unsigned int dependencies : 1;
	} diagnostic;
	struct {
		size_t arena_default;
	} memory;
};

static options opt = {
	.diagnostic = {
		.state = 0,
		.tokens = 0,
		.dependencies = 0
	},
	.memory = {
		.arena_default = MiB(1)
	}
};

//------------------------------------------------------------------------------
//gnu argp

enum argp_groups {
	group_default = 0,
	group_diagnostic,
	group_memory,
};

enum argp_keys {
	key_diagnostic_state = 256,
	key_diagnostic_tokens = 257,
	key_diagnostic_dependencies = 258,
	key_arena_default = 'a',
};

const cstring *argp_program_version = LEMON_VERSION;
const cstring *argp_program_bug_address = "github.com/birendpatel/lemon/issues";
static char args_doc[] = "[filename]";
static char doc[] = "\nThis is the C Lemon compiler for the Lemon language.";

static argp_option options_info[] = {
	{
		.name  = "Dstate",
		.key   = key_diagnostic_state,
		.doc   = "Print the options state.",
		.group = group_diagnostic
	},
	{
		.name  = "Dtokens",
		.key   = key_diagnostic_tokens,
		.doc   = "Print tokens found during lexical analysis.",
		.group = group_diagnostic
	},
	{
		.name  = "Ddeps",
		.key   = key_diagnostic_dependencies,
		.doc   = "Print the dependency graph topological sort.",
		.group = group_diagnostic
	},
	{
		.name  = "Arena",
		.key   = key_arena_default,
		.arg   = "megabytes",
		.doc   = "Set the default arena size up to 1 GiB",
		.group = group_memory
	},

	{0} //terminator required by GNU argp
};

static error_t Parser(int key, char *arg, unused argp_state *state)
{
	switch (key) {
	case key_diagnostic_state:
		opt.diagnostic.state = 1;
		break;

	case key_diagnostic_tokens:
		opt.diagnostic.tokens = 1;
		break;

	case key_diagnostic_dependencies:
		opt.diagnostic.dependencies = 1;
		break;

	case key_arena_default: /* label bypass */ ;
		char *endptr = NULL;
		double value = strtod(arg, &endptr);

		const cstring *msg1 = "bad arena size; using default";
		const cstring *msg2 = "arena size out of range; using default";

		if (arg == endptr || *endptr != '\0') {
			xuser_warn(NULL, 0, msg1);
		} else if (value <= 0.0 || value > 1000.0) {
			xuser_warn(NULL, 0, msg2);
		} else {
			opt.memory.arena_default = MiB(value);
		}
		
		break;

	default:
		return ARGP_ERR_UNKNOWN;
		break;
	}

	return 0;
}

//------------------------------------------------------------------------------
//api

bool OptionsParse(int *argc, char ***argv)
{
	assert(argc);
	assert(argv);

	argp args_data = {
		.options = options_info,
		.parser = Parser,
		.args_doc = args_doc,
		.doc = doc
	};

	int unparsed_index = 0;
	error_t err = 0;

	err = argp_parse(&args_data, *argc, *argv, 0, &unparsed_index, &opt);

	switch (err) {
	case 0:
		*argc = *argc - unparsed_index;
		*argv = *argv + unparsed_index;
		return true;

	case ENOMEM:
		xerror_fatal("out of memory");
		break;

	case EINVAL:
		xuser_error(NULL, 0, "invalid option or option argument");
		break;

	default:
		xerror_fatal("undocumented GNU argp error");
		break;
	}

	return false;
}

//------------------------------------------------------------------------------

void OptionsDstate(void)
{
	if (!opt.diagnostic.state) {
		return;
	}

	const cstring *fmt =
		"Dstate: %d\n"
		"Dtokens: %d\n"
		"Ddeps: %d\n"
		"Arena: %zu\n";

	fprintf(stderr,
		fmt,
		(int) opt.diagnostic.state,
		(int) OptionsDtokens(),
		(int) OptionsDdeps(),
		OptionsArena());
}

bool OptionsDtokens(void)
{
	return opt.diagnostic.tokens;
}

bool OptionsDdeps(void)
{
	return opt.diagnostic.dependencies;
}

size_t OptionsArena(void)
{
	return opt.memory.arena_default;
}
