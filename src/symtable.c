// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

//------------------------------------------------------------------------------
// global symbol table; always the first element of the spaghetti stack 

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

void SymTableGlobalInit(void)
{
	typedef struct pair {
		const cstring *key;
		const symbol value;
	} pair;

	static const pair table[] = {
		NATIVE_TYPE("bool", 1),
		NATIVE_TYPE("byte", 1),
		NATIVE_TYPE("addr", 8),
		NATIVE_TYPE("int8", 1),
		NATIVE_TYPE("int16", 2),
		NATIVE_TYPE("int32", 4),
		NATIVE_TYPE("int64", 8),
		NATIVE_TYPE("uint8", 1),
		NATIVE_TYPE("uint16", 2),
		NATIVE_TYPE("uint32", 4),
		NATIVE_TYPE("uint64", 8),
		NATIVE_TYPE("float32", 4),
		NATIVE_TYPE("float64", 8),
		NATIVE_TYPE("complex64", 8),
		NATIVE_TYPE("complex128", 16),
		NATIVE_TYPE("string", 8),
		NATIVE_FUNC("assert"),
		NATIVE_FUNC("print"),
		NATIVE_FUNC("sizeof"),
		NATIVE_FUNC("typeof"),
	};

	const size_t total_entries = sizeof(table) / sizeof(table[0]);
	const uint64_t map_capacity = MAP_MINIMUM_CAPACITY(total_entries);

	pthread_mutex_lock(&root.global.mutex);

	assert(root.global.configured == false);

	root.entries = SymbolMapInit(map_capacity);

	for (size_t i = 0; i < total_entries; i++) {
		const pair *p = table + i;

		bool ok = SymbolMapInsert(&root.entries, p->key, p->value);
		assert(ok && "duplicate entry in global symbol table");

	}

	root.global.configured = true;
	pthread_mutex_unlock(&root.global.mutex);
}

#undef NATIVE_TYPE
#undef NATIVE_FUNC

void SymTableGlobalFree(void)
{
	pthread_mutex_lock(&root.global.mutex);

	assert(root.global.configured == true);

	SymbolMapFree(&root.entries, NULL);

	root.entries = (map(Symbol)) {
		.len = 0,
		.cap = 0,
		.buffer = NULL
	};

	root.global.configured = false;

	pthread_mutex_unlock(&root.global.mutex);
}

//------------------------------------------------------------------------------


