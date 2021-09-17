/**
 * @file nodes.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief This header contains definitions for all abstract syntax tree nodes.
 * AST nodes are simple bags of data. The user must implement any operations
 * required on these bags of data, such as initialization and traversal.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "xerror.h"
#include "lib/vector.h"
#include "scanner.h"

//AST nodes contain circular and self references. For example, declarations can
//contain statements, and statements can contain declarations. Therefore, the
//typedefs are specified in advance.
typedef struct file file;
typedef struct fiat fiat;
typedef struct type type;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;

/*******************************************************************************
 * @struct member
 * @brief Each member of a Lemon struct is a name-type pair. Public members can
 * be read or written to by functions not in the associated method set.
 * @remark Typically, a member should be created on stack and then copied into
 * an associated vector<member> container.
 ******************************************************************************/
typedef struct member {
	char *name;
	type *typ;
	bool public;
} member;

make_vector(member, member, static)

/*******************************************************************************
 * @struct param
 * @brief Each parameter of a Lemon function is a name-type pair. Mutable
 * parameters can be written to from within the function body. 
 * @remark Typically, a parameter should be created on stack and then copied
 * into an associated vector<param> container.
 ******************************************************************************/
typedef struct param {
	char *name;
	type *typ;
	bool mutable;
} param;

make_vector(param, param, static)

/*******************************************************************************
 * @struct test
 * @brief Every case in a switch statement, including the default case, is
 * wrapped in a struct test.
 * @var test::cond
 * 	@brief Null for default case
 * @var test::pass
 * 	@brief Block statement
 * @remark Typically, a test should be created on stack and then copied into an
 * associated vector<test> container.
 ******************************************************************************/
typedef struct test {
	expr *cond;
	stmt *pass;
} test;

make_vector(test, test, static)

//the vector<size_t> container is used by array literal expressions to track
//designated initializers.
make_vector(size_t, idx, static)

//The implementation of fiat, decl, stmt, and expr vectors  must be postponed.
//Their corresponding vector_init functions need to have access to sizeof(T) at
//compile time, but this knowledge is not available until the corresponding
//structs are defined.
alias_vector(fiat)
declare_vector(fiat, fiat)

alias_vector(decl)
declare_vector(decl, decl)

alias_vector(stmt)
declare_vector(stmt, stmt)

alias_vector(expr)
declare_vector(expr, expr)

/*******************************************************************************
 * @enum typetag
 * @brief Union tags for struct type
 ******************************************************************************/
typedef enum typetag {
	NODE_BASE,
	NODE_POINTER,
	NODE_ARRAY,
} typetag;

/*******************************************************************************
 * @struct type
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

/*******************************************************************************
 * @enum decltag
 * @brief Union tags for struct decl
 ******************************************************************************/
typedef enum decltag {
	NODE_UDT,
	NODE_FUNCTION,
	NODE_VARIABLE,
} decltag;

/*******************************************************************************
 * @struct decl
 * @remark Type declarations from the Lemon grammar are renamed as user-defined
 * types or UDTs avoid namespace issues with type nodes.
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
			type *vartype;
			expr *value;
			bool mutable;
			bool public;
		} variable;
	};
};

//finish vector<decl> implementation now that sizeof(decl) is available
api_vector(decl, decl, static)
impl_vector_init(decl, decl, static)
impl_vector_free(decl, decl, static)
impl_vector_push(decl, decl, static)
impl_vector_get(decl, decl, static)
impl_vector_set(decl, decl, static)
impl_vector_reset(decl, decl, static)

/*******************************************************************************
 * @enum stmttag
 * @brief Union tag for struct stmt
 ******************************************************************************/
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

/*******************************************************************************
 * @struct stmt
 * @remark The break, continue, and fallthrough statements do not have an
 * associated payload in the anonymous union.
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
			decl *shortvar; //may be null if no short decl
			expr *cond;
			stmt *pass;
			stmt *fail; //may be null if no else clause
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

//finish vector<stmt> now that sizeof(stmt) is available 
api_vector(stmt, stmt, static)
impl_vector_init(stmt, stmt, static)
impl_vector_free(stmt, stmt, static)
impl_vector_push(stmt, stmt, static)
impl_vector_get(stmt, stmt, static)
impl_vector_set(stmt, stmt, static)
impl_vector_reset(stmt, stmt, static)

/*******************************************************************************
 * @enum exprtag
 * @brief Union tag for struct expr.
 ******************************************************************************/
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

//finish vector<expr> now that sizeof(expr) is available 
api_vector(expr, expr, static)
impl_vector_init(expr, expr, static)
impl_vector_free(expr, expr, static)
impl_vector_push(expr, expr, static)
impl_vector_get(expr, expr, static)
impl_vector_set(expr, expr, static)
impl_vector_reset(expr, expr, static)

/*******************************************************************************
 * @enum fiattag
 * @brief Union tag for struct fiat
 ******************************************************************************/
typedef enum fiattag {
	NODE_INVALID,
	NODE_DECL,
	NODE_STMT,
} fiattag;

/*******************************************************************************
 * @struct fiat
 * @brief Fiat nodes allow for file and blockstmt nodes to contain their child
 * declarations and child statements in a single vector.
 * *
 * @details Other options are possible. Declaration nodes could have contained
 * pointers to statement nodes, but indirection is relatively expensive. File
 * and blockstmt nodes could have tracked children in separate vector<decl> and
 * vector<stmt> struct members, but this makes symbol resolution more difficult.
 * An abstract layer of fiat nodes seems to be a decent compromise which avoids
 * pointer indirection while makeing symbol resolution easy.
 ******************************************************************************/
struct fiat {
	fiattag tag;
	union {
		decl declaration;
		stmt statement;
	};
};

//finish vector<fiat> now that sizeof(fiat) is available.
api_vector(fiat, fiat, static)
impl_vector_init(fiat, fiat, static)
impl_vector_free(fiat, fiat, static)
impl_vector_push(fiat, fiat, static)
impl_vector_get(fiat, fiat, static)
impl_vector_set(fiat, fiat, static)
impl_vector_reset(fiat, fiat, static)

/*******************************************************************************
 * @struct file
 * @brief The root node of every AST generated by the parser is a file node
 * whose children are either declarations or statements wrapped in fiats. The id
 * is unique file identifier which serves as a reference for import statements.
 ******************************************************************************/
struct file {
	char *name;
	fiat_vector fiats;
};
