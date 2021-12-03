// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// This file defines the abstract syntax tree nodes and provides functionality
// for creating, manipulating, and deleting (sub)trees. Lexical analysis is
// subsumed within the parser.
//
// The documentation in this header makes referneces to the Lemon language 
// specification and the EBNF grammar located at ../langspec.txt. The grammar 
// metasyntax operations '+' and '*' are implemented via vectors. The '|' 
// operator is implemented via tagged anonymous unions.
//
// Unless otherwise specified via a comment, all pointers within the nodes
// are dynamically allocated and set to non-NULL by SyntaxTreeInit. 

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "scanner.h"
#include "str.h"
#include "symtable.h"
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

//returns NULL if tree is ill-formed. On success all of the symbol and symtable
//pointers in the returned tree are set to NULL.
module *SyntaxTreeInit(const cstring *filename);

//------------------------------------------------------------------------------
//<member list>

struct member {
	cstring *name;
	type *typ; 
	symbol *entry;
	size_t line;
	bool public;
};

make_vector(member, Member, static)

//------------------------------------------------------------------------------
//<parameter list>

struct param {
	cstring *name; 
	type *typ;
	symbol *entry;
	size_t line;
	bool mutable;
};

make_vector(param, Param, static)

//------------------------------------------------------------------------------
//<case statement>

struct test {
	expr *cond; //null for the default case
	stmt *pass; //tag == block_stmt
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
	NODE_NAMED,
	NODE_POINTER,
	NODE_ARRAY,
} typetag;

//<type> rule; composite types form a singly linked list where the tail node
//represents the inner-most base type.
struct type {
	typetag tag;
	union {
		struct {
			cstring *name; 
			symbol *entry; //null
		} base;
	
		struct {
			cstring *name; 
			type *reference;
		} named;
	
		struct {
			type *reference;
		} pointer;

		struct {
			type *element; 
			intmax_t len; //may be zero
		} array;
	};

	size_t line;
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
			symbol *entry; //null
			vector(Member) members; //never empty
			bool public;
		} udt;

		struct {
			cstring *name; 
			symbol *entry; //null
			type *ret; //list head, null if func returns void
			stmt *block; 
			vector(Param) params; //may be empty
			bool public;
		} function;

		struct {
			cstring *name;
			symbol *entry; //null
			type *ret; //list-head, null if it returns void
			type *recv;  
			stmt *block;
			vector(Param) params; //may be empty
			bool public;
		} method;

		struct {
			cstring *name; 
			symbol *entry; //null
			type *vartype; //list-head 
			expr *value; //null if no initialisation
			bool mutable;
			bool public; //meaningless if decl is not file scoped
		} variable;
	};

	size_t line;
};

api_vector(decl, Decl, static)
impl_vector_init(decl, Decl, static)
impl_vector_push(decl, Decl, static)
impl_vector_get(decl, Decl, static)
impl_vector_set(decl, Decl, static)
impl_vector_reset(decl, Decl, static)

//------------------------------------------------------------------------------
// pointers to expr, stmt, and decl nodes are always dynamically allocated

typedef enum stmttag{
	NODE_EXPRSTMT,
	NODE_BLOCK,
	NODE_FORLOOP,
	NODE_WHILELOOP,
	NODE_SWITCHSTMT,
	NODE_BRANCH,
	NODE_RETURNSTMT,
	NODE_BREAKSTMT, //no payload
	NODE_CONTINUESTMT, //no payload
	NODE_GOTOLABEL,
	NODE_LABEL,
	NODE_FALLTHROUGHSTMT, //no payload
} stmttag;

struct stmt {
	stmttag tag;
	union {
		expr *exprstmt;
		expr *returnstmt; //NULL if function returns void

		struct {
			cstring *name; 
			symbol *entry;
		} gotostmt;

		struct {
			symtable *table; //null
			vector(Fiat) fiats; //may be empty
		} block;

		struct {
			enum {
				FOR_DECL,
				FOR_INIT,
			} tag;
			union {
				decl *shortvar; //valid when FOR_DECL
				expr *init; //valid when FOR_INIT
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
			decl *shortvar; //null when no short declaration
			expr *cond;
			stmt *pass;
			stmt *fail; //null when no else clause
		} branch;

		struct {
			expr *controller;
			vector(Test) tests;
		} switchstmt;

		struct {
			cstring *name; 
			symbol *entry; //null
			stmt *target; 
		} label;
	};

	size_t line;
};

api_vector(stmt, Stmt, static)
impl_vector_init(stmt, Stmt, static)
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
			expr *attr; //tag == node_ident
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
impl_vector_push(fiat, Fiat, static)
impl_vector_get(fiat, Fiat, static)
impl_vector_set(fiat, Fiat, static)
impl_vector_reset(fiat, Fiat, static)

//------------------------------------------------------------------------------

struct import {
	cstring *alias; //null if import path is the empty string
	symbol *entry; //null
	size_t line;
};

make_vector(import, Import, static)

//------------------------------------------------------------------------------

struct module {
	vector(Import) imports;
	vector(Decl) declarations;
	const cstring *alias;
	struct module *next; //null (requires resolver) 
	symtable *table; //null
	bool flag; //free to use
};
