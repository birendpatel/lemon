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

void ConfigureCompiler(void);
vector(File) CreateSchedule(char **, xerror *);
const cstring *GetRootFileName(char **);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	xerror err = XEUNDEFINED;

	OptionsParse(&argc, &argv);

	ConfigureCompiler();

	vector(File) schedule = CreateSchedule(argv, &err);

	//temporary
	for (size_t i = 0; i < schedule.len; i++) {
		puts(schedule.buffer[i]->alias);
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

void ConfigureCompiler(void)
{
	SymTableConfigGlobal();
}

//vector.len == 0 on failure and return code is XEFILE, XEPARSE, or XEUSER
vector(File) CreateSchedule(char **argv, xerror *err)
{
	assert(argv);

	const cstring *fname = GetRootFileName(argv);

	return JobsCreate(fname, err);
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
