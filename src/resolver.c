// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stddef.h>

#include "defs.h"
#include "file.h"
#include "resolver.h"
#include "vector.h"
#include "xerror.h"


//------------------------------------------------------------------------------

network *ResolverInit(const cstring *filename)
{
	assert(filename);

	network *net = AbortMalloc(sizeof(network));

	net->dependencies = ModuleGraphInit();
	net->head = NULL;
	net->global = NULL;

	bool error = ResolveImports(net, filename);

	if (error) {
		return ResolverFree(net);
	}

	return net;
}

void *ResolverFree(network *net)
{
	assert(net);

	ModuleGraphFree(&net->dependencies, SyntaxTreeFree);

	free(net);

	return NULL;
}

//------------------------------------------------------------------------------
// Recursive dependency resolution combined with simultaneous topological sort.
// A dependency graph is rooted, acyclic, and directed. By the very definition
// of an import, all verticies are reachable via the root. Since both dependency
// resolution and topological sort algorithms can be implemented via a recusive 
// DFS, all of these facts together imply that the two algorithms can execute
// simultaneously. This halves the algorithmic complexity constant by avoiding
// two separate but identical DFS traversals.

static bool ResolveImports(network *net, const cstring *filename)
{
	assert(net);
	assert(filename);

	CEXCEPTION_T e;

	Try {
		bool ok = Insert(net, filename);
		assert(ok && "base case triggered on first insertion");
	} Catch (e) {
		xerror_fatal("cannot resolve dependencies");
		return false;
	}

	return true;
}

// When a vertex is visited twice, if its first visit is currently being tracked
// higher in the call stack then there must be a cycle. If it isn't in the call
// stack then the vertex must be the child of two independent parent verticies.
enum {
	ON_CALL_STACK = false,
	OFF_CALL_STACK = true
};

static bool Insert(network *net, const cstring *filename)
{
	assert(net);
	assert(filename);

	module *vertex = NULL;

	//base case
	if (GraphSearch(net->dependencies, filename, &vertex)) {
		return vertex->flag;
	}

	//recursive case
	vertex = InsertNewModule(net, filename);
	InsertChildren(net, vertex, filename);
	Sort(net, vertex);

	return (vertex->flag = OFF_CALL_STACK);
}

//create a module with module.flag == ON_CALL_STACK. Throw XXGRAPH on failure.
static module *InsertNewModule(network *net, const cstring *filename)
{
	assert(net);
	assert(filename);

	module *ast = SyntaxTreeInit(filename);

	if (!ast) {
		ThrowFatal(XXGRAPH, "cannot create ast; %s", filename);
	}

	ast->flag = ON_CALL_STACK;

	bool ok = GraphInsert(&net->dependencies, filename, ast);
	assert(ok && "parent inserted on base case");

	return ast;
}

//depth-first traversal; indirect recursion on Insert(); Throw XXGRAPH on cycle
static 
void InsertChildren(network *net, module *parent, const cstring *parentname)
{
	assert(net);
	assert(parent);
	assert(parentname);

	const vector(Import) imports = parent->imports;

	for (size_t i = 0; i < imports.len; i++) {
		const cstring *childname = imports.buffer[i].alias;
		
		const bool status = Insert(net, childname);

		if (status == ON_CALL_STACK) {
			ReportCycle(parentname, childname);
			Throw(XXGRAPH);
		}
	}


}

//since C does not have closures a cheeky static variable lets us keep track 
//of the last vertex that was threaded through the intrusive list.
static void Sort(network *net, module *curr)
{
	assert(net);
	assert(curr);

	static module *prev = NULL;

	if (!prev) {
		net->head = curr;
	} else {
		prev->next = curr;
	}

	curr->next = NULL;
	prev = curr;
}

//inform user of the circular dependency
//note; the dependency might not actually exist. the user might have just
//imported the name without using it. But at this stage symbol resolution has
//not occured so usage statistics are not available.
static void ReportCycle(const cstring *parent, const cstring *child)
{
	const cstring *msg = "%s, %s: circular dependency detected";

	RAII(cStringFree) cstring *fparent = FileGetDiskName(parent);
	RAII(cStringFree) cstring *fchild = FileGetDiskName(child);

	XerrorUser(NULL, 0, msg, fparent, fchild);
}

//------------------------------------------------------------------------------
