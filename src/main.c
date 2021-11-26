// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.  //
// This file orchestrates the major compiler phases and performs all cleanup,
// initialisation, and error handling required before, after, and between each
// phase.

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "arena.h"
#include "options.h"
#include "resolver.h"
#include "str.h"
#include "version.h"
#include "xerror.h"

#if GCC_VERSION < 80300
	#error "Lemon requires GCC 8.3.0 or greater."
#endif

void Initialise(int *, char ***);
_Noreturn void Terminate(int);
const cstring *GetRootFileName(char **);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	Initialise(&argc, &argv);

	const cstring *filename = GetRootFileName(argv);

	network *net = ResolverInit(filename);

	if (!net) {
		xerror_fatal("cannot resolve %s", filename);
		Terminate(EXIT_FAILURE);
	}

	//TODO temp
	for (module *curr = net->head; curr != NULL; curr = curr->next) {
		puts(curr->alias);
	}

	Terminate(EXIT_SUCCESS);
}

_Noreturn void Terminate(int status)
{
	assert(status == EXIT_SUCCESS || status == EXIT_FAILURE);

	ArenaFree();
	XerrorFlush();
	exit(status);
}

void Initialise(int *argc, char ***argv)
{
	assert(argc);
	assert(argv);

	const size_t default_arena_size = MiB(1);

	OptionsParse(argc, argv);

	if (!ArenaInit(default_arena_size)) {
		xerror_fatal("cannot initialise new arena");
		Terminate(EXIT_FAILURE);
	}
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
		xuser_warn(NULL, 0, msg, fname);
	}

	return fname;
}
