// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stdint.h>

#include "arena.h"
#include "json.h"
#include "str.h"
#include "symtable.h"

static void SerializeSymTable(json_array *, symtable *);
static json_value ParseTable(symtable *);
static json_value ParseTableTag(const tabletag);
static json_value ParseEntries(map(Symbol));
static json_value ParseSymbol(const symbol);
static json_value ParseNative(const symbol);
static json_value ParseModule(const symbol);
static json_value ParseImport(const symbol);
static json_value ParseFunction(const symbol);
static json_value ParseMethod(const symbol);
static json_value ParseUDT(const symbol);
static json_value ParseVariable(const symbol);
static json_value ParseField(const symbol); 
static json_value ParseParameter(const symbol);
static json_value ParseLabel(const symbol);

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

symtable *SymTableInit(const size_t total_modules)
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

	const size_t total_native = sizeof(table) / sizeof(table[0]);
	const size_t total_entries = total_modules + total_native;

	symtable *global = SymTableSpawn(NULL, TABLE_GLOBAL, total_entries);
	assert(global);

	for (size_t i = 0; i < total_native; i++) {
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

	symtable *child = allocate(sizeof(symtable));

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

const cstring *SymbolLookupName(const symboltag tag)
{
	static const cstring *lookup[] = {
		[SYMBOL_NATIVE] = "native",
		[SYMBOL_MODULE] = "module",
		[SYMBOL_IMPORT] = "import",
		[SYMBOL_FUNCTION] = "function",
		[SYMBOL_METHOD] = "method",
		[SYMBOL_UDT] = "udt",
		[SYMBOL_VARIABLE] = "variable",
		[SYMBOL_FIELD] = "field",
		[SYMBOL_PARAMETER] = "parameter",
		[SYMBOL_LABEL] = "label"
	};

	if (tag < SYMBOL_NATIVE || tag > SYMBOL_LABEL) {
		return "INVALID SYMBOL LOOKUP";
	}

	return lookup[tag];
}

const cstring *SymTableLookupName(const tabletag tag)
{
	static const cstring *lookup[] = {
		[TABLE_GLOBAL] = "global",
		[TABLE_MODULE] = "module",
		[TABLE_FUNCTION] = "function",
		[TABLE_METHOD] = "method",
		[TABLE_UDT] = "udt"
	};

	if (tag < TABLE_GLOBAL || tag > TABLE_UDT) {
		return "INVALID TABLE LOOKUP";
	}

	return lookup[tag];
}

//------------------------------------------------------------------------------
//convert a symbol table parent pointer tree to a JSON parse tree via the
//standard recursive descent algorithm

cstring *SymTableToJSON(symtable *root)
{
	assert(root);

	json_value value = ParseTable(root);

	assert(value.tag == JSON_VALUE_OBJECT);

	return JsonSerializeObject(value.object);
}

//returns a json object representation of the input table and its children
static json_value ParseTable(symtable *root)
{
	assert(root);

	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	JsonObjectAdd(value.object, "table type", ParseTableTag(root->tag));
	JsonObjectAdd(value.object, "entries", ParseEntries(root->entries));

	return value;
}

//returns a json string representation of the symbol table type; note lookup
//is duplicated to avoid -Wcast-qual error
static json_value ParseTableTag(const tabletag tag)
{
	const cstring *tagname = SymTableLookupName(tag);

	return (json_value) {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(tagname)
	};
}

//returns a json object representation of the input hash table
static json_value ParseEntries(map(Symbol) entries)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	for (size_t i = 0; i < entries.cap; i++) {
		if (entries.buffer[i].status != SLOT_CLOSED) {
			continue;
		}

		const cstring *symname = entries.buffer[i].key;
		const symbol sym = entries.buffer[i].value;

		JsonObjectAdd(value.object, symname, ParseSymbol(sym));
	}

	return value;
}

static json_value ParseSymbol(const symbol sym)
{
	assert(sym.tag >= SYMBOL_NATIVE);
	assert(sym.tag <= SYMBOL_LABEL);

	json_value (*const jump[]) (const symbol) = {
		[SYMBOL_NATIVE] = ParseNative,
		[SYMBOL_MODULE] = ParseModule,
		[SYMBOL_IMPORT] = ParseImport,
		[SYMBOL_FUNCTION] = ParseFunction,
		[SYMBOL_METHOD] = ParseMethod,
		[SYMBOL_UDT] = ParseUDT,
		[SYMBOL_VARIABLE] = ParseVariable,
		[SYMBOL_FIELD] = ParseField,
		[SYMBOL_PARAMETER] = ParseParameter,
		[SYMBOL_LABEL] = ParseLabel
	};

	return jump[sym.tag](sym);
}

static json_value ParseNative(const symbol sym)
{
	assert(sym.native.bytes < 256 && "native type is unusually large");

	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	json_value byteinfo = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.native.bytes
	};

	JsonObjectAdd(value.object, "bytes", byteinfo);

	return value;
}

static json_value ParseModule(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.module.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	symtable *table = sym.module.table;
	JsonObjectAdd(value.object, "table", ParseTable(table));

	return value;
}

static json_value ParseImport(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.import.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.import.line
	};

	JsonObjectAdd(value.object, "line", line);

	symtable *table = sym.import.table;
	JsonObjectAdd(value.object, "table", ParseTable(table));

	return value;
}

static json_value ParseFunction(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.function.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	json_value signature = {
		.tag = JSON_VALUE_STRING,
		.string = sym.function.signature
	};

	JsonObjectAdd(value.object, "signature", signature);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.function.line
	};

	JsonObjectAdd(value.object, "line", line);

	symtable *table = sym.function.table;
	JsonObjectAdd(value.object, "table", ParseTable(table));

	return value;
}

static json_value ParseMethod(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.method.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	json_value signature = {
		.tag = JSON_VALUE_STRING,
		.string = sym.method.signature
	};

	JsonObjectAdd(value.object, "signature", signature);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.method.line
	};

	JsonObjectAdd(value.object, "line", line);

	symtable *table = sym.method.table;
	JsonObjectAdd(value.object, "table", ParseTable(table));

	return value;
}

static json_value ParseUDT(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.udt.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	int pub_flag = sym.udt.public;

	json_value pub = {
		.tag = pub_flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "public", pub);

	json_value bytes = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.udt.bytes
	};

	JsonObjectAdd(value.object, "bytes", bytes);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.udt.line
	};

	JsonObjectAdd(value.object, "line", line);

	symtable *table = sym.udt.table;
	JsonObjectAdd(value.object, "table", ParseTable(table));

	return value;
}

static json_value ParseVariable(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.variable.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	int pub_flag = sym.variable.public;

	json_value pub = {
		.tag = pub_flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "public", pub);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.variable.line
	};

	JsonObjectAdd(value.object, "line", line);

	json_value type = {
		.tag = JSON_VALUE_STRING,
		.string = sym.variable.type
	};

	JsonObjectAdd(value.object, "type", type);

	return value;
}

static json_value ParseField(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.field.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	int pub_flag = sym.field.public;

	json_value pub = {
		.tag = pub_flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "public", pub);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.field.line
	};

	JsonObjectAdd(value.object, "line", line);

	json_value type = {
		.tag = JSON_VALUE_STRING,
		.string = sym.field.type
	};

	JsonObjectAdd(value.object, "type", type);

	return value;
}

static json_value ParseParameter(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	json_value typeinfo = {
		.tag = JSON_VALUE_STRING,
		.string = cStringDuplicate(SymbolLookupName(sym.tag))
	};

	JsonObjectAdd(value.object, "symbol type", typeinfo);

	int flag = sym.parameter.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.parameter.line
	};

	JsonObjectAdd(value.object, "line", line);

	json_value type = {
		.tag = JSON_VALUE_STRING,
		.string = sym.parameter.type
	};

	JsonObjectAdd(value.object, "type", type);
	
	return value;
}

static json_value ParseLabel(const symbol sym)
{
	json_value value = {
		.tag = JSON_VALUE_OBJECT,
		.object = JsonObjectInit()
	};

	int flag = sym.label.referenced;

	json_value ref = {
		.tag = flag ? JSON_VALUE_TRUE : JSON_VALUE_FALSE
	};

	JsonObjectAdd(value.object, "referenced", ref);

	json_value line = {
		.tag = JSON_VALUE_NUMBER,
		.number = (int64_t) sym.label.line
	};

	JsonObjectAdd(value.object, "line", line);

	return value;
}
