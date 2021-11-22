// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.  //
// This file orchestrates the major compiler phases and performs all cleanup,
// initialisation, and error handling required before, after, and between each
// phase.

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "options.h"
#include "resolver.h"
#include "str.h"
#include "xerror.h"

#define LEMON_VERSION "Alpha"

//5 digit version code; e.g., 100908 is version 10.9.8 (maj.min.patch)
#ifdef __GNUC__
	#define GCC_VERSION_MAJ (__GNUC__ * 10000)
	#define GCC_VERSION_MIN (__GNUC_MINOR__ * 100)
	#define GCC_VERSION_PCH (__GNUC_PATCHLEVEL__)
	#define GCC_VERSION GCC_VERSION_MAJ + GCC_VERSION_MIN + GCC_VERSION_PCH
#else
	#define GCC_VERSION 0
#endif

#if GCC_VERSION < 80300
	#error "Lemon requires GCC 8.3.0 or greater."
#endif

const cstring *GetRootFileName(char **);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	OptionsParse(&argc, &argv);

	const cstring *filename = GetRootFileName(argv);

	network *net = ResolverInit(filename);

	if (!net) {
		xerror_fatal("cannot resolve %s", filename);
		return EXIT_FAILURE;
	}

	for (module *curr = net->head; curr != NULL; curr = curr->next) {
		puts(curr->alias);
	}

	XerrorFlush();
	return EXIT_SUCCESS;
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
