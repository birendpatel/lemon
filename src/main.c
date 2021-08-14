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
lemon_error run(options *opt, char *source);
void display_header(void);
lemon_error run_file(options *opt, int argc, char **argv, int argi);
void char_vector_cleanup(char_vector *v);
void file_cleanup(FILE **fp);

//vector raii - safe even when v->data is NULL
void char_vector_cleanup(char_vector *v) {
	char_vector_free(v, NULL);
}

#define CHARVEC_RAII __attribute__((__cleanup__(char_vector_cleanup)))

//file raii - stdio fclose is undefined when *fp is NULL.
void file_cleanup(FILE **fp) {
	if (*fp) {
		fclose(*fp);
	}
}

#define FILE_RAII __attribute__((__cleanup__(file_cleanup)))

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
	} else {
		err = run_file(&opt, argc, argv, argi);
	}

	EXIT_NONRECOVERABLE(err);

	return EXIT_SUCCESS;
}

/*******************************************************************************
 * @fn run_repl
 * @returns LEMON_ENOMEM, LEMON_EFULL, or LEMON_ESUCCESS
 ******************************************************************************/
lemon_error run_repl(options *opt)
{
	lemon_error err = LEMON_EUNDEF;
	CHARVEC_RAII char_vector buf = {0};

	err = char_vector_init(&buf, 0, KILOBYTE);

	if (err) {
		return err;
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
			
			if (curr == EOF) {
				fprintf(stdout, "\n");
				return LEMON_ESUCCESS;
			}

			if (curr == '\n') {
				if (prev == '\n') {
					break;
				}

				fprintf(stdout, "... ");
			}
			
			err = char_vector_push(&buf, (char) curr);
			
			if (err) {
				return err;
			}

			prev = curr;
		}

		//satisfy string requirement requested by run()
		err = char_vector_push(&buf, '\0');

		if (err) {
			return err;
		}

		err = run(opt, buf.data);

		if (err) {
			return err;
		}

		char_vector_reset(&buf, NULL);
	}

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
 * @def FILE_ERROR
 * @brief system/library function error wrapper for run_file().
 ******************************************************************************/
#define FILE_ERROR(fname)						       \
	do {								       \
		fprintf(stderr, "%s: ", (fname));			       \
		perror(NULL);						       \
		return LEMON_EFILE;					       \
	} while (0)

/*******************************************************************************
 * @fn run_file
 * @brief Compile from source file.
 * @details Lemon does not yet have a module system. Therefore, all files are
 * processed sequentially in isolation.
 ******************************************************************************/
lemon_error run_file(options *opt, int argc, char **argv, int argi)
{
	lemon_error err = LEMON_EUNDEF;
	long ferr = -1;
	size_t serr = 0;
	size_t bytes = 0;

	for (int i = argi; i < argc; i++) {
		char *fname = argv[i];

		//use raii idiom to avoid complicated goto chains for errors
		FILE_RAII FILE *fp = fopen(fname, "r");
		if (!fp) FILE_ERROR(fname);

		//move to the end of the file
		ferr = fseek(fp, 0L, SEEK_END);
		if (ferr == -1) FILE_ERROR(fname);

		//count total bytes in file
		ferr = ftell(fp);
		if (ferr == -1) FILE_ERROR(fname);
		bytes = (size_t) ferr;

		//move to the beginning of the file
		ferr = fseek(fp, 0L, SEEK_SET);
		if (ferr == -1) FILE_ERROR(fname);

		//allocate space for file including a null terminator
		char *buf = malloc(sizeof(char) * bytes + 1);
		if (!buf) return LEMON_ENOMEM;

		//read file into memory
		serr = fread(buf, sizeof(char), bytes, fp);
		if (serr != bytes) FILE_ERROR(fname);

		//add null terminator as required by run()
		buf[bytes] = '\0';

		err = run(opt, buf);
		if (err) return err;
	}

	return LEMON_ESUCCESS;
}

/*******************************************************************************
 * @fn run
 * @brief Compile source text to bytecode and execute it on the virtual machine.
 * @param source Null terminated char array.
 ******************************************************************************/
lemon_error run(options *opt, char *source)
{
	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	fprintf(stdout, "%s\n", source);

	return LEMON_ESUCCESS;

}

