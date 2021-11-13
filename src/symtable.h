// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

typedef struct symbol symbol;
typedef struct symtable symtable;

//------------------------------------------------------------------------------
// Symbols are associated with a cstring identifier and placed into a hash table

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
			bool referenced;
		} function;

		struct {
			symtable *table;
			bool referenced;
		} method;

		struct {
			symtable *table;
			size_t bytes;
			bool referenced;
		} udt;

		struct {
			bool referenced;
			bool parameter;
		};
	};
};

make_map(symbol, Symbol, static)

//------------------------------------------------------------------------------
// symbol tables are lexically scoped; all symbol tables in memory together
// form an n-ary tree which is threaded intrusively through each table via
// the symtable.parent and symbol.union.table references.

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
		} global;
	};
};
