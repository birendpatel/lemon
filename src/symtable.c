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

#define NATIVE_TYPE(keyname, size) 				               \
{								               \
	.key = keyname,							       \
	.value = {							       \
		.tag = SYMBOL_NATIVE,					       \
		.native = {						       \
			.bytes = size					       \
		}							       \
	}								       \
}

#define NATIVE_FUNC(keyname) 				               	       \
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

void SymTableConfigGlobal(void)
{
	assert(root.global.configured == false);

	typedef struct pair {
		const cstring *key;
		const symbol value;
	} pair;

	static const pair table[] = {
		[0]  = NATIVE_TYPE("bool", 1),
		[1]  = NATIVE_TYPE("byte", 1),
		[2]  = NATIVE_TYPE("addr", 8),
		[3]  = NATIVE_TYPE("int8", 1),
		[4]  = NATIVE_TYPE("int16", 2),
		[5]  = NATIVE_TYPE("int32", 4),
		[6]  = NATIVE_TYPE("int64", 8),
		[7]  = NATIVE_TYPE("uint8", 1),
		[8]  = NATIVE_TYPE("uint16", 2),
		[9]  = NATIVE_TYPE("uint32", 4),
		[10] = NATIVE_TYPE("uint64", 8),
		[11] = NATIVE_TYPE("float32", 4),
		[12] = NATIVE_TYPE("float64", 8),
		[13] = NATIVE_TYPE("complex64", 8),
		[14] = NATIVE_TYPE("complex128", 16),
		[15] = NATIVE_TYPE("string", 8),
		[16] = NATIVE_FUNC("assert"),
		[17] = NATIVE_FUNC("print"),
		[18] = NATIVE_FUNC("sizeof"),
		[19] = NATIVE_FUNC("typeof"),
		[20] = {NULL}
	};

	pthread_mutex_lock(&root.global.mutex);

	root.entries = SymbolMapInit(MAP_DEFAULT_CAPACITY);

	const pair *p = table;

	while (p->key) {
		bool ok = SymbolMapInsert(&root.entries, p->key, p->value);
		assert(ok && "duplicate entry in global symbol table");

		p++;
	}

	root.global.configured = true;
	pthread_mutex_unlock(&root.global.mutex);
}

#undef NATIVE_TYPE
#undef NATIVE_FUNC

//------------------------------------------------------------------------------
