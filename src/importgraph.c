// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "importgraph.h"
#include "defs.h"
#include "lib/vector.h"
#include "lib/map.h"

static void FileCleanup(FILE **);
static cstring *LoadFile(const cstring *);
static cstring *cStringFromFile(FILE *);
static size_t GetFileSize(FILE *);
static xerror Insert(map(Vertex) *, const cstring *);

//------------------------------------------------------------------------------
// the finite import graph is implemented as an adjacency list. Each vertex is
// represented as a (cstring, file) key-value pair within a hash table. The
// vector<import> within the file node provides the list of neighbor vertices.

make_map(file *, Vertex, static)

//------------------------------------------------------------------------------
// API implementation

xerror ImportGraphSort(const cstring *fname, file *ast[])
{
	assert(fname);
	assert(ast);
}

//------------------------------------------------------------------------------
// stage 1; recursive dependency resolution

//on failure sets err to one of XEFILE, XEPARSE, XEUSER, or XEUNDEFINED and
//the returned graph is not functional.
static map(Vertex) CreateGraph(const cstring *fname, xerror *err)
{
	assert(fname);
	assert(err);

	CEXCEPTION_T exception;
	const cstring *msg = "cannot create graph; %s";

	map(Vertex) import_graph = VertexGraphInit(MAP_DEFAULT_CAPACITY);

	Try {
		(void) InsertVertex(&import_graph, fname);
		*err = XESUCCESS;
	} Catch (exception) {
		switch (exception) {
		case XXIO:
			xerror_issue(msg, "IO issue");
			*err = XEFILE;
			break;

		case XXPARSE:
			xerror_issue(msg, "AST parsing failed");
			*err = XEPARSE;
			break;

		case XXUSER:
			XerrorUser(NULL, 0, "compilation failed");
			*err = XEUSER;

		default:
			xerror_fatal(msg, "unknown exception caught");
			*err = XEUNDEFINED;
			assert(0 != 0);
			break;
		}

		VertexMapFree(&import_graph, SyntaxTreeFree);
	}

	return import_graph;
}

//indirect recursion via InsertNeighbors. May Throw() an XXIO or XXPARSE 
//exception. The returned pointer is non-NULL.
static file *InsertVertex(map(Vertex) *import_graph, const cstring *fname)
{
	assert(import_graph);
	assert(fname);

	CXECEPTION_T exception = 0;

	file *root = FetchVertex(import_graph, fname);

	if (root) {
		return root;
	}

	cstring *source_code = LoadFile(fname);

	if (!source_code) {
		exception = XXIO;
		goto fail;
	}

	root = SyntaxTreeInit(source_code, fname);

	if (!root) {
		exception = XXPARSE;
		goto cleanup_source;
	}

	if (root->errors) {
		exception = XXUSER;
		goto cleanup_root;
	}

	(void) VectorMapInsert(import_graph, fname, root);

	InsertNeighbors(import_graph, root->imports);

	return root;

cleanup_root:
	SyntaxTreeFree(root);
cleanup_source:
	free(source_code);
fail:
	Throw(exception);
}

//returns NULL if the vertex does not exist in the graph
static file *FetchVertex(map(Vertex) *import_graph, const cstring *fname)
{
	assert(import_graph);
	assert(fname);

	file *root = NULL;

	(void) VertexMapGet(import_graph, fname, &root);

	return root;
}

//note; indirect recursion.
//this function populates the import.root member ignored by the parser.
static InsertNeighbors(map(Vertex) *import_graph, vector(Import) imports)
{
	assert(import_graph);

	for (size_t i = 0; i < imports.len; i++) {
		import *node = &imports.buffer[i];
		node->root = InsertVertex(import_graph, node.alias);
	}
}

//------------------------------------------------------------------------------
// file management

static void FileCleanup(FILE **handle)
{
	if (*handle) {
		fclose(*handle);
	}
}

//on failure returns NULL, else returns a dynamically allocated cstring. Any
//errors are reported to the xerror log.
static cstring *LoadFile(const cstring *filename)
{
	assert(filename);

	RAII(FileCleanup) FILE *fp = fopen(fname, "r");

	if (!fp) {
		xerror_issue("%s: %s", fname, strerror(errno));
		return NULL;
	}

	cstring *src = cStringFromFile(fp);

	if (!src) {
		const cstring *msg = XerrorDescription(*err);
		xerror_issue("%s: %s: cannot copy file to memory", fname, msg);
	}

	return src;
}

//on failure returns NULL, else returns a dynamically allocated cstring
static cstring *cStringFromFile(FILE *openfile)
{
	assert(openfile);
	assert(err);

	size_t filesize = GetFileSize(openfile);

	if (!filesize) {
		xerror_issue("cannot calculate file size");
		return NULL;
	}

	const size_t buflen = sizeof(char) * filesize + 1;
	cstring *buffer = AbortMalloc(buflen);

	size_t total_read = fread(buffer, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		return NULL;
	}

	buffer[filesize] = '\0';
	return buffer;
}

//on failure returns zero, the openfile position will be set to zero on success
static size_t GetFileSize(FILE *openfile)
{
	assert(openfile);
	assert(bytes);

	rewind(openfile);

	long err = fseek(openfile, 0L, SEEK_END);

	if (err == -1) {
		xerror_issue("fseek: cannot move to EOF: %s", strerror(errno));
		return 0;
	}

	long bytecount = ftell(openfile);

	if (bytecount == -1) {
		xerror_issue("ftell: cannot fetch count: %s", strerror(errno));
		return 0;
	}

	rewind(openfile);

	return (size_t) bytecount;
}
