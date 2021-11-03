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
// of vertices associated with a filename key. Vertex marks are required for the
// topological sorting algorithm.

typedef enum mark {
	UNMARKED,
	TEMPORARY,
	PERMANENT
} mark;

typedef struct vertex {
	file *root;
	mark status;
} vertex;

make_map(vertex, Vertex, static)

typedef map(Vertex) graph;

static vertex VertexInit(const cstring *);
static void VertexFree(vertex);
static graph *GraphInit(void);
static void GraphFree(graph *, bool);
static bool GraphInsert(graph *, const cstring *, vertex);
static vertex GraphSearch(graph *, const cstring *);
static bool GraphModify(graph *, const cstring *, vertex);
static graph *CreateGraph(const cstring *, xerror *);
static vertex ResolveDependency(graph *, const cstring *);
static vertex CreateJob(graph *, const cstring *);
static vector(File) SortGraph(graph *, const cstring *);
const cstring *Visit(graph *, vector(File) *, const cstring *);
static void ReportCycle(const cstring *, const cstring *);

//------------------------------------------------------------------------------
// vertex API

//on failure Throw() either XXIO, XXPARSE, or an XXUSER exception. On success
//returns an unmarked vertex with a valid root pointer.
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

	return (vertex) {
		.root = root,
		.status = UNMARKED
	};
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

	if (free_vertices) {
		VertexMapFree(self, VertexFree);
	} else {
		VertexMapFree(self, NULL);
	}
}

//returns false if the vertex already exists
static bool GraphInsert(graph *self, const cstring *key, vertex v)
{
	assert(self);
	assert(key);
	assert(v.root);

	return VertexMapInsert(self, key, v);
}

//vertex.root == NULL if the vertex does not exist
static vertex GraphSearch(graph *self, const cstring *key)
{
	assert(self);
	assert(key);

	vertex v = {
		.root = NULL
	};

	(void) VertexMapGet(self, key, &v);

	return v;
}

//returns false if the vertex does not exist
static bool GraphModify(graph *self, const cstring *key, vertex v)
{
	assert(self);
	assert(key);

	return VertexMapSet(self, key, v);
}

//------------------------------------------------------------------------------

vector(File) JobsCreate(const cstring *filename, xerror *err)
{
	assert(filename);
	assert(err);

	graph *jobs = CreateGraph(filename, err);

	if (*err) {
		return (vector(File)) {0};
	}

	vector(File) schedule = SortGraph(jobs, filename);

	if (schedule.len == 0) {
		*err = XEUSER;
		GraphFree(jobs, true);
		return (vector(File)) {0};
	}

	GraphFree(jobs, false);

	return schedule;
}

//------------------------------------------------------------------------------

//on failure sets err to one of XEFILE, XEPARSE, XEUSER, or XEUNDEFINED and
//the returned pointer points to a deinitialized graph
static graph *CreateGraph(const cstring *first_jobname, xerror *err)
{
	assert(first_jobname);
	assert(err);

	//the const qualifier guarantees thats the call to GraphFree within the
	//catch block is well defined behavior. On -O2/3 optimisation, gcc may
	//suspect a possible clobber on the jobs pointer if a longjmp occurs.
	//
	//An alternative is to use the volatile qualifier. But this requires
	//changing signatures for downstream functions and slightly defeats the
	//point of optimisation. Responsibility for GraphFree may also be
	//delegated to the caller, but this exposes corrupt graph data. Better
	//to limit the scope and destroy the data quickly.
	graph *const jobs = GraphInit();

	//volatile qualifier prevents -Wclobbered on -O2/3 optimisation due to
	//the catch-block switch statement. 
	volatile CEXCEPTION_T e;

	Try {
		(void) ResolveDependency(jobs, first_jobname);
		*err = XESUCCESS;
	} Catch (e) {
		const cstring *msg = "cannot create graph; %s";

		switch(e) {
		case XXIO:
			xerror_issue(msg, "IO issue");
			*err = XEFILE;
			break;

		case XXPARSE:
			xerror_issue(msg, "AST parsing failed");
			*err = XEPARSE;
			break;

		case XXUSER:
			*err = XEUSER;
			break;

		default:
			xerror_fatal(msg, "unknown exception caught");
			*err = XEUNDEFINED;
			assert(0 != 0);
			break;
		}

		GraphFree(jobs, true);
	}

	return jobs;
}

//As a side effect, this function populates the import.root reference for each
//import node in a given AST. This reference is a backdoor that bypasses symbol
//tables to avoid their public/private verification on variable references. For
//debugging purposes it is occasionally useful to trace through one AST into
//another via the backdoor.
//
//If necessary, in the future this function might only enable the backdoor
//on specific files, such as standard library code.
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

//assumes job does not already exist
static vertex CreateJob(graph *jobs, const cstring *jobname)
{
	assert(jobs);
	assert(jobname);

	vertex job = VertexInit(jobname);

	const bool okay = GraphInsert(jobs, jobname, job);

	assert(okay);

	return job;
}

//------------------------------------------------------------------------------
// modified topological sort; A dependency graph is at least weakly connected
// and has a well-defined start vertex. Therefore, initialising the visit using
// the start vertex will guarantee that every vertex in the graph is visited
// recursively at least once.

//if a cycle is detected the sorting algorithm fails and vector.len is set to 0.
//on success the vector contains references, not copies, to file nodes stored
//in the graph. Deleting the resource in the graph creates a dangling pointer
//in the vector and vise versa.
static vector(File) SortGraph(graph *jobs, const cstring *jobname)
{
	assert(jobs);
	assert(jobname);

	CEXCEPTION_T e;

	vector(File) schedule = FileVectorInit(0, jobs->len);

	Try {
		const cstring *check = Visit(jobs, &schedule, jobname);
		assert(check == NULL);
	} Catch (e) {
		FileVectorReset(&schedule, NULL);
	}

	return schedule;
}

//returns a non-NULL pointer to a jobname when a cycle is detected and then
//Throw()'s an XXUSER exception. On success returns NULL.
const cstring *Visit(graph *jobs, vector(File) *schedule, const cstring *jobname)
{
	assert(jobs);
	assert(jobname);

	vertex job = GraphSearch(jobs, jobname);

	if (job.status == PERMANENT) {
		return NULL;
	}

	if (job.status == TEMPORARY) {
		return jobname;
	}

	job.status = TEMPORARY;
	GraphModify(jobs, jobname, job);

	vector(Import) dependencies = job.root->imports;
	const size_t total_dependencies = dependencies.len;

	size_t i = 0;

	while (i < total_dependencies) {
		const import node = dependencies.buffer[i];

		const cstring *cycle = Visit(jobs, schedule, node.alias);

		if (cycle) {
			ReportCycle(jobname, cycle);
			Throw(XXUSER);
		}

		i++;
	}

	job.status = PERMANENT;
	GraphModify(jobs, jobname, job);

	FileVectorPush(schedule, job.root);
	return NULL;
}

static void ReportCycle(const cstring *jobname_a, const cstring *jobname_b)
{
	const cstring *msg = "%s, %s: circular dependency detected";

	XerrorUser(NULL, 0, msg, jobname_a, jobname_b);
}
