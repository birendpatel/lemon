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

//global sketch
//the array of pairs basically shows what resides in the global symbol table
//by default. since maps exist on heap, the compile-time info in the array will
//get copied into the heap global table on startup when the correct API function
//is invoked by application code.
struct pair {
	cstring *key;
	symbol value;
};

//rough draft, we can expand this later with the full predeclared type suite
pair entries[] = {
	{
		.key = "byte",
		.value = {
			.tag = SYMBOL_NATIVE,
			.native = {
				.bytes = 1
			}
		}
	},
	{
		.key = "i8",
		.value = {
			.tag = SYMBOL_NATIVE,
			.native = {
				.bytes = 1
			}
		}
	},
	{
		.key = "f64",
		.value = {
			.tag = SYMBOL_NATIVE,
			.native = {
				.bytes = 8
			}
		}
	},
	{
		.key = "string",
		.value = {
			.tag = SYMBOL_NATIVE,
			.native = {
				.bytes = 8
			}
		}
	},
	{
		.key = NULL
	}
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
