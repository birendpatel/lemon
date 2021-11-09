// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "symtable.h"
#include "parser.h"
#include "xerror.h"
#include "lib/map.h"

//------------------------------------------------------------------------------
// classic symbol table implementation; each symbol resides in some hash table
// and each hash table resides somewhere in a spaghetti stack. References 
// to tables outside of the active stack are stored within various AST nodes.

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
		};
	};
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
	union {
		struct {
			bool configured;
			pthread_mutex_t mutex;
		} global;
	};
};

//------------------------------------------------------------------------------
// global symbol table; always the first element in the spaghetti stack and once
// initialized it exists for the lifetime of the compiler.

static symtable root = {
	.tag = TABLE_GLOBAL,
	.parent = NULL,
	.entries = {
		.len = 0,
		.cap = 0,
		.buffer = NULL
	},
	.global = {
		.configured = false,
		.mutex = PTHREAD_MUTEX_INITIALIZER
	}
};

#define NATIVE_PAIR(keyname, size) 				               \
{								               \
	.key = keyname,							       \
	.value = {							       \
		.tag = SYMBOL_NATIVE,					       \
		.native = {						       \
			.bytes = size					       \
		}							       \
	}								       \
}

bool SymTableConfigGlobal(void)
{
	struct pair {
		cstring *key;
		symbol value;
	};

	static const struct pair entries[] = {
		NATIVE_PAIR("byte", 1),
		NATIVE_PAIR("i8", 1),
		NATIVE_PAIR("f64", 8),
		NATIVE_PAIR("string", 8),
	};
	
	const size_t num_entries = sizeof(entries) / sizeof(entries[0]);

	pthread_mutex_lock(&root.global.mutex);

	root.entries = SymbolMapInit(MAP_DEFAULT_CAPACITY);

	for (size_t i = 0; num_entries; i++) {
		cstring *key = entries[i].key;
		symbol value = entries[i].value;
		
		bool status = SymbolMapInsert(&root.entries, key, value);

		if (status == false) {
			xerror_fatal("'%s'; duplicate entry", key);
			goto fail;
		}
	}

	root.global.configured = true;

fail:
	pthread_mutex_unlock(&root.global.mutex);
	return root.global.configured;
}

#undef NATIVE_PAIR

//------------------------------------------------------------------------------
