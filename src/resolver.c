// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "jobs.h"
#include "defs.h"
#include "file.h"
#include "xerror.h"
#include "lib/map.h"

//------------------------------------------------------------------------------
// The job graph is an adjacency list. It is implemented as a hash table of
// verticies associated with a filename key. The vertex.on_call_stack member
// is required for the topological sorting algorithm.

typedef struct vertex {
	file *root; 
	bool on_call_stack; 
} vertex;

make_map(vertex, Vertex, static)

typedef map(Vertex) graph;

#define ZERO_VERTEX (vertex) {.root = NULL, .on_call_stack = false}

//------------------------------------------------------------------------------
// vertex API

static vertex VertexInit(const cstring *, const mark);
static void VertexFree(vertex);

//returns the zero vertex on failure
static vertex VertexInit(const cstring *filename)
{
	assert(filename);

	RAII(cStringFree) cstring *code = FileLoad(filename);

	if (!code) {
		return ZERO_VERTEX;
	}
	
	file *root = SyntaxTreeInit(code, filename);

	if (!root) {
		return ZERO_VERTEX;
	}

	vertex v = {
		.root = root,
		.on_call_stack = false
	};

	return v;
}

//okay if v.root == NULL or v == ZERO_VERTEX
static void VertexFree(vertex self)
{
	SyntaxTreeFree(self.root);
}

//------------------------------------------------------------------------------
// graph API

static graph *GraphInit(void);
static void GraphFree(graph **, bool);
static bool GraphInsert(graph *, const cstring *, vertex);
static vertex GraphSearch(graph *, const cstring *);
static bool GraphModify(graph *, const cstring *, vertex);

//returned pointer is dynamically allocated
static graph *GraphInit(void)
{
	graph *g = AbortMalloc(sizeof(graph));

	*g = VertexMapInit(MAP_DEFAULT_CAPACITY);

	return g;
}

//release the graph and *self; set *self to NULL on return
static void GraphFree(graph **self, bool free_vertices)
{
	assert(self);
	assert(*self);

	if (free_vertices) {
		VertexMapFree(*self, VertexFree);
	} else {
		VertexMapFree(*self, NULL);
	}

	free(*self);

	*self = NULL;
}

//returns false if the vertex already exists
static bool GraphInsert(graph *self, const cstring *key, vertex v)
{
	assert(self);
	assert(key);
	assert(v.root);

	return VertexMapInsert(self, key, v);
}

//returns a dummy zero vertex if the vertex does not exist
static vertex GraphSearch(graph *self, const cstring *key)
{
	assert(self);
	assert(key);

	vertex v = ZERO_VERTEX;

	(void) VertexMapGet(self, key, &v);

	return v;
}

//returns true if the vertex associated with key exists in the graph
static bool GraphContainsVertex(graph *self, const cstring *key)
{
	assert(self);
	assert(key);

	return VertexMapGet(self, key, NULL);
}

//returns false if the vertex does not exist
static bool GraphModify(graph *self, const cstring *key, vertex v)
{
	assert(self);
	assert(key);

	return VertexMapSet(self, key, v);
}

//------------------------------------------------------------------------------

static bool ResolveDependency(graph *, vector(File) *, const cstring *);
static vertex CreateJob(graph *, const cstring *);
static bool JobExists(vertex);
static bool IsCyclic(vertex);
static void ReportCycle(const cstring *, const cstring *);

vector(File) JobsCreate(const cstring *rootname)
{
	assert(rootname);

	CEXCEPTION_T e;

	graph *jobs = GraphInit();
	vector(File) schedule = FileVectorInit(0, VECTOR_DEFAULT_CAPACITY);

	Try {
		bool cycle = ResolveDependency(jobs, &schedule, rootname);
		assert(!cycle);
	} Catch (e) {
		xerror_fatal("cannot resolve dependencies");
		GraphFree(&jobs, true);
		FileVectorReset(&schedule, NULL);
		return ZERO_VECTOR(vector(File));
	}

	GraphFree(&jobs, false);

	return schedule;
}

//The dependency graph has a well-defined start vertex (the main file) and the
//sorting and resolution algorithms are both recursive depth-first traversals. 
//These two facts together imply that the resolution and sort can be performed
//simultaneously. This is because a dependency graph must be, at a minimum,
//weakly connected. Therefore by initialising a DFS on the start vertex we can
//guarantee that all verticies are visited at least once.
//
//when a vertex is visted twice, if its first visit is currently being tracked
//higher in the call stack then there must be a cycle. If it isn't in the
//call stack it simply means that the vertex is the child of two independent
//parent verticies.
static bool ResolveDependency
(
	graph *jobs, 
	vector(File) *schedule, 
	const cstring *rootname
)
{
	assert(jobs);
	assert(schedule);
	assert(rootname);

	vertex job = GraphSearch(jobs, rootname);

	if (JobIsValid(job)) {
		return job.on_call_stack;
	}

	job = CreateJob(rootname);

	const vector(Import) imports = job.root->imports;

	for (size_t i = 0; i < imports.len; i++) {
		const cstring *jobname = imports.buffer[i].alias;
		
		const bool cycle = ResolveDependency(jobs, schedule, jobname);

		if (cycle) {
			ReportCycle(rootname, jobname);
			Throw(XXGRAPH);
		}
	}

	job.on_call_stack = false;
	GraphModify(jobs, rootname, job);

	FileVectorPush(schedule, job.root);
	return false;
}

//insert a vertex which is known to not exist into the graph; throw an XXGRAPH
//exception if the vertex cannot be created
static vertex CreateJob(graph *jobs, const cstring *jobname)
{
	assert(jobs);
	assert(jobname);

	vertex job = VertexInit(jobname);

	if (!JobIsValid(job)) {
		ThrowFatal(XXGRAPH, "cannot create job for %s", jobname);
	}

	bool ok = GraphInsert(jobs, jobname, job);
	assert(ok && "contract not upheld; vertex already exists");

	return job;
}

//returns true if the input vertex is not equivalent to the zero vertex
static bool JobIsValid(vertex job)
{
	const vertex dummy = ZERO_VERTEX;

	return job.root != dummy.root || job.root != dummy.on_call_stack;
}

static void ReportCycle(const cstring *job_1, const cstring *job_2)
{
	const cstring *msg = "%s, %s: circular dependency detected";

	RAII(cStringFree) cstring *file_1 = FileGetDiskName(job_1);
	RAII(cStringFree) cstring *file_2 = FileGetDiskName(job_2);

	XerrorUser(NULL, 0, msg, file_1, file_2);
}

