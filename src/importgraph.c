// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
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
// the import graph is represented as an adjacency list, implemented as a hash
// table of (cstring, vertex_info) key-value pairs.

make_vector(cstring *, Edge, static)

//ast is NULL if the associated vertex hasn't been processed
//TODO don't need a vector anymore, can use the import vector within the ast
typedef struct vertex_info {
	file *ast;
	vector(Edge) edges;
} vertex_info;

make_map(vertex_info, Vertex, static)

//------------------------------------------------------------------------------

xerror ImportGraphInit(const cstring *input_file, tagged_ast **list)
{
	assert(input_file);
	assert(list);

	map(Vertex) import_graph = VertexMapInit(MAP_DEFAULT_CAPACITY);

	xerror err = Insert(import_graph, input_file);

	if (err) {
		xerror_issue("cannot create import graph");
		return err;
	}

	return XESUCCESS;
}

//TODO switch to exception handler provided everything in the map can be
//created without additional malloc calls? when you create the AST, you'll
//need to somehow deallocate them if an error occurs. 
static xerror Insert(map(Vertex) *import_graph, const cstring *filename)
{
	assert(import_graph);
	assert(filename);

	//recursion base case; if the vertex already exists in the graph
	//then it is already being processed higher in the call stack.
	if (VertexMapGet(import_graph, filename, NULL)) {
		return XESUCCESS;
	}

	RAII(cStringFree) cstring *source_code = LoadFile(filename);

	if (!source_code) {
		xerror_issue("%s: cannot load into memory", filename);
		return XEFILE;
	}

	vector_info vinfo = {
		.ast = SyntaxTreeInit(source_code, filename);
		.edges = EdgeVectorInit(0, VECTOR_DEFAULT_CAPACITY);
	};

	if (!ast) {
		xerror_issue("%s: cannot create ast");
		return XEPARSE;
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

//on failure returns NULL, else returns a dynamically allocated cstring
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
