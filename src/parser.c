/**
 * @file parser.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Recursive descent parser.
 *
 * @details There are two classes of errors found in the parser: compiler errors
 * and user errors. User errors are presented on stderr but they do not trigger
 * error handling or recovery paths in the compiler. In other words, user errors
 * are never compiler errors. In all situations, the parser simply attempts to
 * synchronize to a new sequence point when a user error occurs. Only when the
 * parser is finished and returns program control to the compile.c compile()
 * function is an xerror (XEPARSE) potentially generated.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h> //uint32_t
#include <stdio.h> //fprintf

#include "defs.h" //fallthrough
#include "nodes.h"
#include "options.h"
#include "scanner.h"

typedef struct parser {
	scanner *scn;
	options *opt;
	token tok;
	uint32_t total_invalid;
} parser;

//parser prototypes
static void parser_advance(parser *self);
static xerror __parse(parser *self, char *fname, file *ast);
static void synchronize(parser *self);
fiat __fiat(parser *self, xerror *err);

//node prototypes
static xerror file_init(file *ast, char *fname);

xerror parse(char *src, options *opt, char *fname, file *ast)
{
	assert(src);
	assert(opt);
	
	parser prs = {0};

	xerror err = scanner_init(&prs.scn, src, opt);
	
	if (err) {
		xerror_issue("cannot initialize scanner");
		return err;
	}

	prs.opt = opt;
	prs.tok = (token) {.type = _INVALID};
	prs.total_invalid = 0;
	
	err = __parse(&prs, fname, ast);

	if (err) {
		xerror_issue("recursive descent failed: %s", xerror_str(err));
	}

	err = scanner_free(prs.scn);

	if (err) {
		xerror_issue("cannot free scanner");
		return err;
	}

	return XESUCCESS;
}

/*******************************************************************************
 * @fn parser_advance
 * @brief Pull a token from the scanner channel.
 * @details If the token is invalid, this function will synchronize to a new
 * sequence point.
 ******************************************************************************/
static void parser_advance(parser *self)
{
	assert(self);

	static const char *fmt = "invalid syntax: line %d: %.*s\n"; 

	(void) scanner_recv(self->scn, &self->tok);
	
	if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
		token_print(self->tok);
	}

	if (self->tok.type == _INVALID) {
		self->total_invalid++;
		
		token tok = self->tok;
		fprintf(stderr, fmt, tok.line, tok.len, tok.lexeme);

		synchronize(self);
	}
}

/*******************************************************************************
 * @fn synchronize
 * @brief Throw away tokens in the scanner channel until a sequence point is
 * found. Currently, a sequence point is defined as a declaration, a statement,
 * or the EOF token.
 ******************************************************************************/
static void synchronize(parser *self)
{
	while (true) {
		(void) scanner_recv(self->scn, &self->tok);

		if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
			token_print(self->tok);
		}

		switch(self->tok.type) {
		case _STRUCT: 
			fallthrough;
		case _FUNC: 
			fallthrough;
		case _LET:
			fallthrough;
		case _RETURN:
			fallthrough;
		case _BREAK:
			fallthrough;
		case _CONTINUE:
			fallthrough;
		case _GOTO:
			fallthrough;
		case _IMPORT:
			fallthrough;
		case _LEFTBRACE:
			fallthrough;
		case _FOR:
			fallthrough;
		case _WHILE:
			fallthrough;
		case _IF:
			fallthrough;
		case _SWITCH:
			fallthrough;
		case _CASE:
			fallthrough;
		case _EOF: 
			break;

		case _INVALID:
			self->total_invalid++;
			fallthrough;
		default: 
			continue;
		}
	}
}

/*******************************************************************************
 * @fn __parse
 * @brief Recursive descent entry point
 ******************************************************************************/
static xerror __parse(parser *self, char *fname, file *ast)
{
	assert(self);
	assert(ast);

	xerror err = file_init(ast, fname);

	if (err) {
		xerror_issue("could not initialize file node");
		return err;
	}

	//prime the parser with an initial state
	parser_advance(self);

	while (self->tok.type != _EOF) {
		fiat node = __fiat(self, &err);

		if (err) {
			xerror_issue("could not create fiat node");
			return err;
		}

		fiat_vector_push(&ast->fiats, node);
	}

	return XESUCCESS;
}

/*******************************************************************************
 * @fn __fiat
 * @brief Dispatch to decl or stmt handler and then wrap the return node.
 ******************************************************************************/
fiat __fiat(parser *self, xerror *err)
{
	fiat node = {0};

	switch (self->tok.type) {
	case _STRUCT:
		node.tag = NODE_DECL;
		parser_advance(self);
		break;
	
	case _FUNC:
		node.tag = NODE_DECL;
		parser_advance(self);
		break;
	
	case _LET:
		node.tag = NODE_DECL;
		parser_advance(self);
		break;

	default:
		node.tag = NODE_STMT;
		parser_advance(self);
		break;
	}

	*err = XESUCCESS;
	return node;
}

/*******************************************************************************
 * @fn file_init
 * @brief initialize fiat vector and add import tag
 * @remark fiat vector needs some initial capacity, but we don't want it to be
 * so small that we dip into kernel mode too often to grow the vector. The magic
 * number 128 is really just a rough heuristic. 
 ******************************************************************************/
#define FILE_CAP 128

static xerror file_init(file *ast, char *fname)
{
	ast->name = fname;
	xerror err = fiat_vector_init(&ast->fiats, 0, FILE_CAP);

	if (err) {
		xerror_issue("could not initalize fiat vector");
	};

	return err;
}
