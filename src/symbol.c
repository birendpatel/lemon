// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <stdbool.h>
#include <stdint.h>

#include "symtab.h"
#include "lib/map.h"

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
			decl *node;
			bool referenced;
		} function;

		struct {
			symtable *table;
			decl *node;
			bool referenced;
		} method;

		struct {
			symtable *table;
			decl *node;
			size_t bytes;
			bool referenced;
		} udt;

		struct {
			decl *node;
			bool referenced;
			bool parameter;
		}
	}
};

make_map(symbol, Symbol, static)

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
};

//if something already found, throw a re-declared sort of error
//if nothing found, throw an error
//
//nodes should store location of slot in the hash table to allow future
//faster accesses instead of having to lookup hash again, but this can only
//be done when the hash table is finished growing b/c hash resizes will change
//references. this is just for optimisation.
//
//instead you can store the specific hash table rather than the slot, this
//solves the issue of a redeclaration within the scope. You can also
//traverse the tree first to count up the total decls and then on the second
//pass create a hash table with a capacity large enough to not trigger
//a growth.

//pop and push hash tables off a stack
//- whenever a new lexical scope is encountered or left
//
//store references to each hash table in block statement nodes

//need some sort of global root hash table where predeclared identifiers reside.
//this is not stuff that the user has declared but stuff that is provided by
//Lemon beforehand, like ints and floats.

//symbol table should include number of times a variable, function, or UDT is
//referenced so as to trigger -Wunused errors
