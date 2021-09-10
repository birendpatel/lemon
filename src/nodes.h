/**
 * @file nodes.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Abstract syntax tree nodes and their heap init-free functions.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h> //uint64_t, uint32_t

#include "xerror.h" //vector error codes
#include "lib/vector.h"
#include "scanner.h" //token_type, token

//node typedefs need to be specified up front due to the large amount of
//circular references. Declarations can contain statements, statements can
//contain declarations, and so on. All AST nodes fit into one of these 5 
//categories.
typedef struct file file;
typedef struct fiat fiat;
typedef struct type type;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;

/*******************************************************************************
 * @struct member
 * @brief Each member of a Lemon struct is a name associated with a type. Public
 * members can be read or written to when outside of the method set.
 * @remark no init/free pair since members are designed to be copied to vectors.
 ******************************************************************************/
typedef struct member {
	char *name;
	type *typ;
	bool public;
} member;

/*******************************************************************************
 * @struct param
 * @brief Each parameter of a Lemon function is a name associated with a type
 * and a mutability constraint.
 * @remark no init/free pair since params are designed to be copied to vectors.
 ******************************************************************************/
typedef struct param {
	char *name;
	type *typ;
	bool mutable;
} param;

//vectors contained in various nodes. Unlike the nodes, these circular and self
//references are more difficult to deal with. The various vector definitions
//are spread throughout this header since the impl macros for vectors  need to 
//know sizeof(T) at compile time. If only we wrote the compiler in C++ or Rust!
make_vector(member, member, static)
make_vector(param, param, static)
make_vector(size_t, idx, static)

alias_vector(fiat)
declare_vector(fiat, fiat)

alias_vector(decl)
declare_vector(decl, decl)

//stmt and expr vectors need to contain pointers to nodes to avoid memory leak
//issues when freeing nodes. Also to avoid unecessary vector memcopying.
alias_vector(stmt)
declare_vector(stmt*, stmt)

alias_vector(expr)
declare_vector(expr*, expr)

typedef enum typetag {
	NODE_BASE,
	NODE_POINTER,
	NODE_ARRAY,
} typetag;

/*******************************************************************************
 * @struct type
 * @brief Tagged union of type nodes
 * @details Base identifiers are represented as a single type node. Composite
 * types are composed as a linked list and match 1:1 to the recursive grammar 
 * rule for composition.
 ******************************************************************************/
struct type {
	typetag tag;
	union {
		char *base;
		type *pointer;

		struct {
			size_t len;
			type *base;
		} array;
	};
};

//returns null on alloc fail
type *type_init(typetag tag);
void type_free(type *node);

typedef enum decltag {
	NODE_UDT,
	NODE_FUNCTION,
	NODE_VARIABLE,
} decltag;

/*******************************************************************************
 * @struct decl
 * @brief Tagged union of declaration nodes.
 * @remark Type declarations are named as user-defined types (UDTs) to avoid
 * namespace issues with type nodes.
 ******************************************************************************/
struct decl {
	decltag tag;
	union {
		struct {
			char *name;
			member_vector members;
			uint32_t line;
			bool public;
		} udt;

		struct {
			char *name;
			type *ret;
			type *recv;
			stmt *block;
			param_vector params;
			uint32_t line;
			bool public;
		} function;

		struct {
			char *name;
			char *type;
			expr *value;
			bool mutable;
			bool public;
		} variable;
	};
};

//finish decl vector
api_vector(decl, decl, static)
impl_vector_init(decl, decl, static)
impl_vector_free(decl, decl, static)
impl_vector_push(decl, decl, static)
impl_vector_get(decl, decl, static)
impl_vector_set(decl, decl, static)
impl_vector_reset(decl, decl, static)

//returns null on alloc fail
decl *decl_init(decltag tag);
void decl_free(decl *node);

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
	NODE_FALLTHROUGHSTMT,
	NODE_IMPORT,
} stmttag;

/*******************************************************************************
 * @struct stmt
 * @brief Tagged union of statement nodes
 * @remark Simple jumps (break, continue, fallthrough) do not have a payload.
 ******************************************************************************/
struct stmt {
	stmttag tag;
	union {
		expr *exprstmt;
		expr *returnstmt; //null when returning on void function
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
				decl *declaration;
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
			decl *declaration; //may be null if no short decl
			expr *cond;
			stmt *pass;
			stmt *fail; //may be null if no else clause
		} branch;

		struct {
			expr *controller;
			idx_vector cases;
			stmt_vector actions;
		} switchstmt;
	};

	uint32_t line;
};

//finish stmt vector
api_vector(stmt*, stmt, static)
impl_vector_init(stmt*, stmt, static)
impl_vector_free(stmt*, stmt, static)
impl_vector_push(stmt*, stmt, static)
impl_vector_get(stmt*, stmt, static)
impl_vector_set(stmt*, stmt, static)
impl_vector_reset(stmt*, stmt, static)

//returns null on alloc fail
stmt *stmt_init(stmttag tag);
void stmt_free(stmt *node);

typedef enum exprtag {
	NODE_ASSIGNMENT,
	NODE_BINARY,
	NODE_UNARY,
	NODE_CALL,
	NODE_SELECTOR,
	NODE_INDEX,
	NODE_ARRAYLIT,
	NODE_RVARLIT,
	NODE_LIT,
	NODE_IDENT,
} exprtag;

/*******************************************************************************
 * @struct expr
 * @brief Tagged union of expression nodes.
 * @details Groups are not represented as they simply point to some expression
 * node and the extra layer of indirection currently provides no value to later
 * compiler stages. All productions from <logical or> through <factor> are 
 * represented as binary nodes. Assignment lvalue checking is deferred to the
 * semantic analyser. 
 ******************************************************************************/
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
			enum {
				UNARY_BASIC,
				UNARY_CAST,
			} tag;
			expr *operand;
			union {
				char *cast;
				token_type operator;
			};
		} unary;

		struct {
			expr *name;
			expr_vector args;
		} call;

		struct {
			expr *name;
			expr *attr;
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
			expr *dist;
			expr_vector args;
		} rvarlit;

		struct {
			char *raw;
			token_type typ;
		} lit;

		struct {
			char *name;
		} ident;
	};

	uint32_t line;
};

//finish expr vector
api_vector(expr*, expr, static)
impl_vector_init(expr*, expr, static)
impl_vector_free(expr*, expr, static)
impl_vector_push(expr*, expr, static)
impl_vector_get(expr*, expr, static)
impl_vector_set(expr*, expr, static)
impl_vector_reset(expr*, expr, static)

//returns null on alloc fail
expr *expr_init(exprtag tag);
void expr_free(expr *node);

typedef enum fiattag {
	NODE_DECL,
	NODE_STMT,
} fiattag;

/*******************************************************************************
 * @struct fiat
 * @brief File nodes need to be able to contain an ordered list of statements
 * and declarations. Fiat nodes allow for a single vector to contain both stmt
 * and decl nodes.
 * @remark fiat; a formal proposition, an authorization, or a decree.
 ******************************************************************************/
struct fiat {
	fiattag tag;
	union {
		decl declaration;
		stmt statement;
	};
};

//finish fiat vector
api_vector(fiat, fiat, static)
impl_vector_init(fiat, fiat, static)
impl_vector_free(fiat, fiat, static)
impl_vector_push(fiat, fiat, static)
impl_vector_get(fiat, fiat, static)
impl_vector_set(fiat, fiat, static)
impl_vector_reset(fiat, fiat, static)

//returns null on alloc fail
fiat *fiat_init(fiattag *tag);
void fiat_free(fiat *node);

/*******************************************************************************
 * @struct file
 * @brief The root node of every AST generated by the parser is a file node
 * while children are either declarations or statements wrapped in fiats. The id
 * is unique file identifier which may be referenced in import statements.
 ******************************************************************************/
struct file {
	char *name;
	fiat_vector fiats;
};

//returns null on alloc fail
file *file_init(void);
void file_free(file *node);
