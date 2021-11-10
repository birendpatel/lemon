// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
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

bool SymTableConfigGlobal(void)
{
	assert(root.global.configured == false);

	typedef struct pair {
		const cstring *key;
		const symbol value;
	} pair;

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

#define FUNCTION_PAIR(keyname) 				               	       \
{								               \
	.key = keyname,							       \
	.value = {							       \
		.tag = SYMBOL_FUNCTION,					       \
		.function = {						       \
			.table = NULL,					       \
			.node = NULL,					       \
			.referenced = false				       \
		}							       \
	}								       \
}

	static const pair table[] = {
		NATIVE_PAIR("bool", 1),
		NATIVE_PAIR("byte", 1),
		NATIVE_PAIR("addr", 8),
		NATIVE_PAIR("int8", 1),
		NATIVE_PAIR("int16", 2),
		NATIVE_PAIR("int32", 4),
		NATIVE_PAIR("int64", 8),
		NATIVE_PAIR("uint8", 1),
		NATIVE_PAIR("uint16", 2),
		NATIVE_PAIR("uint32", 4),
		NATIVE_PAIR("uint64", 8),
		NATIVE_PAIR("float32", 4),
		NATIVE_PAIR("float64", 8),
		NATIVE_PAIR("complex64", 8),
		NATIVE_PAIR("complex128", 16),
		NATIVE_PAIR("string", 8),
		FUNCTION_PAIR("assert"),
		FUNCTION_PAIR("print"),
		FUNCTION_PAIR("sizeof"),
		{.key = NULL}
	};

#undef NATIVE_PAIR
#undef FUNCTION_PAIR

	pthread_mutex_lock(&root.global.mutex);

	root.entries = SymbolMapInit(MAP_DEFAULT_CAPACITY);

	const pair *p = table;

	while (p->key) {
		bool ok = SymbolMapInsert(&root.entries, p->key, p->value);
		
		if (!ok) {
			xerror_fatal("'%s'; duplicate entry", p->key);
			goto fail;
		}

		p++;
	}

	root.global.configured = true;

fail:
	pthread_mutex_unlock(&root.global.mutex);
	return root.global.configured;
}

//------------------------------------------------------------------------------
