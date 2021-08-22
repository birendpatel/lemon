/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon compiler
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compile.h"
#include "defs.h"
#include "lib/vector.h"
#include "options.h"
#include "xerror.h"

#ifndef __GNUC__
	#error "Lemon requires a GNU C compiler"
#endif

//vector<char> used by REPL for input buffering
make_vector(char, char, static inline)

//prototypes
xerror run_repl(options *opt);
xerror run(options *opt, char *source);
void display_header(void);
xerror run_file(options *opt, int argc, char **argv, int argi);
void char_vector_cleanup(char_vector *v);
void file_cleanup(FILE **fp);
xerror run_unknown(options *opt, char *source, size_t n);

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
	xerror err = XEUNDEFINED;
	int argi = 0;
	options opt = options_init();

	err = options_parse(&opt, argc, argv, &argi);

	if (err) {
		xerror_fatal("%s", xerror_str(err));
		exit(EXIT_FAILURE);
	}

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

	if (err) {
		xerror_fatal("%s", xerror_str(err));
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}

/*******************************************************************************
 * @fn run_repl
 * @returns LEMON_ENOMEM, LEMON_EFULL, or LEMON_ESUCCESS
 ******************************************************************************/
xerror run_repl(options *opt)
{
	xerror err = XEUNDEFINED;
	CHARVEC_RAII char_vector buf = {0};

	err = char_vector_init(&buf, 0, KiB(1));

	if (err) {
		xerror_issue("cannot init char vector");
		return XENOMEM;
	}

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
				return XESUCCESS;
			}

			if (curr == '\n') {
				if (prev == '\n') {
					break;
				}

				fprintf(stdout, "... ");
			}

			err = char_vector_push(&buf, (char) curr);

			if (err) {
				xerror_issue("cannot push to char vector");
				return err;
			}

			prev = curr;
		}

		//satisfy string requirement requested by run()
		err = char_vector_push(&buf, '\0');
		
		if (err) {
			xerror_issue("cannot push to char vector");
			return err;
		}

		err = run_unknown(opt, buf.data, buf.len);

		if (err) {
			xerror_issue("failed to run source from REPL");
			return err;
		}

		char_vector_reset(&buf, NULL);
	}

	return XESUCCESS;
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
xerror run_file(options *opt, int argc, char **argv, int argi)
{
	xerror err = XEUNDEFINED;
	long ferr = -1;
	size_t serr = 0;
	size_t bytes = 0;

	for (int i = argi; i < argc; i++) {
		char *fname = argv[i];

		//use raii idiom to avoid complicated goto chains for errors
		FILE_RAII FILE *fp = fopen(fname, "r");

		if (!fp) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}

		//move to the end of the file
		ferr = fseek(fp, 0L, SEEK_END);
		
		if (ferr == -1) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}

		//count total bytes in file
		ferr = ftell(fp);
		
		if (ferr == -1) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}
		
		bytes = (size_t) ferr;

		//move to the beginning of the file
		ferr = fseek(fp, 0L, SEEK_SET);
		
		if (ferr == -1) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}

		//allocate space for file including a null terminator
		char *buf = malloc(sizeof(char) * bytes + 1);
		
		if (!buf) {
			xerror_issue("cannot allocate memory for file read");
			return XENOMEM;
		}

		//read file into memory
		serr = fread(buf, sizeof(char), bytes, fp);
		
		if (serr != bytes) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}

		//add null terminator as required by run()
		buf[bytes] = '\0';

		err = run(opt, buf);
		
		if (err) {
			xerror_issue("cannot compile from file contents");
			return err;
		}
	}

	return XESUCCESS;
}

/*******************************************************************************
 * @fn run_unknown
 * @brief Assess if the input is a shell command or if it is lemon source and
 * invoke the correct actions.
 * @param source Null terminated char array
 ******************************************************************************/
//TODO: refactor shell handling and include the waitpid information.
xerror run_unknown(options *opt, char *source, size_t n)
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
			xerror_issue("cannot exec shell in child process");
			xerror_flush();
			break;
		default:
			xerror_issue("shell termination status: %d", err);
			xerror_flush();
		}

		/* dont shutdown the REPL when the shell fails */
		return XESUCCESS;
	}

	return run(opt, source);	
}

/*******************************************************************************
 * @fn run
 * @brief Compile source text to bytecode and execute it on the virtual machine.
 * @param src Null terminated char array.
 ******************************************************************************/
xerror run(options *opt, char *src)
{
	assert(opt);
	assert(src);

	xerror err = XEUNDEFINED;

	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	err = compile(opt, src);

	return err;
}

