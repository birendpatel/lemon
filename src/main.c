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

xerror ConfigOptions(options *opt, int argc, char **argv, int *total_parsed);
xerror ExecUnknown(options *opt, int argc, char **argv, int total_parsed);
xerror ExecFile(options *opt, int argc, char **argv, int argi);
xerror ExecRepl(options *opt);
xerror CollectInput(dynamic_string *userinput);
bool IsShellCommand(string src);
void ExecShell(options *opt, string src);
void ShowHeader(void);
void ShowHelp(void);
xerror StringFromFile(FILE *fp, string dest, size_t n);
xerror GetFilesize(FILE *fp, size_t *n);
xerror Compile(options *opt, string source, string alias);

int main(int argc, char **argv)
{
	options opt = {0};
	int total_parsed = 0;
	xerror err = XEUNDEFINED;

	err = ConfigOptions(&opt, argc, argv, &total_parsed);

	if (err) {
		goto fail;
	}

	err = ExecUnknown(&opt, argc, argv, total_parsed);

	if (err) {
		goto fail;
	}

	XerrorFlush();
	return EXIT_SUCCESS;

fail:
	xerror_fatal("%s", XerrorDescription(err));
	ShowHelp();
	return EXIT_FAILURE;
}

xerror ConfigOptions(options *opt, int argc, char **argv, int *total_parsed)
{
	assert(opt);
	assert(argc);
	assert(argv);
	assert(total_parsed);

	*opt = OptionsInit();

	xerror err = OptionsParse(opt, argc, argv, total_parsed);
	
	bool state_flag = opt->diagnostic & DIAGNOSTIC_OPT;

	if (!err && state_flag) {
		OptionsPrint(opt, stderr);
	}

	return err;
}

xerror ExecUnknown(options *opt, int argc, char **argv, int total_parsed)
{
	assert(opt);
	assert(argc);
	assert(argv);

	xerror err = XEUNDEFINED;

	if (total_parsed == argc) {
		err = ExecRepl(opt);
	} else {
		err = ExecFile(opt, argc, argv, total_parsed);

		if (!err && (opt->user & USER_INTERACTIVE)) {
			err = ExecRepl(opt);
		}
	}

	return err;
}

void ShowHelp(void)
{
	const string msg =
		"\nProgram failed. Report an issue: "
		"https://github.com/birendpatel/lemon/issues\n";

	fprintf(stderr, "%s", msg);
}

void ShowHeader(void)
{
	const string msg =
		"Lemon (%s) (Compiled %s %s)\n"
		"Copyright (C) 2021 Biren Patel.\n"
		"GNU General Public License v3.0.\n\n"
		"- Double tap 'return' to execute source code.\n"
		"- Prefix input with '$' to execute a shell command.\n"
		"- Exit with Ctrl-C.\n\n";

	fprintf(stdout, msg, LEMON_VERSION, __DATE__, __TIME__);
}

xerror ExecRepl(options *opt)
{
	RAII(DynamicStringFree) dynamic_string input = {0};
	DynamicStringInit(&input, KiB(1));

	ShowHeader();

	while (true) {
		xerror err = CollectInput(&input);

		if (err == XECLOSED) {
			return XESUCCESS;
		}

		string source = (ViewFromDynamicString(&input)).data;

		if (IsShellCommand(source)) {
			ExecShell(opt, source);
		} else {
			err = Compile(opt, source, NULL);

			if (err) {
				xerror_issue("cannot compile from repl");
				return err;
			}
		}

		DynamicStringReset(&input);
	}
}

//returns XECLOSED if encountered a SIGINT.
xerror CollectInput(dynamic_string *buffer)
{
	assert(buffer);

	xerror err = XEUNDEFINED;
	int prev = 0;
	int curr = 0;

	fprintf(stdout, ">>> ");

	while (true) {
		fflush(stdout);

		curr = fgetc(stdin);

		if (curr == EOF) {
			fprintf(stdout, "\n");
			err = XECLOSED;
			break;
		}

		if (curr == '\n') {
			if (prev == '\n') {
				err = XESUCCESS;
				break;
			}

			fprintf(stdout, "... ");
		}

		DynamicStringAppend(buffer, (char) curr);

		prev = curr;
	}

	return err;
}

xerror ExecFile(options *opt, int argc, char **argv, int argi)
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
		err =  GetFilesize(fp, &len);

		if (err) {
			xerror_issue("%s: cannot get bytecount", argv[i]);
			goto fail;
		}

		string src = StringInit(len);
		err = StringFromFile(fp, src, len);

		if (err) {
			xerror_issue("%s: cannot copy to memory", argv[i]);
			goto fail;
		}

		fclose(fp);
		err = Compile(opt, src, argv[i]);
		StringFree(src);

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
xerror GetFilesize(FILE *openfile, size_t *bytes)
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

xerror StringFromFile(FILE *openfile, string dest, size_t filesize)
{
	assert(openfile);

	size_t total_read = fread(dest, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		return XEFILE;
	}

	return XESUCCESS;
}

bool IsShellCommand(string src)
{
	if (src[0] == '$') {
		return true;
	}

	return false;
}

void ExecShell(options *opt, string src)
{
	assert(opt);
	assert(src);

	int err = 0;
	const string null_command = "$\n\0";
	bool match = !strcmp(null_command, src);

	if (match) {
		err = system(NULL);
	} else {
		err = system(src + 1);
	}

	if (err == -1) {
		perror(NULL);
	} 
}

xerror Compile(options *opt, string src, string alias)
{
	assert(opt);
	assert(src);

	if (opt->diagnostic & DIAGNOSTIC_PASS) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	xerror err = XEUNDEFINED;

	file *ast = parse(opt, &err, src, alias);

	if (err) {
		xerror_issue("cannot create abstact syntax tree");
	}

	return err;
}

