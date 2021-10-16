// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The main file is responsible for orchestrating the compiler phases and for
// handling all initialization and cleanup required before, after, and between
// these phases.

#include <assert.h>
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

void ConfigureOptions(int *, char ***);
void ShowHeader(void);
xerror ExecuteFromRepl(const options *);
int ReadTerminal(string *);
bool IsShellCommand(const string *);
void ExecShell(const options *, const string *);
void TryStringFree(string *str)
void ShowHeader(void);
void ReportFailure(xerror);
void FileCleanup(FILE **);
xerror ExecuteFromFiles(const options *, const int, char **);
string FileToString(FILE *openfile, xerror *err);
xerror GetFilesize(FILE *, size_t *);
xerror Compile(const options *, string *, const cstring *);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	ConfigureOptions(&argc, &argv);

	xerror err = XEUNDEFINED;

	if (argv[argc]) {
		err = ExecuteFromFiles(argc, argv);
	} else {
		err = ExecuteFromRepl();
	}

	Terminate(err);
}

void ConfigureOptions(int *argc, char ***argv)
{
	assert(argc);
	assert(argv);

	OptionsParse(argc, argv);

	bool flag = OptionsGetFlag(DIAGNOSTIC_OPTIONS_STATE);

	if (flag) {
		OptionsPrint(stderr);
	}
}

void Terminate(xerror err)
{
	if (err) {
		xerror_fatal("%s", XerrorDescription(err));
		exit(EXIT_FAILURE);
	}

	XerrorFlush();
	exit(EXIT_SUCCESS);
}

void TryStringFree(string *s)
{
	static const cstring *errormsg =
		"string at %p is UAF exploitable; "
		"StringFree failed; "
		"induced memory leak";

	xerror err = StringFree(s);

	if (err) {
		xerror_issue(errormsg, (void *) s);
	}
}

//------------------------------------------------------------------------------

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

xerror ExecuteFromRepl(const options *opt)
{
	xerror err = XEUNDEFINED;
	const xerror fatal_errors = ~(XESUCCESS | XEPARSE);

	RAII(TryStringFree) string input = StringInit(KiB(1));

	ShowHeader();

	while (true) {
		if (ReadTerminal(&input) == EOF) {
			return XESUCCESS;
		}
		
		if (IsShellCommand(input)) {
			ExecShell(opt, input);
		} else {
			err = Compile(opt, input, NULL);

			if (err & fatal_errors) {
				xerror_issue("cannot compile from repl");
				return err;
			}
		}

		StringReset(&input);
	}
}

//returns EOF if encountered a SIGINT.
int ReadTerminal(string *input)
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
			return EOF;
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

	return 0;
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

	const cstring *null_command = "$";
	const cstring *input = src->vec.buffer;
	
	bool match = !strcmp(null_command, input);

	if (match) {
		err = system(NULL);
	} else {
		const cstring *command = input + 1;
		err = system(command);
	}

	if (err == -1) {
		perror(NULL);
	}
}

//------------------------------------------------------------------------------

void FileCleanup(FILE **handle)
{
	if (*handle) {
		fclose(*handle);
	}
}

xerror ExecuteFromFiles(const options *opt, const int argc, char **argv)
{
	assert(opt);
	assert(argc);
	assert(argv);

	for (int i = 0; i < argc; i++) {
		xerror err = XEUNDEFINED;
		const cstring *fname = argv[i];
		RAII(FileCleanup) FILE *fhandle = fopen(fname, "r");

		if (!fhandle) {
			xerror_issue("%s: %s", fname, strerror(errno));
			return XEFILE;
		}
	
		RAII(TryStringFree) string src = FileToString(fhandle, &err);

		if (err) {
			xerror_issue("%s: cannot copy file to memory", fname);
			return XEFILE;
		}

		err = Compile(opt, &src, fname);

		if (err) {
			xerror_issue("%s: cannot compile from file", fname);
			return XEFILE;
		}
	}

	return XESUCCESS;
}

string FileToString(FILE *openfile, xerror *err)
{
	assert(openfile);
	assert(err);

	size_t filesize = 0;
	*err = GetFilesize(openfile, &filesize);

	if (*err) {
		xerror_issue("cannot calculate file size");
		return (string) {0};
	}

	char *buffer = NULL;
	size_t buffer_length = sizeof(char) * filesize + 1;
	kmalloc(buffer, filesize);

	size_t total_read = fread(buffer, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		*err = XEFILE;
		return (string) {0};
	}

	buffer[filesize] = '\0';
	string s = StringFromHeapCString(buffer);
	
	*err = XESUCCESS;
	return s;
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

//------------------------------------------------------------------------------

xerror Compile(const options *opt, string *src, const cstring *alias)
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

