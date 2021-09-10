/**
 * @file nodes.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Abstract syntax tree nodes and their heap init-free functions.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h> //uint64_t, uint32_t

#include "xerror.h"
#include "lib/vector.h"
#include "scanner.h"

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
 ******************************************************************************/
typedef struct param {
	char *name;
	type *typ;
	bool mutable;
} param;

//vectors contained in various nodes.
alias_vector(fiat)
declare_vector(fiat, fiat)

alias_vector(decl)
declare_vector(decl, decl)

alias_vector(member)
declare_vector(member, member)

alias_vector(param)
declare_vector(param, param)

alias_vector(expr)
declare_vector(expr, expr)

alias_vector(idx)
declare_vector(size_t, idx)

/*******************************************************************************
 * @struct type
 * @brief Tagged union of type nodes
 * @details Base identifiers are represented as a single type node. Composite
 * types are composed as a linked list and match 1:1 to the recursive grammar 
 * rule for composition.
 ******************************************************************************/
struct type {
	enum {
		NODE_BASE,
		NODE_POINTER,
		NODE_ARRAY,
	} tag;

	union {
		char *base;
		type *pointer;

		struct {
			size_t length;
			type *base;
		} array;
	};
};

/*******************************************************************************
 * @struct decl
 * @brief Tagged union of declaration nodes.
 * @remark Type declarations are named as user-defined types (UDTs) to avoid
 * namespace issues with type nodes.
 ******************************************************************************/
struct decl {
	enum {
		NODE_UDT,
		NODE_FUNCTION,
		NODE_VARIABLE,
		NODE_STATEMENT,
	} tag;

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

		//file vectors need to be able to contain both declarations and 
		//statements. The statement pointer is a costly but necessary
		//layer of indirection.
		stmt *statement;
	};
};

/*******************************************************************************
 * @struct stmt
 * @brief Tagged union of statement nodes
 * @remark Simple jumps (break, continue, fallthrough) do not have a payload.
 ******************************************************************************/
struct stmt {
	enum {
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
	} tag;

	union {
		expr *exprstmt;
		expr *returnstmt;
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
			stmt *fail;
		} branch;
	};

	uint32_t line;
};

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
	enum {
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
	} tag;
	
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

/*******************************************************************************
 * @struct fiat
 * @brief File nodes need to be able to contain an ordered list of statements
 * and declarations. Fiat nodes allow for a single vector to contain both stmt
 * and decl nodes.
 * @remark fiat; a formal proposition, an authorization, or a decree.
 ******************************************************************************/
struct fiat {
	enum {
		NODE_DECL,
		NODE_STMT,
	} tag;

	union {
		decl declaration;
		stmt statement;
	};
};

/*******************************************************************************
 * @struct file
 * @brief The root node of every AST generated by the parser is a file node
 * while children are either declarations or statements wrapped in fiats. The id
 * is unique file identifier which may be referenced in import statements.
 ******************************************************************************/
struct file {
	uint64_t id;
	fiat_vector fiats;
};
