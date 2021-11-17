// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stddef.h>

#include "defs.h"
#include "file.h"
#include "parser.h"
#include "resolver.h"
#include "vector.h"
#include "xerror.h"

static bool ResolveDependencies(network *, const cstring *);
static bool Insert(network *, const cstring *);
static module *InsertNewModule(network *, const cstring *);
static void InsertChildren(network *, module *, const cstring *);
static void Sort(network *, module *);
static void ReportCycle(const cstring *, const cstring *);

static bool ResolveSymbols(network *);
static symbol *InsertSymbol(frame *, const cstring *, symbol);
static void ReportRedeclaration(frame *, const cstring *);
static symtable *PushSymTable(frame *, const tabletag, const size_t);
static symtable *PopSymTable(frame *);
static void ResolveModule(frame *);
static void ResolveImports(frame *);

//------------------------------------------------------------------------------

network *ResolverInit(const cstring *filename)
{
	assert(filename);

	network *net = AbortMalloc(sizeof(network));

	net->dependencies = ModuleGraphInit();
	net->head = NULL;
	net->global = SymTableInit();

	bool ok = ResolveDependencies(net, filename);

	if (!ok) {
		return ResolverFree(net);
	}

	ok = ResolveSymbols(net);

	if (!ok) {
		return ResolverFree(net);
	}

	return net;
}

void *ResolverFree(network *net)
{
	assert(net);

	ModuleGraphFree(&net->dependencies, SyntaxTreeFree);

	SymTableFree(net->global);

	free(net);

	return NULL;
}

//------------------------------------------------------------------------------
// Phase 1:
// Recursive dependency resolution combined with simultaneous topological sort.
// A dependency graph is rooted, acyclic, and directed. By the very definition
// of an import, all verticies are reachable via the root. Since both dependency
// resolution and topological sort algorithms can be implemented via a recusive 
// DFS, all of these facts together imply that the two algorithms can execute
// simultaneously. This halves the algorithmic complexity constant by avoiding
// two separate but identical DFS traversals.

//returns false if failed
static bool ResolveDependencies(network *net, const cstring *filename)
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
	if (ModuleGraphSearch(&net->dependencies, filename, &vertex)) {
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

	bool ok = ModuleGraphInsert(&net->dependencies, filename, ast);
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
// Phase 2:
// A well defined dependency graph now makes it possible to resolve all source
// code symbols. No linker is required as imported ASTs are fully accessible
// via their graph neighbors.

typedef struct frame {
	module *ast;
	symtable *top;
	size_t line;
} frame;

static bool ResolveSymbols(network *net)
{
	assert(net);
	assert(net->head);
	assert(net->global);

	CEXCEPTION_T e;

	module *node = net->head;

	while (ast) {
		Try {
			frame active = (frame) {
				.modname = node->alias,
				.top = net->global,
				.line = 0
			};

			ResolveModule(&active);
		} Catch (e) {
			xerror_fatal("cannot resolve symbols; %s", node->alias);
			return false;
		}

		node = node->next;
	}

	return true;
}

//------------------------------------------------------------------------------
// helper functions for symbol resolution

//throws XXSYMBOL exception if symbol alredy exists in the active table
static symbol *InsertSymbol(frame *self, const cstring *key, symbol value)
{
	assert(self);
	assert(key);

	symbol *ref = SymTableInsert(self->top, key, value);

	if (!ref) {
		ReportRedeclaration(self, key);
		Throw(XXSYMBOL);
	}

	return ref;
}

//notify user; not all symbols have line information so redeclarations of some
//symbols, like native types in the global table, print generic messages.
static void ReportRedeclaration(frame *self, const cstring *key)
{
	assert(self);
	assert(key);
	
	const cstring *longmsg = "%s redeclared; previously on line %zu";
	const cstring *shortmsg = "%s redeclared";

	const cstring *fname = self->ast->alias;
	const size_t line = self->line;
	const size_t prev_line = 0;
	symtable *container = NULL;

	symbol *current = SymTableLookup(self->top, key, &container);

	assert(container == self->top && "redeclaration in new scope");
	assert(current && "false redeclaration; symbol is new");
	assert(current->tag != SYMBOL_NATIVE && "native access in non-global");
	assert(current->tag != SYMBOL_MODULE && "module redeclared itself");

	switch (current->tag) {
	case SYMBOL_IMPORT:
		prev_line = current->import_data.node->line;
		break;

	case SYMBOL_FUNCTION:
		prev_line = current->function_data.node->line;
		break;

	case SYMBOL_METHOD:
		prev_line = current->function_data.node->line;
		break;

	case SYMBOL_UDT:
		prev_line = current->function_data.node->line;
		break;

	case SYMBOL_VARIBLE:
		prev_line = current->function_data.node->line;
		break;

	default:
		assert(prev_line == 0);
		break;
	}

	if (prev_line) {
		XerrorUser(fname, line, longmsg, key, prev_line); 
	} else {
		XerrorUser(fname, line, shortmsg, key);
	}
}

//returned pointer is always non-null
static symtable *PushSymTable(frame *self, const tabletag tag, const size_t cap)
{
	assert(self);

	symtable *top = SymTableSpawn(self->top, tag, cap);

	self->top = top;

	return top;
}

//restore the parent of the current active table; returns the child
static symtable *PopSymTable(frame *self)
{
	assert(self);

	symtable *old_top = self->top;

	assert(old_top->parent && "attempted to pop global symbol table");

	self->top = old_top->parent;

	return old_top;
}

//------------------------------------------------------------------------------

static void ResolveModule(frame *self)
{
	assert(self);

	module *node = self->ast;
	
	const size_t capacity = node->imports.len + node->declarations.len;
	
	node->table = PushSymTable(self, TABLE_MODULE, capacity);

	ResolveImports(self);
}

static void ResolveImports(frame *self)
{
	assert(self);

	symbol entry = {
		.tag = SYMBOL_IMPORT,
		.import = {
			.referenced = false
		}
	};

	vector(Import) imports = self->imports;

	for (size_t i = 0; i < imports.len; i++) {
		import *node = &imports.buffer[i];
		
		const cstring *name = node->alias;
		self->line = node->line;

		symbol *ref = InsertSymbol(self, name, entry);
		node->entry = ref;
	}
}
