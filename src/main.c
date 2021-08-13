/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon compiler
 */

#include <stdio.h>
#include <stdlib.h>

#include "lemon.h"
#include "lib/vector.h"
#include "options.h"

//data structures
make_vector(char, char, static inline)

//prototypes
lemon_error run_repl(options *opt);

/*******************************************************************************
 * @fn main
 * @brief Main parses the command line options and then delegates compilation
 * to either the REPL or the file reader.
 ******************************************************************************/
int main(int argc, char **argv)
{
	lemon_error err = LEMON_EUNDEF;
	int argi = 0;
	options opt = options_init();

	err = options_parse(&opt, argc, argv, &argi);

	if (err) {
		goto fail;
	}

	if (opt.diagnostic & DIAGNOSTIC_OPT) {
		options_display(&opt);
	}

	if (argi == argc) {
		err = run_repl(&opt);

		if (err) {
			goto fail;
		}
	} 

	return EXIT_SUCCESS;

fail:
	fprintf(stderr, "lemon error: %s\n", lemon_describe(err));
	exit(EXIT_FAILURE);
}

/*******************************************************************************
 * @fn run_repl
 * @brief Initialize the REPL on stdin.
 * @details User input is read into a vector so that the user is not limited in
 * how much they are allowed to write (unless they run out of heap).
 * @returns LEMON_ENOMEM
 ******************************************************************************/
lemon_error run_repl(options *opt)
{
	int err = 0;
	char_vector buf;
	
	err = char_vector_init(&buf, 0, 8);

	if (err == VECTOR_ENOMEM) {
		return LEMON_ENOMEM;
	}

	fprintf(stderr, "repl goes here");

	char_vector_free(&buf);

	return LEMON_ESUCCESS;
}
