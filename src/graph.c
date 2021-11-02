// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// the finite import graph is implemented as an adjacency list. Each vertex is
// represented as a (cstring, file) key-value pair within a hash table. The
// vector<import> within the file node provides the list of neighbor vertices.
// Directed edges represent dependencies such that A -> B := A depends on B.

#include <assert.h>
#include <stdbool.h>

#include "graph.h"
#include "defs.h"
#include "file.h"
#include "lib/vector.h"
#include "lib/map.h"

//------------------------------------------------------------------------------
// The finite import graph is implemented as an adjacency list where each vertex
// is associated with a string key and placed into a hash table.

typedef struct vertex {
	file *root;
} vertex;

make_map(vertex, Vertex, static)

typedef map(Vertex) graph;

//------------------------------------------------------------------------------
// vertex API

//on failure Throw() either XXIO, XXPARSE, or an XXUSER exception.
static vertex VertexInit(const cstring *fname)
{
	assert(fname);

	cstring *source_code = FileLoad(fname);

	if (!source_code) {
		Throw(XXIO);
	}

	file *root = SyntaxTreeInit(source_code, fname);

	if (!root) {
		free(source_code);
		Throw(XXPARSE);
	}

	if (root->errors) {
		free(source_code);
		SyntaxTreeFree(root);
		Throw(XXUSER);
	}

	return (vertex) {.root = root};
}

static void VertexFree(vertex v)
{
	SyntaxTreeFree(v.root);
}

//------------------------------------------------------------------------------
// graph API

static graph *GraphInit(void)
{
	graph *g = AbortMalloc(sizeof(graph));

	*g = VertexMapInit(MAP_DEFAULT_CAPACITY);

	return g;
}

//okay if *g == NULL. sets *g == NULL on return.
static void GraphFree(graph **g, bool remove_vertices)
{
	assert(g);

	if (!*g) {
		return;
	}

	if (remove_verticies) {
		VertexMapFree(*g, VertexFree);
	} else {
		VertexMapFree(*g, NULL);
	}

	*g = NULL;
}

static bool GraphInsert(graph *g, const cstring *name, vertex v)
{
	assert(g);
	assert(name);
	assert(v.root);

	return VertexMapInsert(g, name, v);
}

//if vertex does not exist in the graph then v.root == NULL
static vertex GraphSearch(graph *g, const cstring *name)
{
	assert(g);
	assert(name);

	vertex v = {
		.root = NULL
	};

	(void) VertexMapGet(g, name, &v);

	return v;
}

//------------------------------------------------------------------------------

xerror GraphSort(const cstring *fname, file *ast[])
{
	assert(fname);
	assert(ast);

	xerror err = XEUNDEFINED;

	graph *g = CreateDependencyGraph(fname, &err);
}

//------------------------------------------------------------------------------

//on failure sets err to one of XEFILE, XEPARSE, XEUSER, or XEUNDEFINED and
//the returned pointer is NULL
static graph *CreateDependencyGraph(const cstring *fname, xerror *err)
{
	assert(fname);
	assert(err);

	CEXCEPTION_T exception;
	const cstring *msg = "cannot create graph; %s";

	graph *g = GraphInit();

	Try {
		(void) ResolveDependency(g, fname);
		*err = XESUCCESS;
	} Catch (exception) {
		switch(exception) {
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

		GraphFree(&g, true);
	}

	return g;
}

//May indirectly Throw() XXIO, XXPARSE, or XXUSER.
static vertex ResolveDependency(graph *g, const cstring *fname)
{
	assert(g);
	assert(fname);

	vertex v = GraphSearch(g, fname);

	if (v.root) {
		return v;
	}

	v = VertexInit(fname);

	(void) GraphInsert(g, fname, v);

	vector(Import) dependencies = v.root->imports;

	for (size_t i = 0; i < dependencies.len; i++) {
		import *node = &dependencies.buffer[i];
		vertex new = ResolveDependency(g, node->alias);
		node->root = new.root;
	}

	return v;
}
