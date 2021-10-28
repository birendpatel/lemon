// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// main.c compiles lemon source code into bytecode and executes it on a virtual
// machine.
//
// The program begins by parsing the input file into an abstract syntax tree.
// A directed dependency graph is then created recursively depth-first by
// analysing the import statements. Graph nodes represent the imported files
// and directed edges represent dependencies.
//
// Once the graph is finalized, the program verifies that it is acyclic. Then,
// a symbol table is created for each graph node in topological order. Semantic
// analysis is carried on each node independently.
//
// Finally, the optimisation and bytecode generation phases are performed and
// the output program is sent to the virtual machine for execution. The virtual
// machine controls all runtime aspects including garbage collection, overflow
// detection, and debugging.

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
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
void FileCleanup(FILE **);
xerror LoadFile(const cstring *);
xerror ExecuteFromFiles(const int, char **); 
cstring *cStringFromFile(FILE *, xerror *);
xerror GetFilesize(FILE *, size_t *);
xerror Compile(const cstring *, const cstring *);
file *CreateSyntaxTree(const cstring *, const cstring *);

//------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	OptionsParse(&argc, &argv);

	const cstring *input_file = argv[0];

	if (!input_file) {
		const cstring *msg = "no input files; nothing to compile";
		XerrorUser(NULL, 0, msg);
		return EXIT_FAILURE;
	}

	const cstring *sentinel = argv[1];

	if (sentinel != NULL) {
		const cstring *msg = "all input files except %s were ignored";
		XerrorUser(NULL, 0, msg, input_file);
	}

	xerror err = LoadFile(input_file);

	if (err) {
		xerror_fatal("%s", XerrorDescription(err));
		return EXIT_FAILURE;
	}

	XerrorFlush();
	return EXIT_SUCCESS;
}

void FileCleanup(FILE **handle)
{
	if (*handle) {
		fclose(*handle);
	}
}

xerror LoadFile(const cstring *fname)
{
	xerror err = XEUNDEFINED;

	RAII(FileCleanup) FILE *fp = fopen(fname, "r");

	if (!fp) {
		xerror_issue("%s: %s", fname, strerror(errno));
		return XEFILE;
	}

	RAII(cStringFree) cstring *src = cStringFromFile(fp, &err);

	if (err) {
		xerror_issue("%s: cannot copy file to memory", fname);
		return XEFILE;
	}

	err = Compile(src, fname);

	if (err) {
		xerror_issue("%s: cannot compile from file", fname);
		return XEFILE;
	}

	return XESUCCESS;
}

//on failure returns NULL, else returns a dynamically allocated cstring
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

	bool trace = OptionsGetFlag(DIAGNOSTIC_COMPILER_PASSES);

	if (trace) {
		fprintf(stderr, "compiler pass: parsing\n");
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
			const cstring *msg = "%zu syntax errors found\n";
			XerrorUser(alias, 0, msg, ast->errors);
		} else {
			const cstring *msg = "1 syntax error found\n";
			XerrorUser(alias, 0, msg); 
		}

		SyntaxTreeFree(ast);
		return NULL;
	}

	return ast;
}
