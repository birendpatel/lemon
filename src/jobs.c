// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "jobs.h"
#include "defs.h"
#include "file.h"
#include "lib/map.h"

//------------------------------------------------------------------------------
// The job graph is represented by an adjacency list implemented as a hash table
// of vertices associated with a filename key.

typedef struct vertex {
	file *root;
} vertex;

make_map(vertex, Vertex, static)

typedef map(Vertex) graph;

//------------------------------------------------------------------------------
// vertex API

//on failure Throw() either XXIO, XXPARSE, or an XXUSER exception.
static vertex VertexInit(const cstring *filename)
{
	assert(filename);

	cstring *source_code = FileLoad(filename);

	if (!source_code) {
		Throw(XXIO);
	}

	file *root = SyntaxTreeInit(source_code, filename);

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

//okay if v.root == NULL 
static void VertexFree(vertex self)
{
	SyntaxTreeFree(self.root);
}

//------------------------------------------------------------------------------
// graph API

static graph *GraphInit(void)
{
	graph *g = AbortMalloc(sizeof(graph));

	*g = VertexMapInit(MAP_DEFAULT_CAPACITY);

	return g;
}

//okay if g == NULL 
static void GraphFree(graph *self, bool free_vertices)
{
	if (!self) {
		return;
	}

	if (free_verticies) {
		VertexMapFree(self, VertexFree);
	} else {
		VertexMapFree(self, NULL);
	}
}

//returns false if the vertex already exists
static bool GraphInsert(graph *self, const cstring *filename, vertex v)
{
	assert(self);
	assert(name);
	assert(v.root);

	return VertexMapInsert(self, name, v);
}

//on return vertex.root == NULL if the vertex does not exist
static vertex GraphSearch(graph *self, const cstring *name)
{
	assert(self);
	assert(name);

	vertex v = {
		.root = NULL
	};

	(void) VertexMapGet(self, name, &v);

	return v;
}

//------------------------------------------------------------------------------

vector(File) JobsCreate(const cstring *filename, xerror *err)
{
	assert(filename);
	assert(err);

	graph *jobs = CreateJobGraph(filename, err);


	GraphFree(jobs, false);
}

//------------------------------------------------------------------------------

//on failure sets err to one of XEFILE, XEPARSE, XEUSER, or XEUNDEFINED and
//the returned pointer is NULL
static graph *CreateJobGraph(const cstring *first_jobname, xerror *err)
{
	assert(first_jobname);
	assert(err);

	CEXCEPTION_T exception;
	const cstring *msg = "cannot create graph; %s";

	graph *jobs = GraphInit();

	Try {
		(void) ResolveDependency(first_jobname, filename);
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

		GraphFree(jobs, true);
		jobs = NULL;
	}

	return jobs;
}

//as a side effect, this function populates the import.root reference for each
//import node in a given AST. This enrichment technically converts ASTs into
//graphs. The first call to this recursive function creates one massive directed
//and possibly cyclic abstract syntax graph.
static vertex ResolveDependency(graph *jobs, const cstring *jobname)
{
	assert(jobs);
	assert(jobname);

	vertex job = GraphSearch(jobs, jobname);
	const bool job_exists = job.root;

	if (job_exists) {
		return job;
	} else {
		job = CreateJob(jobs, jobname);
	}

	vector(Import) dependencies = job.root->imports;
	const size_t total_dependencies = dependencies.len;

	size_t i = 0;

	while (i < total_dependencies) {
		import *node = &dependencies.buffer[i];

		vertex referenced_job = ResolveDependency(jobs, node->alias);

		node->root = referenced_job.root;

		i++;
	}

	return job;
}

//assumes job does not exist
static vertex CreateJob(graph *jobs, const cstring *jobname)
{
	assert(jobs);
	assert(jobname);

	vertex job = VertexInit(jobname);

	(void) GraphInsert(jobs, jobname, job);

	return job;
}
