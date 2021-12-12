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
static void 
ReportUnexpected(frame *, const cstring *, const symboltag, const size_t);

static void check_pair(int *);
static void PushSymTable(frame *, const tabletag, const size_t);
static void PopSymTable(frame *);
static void LoadTemporaryStack(frame *, symtable *);
static void UnloadTemporaryStack(frame *);
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
static symbol *LookupBaseType(frame *, type *);
static symbol *LookupNamedType(frame *, type *);

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

make_vector(symtable *, SymTable, static)

//the frame tracks stacks of data during the depth-first AST traveral
//@ast: root node of the AST currently undergoing symbol resolution
//@top: leaf table of the active stack within the n-ary symtable tree; non-null
//@history: stack of symbol table stacks; whenever the compiler needs to context
//switch to a different module's root symbol table, the previous symbol table
//stack is recorded in the history for later restoration.
struct frame {
	module *ast;
	symtable *top;
	vector(SymTable) history;
};

static bool ResolveSymbols(network *net)
{
	assert(net);
	assert(net->head);
	assert(net->global);

	CEXCEPTION_T e;

	frame active = {
		.ast = NULL,
		.top = NULL,
		.history = SymTableVectorInit(0, VECTOR_DEFAULT_CAPACITY)
	};

	Try {
		module *node = net->head;

		while (node) {
			active.ast = node;
			active.top = net->global;

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

	const cstring *msg = "'%s' was not declared before use";
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

static void ReportUnexpected
(
	frame *self,
	const cstring *want,
	const symboltag have,
	const size_t line
)
{
	assert(self);
	assert(want);

	const cstring *havename = SymbolLookupName(have);
	const cstring *msg = "expected '%s' but found '%s'";

	xuser_error(self->ast->alias, line, msg, want, havename);
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

//save the current symbol table chain and load a new one temporarily
static void LoadTemporaryStack(frame *self, symtable *table)
{
	assert(self);
	assert(table);

	SymTableVectorPush(&self->history, self->top);

	self->top = table;
}

//restore the previous symbol table chain
static void UnloadTemporaryStack(frame *self)
{
	assert(self);
	assert(self->history.len != 0 && "stack is empty");

	self->top = SymTableVectorPop(&self->history);
}

//debug mode wrappers to ensure that every push is paired with a pop
static void check_pair(int *pushed)
{
	assert(pushed);
	assert(*pushed == 0);
}

#define push(self, tag, cap) 					               \
	__attribute__((cleanup(check_pair))) int sym_table__pushed = 0;        \
								               \
	do {								       \
		PushSymTable(self, tag, cap);			               \
		sym_table__pushed++;				               \
		assert(sym_table__pushed == 1);  			       \
	} while (0)

#define pop(self) 							       \
	do {								       \
		PopSymTable(self);					       \
		sym_table__pushed--;			        	       \
		assert(sym_table__pushed == 0);                                \
	} while (0)

#define push_stack(self, table) 					       \
	__attribute__((cleanup(check_pair))) int sym_table_stack__pushed = 0;  \
								               \
	do {								       \
		LoadTemporaryStack(self, table);			       \
		sym_table_stack__pushed++;				       \
		assert(sym_table_stack__pushed == 1);			       \
	} while (0)

#define pop_stack(self) 						       \
	do {								       \
		UnloadTemporaryStack(self);				       \
		sym_table_stack__pushed--;			       	       \
		assert(sym_table_stack__pushed == 0);                          \
	} while (0)

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
	push(self, TABLE_MODULE, capacity);

	symref->module.table = self->top;
	node->table = self->top;

	ResolveImports(self);
	ResolveDeclarations(self);

	pop(self);
}

//introduces an import symbol scoped to the frame's active module; the symbol
//contains a backdoor to the referenced module's top-level symbol table.
//
//note; the module symbol gets marked with a reference but the import symbol
//does not. This lets the sematic analyser differentiate between two classes
//of errors: "module unused" versus "module imported but unused". Only the
//main file can ever issue a "module unused". But, this is only relevant for
//backend compiler tasks and assertions - not for end-user warnings.
static void ResolveImports(frame *self)
{
	assert(self);
	assert(self->top->tag == TABLE_MODULE);

	symbol entry = {
		.tag = SYMBOL_IMPORT,
		.import = {
			.table = NULL,
			.line = 0,
			.referenced = false
		}
	};

	vector(Import) imports = self->ast->imports;

	size_t i = 0;

	while (i < imports.len) {
		import *node = &imports.buffer[i];

		symbol *symref = LookupSymbol(self, node->alias, node->line);
		assert(symref->tag == SYMBOL_MODULE);
		symref->module.referenced = true;

		entry.import.table = symref->module.table;
		entry.import.line = node->line;
		node->entry = InsertSymbol(self, node->alias, entry);

		i++;
	}
}

static void ResolveDeclarations(frame *self)
{
	assert(self);
	assert(self->top->tag == TABLE_MODULE);

	static void (*const jump[])(frame *, decl *) = {
		[NODE_UDT] = ResolveUDT,
		/*
		[NODE_FUNCTION] = ResolveFunction,
		[NODE_METHOD] = ResolveMethod,
		[NODE_VARIABLE] = ResolveVariable
		*/
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
			.public = node->udt.public
		}
	};

	symbol *symref = InsertSymbol(self, node->udt.name, sym);

	const size_t capacity = node->udt.members.len;
	push(self, TABLE_UDT, capacity);

	symref->udt.table = self->top;
	node->udt.entry = symref;

	ResolveMembers(self, node->udt.members);

	pop(self);
}

static void ResolveMembers(frame *self, vector(Member) members)
{
	assert(self);
	assert(members.len > 0);

	symbol sym = {
		.tag = SYMBOL_FIELD,
		.field = {
			.type = NULL,
			.line = 0,
			.referenced = false,
			.public = false
		}
	};

	size_t fail = 0;
	size_t i = 0;

	while (i < members.len) {
		member *node = &members.buffer[i];

		sym.field.line = node->line;
		sym.field.type = StringFromType(node->typ);
		sym.field.public = node->public;

		symbol *symref = InsertSymbol(self, node->name, sym);
		
		node->entry = symref;

		//check all field types before throwing an exception; lets us 
		//generate many error messages within a single compile attempt
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

	switch (node->tag) {
	case NODE_BASE:
		return LookupBaseType(self, node);

	case NODE_NAMED:
		return LookupNamedType(self, node);
	
	default:
		assert(0 != 0 && "invalid node tag");
		__builtin_unreachable();
	}
}

//check that the atomic type exists in the current symbol table stack and if so
//then mark it as referenced and return the associated symbol. Otherwise return
//NULL.
static symbol *LookupBaseType(frame *self, type *node)
{
	assert(self);
	assert(node);
	assert(node->tag == NODE_BASE);

	const size_t line = node->line;
	const cstring *key = node->base.name;
	
	symbol *symref = LookupSymbol(self, key, line);
	const symboltag tag = symref->tag;

	switch (tag) {
	case SYMBOL_UDT:
		symref->udt.referenced = true;
		__attribute__((fallthrough));

	case SYMBOL_NATIVE:
		return symref;

	default: /* label bypass */ ;
		ReportUnexpected(self, "type", tag, line);
		return NULL;
	}
}

//check that the named type exists in the current symbol table stack and that its
//base type is a public UDT. Mark the base type as referenced and return the
//import symbol. Otherwise return NULL.
static symbol *LookupNamedType(frame *self, type *node)
{
	assert(self);
	assert(node);
	assert(node->tag == NODE_NAMED);

	const size_t line = node->line;
	const cstring *key = node->named.name;

	symbol *symref = LookupSymbol(self, key, line);
	const symboltag tag = symref->tag;

	switch (tag) {
	case SYMBOL_IMPORT:
		symref->import.referenced = true;
		break;

	default:
		ReportUnexpected(self, "imported module name", tag, line);
		goto fail;
	}

	push_stack(self, symref->import.table);

	symbol *underlying = LookupBaseType(self, node->named.reference);

	if (underlying->tag == SYMBOL_NATIVE) {
		const cstring *msg = "named global type is redundant";
		xuser_error(self->ast->alias, line, msg);
		goto restore;
	}

	assert(underlying->tag == SYMBOL_UDT);

	if (!underlying->udt.public) {
		const cstring *msg = "reference to a private type";
		xuser_error(self->ast->alias, line, msg);
		goto restore;
	}

	underlying->udt.referenced = true;

	pop_stack(self);
	return symref;

restore:
	pop_stack(self);

fail:
	return NULL;
}

