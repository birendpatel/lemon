// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stddef.h>

#include "defs.h"
#include "file.h"
#include "resolver.h"
#include "vector.h"
#include "xerror.h"

graph(Module) ResolverInit(const cstring *filename)
{
	assert(filename);

	graph(Module) graph = ModuleGraphInit();

	bool error = ResolveImports(&graph, filename);

	if (error) {
		ResolverFree(&graph);
		return ModuleGraphGetZero();
	}
}

void ResolverFree(graph(Module) *graph)
{
	assert(graph);

	ModuleGraphFree(graph, SyntaxTreeFree);
}

//------------------------------------------------------------------------------
// Recursive dependency resolution combined with simultaneous topological sort.
// Considered separately, the sorting and resolution algorithms are depth-first
// recursive traversals. The dependency graph, by the definition of an import,
// has a well-defined start vertex (the ResolverInit input filename). These two
// facts together imply the dependency graph must be at least weakly connected.
// Therefore, if a DFS is initialised on the start vertex then it is guaranteed
// that every vertex will be visited at least once. And so the sorting algorithm
// can not only be simplified (no O(n) start loop) but also embedded into the
// resolution algorithm.
//
// When a vertex is visited twice, if its first visit is currently being tracked
// higher in the call stack then there must be a cycle. If it isn't in the call
// stack then the vertex must be the child of two independent parent verticies.

enum {
	ON_CALL_STACK = false,
	OFF_CALL_STACK = true
};

static bool ResolveImports(graph(Module) *self, const cstring *filename)
{
	assert(self);
	assert(filename);

	CEXCEPTION_T e;

	Try {
		module *root = NewModule(filename);
		(void) Insert(self, root, filename);
	} Catch (e) {
		xerror_fatal("cannot resolve dependencies");
		return false;
	}

	return true;
}

static bool Insert(graph(Module) *self, module *parent, const cstring *filename)
{
	assert(self);
	assert(parent);
	assert(filename);

	module *ast = NULL;

	//base case
	if (GraphSearch(self, filename, &ast)) {
		return ast->flag;
	}

	ast = NewModule(filename);
	(void) GraphInsert(self, filename, ast);

	const vector(Import) imports = ast->imports;

	//depth-first traversal
	for (size_t i = 0; i < imports.len; i++) {
		const cstring *childname = imports.buffer[i].alias;
		
		const bool status = Insert(self, ast, childname);

		if (status == ON_CALL_STACK) {
			ReportCycle(filename, childname);
			Throw(XXGRAPH);
		}
	}

	//thread the intrusive list (sort)
	parent->next = ast;
	ast->next = NULL;
	
	ast->flag = OFF_CALL_STACK;
	return ast->flag;
}

//create a module with module.flag == ON_CALL_STACK. Throw XXGRAPH on failure.
static module *NewModule(const cstring *filename)
{
	assert(filename);

	module *ast = SyntaxTreeInit(filename);

	if (!ast) {
		ThrowFatal(XXGRAPH, "cannot create ast; %s", filename);
	}

	ast->flag = ON_CALL_STACK;

	return ast;
}

//inform user of the circular dependency
//note; the dependency might not actually exist. the user might have just
//imported the name without using it. But at this stage symbol resolution has
//not occured so usage statistics are not available.
static void ReportCycle(const cstring *name1, const cstring *name2)
{
	const cstring *msg = "%s, %s: circular dependency detected";

	RAII(cStringFree) cstring *fname1 = FileGetDiskName(name1);
	RAII(cStringFree) cstring *fname2 = FileGetDiskName(name2);

	XerrorUser(NULL, 0, msg, fname1, fname2);
}

