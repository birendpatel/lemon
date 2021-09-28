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

xerror config_options(options *opt, int argc, char **argv, int *total_parsed);
xerror exec_unknown(options *opt, int argc, char **argv, int total_parsed);
xerror exec_from_file(options *opt, int argc, char **argv, int argi);
xerror exec_from_repl(options *opt);
xerror exec_shell_command(options *opt, char *src);
void display_header(void);
void display_help(void);
xerror copy_file_to_string(FILE *fp, char **buf, size_t n);
xerror get_filesize(FILE *fp, size_t *n);
xerror compile(options *opt, char *source, char *fname);

int main(int argc, char **argv)
{
	options opt = {0};
	int total_parsed = 0;
	xerror err = XEUNDEFINED;

	err = config_options(&opt, argc, argv, &total_parsed);
	if (err) { goto fail; }

	err = exec_unknown(&opt, argc, argv, total_parsed);
	if (err) { goto fail; }

	xerror_flush();
	return EXIT_SUCCESS;

fail:
	xerror_fatal("%s", xerror_str(err));
	display_help();
	return EXIT_FAILURE;
}

xerror config_options(options *opt, int argc, char **argv, int *total_parsed)
{
	assert(opt);
	assert(argc);
	assert(argv);
	assert(total_parsed);

	*opt = options_init();

	xerror err = options_parse(opt, argc, argv, total_parsed);

	if (!err && opt->diagnostic & DIAGNOSTIC_OPT) {
		options_fprintf(opt, stderr);
	}

	return err;
}

xerror exec_unknown(options *opt, int argc, char **argv, int total_parsed)
{
	assert(opt);
	assert(argc);
	assert(argv);

	xerror err = XEUNDEFINED;

	if (total_parsed == argc) {
		err = exec_from_repl(opt);
	} else {
		err = exec_from_file(opt, argc, argv, total_parsed);

		if (!err && (opt->user & USER_INTERACTIVE)) {
			err = exec_from_repl(opt);
		}
	}

	return err;
}

void display_help(void)
{
	const char *msg =
		"\nProgram failed. Report an issue: "
		"https://github.com/birendpatel/lemon/issues\n";

	fprintf(stderr, "%s", msg);
}

void display_header(void)
{
	const char *msg =
		"Lemon (%s) (Compiler %s %s)\n"
		"Copyright (C) 2021 Biren Patel.\n"
		"GNU General Public License v3.0.\n\n"
		"- Double tap 'return' to execute source code.\n"
		"- Prefix input with '$' to execute a shell command.\n"
		"- Exit with Ctrl-C.\n\n";

	fprintf(stdout, msg, LEMON_VERSION, __DATE__, __TIME__);
}

//C-style template which creates a vector<char> aliased as stdin_vector.
//see /src/lib/vector.h
make_vector(char, stdin, static inline)

xerror exec_from_repl(options *opt)
{
	xerror err = XESUCCESS;

	stdin_vector userinput = {0};
	stdin_vector_init(&userinput, 0, KiB(1));

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
				err = XESUCCESS;
				goto cleanup;
			}

			if (curr == '\n') {
				if (prev == '\n') {
					break;
				}

				fprintf(stdout, "... ");
			}

			stdin_vector_push(&userinput, (char) curr);

			prev = curr;
		}

		stdin_vector_push(&userinput, '\0');

		bool not_shell_cmd = exec_shell_command(opt, userinput.data);

		if (not_shell_cmd) {
			xerror err = compile(opt, userinput.data, NULL);

			if (err) {
				xerror_issue("cannot compile from repl");
				goto cleanup;
			}
		}

		stdin_vector_reset(&userinput, NULL);
	}

cleanup:
	stdin_vector_free(&userinput, NULL);
	return err;
}

xerror exec_from_file(options *opt, int argc, char **argv, int argi)
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
		err =  get_filesize(fp, &len);

		if (err) {
			xerror_issue("%s: cannot get bytecount", argv[i]);
			goto fail;
		}

		char *buf = NULL;
		err = copy_file_to_string(fp, &buf, len);

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

//this function moves the file position indicator but will move it back to the
//original position on a successful return.
xerror get_filesize(FILE *openfile, size_t *bytes)
{
	assert(openfile);
	assert(bytes);

	xerror xerr = XESUCCESS;
	long savepoint = ftell(openfile);

	if (savepoint == -1) {
		xerror_issue("fseek: cannot save fpos: %s", strerror(errno));
		return XEFILE;
	}

	long err = fseek(openfile, 0L, SEEK_END);

	if (err == -1) {
		xerror_issue("fseek: cannot move to EOF: %s", strerror(errno));
		xerr = XEFILE;
		goto reset;
	}

	long bytecount = ftell(openfile);

	if (bytecount == -1) {
		xerror_issue("ftell: cannot fetch count: %s", strerror(errno));
		xerr = XEFILE;
		goto reset;
	}

	*bytes = (size_t) bytecount;

	xerr = XESUCCESS;

reset:
	err = fseek(openfile, savepoint, SEEK_SET);

	if (err) {
		xerror_issue("fseek: cannot reset fpos: %s", strerror(errno));
		xerr = XEFILE;
	}

	return xerr;
}

xerror copy_file_to_string(FILE *openfile, char **dest, size_t filesize)
{
	assert(openfile);

	char *string = NULL;

	kmalloc(string, sizeof(char) * filesize + 1);

	size_t total_read = fread(string, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		return XEFILE;
	}

	string[filesize] = '\0';

	*dest = string;

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

