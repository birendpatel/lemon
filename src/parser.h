// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Compiler phase 2: syntax analysis: tokens -> parser -> abstract syntax tree
//
// This file defines the abstract syntax tree nodes. The documentation in this
// header makes referneces to the Lemon language specification and the EBNF
// grammar located at ../langspec.txt. The grammar metasyntax operations '+' and
// '*' are implemented via vectors. The '|' operator is implemented via tagged
// anonymous unions.

#include <stdbool.h>
#include <stdint.h>

#include "str.h"
#include "vector.h"

//AST nodes
typedef struct file file;
typedef struct fiat fiat;
typedef struct type type;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;
typedef struct import import;

//------------------------------------------------------------------------------
//Returns NULL if initialisation fails or the AST is ill-formed. On success all
//node members are valid except symtable references.

file *const SyntaxTreeInit(const cstring *src, const cstring *alias);

//------------------------------------------------------------------------------
// The input AST must not be null. Only allocations made by SyntaxTreeInit are
// cleaned up. Any node modifications or deletions made to the AST after the
// SyntaxTreeInit call are not released by this function.

void SyntaxTreeFree(file *ast);

//------------------------------------------------------------------------------
//<member list>

typedef struct member {
	cstring *name;
	type *typ;
	bool public;
} member;

make_vector(member, Member, static)

//------------------------------------------------------------------------------
//<parameter list>

typedef struct param {
	cstring *name;
	type *typ;
	bool mutable;
} param;

make_vector(param, Param, static)

//------------------------------------------------------------------------------
//<case statement>

typedef struct test {
	expr *cond; //NULL for the default case
	stmt *pass; //guaranteed to be a block statement
} test;

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
} typetag;

//<type> rule; composite types form a singly linked list where the tail node
//represents the inner-most base type.
struct type {
	typetag tag;
	union {
		cstring *base;
		type *pointer;

		struct {
			type *base;
			intmax_t len;
		} array;
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
		struct {
			cstring *name;
			vector(Member) members;
			bool public;
		} udt;

		struct {
			cstring *name;
			type *ret; //NULL if function returns void
			stmt *block;
			vector(Param) params;
			bool public;
		} function;

		struct {
			cstring *name;
			type *ret; //NULL if method returns void
			type *recv; 
			stmt *block;
			vector(Param) params;
			bool public;
		} method;

		struct {
			cstring *name;
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

		struct {
			decl *shortvar; //NULL if no short declaration
			expr *cond;
			stmt *pass;
			stmt *fail; //NULL if no else clause
		} branch;

		struct {
			expr *controller;
			vector(Test) tests;
		} switchstmt;

		struct {
			cstring *name;
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

		struct {
			expr *name;
			expr *attr; //attr->tag == NODE_IDENT
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

struct import {
	cstring *alias; //NULL if import path is an empty string
};

make_vector(import, Import, static)

struct file {
	vector(Import) imports;
	vector(Decl) declarations;
	const cstring *alias;
	size_t errors;
};
