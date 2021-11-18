// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stddef.h>

#include "defs.h"
#include "file.h"
#include "parser.h"
#include "resolver.h"
#include "vector.h"
#include "xerror.h"

typedef struct frame frame;

static bool ResolveDependencies(network *, const cstring *);
static bool InsertModule(network *, const cstring *);
static void InsertChildren(network *, module *, const cstring *);
static void Sort(network *, module *);
static void ReportCycle(const cstring *, const cstring *);

static bool ResolveSymbols(network *);
static symbol *InsertSymbol(frame *, const cstring *, symbol);
static void ReportRedeclaration(frame *, const cstring *);
static symtable *PushSymTable(frame *, const tabletag, const size_t);
static symtable *PopSymTable(frame *);
static type *GetBaseType(frame *, type *);
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
// Phase 1: resolve import directives
//
// Dependency resolution is combined with a simultaneous topological sort. This
// is possible because a well-formed dependency digraph is rooted and acyclic
// and all vertices are reachable via the root (by the definition of an import).
// Since both dependency resolution and sorting are recursive DFS algorithms
// they can share the traversal logic.

//returns false if failed
static bool ResolveDependencies(network *net, const cstring *filename)
{
	assert(net);
	assert(filename);

	CEXCEPTION_T e;

	Try {
		bool ok = InsertModule(net, filename);
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

static bool InsertModule(network *net, const cstring *filename)
{
	assert(net);
	assert(filename);

	module *vertex = NULL;

	if (ModuleGraphSearch(&net->dependencies, filename, &vertex)) {
		return vertex->flag;
	}

	vertex = SyntaxTreeInit(filename);

	if (!vertex) {
		ThrowFatal(XXGRAPH, "cannot create AST; %s", filename);
	}

	(void) ModuleGraphInsert(&net->dependencies, filename, vertex);
	vertex->flag = ON_CALL_STACK;
	InsertChildren(net, vertex, filename);
	Sort(net, vertex);

	return (vertex->flag = OFF_CALL_STACK);
}

//indirect recursion via InsertModule(); Throw XXGRAPH on cycle
static
void InsertChildren(network *net, module *parent, const cstring *parentname)
{
	assert(net);
	assert(parent);
	assert(parentname);

	const vector(Import) imports = parent->imports;

	for (size_t i = 0; i < imports.len; i++) {
		const cstring *childname = imports.buffer[i].alias;

		const bool status = InsertModule(net, childname);

		if (status == ON_CALL_STACK) {
			ReportCycle(parentname, childname);
			Throw(XXGRAPH);
		}
	}


}

//since C does not allow closures a cheeky static variable keeps track of the
//previous vertex that was threaded through the intrusive list.
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

static void ReportCycle(const cstring *parent, const cstring *child)
{
	const cstring *msg = "%s has circular dependency with %s";

	RAII(cStringFree) cstring *fparent = FileGetDiskName(parent);
	RAII(cStringFree) cstring *fchild = FileGetDiskName(child);

	XerrorUser(NULL, 0, msg, fparent, fchild);
}

//------------------------------------------------------------------------------
// Phase 2: resolve symbols

struct frame {
	module *ast;
	symtable *top;
	size_t line;
};

static bool ResolveSymbols(network *net)
{
	assert(net);
	assert(net->head);
	assert(net->global);

	CEXCEPTION_T e;

	Try {
		module *node = net->head;

		while (node) {
			frame active = {
				.ast = node,
				.top = net->global,
				.line = 0
			};

			ResolveModule(&active);

			node = node->next;
		}
	} Catch (e) {
		xerror_fatal("symbol resolution failed");
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------

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
	size_t prev_line = 0;
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

	case SYMBOL_VARIABLE:
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

//recursively unwind the type list and return the tail node
static type *GetBaseType(frame *self, type *node)
{
	assert(self);
	assert(node);

	switch (node->tag) {
	case NODE_BASE:
		return node;

	case NODE_POINTER:
		return GetBaseType(self, node->pointer.reference);

	case NODE_ARRAY:
		return GetBaseType(self, node->array.element);

	default:
		assert(0 != 0 && "invalid type tag");
		__builtin_unreachable();
	}
}
//------------------------------------------------------------------------------

static void ResolveModule(frame *self)
{
	assert(self);

	module *node = self->ast;

	const size_t capacity = node->imports.len + node->declarations.len;

	node->table = PushSymTable(self, TABLE_MODULE, capacity);

	ResolveImports(self);

	ResolveDeclarations(self);

	(void) PopSymTable(self);
}

static void ResolveImports(frame *self)
{
	assert(self);

	symbol entry = {
		.tag = SYMBOL_IMPORT,
		.import_data = {
			.referenced = false
		}
	};

	vector(Import) imports = self->ast->imports;

	for (size_t i = 0; i < imports.len; i++) {
		import *node = &imports.buffer[i];

		const cstring *name = node->alias;
		self->line = node->line;

		symbol *ref = InsertSymbol(self, name, entry);
		node->entry = ref;
	}
}

static void ResolveDeclarations(frame *self)
{
	assert(self);

	static void (*const jump[])(frame *, decl *) = {
		[NODE_UDT] = ResolveUDT,
	};

	vector(Decl) declarations = self->ast.declarations;

	size_t i = 0;

	while (i < declarations.len) {
		decl *node = &declarations.buffer[i];
		jump[node->tag](self, node);

		i++;
	}
}

//------------------------------------------------------------------------------

static void ResolveUDT(frame *self, decl *node)
{
	assert(self);
	assert(node);
	assert(node->tag = NODE_UDT);

	symbol sym = {
		.tag = SYMBOL_UDT,
		.udt_data = {
			.table = NULL,
			.node = node,
			.bytes = 0, // not calculated by resolver
			.referenced = false
		}
	};

	//place udt symbol in the parent
	symbol *symref = InsertSymbol(self, node->name, sym);

	//THEN load the udt table itself; returned pointer from PushSymTable
	//lets us backfill the child pointer in order to doubly link the tables
	const size_t capacity = node->members.len;
	symref->udt_data.table = PushSymTable(self, TABLE_UDT, capacity);

	ResolveMembers(self, node->members);

	(void) PopSymTable(self);
}

//note; resolves the member's type but if the type is a named reference to a
//global native then an error is issued. The type must be public, but native
//types have no public/private qualifier and so the compiler refuses to resolve
//the reference. In any case, even if the compiler could resolve it, it doesnt
//make sense to do so because something like math.bool is obviously meaningless
static void ResolveMembers(frame *self, vector(Member) *members)
{
	assert(self);
	assert(members.len > 0);

	size_t i = 0;

	symbol sym = {
		.tag = SYMBOL_FIELD,
		.field_data = {
			.node = NULL,
			.referenced = false
		}
	};

	while (i < members.len) {
		member *node = &members.buffer[i];

		sym.field_data.node = node;
		node->entry = InsertSymbol(self, node->name, sym);

		//TODO check type

		i++;
	}
}
