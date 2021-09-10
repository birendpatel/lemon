/**
 * @file nodes.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief init-free function pairs for AST nodes. The xerror API isn't used for
 * node functions since the only failure that can occur is ENOMEM. Instead, all
 * init functions return NULL on error.
 *
 * @details The freeing of AST nodes is accomplished recursively in post-order
 * fashion. For a larger program, invoking file_free may incur a noticeable hit
 * on performance. And it may be better to free the resources in a separate
 * thread after the linear IR is created.
 */

#include <assert.h>
#include <stdlib.h>

#include "nodes.h"

type *type_init(typetag tag)
{
	assert(tag >= NODE_BASE && tag <= NODE_ARRAY);

	type *new = malloc(sizeof(type));

	if (!new) {
		return NULL;
	}

	new->tag = tag;

	switch (tag) {
	case NODE_BASE:
		new->base = NULL;
		break;

	case NODE_POINTER:
		new->pointer = NULL;
		break;

	case NODE_ARRAY:
		new->array.len = 0;
		new->array.base = NULL;
		break;
	}

	return new;
}

void type_free(type *node)
{
	if (!node) {
		return;
	}

	switch (node->tag) {
	case NODE_BASE:
		free(node->base);
		break;

	case NODE_POINTER:
		free(node->pointer);
		break;

	case NODE_ARRAY:
		free(node->array.base);
		break;
	}

	free(node);
}

//parameter and member vectors need some default capacity. Four is a heuristic.
#define MEMBER_CAP 4
#define PARAM_CAP 4

decl *decl_init(decltag tag)
{
	assert(tag >= NODE_UDT && tag <= NODE_VARIABLE);

	int err = 0;

	decl *new = malloc(sizeof(decl));

	if (!new) {
		return NULL;
	}

	switch (tag) {
	case NODE_UDT:
		err = member_vector_init(&new->udt.members, 0, MEMBER_CAP);

		if (err) {
			goto fail;
		}

		new->udt.name = NULL;
		new->udt.line = 0;
		new->udt.public = false;

		break;

	case NODE_FUNCTION:
		err = param_vector_init(&new->function.params, 0, PARAM_CAP);

		if (err) {
			goto fail;
		}

		new->function.name = NULL;
		new->function.ret = NULL;
		new->function.recv = NULL;
		new->function.block = NULL;
		new->function.line = 0;
		new->function.public = false;

		break;

	case NODE_VARIABLE:
		new->variable.name = NULL;
		new->variable.type = NULL;
		new->variable.value = NULL;
		new->variable.mutable = false;
		new->variable.public = false;

		break;
	}

	return new;

fail:
	free(new);
	return NULL;
}

void decl_free(decl *node)
{
	if (!node) {
		return;
	}

	switch (node->tag) {
	case NODE_UDT:
		free(node->udt.name);
		member_vector_free(&node->udt.members, NULL);
		break;
	
	case NODE_FUNCTION:
		free(node->function.name);
		type_free(node->function.ret);
		type_free(node->function.recv);
		stmt_free(node->function.block);
		param_vector_free(&node->function.params, NULL);
		break;

	case NODE_VARIABLE:
		free(node->variable.name);
		free(node->variable.type);
		expr_free(node->variable.value);
		break;
	}

	free(node);
}

//capacity for fiat_vector in block statements. Unlike param and member capacity
//block statements can vary widely in size. We use the magic number four again,
//but it will trigger a vector growth in most situations. Better to err on the
//side of not asking for enough memory, than asking for too much.
#define BLOCK_CAP 4

//capacity for idx_vector and stmt_vector in switch statements. Not as much of
//an issue with the magic number here. It might grow a tiny bit, but large jump 
//tables and single dispatching are rarer situations.
#define SWITCH_CAP 4

stmt *stmt_init(stmttag tag)
{
	assert(tag >= NODE_EXPRSTMT && tag <= NODE_IMPORT);

	int err = 0;

	stmt *new = malloc(sizeof(stmt));

	if (!new) {
		return NULL;
	}

	switch (tag) {
	case NODE_EXPRSTMT:
		new->exprstmt = NULL;
		break;

	case NODE_BLOCK:
		err = fiat_vector_init(&new->block.fiats, 0, BLOCK_CAP);

		if (err) {
			goto fail;
		}

		break;

	case NODE_FORLOOP:
		new->forloop.tag = FOR_INIT;
		new->forloop.init = NULL; //implicitly clears the decl pointer
		new->forloop.cond = NULL;
		new->forloop.post = NULL;
		new->forloop.block = NULL;
		break;

	case NODE_WHILELOOP:
		new->whileloop.cond = NULL;
		new->whileloop.block = NULL;
		break;

	case NODE_SWITCHSTMT:
		new->switchstmt.controller = NULL;
		
		err = idx_vector_init(&new->switchstmt.cases, 0, SWITCH_CAP);

		if (err) {
			goto fail;
		}

		err = stmt_vector_init(&new->switchstmt.actions, 0, SWITCH_CAP);

		if (err) {
			goto fail;
		}

		break;

	case NODE_BRANCH:
		new->branch.declaration = NULL;
		new->branch.cond = NULL;
		new->branch.pass = NULL;
		new->branch.fail = NULL;
		break;

	case NODE_RETURNSTMT:
		new->returnstmt = NULL;
		break;

	case NODE_GOTOLABEL:
		new->gotolabel = NULL;
		break;

	case NODE_IMPORT:
		new->import = NULL;
		break;

	case NODE_BREAKSTMT:
		break;
	
	case NODE_CONTINUESTMT:
		break;

	case NODE_FALLTHROUGHSTMT:
		break;
	}

	return new;

fail:
	free(new);
	return NULL;
}

void stmt_free(stmt *node)
{
	if (!node) {
		return;
	}
	
	switch (node->tag) {
	case NODE_EXPRSTMT:
		expr_free(node->exprstmt);
		break;

	case NODE_BLOCK:
		fiat_vector_free(&node->block.fiats, NULL);
		break;

	case NODE_FORLOOP:
		if (node->forloop.tag == FOR_DECL) {
			decl_free(node->forloop.declaration);
		} else {
			expr_free(node->forloop.init);
		}

		expr_free(node->forloop.cond);
		expr_free(node->forloop.post);
		stmt_free(node->forloop.block);

		break;

	case NODE_WHILELOOP:
		expr_free(node->whileloop.cond);
		stmt_free(node->whileloop.block);
		break;

	case NODE_SWITCHSTMT:
		expr_free(node->switchstmt.controller);
		idx_vector_free(&node->switchstmt.cases, NULL);
		stmt_vector_free(&node->switchstmt.actions, stmt_free);
		break;

	case NODE_BRANCH:
		decl_free(node->branch.declaration);
		expr_free(node->branch.cond);
		stmt_free(node->branch.pass);
		stmt_free(node->branch.fail);
		break;

	case NODE_RETURNSTMT:
		free(node->returnstmt);
		break;

	case NODE_GOTOLABEL:
		free(node->gotolabel);
		break;

	case NODE_IMPORT:
		free(node->import);
		break;

	case NODE_BREAKSTMT:
		break;

	case NODE_CONTINUESTMT:
		break;

	case NODE_FALLTHROUGHSTMT:
		break;
	}

	free(node);
}
