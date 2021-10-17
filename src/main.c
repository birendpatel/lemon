// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// main.c is responsible for orchestrating the major compiler phases: parsing,
// static analysis, optimisation, and code generation. The file handles all
// initialisation and cleanup required before, after, and between each phase.

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

void ShowHeader(void);
xerror ExecuteFromRepl(void);
cstring *ReadStandardInput(void);
xerror ExecuteShell(const cstring *);
void FileCleanup(FILE **);
void ExecuteFromFiles(const int, char **); 
string FileToString(FILE *, xerror *);
cstring *cStringFromFile(FILE *, xerror *);
xerror Compile(const options *, string *, const cstring *);
file *CreateSyntaxTree(const cstring *, const cstring *);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	OptionsParse(&argc, &argv);

	xerror err = XEUNDEFINED;

	if (argv[argc]) {
		err = ExecuteFromFiles(argc, argv);
	} else {
		ShowHeader();
		err = ExecuteFromRepl();
	}

	if (err) {
		xerror_fatal("%s", XerrorDescription(err));
		return EXIT_FAILURE;
	}

	XerrorFlush();
	return EXIT_SUCCESS;
}

//------------------------------------------------------------------------------
//repl management

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

xerror ExecuteFromRepl(void)
{
	xerror err = XEUNDEFINED;
	const xerror fatal_errors = ~(XESHELL | XEPARSE);

	cstring *input = ReadStandardInput();

	while (input) {
		if (input[0] == '$') {
			err = ExecuteShell(input);
		} else {
			err = Compile(input, NULL);
		}

		free(input);

		if (err & fatal_errors) {
			xerror_issue("cannot execute via REPL");
			return err;
		}

		input = ReadStandardInput();
	}

	return XESUCCESS;
}

//if encountered SIGINT return NULL, else return a dynamically allocated pointer
cstring *ReadStandardInput(void)
{
	vstring buffer = vStringInit(KiB(1));

	int prev = 0;
	int curr = 0;

	printf(">>> ");

	while (true) {
		fflush(stdout);

		curr = getchar(); 

		switch(curr) {
		case EOF:
			printf("\n");
			return NULL;

		case '\n':
			if (prev == '\n') {
				vStringTrim(&buffer, '\n');
				return cStringFromvString(&buffer);
			} 
			
			printf("... ");
			fallthrough;

		default:
			vStringAppend(&buffer, (char) curr);
			prev = curr;
			break;
		}
	}
}

xerror ExecuteShell(const cstring *cstr)
{
	assert(cstr);
	assert(cstr[0] == '$');

	cstring *cmd = NULL;
	const cstring *santized_input = cstr + 1;

	if (*sanitized_input) {
		cmd = sanitized_input;
	}

	int err = system(cmd);

	if (err == -1) {
		perror(NULL);
		return XESHELL;
	}

	return XESUCCESS;
}

//------------------------------------------------------------------------------
//file management

void FileCleanup(FILE **handle)
{
	if (*handle) {
		fclose(*handle);
	}
}

void ExecuteFromFiles(const int argc, char **argv)
{
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
	
		RAII(StdlibFree) cstring *src = cStringFromFile(fhandle, &err);

		if (err) {
			xerror_issue("%s: cannot copy file to memory", fname);
			return XEFILE;
		}

		err = Compile(&src, fname);

		if (err) {
			xerror_issue("%s: cannot compile from file", fname);
			return XEFILE;
		}
	}

	return XESUCCESS;
}

//on failure return NULL, else return a dynamically allocated pointer
cstring *cStringFromFile(FILE *openfile, xerror *err)
{
	assert(openfile);
	assert(err);

	size_t filesize = 0;
	*err = GetFilesize(openfile, &filesize);

	if (*err) {
		xerror_issue("cannot calculate file size");
		return NULL;
	}

	const size_t buflen = sizeof(char) * filesize + 1;
	cstring *buffer = AbortMalloc(buflen);

	size_t total_read = fread(buffer, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		*err = XEFILE;
		return NULL;
	}

	buffer[filesize] = '\0';
	*err = XESUCCESS;
	return buffer;
}

//on return the openfile position will be set to zero
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
//compiler passes

xerror Compile(const cstring *src, const cstring *alias)
{
	assert(src);
	assert(alias);

	if (OptionsGetFlag(DIAGNOSTIC_COMPILER_PASSES)) {
		fprintf(stderr, "compiler pass: echo\n");
	}

	file *ast = CreateSyntaxTree(src, alias);

	if (!ast) {
		xerror_issue("cannot create abstract syntax tree");
		return XEPARSE;
	}

	return XESUCCESS;
}

file *CreateSyntaxTree(const cstring *src, const cstring *alias)
{
	assert(src);
	assert(alias);

	file *ast = SyntaxTreeInit(src, alias);

	if (!ast) {
		return NULL;
	}

	if (ast->errors) {
		if (ast->errors > 1) {
			XerrorUser(0, "%zu syntax errors found\n", ast->errors);
		} else {
			XerrorUser(0, "1 syntax error found\n");
		}

		SyntaxTreeFree(ast);
		return NULL;
	}

	return ast;
}
