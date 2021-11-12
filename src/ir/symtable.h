// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The classic symbol table implementation; each symbol resides somewhere in
// a hash table and each hash table resides somewhere in a parent pointer tree
// intrusively built into the symtable.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "map.h"

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
