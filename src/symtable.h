// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "map.h"
#include "str.h"

typedef struct symbol symbol;
typedef struct symtable symtable;

//------------------------------------------------------------------------------
// Symbols are associated with a cstring identifier and placed into a hash table
//
// the data type is compressed from the parser linked list into a compact string
// representation. For example, [10]*int32 has a parser list representation as
// [10] --> * --> int32, while in the symbol table it is just "[10]*int32".
// This essentially converts the type back into the original source literal.

typedef enum symboltag {
	SYMBOL_NATIVE,
	SYMBOL_MODULE,
	SYMBOL_IMPORT,
	SYMBOL_FUNCTION,
	SYMBOL_METHOD,
	SYMBOL_UDT,
	SYMBOL_VARIABLE,
	SYMBOL_FIELD,
	SYMBOL_PARAMETER,
	SYMBOL_LABEL,
} symboltag;

struct symbol {
	symboltag tag;
	union {
		struct {
			size_t bytes;
		} native;

		struct {
			symtable *table;
			struct {
				unsigned int referenced: 1;
			};
		} module;

		struct {
			symtable *table;
			size_t line;
			struct {
				unsigned int referenced: 1;
			};
		} import;

		//signature translates func (int32, bool) -> float64 to the
		//string "int32,bool,float64", void parameter list results in
		//":float64" and void return results in "int32,bool:"
		struct {
			symtable *table;
			cstring *signature;
			size_t line;
			struct {
				unsigned int referenced: 1;
			};
		} function;

		struct {
			symtable *table;
			cstring *signature;
			size_t line;
			struct {
				unsigned int referenced: 1;
			};
		} method;

		//@bytes not calculated during symbol resolution
		struct {
			symtable *table;
			size_t bytes;
			size_t line;
			struct {
				unsigned int referenced: 1;
				unsigned int public: 1;
			};
		} udt;

		struct {
			cstring *type;
			size_t line;
			struct {
				unsigned int referenced: 1;
				unsigned int public: 1;
			};
		} variable;

		struct {
			cstring *type;
			size_t line;
			struct {
				unsigned int referenced: 1;
				unsigned int public: 1;
			};
		} field;
		
		struct {
			cstring *type;
			size_t line;
			struct {
				unsigned int referenced: 1;
			};
		} parameter;

		struct {
			size_t line;
			struct {
				unsigned int referenced: 1;
			};
		} label;
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
//types and functions; returned pointer is non-NULL.
//
//the total_modules input ensures that the global symbol table will not resize
//so that pointers returned by SymTableInsert always remain valid.
symtable *SymTableInit(const size_t total_modules);

//always returns a valid child symbol table; input capacity guarantees that the
//underlying hash table buffer will not issue a dynamic resize when the total
//inserted symbols <= cap
symtable *SymTableSpawn(symtable *parent, const tabletag tag, const size_t cap);

//returns NULL if the symbol already exists. On success the returned pointer
//will remain valid for the compiler lifetime provided that the capacity 
//contract on SymTableSpawn is upheld.
symbol *SymTableInsert(symtable *table, const cstring *key, symbol value);

//returns NULL if the key does not exist in the input table or its ancestors; 
//if the target is non-null then it contains a pointer  on return to the table 
//in which the key exists
symbol *SymTableLookup(symtable *table, const cstring *key, symtable **target);

//convert symbol tag to a printable name
const cstring *SymbolLookupName(const symboltag tag);

//print the input symbol table and its children, grandchildren, etc.
void SymTablePrint(symtable *root);
