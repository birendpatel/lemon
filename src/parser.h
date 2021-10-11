// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The parser API provides functionality for creating, manipulating, and
// destroying abstract syntax trees. Lexical analysis is subsumed within the
// parsing process.
//
// The documentation in this header makes references to the Lemon language
// specification and the EBNF grammar located at ../langspec.txt. As a general
// guideline, the grammar metasyntax operators '+' and '*' are typically
// implemented as vectors. The '|' operator is implemented via tagged unions.
// The '?' operator is often represented as a boolean true or non-null pointer.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "options.h"
#include "scanner.h"
#include "xerror.h"

#include "lib/str.h"
#include "lib/vector.h"

//AST nodes
typedef struct file file; //root
typedef struct fiat fiat;
typedef struct type type;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;

//ASTs are created on heap. On error the returned pointer is NULL. Any compiler
//errors that occured during parsing are logged to the xerror buffer.
file *SyntaxTreeInit(options *opt, string src, string alias);

//file pointer will be set to NULL on return
void SyntaxTreeFree(file *ast);

//<member list>
typedef struct member {
	string name;
	type *typ;
	bool public;
} member;

make_vector(member, member, static)

//<parameter list>
typedef struct param {
	string name;
	type *typ;
	bool mutable;
} param;

make_vector(param, param, static)

//<case statement>
typedef struct test {
	expr *cond; //NULL for default case
	stmt *pass; //block statement
} test;

make_vector(test, test, static)

//<tagged index> within the <array literal> rule
make_vector(intmax_t, idx, static)

make_vector(expr *, expr, static)

//some vectors need to be forward declared for node references, but the
//implementations must be postponed until sizeof(T) is available.
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

//<type> grammar rule. Composite types are implemented as a singly linked
//list. The tail of the list represents the inner-most base type.
struct type {
	typetag tag;
	union {
		string base;
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
			string name;
			member_vector members;
			bool public;
		} udt;

		struct {
			string name;
			type *ret;
			type *recv;
			stmt *block;
			param_vector params;
			bool public;
		} function;

		struct {
			string name;
			type *vartype;
			expr *value;
			bool mutable;
			bool public;
		} variable;
	};

	size_t line;
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
		string gotolabel;
		string import;

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
			string name;
			stmt *target;
		} label;
	};

	size_t line;
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
			expr *attr; //attr->tag == NODE_IDENT
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
			string dist;
			expr_vector args;
		} rvarlit;

		struct {
			string rep;
			token_type littype;
		} lit;

		struct {
			string name;
		} ident;
	};

	size_t line;
};

typedef enum fiattag {
	NODE_INVALID, //ill-formed tree
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

struct file {
	string alias;
	fiat_vector fiats;
};
