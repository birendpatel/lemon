// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <stdbool.h>
#include <stdlib.h>

#include "defs.h"
#include "options.h"
#include "parser.h"
#include "jobs.h"
#include "xerror.h"
#include "lib/str.h"

#if GCC_VERSION < 80300
	#error "Lemon requires GCC 8.3.0 or greater."
#endif

const cstring *ParseRemainingInputs(char **);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	OptionsParse(&argc, &argv);

	const cstring *root_file = ParseRemainingInputs(argv);

	if (!root_file) {
		return EXIT_FAILURE;
	}

	xerror err = XEUNDEFINED;

	vector(File) schedule = JobsCreate(root_file, &err);

	for (size_t i = 0; i < schedule.len; i++) {
		SyntaxTreeFree(schedule.buffer[i]);
	}

	switch (err) {
	case XESUCCESS:
		XerrorFlush();
		return EXIT_SUCCESS;
		break;

	case XEUSER:
		XerrorUser(NULL, 0, "compilation failed");
		XerrorFlush();
		return EXIT_FAILURE;

	default:
		xerror_fatal("%s", XerrorDescription(err));
		return EXIT_FAILURE;
	}
}

//returns NULL if nothing remains in argv
const cstring *ParseRemainingInputs(char **argv)
{
	assert(argv);

	const cstring *root_file = argv[0];

	if (!root_file) {
		const cstring *msg = "no input file; nothing to compile";
		XerrorUser(NULL, 0, msg);
		return NULL;
	}

	const cstring *sentinel = argv[1];

	if (sentinel != NULL) {
		const cstring *msg = "all input files except %s were ignored";
		XwarnUser(NULL, 0, msg, root_file);

		const cstring *advice = 
			"lemon resolves file dependencies automatically";

		XhelpUser(NULL, 0, advice);
	}

	return root_file;
}
