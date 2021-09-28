// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#ifndef __GNUC__
	#error "Lemon requires a GNU C compiler"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "nodes.h"
#include "options.h"
#include "parser.h"
#include "xerror.h"

#include "lib/vector.h"

xerror run_from_repl(options *opt);
xerror compile(options *opt, char *source, char *fname);
void display_header(void);
xerror run_from_file(options *opt, int argc, char **argv, int argi);
void file_cleanup(FILE **fp);
xerror exec_shell_command(options *opt, char *src);
xerror get_bytecount(FILE *fp, size_t *n);
xerror duplicate_file_contents_as_string(FILE *fp, char **buf, size_t n);
void display_help(void);

int main(int argc, char **argv)
{
	options opt = options_init();

	int total_parsed = 0;
	xerror err = options_parse(&opt, argc, argv, &total_parsed);

	if (err) {
		goto fail;
	}

	if (opt.diagnostic & DIAGNOSTIC_OPT) {
		options_fprintf(&opt, stderr);
	}

	if (total_parsed == argc) {
		err = run_from_repl(&opt);
	} else {
		err = run_from_file(&opt, argc, argv, total_parsed);

		if (!err && (opt.user & USER_INTERACTIVE)) {
			err = run_from_repl(&opt);
		}
	}

	if (err) {
		goto fail;
	}

	xerror_flush();
	return EXIT_SUCCESS;

fail:
	xerror_fatal("%s", xerror_str(err));
	display_help();
	return EXIT_FAILURE;
}

void display_help(void)
{
	static const char *msg =
		"\nProgram failed. Report an issue: "
		"https://github.com/birendpatel/lemon/issues\n";

	fprintf(stderr, "%s", msg);
}

void display_header(void)
{
	static const char *msg =
		"Lemon (%s) (Compiler %s %s)\n"
		"Copyright (C) 2021 Biren Patel.\n"
		"GNU General Public License v3.0.\n\n"
		"- Double tap 'return' to execute source code.\n"
		"- Prefix input with '$' to execute a shell command.\n"
		"- Exit with Ctrl-C.\n\n";

	fprintf(stdout, msg, LEMON_VERSION, __DATE__, __TIME__);
}

make_vector(char, stdin, static inline)

void stdin_vector_cleanup(stdin_vector *vec) {
	stdin_vector_free(vec, NULL);
}

#define RAII __attribute__((__cleanup__(stdin_vector_cleanup)))

xerror run_from_repl(options *opt)
{
	RAII stdin_vector buf = {0};
	stdin_vector_init(&buf, 0, KiB(1));

	display_header();

	while (true) {
		int prev = 0;
		int curr = 0;

		fprintf(stdout, ">>> ");

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

			stdin_vector_push(&buf, (char) curr);

			prev = curr;
		}

		stdin_vector_push(&buf, '\0');

		bool not_shell_cmd = exec_shell_command(opt, buf.data);

		if (not_shell_cmd) {
			xerror err = compile(opt, buf.data, NULL);

			if (err) {
				xerror_issue("cannot compile from repl");
				return err;
			}
		}

		stdin_vector_reset(&buf, NULL);
	}

	return XESUCCESS;
}

xerror run_from_file(options *opt, int argc, char **argv, int argi)
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
		err = duplicate_file_contents_as_string(fp, &buf, len);

		if (err) {
			xerror_issue("%s: cannot copy to memory", argv[i]);
			goto fail;
		}

		fclose(fp);
		err = compile(opt, buf, argv[i]);
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

//fp must be an open file. On success, the file position indicator will be set
//set to the beginning of the file.
xerror get_bytecount(FILE *fp, size_t *n)
{
	assert(fp);
	assert(n);

	rewind(fp);

	long err = fseek(fp, 0L, SEEK_END);

	if (err == -1) {
		xerror_issue("fseek: %s", strerror(errno));
		return XEFILE;
	}

	long bytecount = ftell(fp);

	if (bytecount == -1) {
		xerror_issue("ftell: %s", strerror(errno));
		return XEFILE;
	}

	*n = (size_t) bytecount;

	rewind(fp);

	return XESUCCESS;
}

//copy the contents of the open file fp into a heap buf as a string of length
//n + 1, n >= 0. returns XEFILE if read of fp fails.
xerror duplicate_file_contents_as_string(FILE *fp, char **buf, size_t n)
{
	assert(fp);

	kmalloc(*buf, sizeof(char) * n + 1);

	size_t total_read = fread(*buf, sizeof(char), n, fp);

	if (total_read != n) {
		xerror_issue("%s", strerror(errno));
		return XEFILE;
	}

	(*buf)[n] = '\0';

	return XESUCCESS;
}

//returns XESHELL if input src is not a shell command. Else, return XESUCCESS
//even if the shell command failed. (shell failures are not compiler failures).
//
//src must be null terminated.
xerror exec_shell_command(options *opt, char *src)
{
	assert(opt);
	assert(src);

	if (src[0] != '$') {
		return XESHELL;
	}

	int err = 0;
	const char *null_command = "$\n\0";
	bool match = !strcmp(null_command, src);

	if (match) {
		err = system(NULL);
	} else {
		err = system(src + 1);
	}

	if (err == -1) {
		perror(NULL);
	} else if (WIFEXITED(err)) {
		fprintf(stderr, "termination status; %d\n", WEXITSTATUS(err));
	}

	return XESUCCESS;
}

xerror compile(options *opt, char *src, char *fname)
{
	assert(opt);
	assert(src);

	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	file *ast = NULL;
	xerror err = parse(opt, src, fname, &ast);

	if (err) {
		xerror_issue("cannot create abstact syntax tree");
	}

	return err;
}

