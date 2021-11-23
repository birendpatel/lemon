// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stdint.h>

#include "arena.h"
#include "symtable.h"

//------------------------------------------------------------------------------

#define NATIVE_TYPE(keyname, size) 				               \
{								               \
	.key = keyname,							       \
	.value = {							       \
		.tag = SYMBOL_NATIVE,					       \
		.native = {					               \
			.bytes = size					       \
		}							       \
	}								       \
}

#define NATIVE_FUNC(keyname) 				               	       \
{								               \
	.key = keyname,							       \
	.value = {							       \
		.tag = SYMBOL_FUNCTION,					       \
		.function = {					               \
			.table = NULL,					       \
			.referenced = false				       \
		}							       \
	}								       \
}

symtable *SymTableInit(void)
{
	typedef struct pair {
		const cstring *key;
		const symbol value;
	} pair;

	//the global hash table can be mostly constructed at compile time if 
	//we precalculate the FNV1a hashes and then memcpy to heap at runtime.
	//That is 100% premature optimisation so for now we sit each key-value
	//pair in the .data segment and pay the full runtime transfer penalty.
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

	symtable *global = SymTableSpawn(NULL, TABLE_GLOBAL, total_entries);
	assert(global);

	for (size_t i = 0; i < total_entries; i++) {
		const pair *p = table + i;
		symbol *entry = SymTableInsert(global, p->key, p->value);
		assert(entry && "duplicate entry");
	}

	return global;
}

#undef NATIVE_TYPE
#undef NATIVE_FUNC

symtable *SymTableSpawn(symtable *parent, const tabletag tag, const size_t cap)
{
	assert((parent != NULL) ^ (tag == TABLE_GLOBAL)); //logical xor

	symtable *child = ArenaAllocate(sizeof(symtable));

	*child = (symtable) {
		.tag = tag,
		.parent = parent,
		.entries = SymbolMapInit(MAP_MINIMUM_CAPACITY(cap))
	};

	return child;
}

symbol *SymTableInsert(symtable *table, const cstring *key, symbol value)
{
	assert(table);
	assert(key);
	
	return SymbolMapInsert(&table->entries, key, value);
}

symbol *SymTableLookup(symtable *table, const cstring *key, symtable **target)
{
	assert(table);
	assert(key);

	symbol *entry = NULL;

	//base case
	if (SymbolMapGetRef(&table->entries, key, &entry)) {
		if (target) {
			*target = table;
		}

		return entry;
	}

        //recursive case
	if (table->parent) {
		return SymTableLookup(table->parent, key, target);
	}

	//else table is global and therefore the symbol cannot exist
	return NULL;
}
