/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief The main file is responsible for options parsing, REPL management,
 * disk IO, and log management.
 * @details call graph:
 *
 *
 *   main ----> options_parse
 *     |
 *     |------> display_header
 *     |
 *     |------> help
 *     |
 *     |
 *     |          |----------> char_vector_cleanup
 *     |          |
 *     |          |
 *     |------> run_repl ----> run_unknown-------|
 *     |	 	                         |----> run
 *     |------> run_file-------------------------|
 *                |
 * 	          |-------> get_bytecount
 *	          |
 *	          |-------> disk_to_heapstr
 *
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

//vector<char> is used by the REPL for input buffering.
make_vector(char, char, static inline)

void char_vector_cleanup(char_vector *v) {
	char_vector_free(v, NULL);
}

#define RAII __attribute__((__cleanup__(char_vector_cleanup)))

//prototypes
xerror run_repl(options *opt);
xerror run(options *opt, char *source, char *fname);
void display_header(void);
xerror run_file(options *opt, int argc, char **argv, int argi);
void char_vector_cleanup(char_vector *v);
void file_cleanup(FILE **fp);
xerror run_unknown(options *opt, char *source, size_t n);
xerror get_bytecount(FILE *fp, size_t *n);
xerror disk_to_heapstr(FILE *fp, char **buf, size_t n);
void help(void);

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
		goto fail;
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
		goto fail;
	}

	xerror_flush();
	return EXIT_SUCCESS;

fail:
	xerror_fatal("%s", xerror_str(err));
	help();
	return EXIT_FAILURE;
}

/*******************************************************************************
 * @fn help
 * @brief Display message on fatal program termination
 ******************************************************************************/
void help(void)
{
	const char *help  = "\nProgram failed. Report an issue: " 
			    "https://github.com/birendpatel/lemon/issues\n";

	fprintf(stderr, "%s", help);
}

/*******************************************************************************
 * @fn run_repl
 * @returns LEMON_ENOMEM, LEMON_EFULL, or LEMON_ESUCCESS
 ******************************************************************************/
xerror run_repl(options *opt)
{
	xerror err = XEUNDEFINED;
	RAII char_vector buf = {0};

	char_vector_init(&buf, 0, KiB(1));

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

			char_vector_push(&buf, (char) curr);

			prev = curr;
		}

		//satisfy string requirement requested by run()
		char_vector_push(&buf, '\0');
		
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
	fprintf(stdout, "Copyright (C) 2021 Biren Patel. ");
	fprintf(stdout, "GNU General Public License v3.0.\n\n");

	fprintf(stdout, "- Double tap 'return' to execute your source code.\n");
	fprintf(stdout, "- Run shell commands by prefixing them with '$'.\n");
	fprintf(stdout, "- Exit Lemon with a keyboard interrupt (Ctrl-C)\n\n");

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
	assert(opt);
	assert(argc);
	assert(argv);
	assert(argi < argc);

	xerror err = XEUNDEFINED;
	FILE *fp = NULL;

	for (int i = argi; i < argc; i++) {
		fp = fopen(argv[i], "r");

		if (!fp) {
			xerror_issue("%s: %s", argv[i], strerror(errno));
			err = XEFILE;
			goto fail;
		}

		size_t len = 0;
		err =  get_bytecount(fp, &len);

		if (err) {
			xerror_issue("%s: cannot get bytecount", argv[i]);
			goto fail;
		}

		char *buf = NULL;
		err = disk_to_heapstr(fp, &buf, len);

		if (err) {
			xerror_issue("%s: cannot copy to memory", argv[i]);
			goto fail;
		}

		fclose(fp);
		err = run(opt, buf, argv[i]);
		free(buf);
		
		if (err) {
			xerror_issue("%s: cannot compile contents", argv[i]);
			goto fail;
		}
	}

	return XESUCCESS;

fail:
	if (fp) {
		fclose(fp);
	}

	return err;
}

/*******************************************************************************
 * @fn get_bytecount
 * @brief Extract the total number of bytes in a file
 * @param fp fp must be an open file. On success, the file position indicator
 * will be set to the beginning of the file.
 ******************************************************************************/
xerror get_bytecount(FILE *fp, size_t *n)
{
	assert(fp);
	assert(n);

	long file_err = 0;

	rewind(fp);

	//move to the end of the file
	file_err = fseek(fp, 0L, SEEK_END);

	if (file_err == -1) {
		xerror_issue("fseek: %s", strerror(errno));
		return XEFILE;
	}

	//extract total bytes
	file_err = ftell(fp);

	if (file_err == -1) {
		xerror_issue("ftell: %s", strerror(errno));
		return XEFILE;
	}

	*n = (size_t) file_err;

	rewind(fp);

	return XESUCCESS;
}

/*******************************************************************************
 * @fn disk_to_heapstr
 * @brief Read file contents into memory as a C string
 * @param fp Must be an open file
 * @param buf Will be a valid pointer to heap if function is successful. User
 * is responsible for managing the memory.
 * @param n Ok if zero
 * @return XEFILE if read fails.
 ******************************************************************************/
xerror disk_to_heapstr(FILE *fp, char **buf, size_t n)
{
	assert(fp);

	kmalloc(*buf, sizeof(char) * n + 1);

	size_t match = fread(*buf, sizeof(char), n, fp);

	if (n != match) {
		xerror_issue("%s", strerror(errno));
		return XEFILE;
	}

	(*buf)[n] = '\0';

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

		//we dont use xerror handler here because shell failures are not
		//compiler failures. Instead, we pass the shell information onto
		//the user but otherwise the compiler itself hasn't encountered
		//an issue and is in a perfectly usable state.
		switch (err) {
		case -1:
			perror(NULL);
			break;
		case 127:
			fprintf(stderr, "cannot exec shell in child process\n");
			break;
		default:
			fprintf(stderr, "shell termination status: %d\n", err);
		}

		/* dont shutdown the REPL when the shell fails */
		return XESUCCESS;
	}

	return run(opt, source, NULL);	
}

/*******************************************************************************
 * @fn run
 * @brief Compile source text to bytecode and execute it on the virtual machine.
 * @param src Null terminated char array.
 ******************************************************************************/
xerror run(options *opt, char *src, char *fname)
{
	assert(opt);
	assert(src);

	xerror err = XEUNDEFINED;

	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	err = compile(src, opt, fname);

	return err;
}

