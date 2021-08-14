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
void display_header(void);

/*******************************************************************************
 * @def EXIT_NONRECOVERABLE
 * @brief Error handling wrapper for errors which the callee cannot recover 
 * from.
 * @details Design note: Do not move this def to the lemon header. Only main.c
 * and options.c reserve the right to terminate the program.
 ******************************************************************************/
#define EXIT_NONRECOVERABLE(err)					       \
	do {								       \
		if ((err) != LEMON_ESUCCESS) {			               \
			const char *desc = lemon_describe(err);                \
			fprintf(stderr, "lemon error: %s\n", desc);            \
			exit(EXIT_FAILURE);                                    \
		}                                                              \
	} while (0)

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
	
	EXIT_NONRECOVERABLE(err);

	if (opt.diagnostic & DIAGNOSTIC_OPT) {
		options_display(&opt);
	}

	if (argi == argc) {
		err = run_repl(&opt);
	}// else {
	//	err = run_file(argv, argi, &opt);
	//}

	EXIT_NONRECOVERABLE(err);

	return EXIT_SUCCESS;
}

/*******************************************************************************
 * @fn run_repl
 * @returns LEMON_ENOMEM or LEMON_ESUCCESS
 ******************************************************************************/
lemon_error run_repl(options *opt)
{
	int err = 0;
	char_vector buf;

	err = char_vector_init(&buf, 0, KILOBYTE / 4);

	if (err == VECTOR_ENOMEM) {
		return LEMON_ENOMEM;
	}

	display_header();

	while (true) {
		int prev = 0;
		int curr = 0;

		fprintf(stdout, ">>> ");

		//read input until a double newline occurs
		while (true) {
			fflush(stdout);
			curr = fgetc(stdin);
			
			//handle ctrl-d signal
			if (curr == EOF) {
				goto kill;
			}

			if (curr == '\n') {
				if (prev == '\n') {
					break;
				}

				fprintf(stdout, "... ");
			}

			char_vector_push(&buf, (char) curr);
			prev = curr;
		}

		//satisfy string requirement requested by run()
		char_vector_push(&buf, '\0');

		run(buf.data, buf.len, opt);

		char_vector_reset(&buf, NULL);
	}

kill:
	fprintf(stdout, "\n");

	char_vector_free(&buf, NULL);

	return LEMON_ESUCCESS;
}

/*******************************************************************************
 * @fn display_header
 * @brief License, version, compile date, etc displayed on the REPL.
 ******************************************************************************/
void display_header(void)
{
	fprintf(stdout, "Lemon %s ", LEMON_VERSION);
	fprintf(stdout, "(Compiled %s %s)\n", __DATE__, __TIME__);
	fprintf(stdout, "GNU General Public License v3.0.\n\n");

	return;
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

