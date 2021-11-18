// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// This file defines the abstract syntax tree nodes. The documentation in this
// header makes referneces to the Lemon language specification and the EBNF
// grammar located at ../langspec.txt. The grammar metasyntax operations '+' and
// '*' are implemented via vectors. The '|' operator is implemented via tagged
// anonymous unions.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "scanner.h"
#include "str.h"
#include "vector.h"

typedef struct module module;
typedef struct fiat fiat;
typedef struct type type;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;
typedef struct import import;
typedef struct member member;
typedef struct param param;
typedef struct test test;

typedef struct symbol symbol;
typedef struct symtable symtable;

//------------------------------------------------------------------------------
//<member list>
//@entry: tag == SYMBOL_FIELD

struct member {
	cstring *name;
	type *typ;
	symbol *entry;
	bool public;
};

make_vector(member, Member, static)

//------------------------------------------------------------------------------
//<parameter list>
//@entry: tag = SYMBOL_PARAMETER

struct param {
	cstring *name;
	type *typ;
	symbol *entry; 
	bool mutable;
};

make_vector(param, Param, static)

//------------------------------------------------------------------------------
//<case statement>
//@cond: NULL for the default case
//@pass: tag == BLOCK_STMT

struct test {
	expr *cond; 
	stmt *pass;
};

make_vector(test, Test, static)

//------------------------------------------------------------------------------

//<tagged index> integer literals within the <array literal> rule
make_vector(intmax_t, Index, static)

//<arguments> within the <call>, <rvar literal>, and <array literal> rules
make_vector(expr *, Expr, static)

//some vectors need to be forward declared for node references, but the
//implementations must be postponed until sizeof(T) is available.
alias_vector(Fiat)
declare_vector(fiat, Fiat)

alias_vector(Decl)
declare_vector(decl, Decl)

alias_vector(Stmt)
declare_vector(stmt, Stmt)

//------------------------------------------------------------------------------

typedef enum typetag {
	NODE_BASE,
	NODE_POINTER,
	NODE_ARRAY,
	NODE_NAMED,
} typetag;

//<type> rule; composite types form a singly linked list where the tail node
//represents the inner-most base type.
struct type {
	typetag tag;
	union {
		//@reference: non-null
		struct {
			type *reference;
		} pointer;

		//@element: non-null
		//@len: may be zero
		struct {
			type *element;
			intmax_t len;
		} array;

		//@reference: non-null
		struct {
			cstring *name;
			type *reference;
		} named;

		//@entry: tag == SYMBOL_NATIVE
		struct {
			cstring *name;
			symbol *entry;
		} base;
	};
};

//------------------------------------------------------------------------------

typedef enum decltag {
	NODE_UDT,
	NODE_FUNCTION,
	NODE_METHOD,
	NODE_VARIABLE,
} decltag;

//<declaration> rule; due to difficulties with managing namespace collisions
//in C, type declarations from the lemon grammar are nicknamed as UDTs (User
//Defined Types).
struct decl {
	decltag tag;
	union {
		//@entry: tag == SYMBOL_UDT
		struct {
			cstring *name;
			symbol *entry;
			vector(Member) members;
			bool public;
		} udt;

		//@entry: tag == SYMBOL_FUNCTION
		//@ret: NULL if function returns void
		struct {
			cstring *name;
			symbol *entry;
			type *ret; 
			stmt *block;
			vector(Param) params;
			bool public;
		} function;

		//@entry: tag == SYMBOL_METHOD
		//@ret: NULL if method returns void
		struct {
			cstring *name;
			symbol *entry;
			type *ret;
			type *recv; 
			stmt *block;
			vector(Param) params;
			bool public;
		} method;

		//@entry: tag == SYMBOL_VARIABLE
		struct {
			cstring *name;
			symbol *entry;
			type *vartype;
			expr *value;
			bool mutable;
			bool public;
		} variable;
	};

	size_t line;
};

api_vector(decl, Decl, static)
impl_vector_init(decl, Decl, static)
impl_vector_free(decl, Decl, static)
impl_vector_push(decl, Decl, static)
impl_vector_get(decl, Decl, static)
impl_vector_set(decl, Decl, static)
impl_vector_reset(decl, Decl, static)

//------------------------------------------------------------------------------

typedef enum stmttag{
	NODE_EXPRSTMT,
	NODE_BLOCK,
	NODE_FORLOOP,
	NODE_WHILELOOP,
	NODE_SWITCHSTMT,
	NODE_BRANCH,
	NODE_RETURNSTMT,
	NODE_BREAKSTMT,
	NODE_CONTINUESTMT,
	NODE_GOTOLABEL,
	NODE_LABEL,
	NODE_FALLTHROUGHSTMT,
} stmttag;

struct stmt {
	stmttag tag;
	union {
		expr *exprstmt;
		expr *returnstmt; //NULL if function returns void
		cstring *gotolabel;

		struct {
			symtable *table;
			vector(Fiat) fiats;
		} block;

		struct {
			enum {
				FOR_DECL,
				FOR_INIT,
			} tag;
			union {
				decl *shortvar;
				expr *init;
			};
			expr *cond;
			expr *post;
			stmt *block;
		} forloop;

		struct {
			expr *cond;
			stmt *block;
		} whileloop;

		//@shortvar: NULL if no short declaration
		//@fail: NULL if no else clause
		struct {
			decl *shortvar; 
			expr *cond;
			stmt *pass;
			stmt *fail; 
		} branch;

		struct {
			expr *controller;
			vector(Test) tests;
		} switchstmt;

		//@entry: tag == SYMBOL_LABEL
		struct {
			cstring *name;
			symbol *entry;
			stmt *target;
		} label;
	};

	size_t line;
};

api_vector(stmt, Stmt, static)
impl_vector_init(stmt, Stmt, static)
impl_vector_free(stmt, Stmt, static)
impl_vector_push(stmt, Stmt, static)
impl_vector_get(stmt, Stmt, static)
impl_vector_set(stmt, Stmt, static)
impl_vector_reset(stmt, Stmt, static)

//------------------------------------------------------------------------------

typedef enum exprtag {
	NODE_ASSIGNMENT,
	NODE_BINARY,
	NODE_UNARY,
	NODE_CAST,
	NODE_CALL,
	NODE_SELECTOR,
	NODE_INDEX,
	NODE_ARRAYLIT,
	NODE_RVARLIT,
	NODE_LIT,
	NODE_IDENT,
} exprtag;

struct expr {
	exprtag tag;
	union {
		struct {
			expr *lvalue;
			expr *rvalue;
		} assignment;

		struct {
			expr *left;
			expr *right;
			token_type operator;
		} binary;

		struct {
			expr *operand;
			token_type operator;
		} unary;

		struct {
			expr *operand;
			type *casttype;
		} cast;

		struct {
			expr *name;
			vector(Expr) args;
		} call;

		//@attr: tag == NODE_IDENT
		struct {
			expr *name;
			expr *attr; 
		} selector;

		struct {
			expr *name;
			expr *key;
		} index;

		struct {
			vector(Index) indicies;
			vector(Expr) values;
		} arraylit;

		struct {
			cstring *dist;
			vector(Expr) args;
		} rvarlit;

		struct {
			cstring *rep;
			token_type littype;
		} lit;

		struct {
			cstring *name;
		} ident;
	};

	size_t line;
};

//------------------------------------------------------------------------------

typedef enum fiattag {
	NODE_DECL,
	NODE_STMT,
} fiattag;

struct fiat {
	fiattag tag;
	union {
		decl declaration;
		stmt statement;
	};
};

api_vector(fiat, Fiat, static)
impl_vector_init(fiat, Fiat, static)
impl_vector_free(fiat, Fiat, static)
impl_vector_push(fiat, Fiat, static)
impl_vector_get(fiat, Fiat, static)
impl_vector_set(fiat, Fiat, static)
impl_vector_reset(fiat, Fiat, static)

//------------------------------------------------------------------------------
// @alias: NULL if import path is the empty string

struct import {
	cstring *alias;
	symbol *entry;
	size_t line;
};

make_vector(import, Import, static)

//------------------------------------------------------------------------------
// @alias: non-null
// @next: intrusive linked list; not populated by parser
// @table: tag == TABLE_MODULE
// @flag: unused; free to read or write for any purpose

struct module {
	vector(Import) imports;
	vector(Decl) declarations;
	const cstring *alias;
	struct module *next;
	symtable *table;
	bool flag;
};
