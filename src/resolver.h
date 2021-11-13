// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The resolver recursively locates all names in the source code and generates
// a corresponding object for each name. Names fall into one of two categories:
// directives and identifiers. Directives generate abstract syntax trees while
// identifiers generate symbols and symbol tables. These generated objects are 
// brought together into a directed acyclic graph.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ast.h"
#include "symtable.h"
#include "graph.h"
#include "str.h"

//------------------------------------------------------------------------------
// The graph verticies contain an intrusive null-terminated linked list located
// on module.next. This list is threaded through the verticies in topological
// order. The topological order is defined such that the vertex located at 
// position i in the linked list does not depend on the vertex at position j for 
// 0 <= i < j < len(list).

make_graph(module *, Module, static)

struct ... {
	graph(Module) graph;
	symtable *global;
};

//-----------------------------------------------------------------------------
// symbols are associated with a cstring identifier and placed into a hash table

typedef struct symbol symbol;
typedef struct symtable symtable;

struct symbol {
	enum {
		SYMBOL_NATIVE,
		SYMBOL_FILE,
		SYMBOL_FUNCTION,
		SYMBOL_METHOD,
		SYMBOL_UDT,
		SYMBOL_VARIABLE,
	} tag;
	union {
		struct {
			size_t bytes;
		} native;

		struct {
			symtable *table;
			bool referenced;
		} file;

		struct {
			symtable *table;
			bool referenced;
		} function;

		struct {
			symtable *table;
			bool referenced;
		} method;

		struct {
			symtable *table;
			size_t bytes;
			bool referenced;
		} udt;

		struct {
			bool referenced;
			bool parameter;
		};
	};
};

make_map(symbol, Symbol, static)

//------------------------------------------------------------------------------
// symbol tables are lexically scoped; all symbol tables in memory together
// form an n-ary tree which is threaded intrusively through each table via
// the symtable.parent and symbol.union.table references.

struct symtable {
	enum {
		TABLE_GLOBAL,
		TABLE_FILE,
		TABLE_FUNCTION,
		TABLE_METHOD,
		TABLE_UDT,
	} tag;
	symtable *parent;
	map(Symbol) entries;
	union {
		struct {
			bool configured;
		} global;
	};
};
