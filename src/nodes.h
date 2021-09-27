// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
// Definitions for abstract syntax tree nodes.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xerror.h"
#include "lib/vector.h"
#include "scanner.h"

typedef struct file file;
typedef struct fiat fiat;
typedef struct type type;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;

//implementation of the <member list> grammar rule
typedef struct member {
	char *name;
	type *typ;
	bool public;
} member;

make_vector(member, member, static)

//implementation of the <parameter list> grammar rule
typedef struct param {
	char *name;
	type *typ;
	bool mutable;
} param;

make_vector(param, param, static)

//implementation of the <case statement> grammar rule.
typedef struct test {
	expr *cond; //NULL for default case
	stmt *pass; //block statement
} test;

make_vector(test, test, static)

make_vector(intmax_t, idx, static)

make_vector(expr *, expr, static)

//some vector implementations must be postponed until sizeof(T) is known.
alias_vector(fiat)
declare_vector(fiat, fiat)

alias_vector(decl)
declare_vector(decl, decl)

alias_vector(stmt)
declare_vector(stmt, stmt)

typedef enum typetag {
	NODE_BASE,
	NODE_POINTER,
	NODE_ARRAY,
} typetag;

//implementation of the <type> grammar rule. Composite types are composed as a
//linked list.
struct type {
	typetag tag;
	union {
		char *base;
		type *pointer;

		struct {
			type *base;
			intmax_t len;
		} array;
	};
};

typedef enum decltag {
	NODE_UDT,
	NODE_FUNCTION,
	NODE_VARIABLE,
} decltag;

//note; type declarations from the lemon grammar are renamed as user-defined
//types (UDTs) to avoid namespace collision with type nodes.
struct decl {
	decltag tag;
	union {
		struct {
			char *name;
			member_vector members;
			bool public;
		} udt;

		struct {
			char *name;
			type *ret;
			type *recv;
			stmt *block;
			param_vector params;
			bool public;
		} function;

		struct {
			char *name;
			type *vartype;
			expr *value;
			bool mutable;
			bool public;
		} variable;
	};

	uint32_t line;
};

api_vector(decl, decl, static)
impl_vector_init(decl, decl, static)
impl_vector_free(decl, decl, static)
impl_vector_push(decl, decl, static)
impl_vector_get(decl, decl, static)
impl_vector_set(decl, decl, static)
impl_vector_reset(decl, decl, static)

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
	NODE_IMPORT,
} stmttag;

struct stmt {
	stmttag tag;
	union {
		expr *exprstmt;
		expr *returnstmt; //NULL if function returns void
		char *gotolabel;
		char *import;

		struct {
			fiat_vector fiats;
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

		struct {
			decl *shortvar; //NULL if no short declaration
			expr *cond;
			stmt *pass;
			stmt *fail; //NULL if no else clause
		} branch;

		struct {
			expr *controller;
			test_vector tests;
		} switchstmt;

		struct {
			char *name;
			stmt *target;
		} label;
	};

	uint32_t line;
};

api_vector(stmt, stmt, static)
impl_vector_init(stmt, stmt, static)
impl_vector_free(stmt, stmt, static)
impl_vector_push(stmt, stmt, static)
impl_vector_get(stmt, stmt, static)
impl_vector_set(stmt, stmt, static)
impl_vector_reset(stmt, stmt, static)

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
			expr_vector args;
		} call;

		struct {
			expr *name;
			expr *attr; //must be a NODE_IDENT
		} selector;

		struct {
			expr *name;
			expr *key;
		} index;

		struct {
			idx_vector indicies;
			expr_vector values;
		} arraylit;

		struct {
			char *dist;
			expr_vector args;
		} rvarlit;

		struct {
			char *rep;
			token_type littype;
		} lit;

		struct {
			char *name;
		} ident;
	};

	uint32_t line;
};

typedef enum fiattag {
	NODE_INVALID,
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

api_vector(fiat, fiat, static)
impl_vector_init(fiat, fiat, static)
impl_vector_free(fiat, fiat, static)
impl_vector_push(fiat, fiat, static)
impl_vector_get(fiat, fiat, static)
impl_vector_set(fiat, fiat, static)
impl_vector_reset(fiat, fiat, static)

//AST root node
struct file {
	char *name;
	fiat_vector fiats;
};
