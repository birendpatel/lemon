// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "options.h"
#include "parser.h"
#include "xerror.h"
#include "lib/str.h"

#if GCC_VERSION < 80300
	#error "Lemon requires GCC 8.3.0 or greater."
#endif

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	OptionsParse(&argc, &argv);

	const cstring *input_file = argv[0];

	if (!input_file) {
		const cstring *msg = "no input files; nothing to compile";
		XerrorUser(NULL, 0, msg);
		return EXIT_FAILURE;
	}

	const cstring *sentinel = argv[1];

	if (sentinel != NULL) {
		const cstring *msg = "all input files except %s were ignored";
		XerrorUser(NULL, 0, msg, input_file);
	}

	xerror err = LoadFile(input_file);

	if (err) {
		xerror_fatal("%s", XerrorDescription(err));
		return EXIT_FAILURE;
	}

	XerrorFlush();
	return EXIT_SUCCESS;
}
