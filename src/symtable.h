// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "map.h"

typedef struct symbol symbol;
typedef struct symtable symtable;

typedef struct module module;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct import import;
typedef struct member member;
typedef struct param param;

//------------------------------------------------------------------------------
// Symbols are associated with a cstring identifier and placed into a hash table

typedef enum symboltag {
	SYMBOL_NATIVE,
	SYMBOL_MODULE,
	SYMBOL_IMPORT,
	SYMBOL_FUNCTION,
	SYMBOL_METHOD,
	SYMBOL_UDT,
	SYMBOL_VARIABLE,
	SYMBOL_FIELD,
	SYMBOL_VARIABLE,
	SYMBOL_PARAMETER,
	SYMBOL_LABEL,
} symboltag;

struct symbol {
	symboltag tag;
	union {
		struct {
			size_t bytes;
		} native_data;

		struct {
			symtable *table;
			module *node;
			bool referenced;
		} module_data;

		struct {
			import *node;
			bool referenced;
		} import_data;

		struct {
			symtable *table;
			decl *node;
			bool referenced;
		} function_data;

		struct {
			symtable *table;
			decl *node;
			bool referenced;
		} method_data;

		struct {
			symtable *table;
			decl *node;
			size_t bytes;
			bool referenced;
		} udt_data;

		struct {
			decl *node;
			bool referenced;
		} variable_data;

		struct {
			member *node;
			bool referenced;
		} field_data;
		
		struct {
			param *node;
			bool referenced;
		} parameter_data;

		struct {
			stmt *node;
			bool referenced;
		} label_data;
	};
};

make_map(symbol, Symbol, static)

//------------------------------------------------------------------------------
// symbol tables are lexically scoped; all symbol tables in memory together
// form an n-ary tree traversed via symtable.parent and symbol.union.table.
//
// @parent: NULL if and only if tag == TABLE_GLOBAL

typedef enum tabletag {
	TABLE_GLOBAL,
	TABLE_MODULE,
	TABLE_FUNCTION,
	TABLE_METHOD,
	TABLE_UDT,
} tabletag;

struct symtable {
	tabletag tag;
	symtable *parent;
	map(Symbol) entries;
};

//------------------------------------------------------------------------------
//API

//create the global symbol table and populate it with the predeclared native
//types and functions; returned pointer is non-NULL
symtable *SymTableInit(void);

//recursively release all symbol tables in memory; input must not be NULL
void SymTableFree(symtable *global);

//always returns a valid child symbol table; input capacity guarantees that the
//underlying hash table buffer will not issue a dynamic resize when the total
//inserted symbols <= cap
symtable *SymTableSpawn(symtable *parent, const tabletag tag, const size_t cap);

//returns NULL if the symbol already exists. On success the returned pointer
//will remain valid until SymTableFree provided that the capacity contract on
//SymTableSpawn is upheld.
symbol *SymTableInsert(symtable *table, const cstring *key, symbol value);

//returns NULL if the key does not exist in the input table or its ancestors; 
//if the target is non-null then it contains a pointer  on return to the table 
//in which the key exists
symbol *SymTableLookup(symtable *table, const cstring *key, symtable **target);
