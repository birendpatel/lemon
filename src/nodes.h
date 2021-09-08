/**
 * @file nodes.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Abstract syntax tree nodes and their heap init-free functions.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h> //uint64_t

#include "xerror.h"
#include "lib/vector.h"

//node typedefs need to be specified up front due to the large amount of
//circular references. exprs can contain stmts, stmts can contain exprs, etc..
typedef struct file file;
typedef struct action action;
typedef struct decl decl;
typedef struct stmt stmt;
typedef struct expr expr;

//declaration node flags
#define MUTABLE	(1 << 0) /**< @brief The param/var/member is mutable */
#define PUBLIC	(1 << 1) /**< @brief The func/member is public */
#define POINTER	(1 << 2) /**< @brief The type is a pointer to a base type */
#define RECVPTR (1 << 3) /**< @brief The receiver is a pointer to a UDT */

//function parameters and struct members
//i.e., x: *i32 generates (param) {"x", "i32", POINTER}
typedef struct param {
	char *name;
	char *type;
	uint32_t flags;
} param;

//vectors contained in various nodes. Since nodes are simple bags of data, the
//user (aka the parser) must implement the actual vector operations.
alias_vector(action)
declare_vector(action, action)

alias_vector(param)
declare_vector(param, param)

/*******************************************************************************
 * @struct decl
 * @brief Tagged union of declaration nodes.
 * @remark The Lemon grammar expands declarations to include statements. But,
 * statement nodes are handled in a separate struct due to their complexity.
 ******************************************************************************/
struct decl {
	enum {
		TYP,
		FUNC,
		VAR,
	} type;
	union {
		struct {
			char *name
			param_vector params;
			uint32_t flags;
			uint32_t line;
		} typ;
		struct {
			char *name;
			char *ret;
			char *recv;
			stmt *block;
			param_vector params;
			uint32_t flags;
			uint32_t line;
		} func;
		struct {
			char *name;
			char *type;
			expr *value;
			uint32_t flags;
			uint32_t line;
		} var;
	};
};

/*******************************************************************************
 * @struct stmt
 * @brief Tagged union of statement nodes
 ******************************************************************************/
struct stmt {
	enum {
		GENERIC,
	} type;
	union {
		struct {
			expr *expression;
		} generic;
	};
};

/*******************************************************************************
 * @struct action
 * @brief Thin wrapper over decls and stmts to allow polymorphism in the
 * vector elements for file nodes.
 ******************************************************************************/
struct action {
	enum {
		DECL,
		STMT,
	} type;
	union {
		decl declaration;
		stmt statement;
	};
};

/*******************************************************************************
 * @struct file
 * @brief The root node of every AST generated by the parser is a file node
 * whose children are declaration nodes. The id is simply a unique randomly
 * generated number for differentiating the ASTs of each input file.
 * @details <file> ::= <declaration>* "EOF"
 ******************************************************************************/
struct file {
	uint64_t id;
	action_vector actions;
};