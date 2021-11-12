// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "defs.h"
#include "options.h"
#include "parser.h"
#include "jobs.h"
#include "symtable.h"
#include "xerror.h"
#include "lib/str.h"

#if GCC_VERSION < 80300
	#error "Lemon requires GCC 8.3.0 or greater."
#endif

vector(File) CreateSchedule(char **); 
const cstring *GetRootFileName(char **);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	OptionsParse(&argc, &argv);

	SymTableGlobalInit();

	vector(File) schedule = CreateSchedule(argv);

	//temporary
	for (size_t i = 0; i < schedule.len; i++) {
		puts(schedule.buffer[i]->alias);
	}

	if (FileVectorIsDummy(&schedule)) {
		xerror_fatal("compilation failed");
		return EXIT_FAILURE;
	} else {
		XerrorFlush();
		return EXIT_SUCCESS;
	}
}

//returns the zero vector on failure
vector(File) CreateSchedule(char **argv)
{
	assert(argv);

	const cstring *fname = GetRootFileName(argv);

	return JobsCreate(fname);
}

//returns "main" if argv is empty
const cstring *GetRootFileName(char **argv)
{
	assert(argv);

	const cstring *fname = argv[0];

	if (!fname) {
		return "main";
	}

	const cstring *sentinel = argv[1];

	if (sentinel != NULL) {
		const cstring *msg = "all input files except %s were ignored";
		XwarnUser(NULL, 0, msg, fname);
	}

	return fname;
}
