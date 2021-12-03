// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stddef.h>

#include "arena.h"
#include "file.h"
#include "resolver.h"
#include "vector.h"
#include "xerror.h"

typedef struct frame frame;

static bool ResolveDependencies(network *, const cstring *);
static bool InsertModule(network *, const cstring *);
static void InsertChildren(network *, module *, const cstring *);
static void Sort(network *, module *);
static void ReportCycle(const cstring *, const cstring *);

static symbol *LookupSymbol(frame *, const cstring *, const size_t);
static void ReportUndeclared(frame *, const cstring *, const size_t);
static symbol *InsertSymbol(frame *, const cstring *, symbol);
static void ReportRedeclaration(frame *, const cstring *, symbol);
static size_t GetSymbolLine(symbol);

static void PushSymTable(frame *, const tabletag, const size_t);
static void PopSymTable(frame *);
static type *UnwindType(type *);
static cstring *StringFromType(type *);
static void StringFromType__recursive(vstring *, type *);

static bool ResolveSymbols(network *);
static void ResolveModule(frame *);
static void ResolveImports(frame *);
static void ResolveDeclarations(frame *);
static void ResolveUDT(frame *, decl *);
static void ResolveMembers(frame *, vector(Member));
static symbol *LookupMemberType(frame *, type *);

//------------------------------------------------------------------------------

network *ResolverInit(const cstring *filename)
{
	assert(filename);

	network *net = allocate(sizeof(network));

	net->dependencies = ModuleGraphInit();
	net->head = NULL;

	bool ok = ResolveDependencies(net, filename);

	if (!ok) {
		return NULL; 
	}

	//graph API doesn't allow for removals so the len member is the true
	//underlying hash table count.
	net->global = SymTableInit(net->dependencies.len);

	ok = ResolveSymbols(net);

	if (!ok) {
		return NULL;
	}

	return net;
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
		xerror_fatal("cannot create AST; %s", filename);
		Throw(XXGRAPH);
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

	cstring *fparent = FileGetDiskName(parent);
	cstring *fchild = FileGetDiskName(child);

	xuser_error(NULL, 0, msg, fparent, fchild);
}

//------------------------------------------------------------------------------
// Phase 2: resolve symbols

//@ast: root node of the AST currently undergoing symbol resolution
//@top: active LIFO stack within the n-ary symtable tree; non-null
struct frame {
	module *ast;
	symtable *top;
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
//symbol resolution utilities

//throw XXSYMBOL exception is key does not exist in the active stack
static symbol *LookupSymbol(frame *self, const cstring *key, const size_t line)
{
	assert(self);
	assert(key);
	assert(line);

	symbol *symref = SymTableLookup(self->top, key, NULL);

	if (!symref) {
		ReportUndeclared(self, key, line);
		Throw(XXSYMBOL);
	}

	return symref;
}

static void ReportUndeclared(frame *self, const cstring *key, const size_t line)
{
	assert(self);
	assert(key);
	assert(line);

	const cstring *msg = "%s was not declared before use";
	const cstring *fname = self->ast->alias;

	xuser_error(fname, line, msg, key);
}

//throws XXSYMBOL exception if symbol alredy exists in the active table
static symbol *InsertSymbol(frame *self, const cstring *key, symbol value)
{
	assert(self);
	assert(key);

	symbol *symref = SymTableInsert(self->top, key, value);

	if (!symref) {
		ReportRedeclaration(self, key, value);
		Throw(XXSYMBOL);
	}

	return symref;
}

//notify user of attempt to redeclare a variable within the same scope
static void ReportRedeclaration(frame *self, const cstring *key, symbol value)
{
	assert(self);
	assert(key);

	const cstring *msg = "%s redeclared; previously declared on line %zu";
	const cstring *fname = self->ast->alias;

	symtable *table = NULL;
	symbol *symref = SymTableLookup(self->top, key, &table);

	assert(table);
	assert(table == self->top && "redeclared var is not in same scope");
	assert(table->tag != TABLE_GLOBAL && "key redeclared in global scope");

	const size_t curr_line = GetSymbolLine(value);
	const size_t prev_line = GetSymbolLine(*symref);

	assert(prev_line);

	xuser_error(fname, curr_line, msg, key, prev_line);
}

//returns 0 if symbol does not carry line information
static size_t GetSymbolLine(symbol sym)
{
	switch (sym.tag) {
	case SYMBOL_NATIVE:
		return 0;

	case SYMBOL_MODULE:
		return 0;

	case SYMBOL_IMPORT:
		return sym.import.line;

	case SYMBOL_FUNCTION:
		return sym.function.line;

	case SYMBOL_METHOD:
		return sym.method.line;

	case SYMBOL_UDT:
		return sym.udt.line;

	case SYMBOL_VARIABLE:
		return sym.variable.line;

	case SYMBOL_FIELD:
		return sym.field.line;

	case SYMBOL_LABEL:
		return sym.label.line;

	default:
		assert(0 != 0 && "invalid symbol tab");
		__builtin_unreachable();
	}
}

//------------------------------------------------------------------------------
//frame management

static void PushSymTable(frame *self, const tabletag tag, const size_t cap)
{
	assert(self);

	symtable *top = SymTableSpawn(self->top, tag, cap);

	self->top = top;
}

static void PopSymTable(frame *self)
{
	assert(self);

	symtable *old_top = self->top;

	assert(old_top->parent && "attempted to pop global symbol table");

	self->top = old_top->parent;
}

//------------------------------------------------------------------------------

//return the tail (NODE_BASE) or penultimate node (NODE_NAMED) in the singly
//linked type list.
static type *UnwindType(type *node)
{
	assert(node);

	switch (node->tag) {
	case NODE_BASE:
		__attribute__((fallthrough));

	case NODE_NAMED:
		return node;

	case NODE_POINTER:
		return UnwindType(node->pointer.reference);

	case NODE_ARRAY:
		return UnwindType(node->array.element);

	default:
		assert(0 != 0 && "invalid type tag");
		__builtin_unreachable();
	}
}

//unwind the singly linked type list and compress it recursively into a compact
//string notation; i..e, the type list [10] -> * -> int32 becomes "[10]*int32"
static cstring *StringFromType(type *node)
{
	assert(node);

	const size_t typical_type_length = 16;

	vstring vstr = vStringInit(typical_type_length);

	StringFromType__recursive(&vstr, node);

	return cStringFromvString(&vstr);
}

static void StringFromType__recursive(vstring *vstr, type *node)
{
	assert(node);
	assert(vstr);
	
	switch (node->tag) {
	case NODE_BASE:
		vStringAppendcString(vstr, node->base.name);
		break;

	case NODE_NAMED:
		vStringAppendcString(vstr, node->named.name);
		StringFromType__recursive(vstr, node->named.reference);
		break;

	case NODE_POINTER:
		vStringAppend(vstr, '*');
		StringFromType__recursive(vstr, node->pointer.reference);
		break;
	
	case NODE_ARRAY:
		vStringAppend(vstr, '[');
		vStringAppendIntMax(vstr, node->array.len);
		vStringAppend(vstr, ']');
		StringFromType__recursive(vstr, node->array.element);
		break;

	default:
		assert(0 != 0 && "invalid type tag");
		__builtin_unreachable();
	}
}

//------------------------------------------------------------------------------

static void ResolveModule(frame *self)
{
	assert(self);

	symbol sym = {
		.tag = SYMBOL_MODULE,
		.module = {
			.table = NULL,
			.referenced = false
		}
	};

	module *const node = self->ast;
	symbol *symref = InsertSymbol(self, node->alias, sym);

	const size_t capacity = node->imports.len + node->declarations.len;
	PushSymTable(self, TABLE_MODULE, capacity);

	symref->module.table = self->top;
	node->table = self->top;

	ResolveImports(self);
	ResolveDeclarations(self);

	PopSymTable(self);
}

static void ResolveImports(frame *self)
{
	assert(self);

	symbol entry = {
		.tag = SYMBOL_IMPORT,
		.import = {
			.line = 0,
			.referenced = false
		}
	};

	vector(Import) imports = self->ast->imports;

	for (size_t i = 0; i < imports.len; i++) {
		import *node = &imports.buffer[i];

		const cstring *name = node->alias;
		entry.import.line = node->line;

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

	vector(Decl) declarations = self->ast->declarations;

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
	assert(node->tag == NODE_UDT);

	symbol sym = {
		.tag = SYMBOL_UDT,
		.udt = {
			.table = NULL,
			.bytes = 0,
			.line = node->line,
			.referenced = false,
			.public = false
		}
	};

	//place udt symbol in the parent
	symbol *symref = InsertSymbol(self, node->udt.name, sym);

	//THEN load the udt table itself; returned pointer from PushSymTable
	//lets us backfill the child pointer in order to doubly link the tables
	const size_t capacity = node->udt.members.len;
	PushSymTable(self, TABLE_UDT, capacity);
	symref->udt.table = self->top;

	ResolveMembers(self, node->udt.members);

	PopSymTable(self);
}

//note; resolves the member's type but if the type is a named reference to a
//global native then an error is issued. The type must be public, but native
//types have no public/private qualifier and so the compiler refuses to resolve
//the reference. In any case, even if the compiler could resolve it, it doesnt
//make sense to do so because something like math.bool is obviously meaningless
static void ResolveMembers(frame *self, vector(Member) members)
{
	assert(self);
	assert(members.len > 0);

	size_t fail = 0;
	size_t i = 0;

	symbol sym = {
		.tag = SYMBOL_FIELD,
		.field = {
			.type = NULL,
			.line = 0,
			.referenced = false,
			.public = false
		}
	};

	while (i < members.len) {
		member *node = &members.buffer[i];

		sym.field.line = node->line;

		node->entry = InsertSymbol(self, node->name, sym);

		if (!LookupMemberType(self, node->typ)) {
			fail++;
		}

		i++;
	}

	if (fail) {
		Throw(XXSYMBOL);
	}
}

//if the node is a base type, returns a reference to the type symbol entry. If
//the node is a named type, returns a reference to its base type symbol entry.
//returns NULL if any lookup fails or if the lookup succeeds but the referenced
//symbol is semantically meaningless.
static symbol *LookupMemberType(frame *self, type *node)
{
	assert(self);
	assert(node);

	node = UnwindType(node);
	const cstring *fname = self->ast->alias;
	symbol *ref = NULL;
	cstring *key = NULL;

	switch (node->tag) {
	case NODE_BASE:
		key = node->base.name;
		ref = LookupSymbol(self, key, node->line);

		switch (ref->tag) {
		case SYMBOL_UDT:
			ref->udt.referenced = true;
			__attribute__((fallthrough));

		case SYMBOL_NATIVE:
			return ref;

		default:
			//TODO line info
			xuser_error(fname, 0, "'%s' not a declared type", key);
		}

		return NULL;

	case NODE_NAMED:
		key = node->named.name;
		ref = LookupSymbol(self, key, node->line);

		if (ref->tag != SYMBOL_IMPORT) {
			xuser_error(fname, 0 , "'%s' is not an import", key);
			return NULL;
		}

		ref->import.referenced = true;

		//TODO have to actually load the other module's symbol table..
		//this is still searching in the local spaghetti stack
		ref = LookupMemberType(self, node->named.reference);

		if (!ref) {
			return NULL;
		}

		if (ref->tag == SYMBOL_NATIVE) {
			const cstring *msg = "named global type is redundant";
			xuser_error(fname, 0, msg);
			return NULL;
		}

		assert(ref->tag == SYMBOL_UDT);

		if (!ref->udt.public) {
			const cstring *msg = "reference to private type";
			xuser_error(fname, 0, msg);
			return NULL;
		}

		return ref;

	default:
		assert(0 != 0 && "invalid node tag");
		__builtin_unreachable();
	}
}

//------------------------------------------------------------------------------
