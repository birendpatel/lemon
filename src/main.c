/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon compiler
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
lemon_error run_unknown(options *opt, char *source, size_t n);

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

	EXIT_ERROR(err, lemon_describe(err), EXIT_FAILURE);

	if (opt.diagnostic & DIAGNOSTIC_OPT) {
		options_display(&opt);
	}

	if (argi == argc) {
		display_header();
		err = run_repl(&opt);
	} else {
		err = run_file(&opt, argc, argv, argi);

		if (!err && (opt.user & USER_INTERACTIVE)) {
			err = run_repl(&opt);
		}
	}

	EXIT_ERROR(err, lemon_describe(err), EXIT_FAILURE);

	return EXIT_SUCCESS;
}

/*******************************************************************************
 * @fn run_repl
 * @returns LEMON_ENOMEM, LEMON_EFULL, or LEMON_ESUCCESS
 ******************************************************************************/
lemon_error run_repl(options *opt)
{
	static const char *init_msg = "cannot init char vector";
	static const char *push_msg = "cannot push to char vector";
	static const char *run_msg  = "failed to run source from REPL";
	lemon_error err = LEMON_EUNDEF;

	CHARVEC_RAII char_vector buf = {0};
	err = char_vector_init(&buf, 0, KiB(1));
	RETURN_ERROR(err, init_msg, LEMON_ENOMEM);

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
			RETURN_ERROR(err, push_msg, err);

			prev = curr;
		}

		//satisfy string requirement requested by run()
		err = char_vector_push(&buf, '\0');
		RETURN_ERROR(err, push_msg, err);

		err = run_unknown(opt, buf.data, buf.len);
		RETURN_ERROR(err, run_msg, err);

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
	fprintf(stdout, "Copyright (C) 2021 Biren Patel\n");
	fprintf(stdout, "GNU General Public License v3.0.\n\n");

	return;
}

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
		RETURN_ERROR_HEAD(!fp, fname, strerror(errno), LEMON_EFILE);

		//move to the end of the file
		ferr = fseek(fp, 0L, SEEK_END);
		RETURN_ERROR_HEAD(ferr == -1, fname, strerror(errno), LEMON_EFILE);

		//count total bytes in file
		ferr = ftell(fp);
		RETURN_ERROR_HEAD(ferr == -1, fname, strerror(errno), LEMON_EFILE);
		bytes = (size_t) ferr;

		//move to the beginning of the file
		ferr = fseek(fp, 0L, SEEK_SET);
		RETURN_ERROR_HEAD(ferr == -1, fname, strerror(errno), LEMON_EFILE);

		//allocate space for file including a null terminator
		char *buf = malloc(sizeof(char) * bytes + 1);
		RETURN_ERROR(!buf, "no heap for file read", LEMON_ENOMEM);

		//read file into memory
		serr = fread(buf, sizeof(char), bytes, fp);
		RETURN_ERROR_HEAD(serr != bytes, fname, strerror(errno), LEMON_EFILE);

		//add null terminator as required by run()
		buf[bytes] = '\0';

		err = run(opt, buf);
		RETURN_ERROR(err, "cannot compile from source file", err);
	}

	return LEMON_ESUCCESS;
}

/*******************************************************************************
 * @fn run_unknown
 * @brief Assess if the input is a shell command or if it is lemon source and
 * invoke the correct actions.
 * @param source Null terminated char array
 ******************************************************************************/
//TODO: refactor shell handling and include the waitpid information.
lemon_error run_unknown(options *opt, char *source, size_t n)
{
	assert(opt);
	assert(source);
	assert(n > 0);
	assert(source[n - 1] == '\0');

	if (source[0] == '$') {
		int err = 0;

		//let user expect behavior that they would expect from the man 3
		//system docs. Therefore, if user enters '$\n\0' then the return
		//status indicates if the shell is available on the system. Since
		//the REPL waits for a double-enter, the check for a final NULL
		//terminator is necessary.
		if (strncmp("$\n\0", source, 3) == 0) {
			err = system(NULL);
		} else {
			err = system(source + 1);
		}

		switch (err) {
		case -1:
			perror(NULL);
			break;
		case 127:
			fprintf(stderr, "cannot exec shell in child process");
			break;
		default:
			fprintf(stdout, "shell termination status: %d\n", err);
		}

		/* dont shutdown the REPL when the shell fails */
		return LEMON_ESUCCESS;
	}

	return run(opt, source);	
}

/*******************************************************************************
 * @fn run
 * @brief Compile source text to bytecode and execute it on the virtual machine.
 * @param source Null terminated char array.
 ******************************************************************************/
lemon_error run(options *opt, char *source)
{
	assert(opt);
	assert(source);

	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	fprintf(stdout, "%s\n", source);

	return LEMON_ESUCCESS;

}

