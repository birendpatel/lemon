// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The main file contains all of the initialization and cleanup required before
// and after compilation and execution. The main tasks are command line options
// parsing, file IO, REPL management, symbol table management, and disassembly.
//
// The file also orchestrates the compiler phases and invokes each phase in turn
// according to which optimisation and diagnostic flags have been set.

#include <errno.h>
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

options ConfigOptions(int *, char ***);
xerror ExecUnknown(const options *, const int, char **);
xerror ExecRepl(const options *);
xerror ReadTerminal(string *);
bool IsShellCommand(const string *);
void ExecShell(const options *, const string *);
void TryStringFree(string *str)
void ShowHeader(void);
void ShowHelp(void);
xerror ExecFile(const options *, const int, char **);
xerror CopyFileToString(FILE *, string *, const size_t);
xerror GetFilesize(FILE *, size_t *);
xerror Compile(const options *, const string *, const cstring *);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	options opt = ConfigOptions(&argc, &argv);

	xerror err = ExecUnknown(&opt, argc, argv);

	if (err) {
		xerror_fatal("%s", XerrorDescription(err));
		ShowHelp();
		return EXIT_FAILURE;
	}

	XerrorFlush();
	return EXIT_SUCCESS;
}

options ConfigOptions(int *argc, char ***argv)
{
	assert(argc);
	assert(argv);

	options opt = OptionsInit();

	OptionsParse(&opt, argc, argv);

	if (opt->diagnostic & DIAGNOSTIC_OPT) {
		OptionsPrint(opt, stderr);
	}

	return opt;
}

xerror ExecUnknown(const options *opt, const int argc, char **argv)
{
	assert(opt);
	assert(argv);

	xerror err = XEUNDEFINED;

	if (argv[argc] == NULL) {
		ShowHeader();
		err = ExecRepl(opt);
	} else {
		err = ExecFile(opt, argc, argv);
		const bool launch_repl = opt->user & USER_INTERACTIVE;

		if (!err && launch_repl) {
			err = ExecRepl(opt);
		}
	}

	return err;
}

void ShowHelp(void)
{
	static const cstring *msg =
		"\nProgram failed. Report an issue: "
		"https://github.com/birendpatel/lemon/issues\n";

	fputs(msg, stderr);
}

void ShowHeader(void)
{
	static const cstring *msg =
		"Lemon (%s) (Compiled %s %s)\n"
		"Copyright (C) 2021 Biren Patel.\n"
		"GNU General Public License v3.0.\n\n"
		"- Double tap 'return' to execute source code.\n"
		"- Prefix input with '$' to execute a shell command.\n"
		"- Exit with Ctrl-C.\n\n";

	fprintf(stdout, msg, LEMON_VERSION, __DATE__, __TIME__);
}

void TryStringFree(string *str)
{
	xerror err = StringFree(str);

	if (err) {
		xerror_issue("use after free is possible; induced memory leak");
	}
}

//------------------------------------------------------------------------------

xerror ExecRepl(const options *opt)
{
	xerror err = XEUNDEFINED;
	const xerror fatal_errors = ~(XESUCCESS | XEPARSE);

	string input = StringInit(KiB(1));

	while (true) {
		if (ReadTerminal(&input) == XECLOSED) {
			err = XESUCCESS;
			goto cleanup;
		}
		
		if (IsShellCommand(input)) {
			ExecShell(opt, input);
		} else {
			err = Compile(opt, input, NULL);

			if (err & fatal_errors) {
				xerror_issue("cannot compile from repl");
				goto cleanup;
			}
		}

		StringReset(&input);
	}

cleanup:
	TryStringFree(&input);
	return err;
}

//returns XECLOSED if encountered a SIGINT.
xerror ReadTerminal(string *input)
{
	assert(input);

	const cstring *wait_msg = ">>> ";
	const cstring *continue_msg = "... ";
	xerror err = XEUNDEFINED;
	int prev = 0;
	int curr = 0;

	fprintf(stdout, wait_msg);

	while (true) {
		fflush(stdout);

		curr = fgetc(stdin);

		if (curr == EOF) {
			fprintf(stdout, "\n");
			return XECLOSED;
		}

		if (curr == '\n') {
			if (prev == '\n') {
				break;
			}

			fprintf(stdout, continue_msg);
		}

		StringAppend(input, (char) curr);

		prev = curr;
	}

	StringTrim(input, '\n');

	return XESUCCESS;
}

//------------------------------------------------------------------------------

bool IsShellCommand(const string *input)
{
	const char shell_indicator = '$';
	const char first_character = StringGet(input, 0);

	if (first_character == shell_indicator) {
		return true;
	}

	return false;
}

void ExecShell(const options *opt, const string *src)
{
	assert(opt);
	assert(src);

	int err = 0;

	const cstring *null_command = "$\n\0";
	const cstring *raw_input = src->vec.buffer;
	
	bool match = !strcmp(null_command, raw_input);

	if (match) {
		err = system(NULL);
	} else {
		err = system(raw_input + 1);
	}

	if (err == -1) {
		perror(NULL);
	}
}

//------------------------------------------------------------------------------

xerror ExecFile(const options *opt, const int argc, char **argv)
{
	assert(opt);
	assert(argc);
	assert(argv);

	xerror err = XEUNDEFINED;
	FILE *file_handle = NULL;
	string src = STRING_DEFAULT_VALUE;

	for (int i = 0; i < argc; i++) {
		const cstring *fname = argv[i];
		file_handle = fopen(fname, "r");

		if (!file_handle) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}

		err = CopyFileToString(file_handle, &src);

		if (err) {
			xerror_issue("%s: cannot copy file to memory", fname);
			goto clean_file;
		}

		err = Compile(opt, &src, fname);

		if (err) {
			xerror_issue("%s: cannot compile from file", fname);
			goto clean_string;
		}

		fclose(file_handle);
		TryStringFree(&src);
	}

	return XESUCCESS;

clean_string:
	TryStringFree(&src);

clean_file:
	fclose(fp);
	return err;
}

//on return the file position will be set to the beginning of the file
xerror GetFilesize(FILE *openfile, size_t *bytes)
{
	assert(openfile);
	assert(bytes);

	xerror xerr = XESUCCESS;

	rewind(openfile);

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
	rewind(openfile);
	return xerr;
}

xerror CopyFileToString(FILE *openfile, string *dest)
{
	assert(openfile);
	assert(dest);

	size_t filesize = 0;
	xerror err = GetFilesize(openfile, &filesize);

	if (err) {
		xerror_issue("cannot calculate file size");
		return XEFILE;
	}

	char *buffer = NULL;
	size_t buffer_length = sizeof(char) * filesize + 1;
	kmalloc(buffer, filesize);

	size_t total_read = fread(buffer, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		return XEFILE;
	}

	buffer[filesize] = '\0';
	cstring *source_code = buffer;

	StringFromHeapCString(dest, source_code);

	return XESUCCESS;
}

//------------------------------------------------------------------------------

xerror Compile(const options * opt, string * src, const cstring *alias)
{
	assert(opt);
	assert(src);
	assert(alias);

	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	file *ast = SyntaxTreeInit(opt, src, alias);

	if (!ast) {
		xerror_issue("cannot create abstact syntax tree");
		return XEPARSE;
	}

	if (ast->errors) {
		if (ast->errors > 1) {
			XerrorUser(0, "%zu syntax errors found\n", ast->errors);
		} else {
			XerrorUser(0, "1 syntax error found\n");
		}

		SyntaxTreeFree(ast);
		return XEPARSE;
	}

	return XESUCCESS;
}

