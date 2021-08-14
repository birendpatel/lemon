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

//vector<char> used by REPL for input buffering
make_vector(char, char, static inline)

//prototypes
lemon_error run_repl(options *opt);
lemon_error run(char *source, size_t n, options *opt);

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
 * @returns LEMON_ENOMEM or LEMON_ESUCCESS
 ******************************************************************************/
lemon_error run_repl(options *opt)
{
	int err = 0;
	char_vector buf;

	//heuristic: 0.25 KB buffer is enough to avoid syscalls for most users.
	err = char_vector_init(&buf, 0, 256);

	if (err == VECTOR_ENOMEM) {
		return LEMON_ENOMEM;
	}

	//note: no signal handlers other than ctrl-D; defaults are good enough.
	while (true) {
		int prev = 0;
		int curr = 0;

		fprintf(stdout, ">>> ");
		fflush(stdout);

		//read input until a double newline occurs
		while (true) {
			curr = fgetc(stdin);

			if (curr == EOF) {
				goto kill;
			}

			if (curr == '\n') {
				if (prev == '\n') {
					break;
				}

				fprintf(stdout, "... ");
				fflush(stdout);
			}

			char_vector_push(&buf, (char) curr);
			prev = curr;
		}

		run(buf.data, buf.len, opt);

		char_vector_reset(&buf, NULL);
	}

kill:
	fprintf(stdout, "\n");
	fflush(stdout);

	char_vector_free(&buf, NULL);

	return LEMON_ESUCCESS;
}

/*******************************************************************************
 * @fn run
 * @brief Compile source text to bytecode and execute it on the virtual machine.
 * @param source Char array of length n. Does not have to be null terminated.
 ******************************************************************************/
lemon_error run(char *source, size_t n, options *opt)
{
	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	for (size_t i = 0; i < n; i++) {
		printf("%c", source[i]);
	}

	return LEMON_ESUCCESS;

}

