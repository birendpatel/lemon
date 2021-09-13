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
#include <stdarg.h> //va_args
#include <stdbool.h>
#include <stdlib.h> //malloc
#include <stdint.h> //uint32_t
#include <stdio.h> //fprintf
#include <string.h> //memcpy

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

//recursive descent prototypes
static xerror __parse(parser *self, char *fname, file *ast);
static fiat __fiat(parser *self, xerror *err);
static decl __struct(parser *self, xerror *err);
static xerror __members(parser *self, decl *node);

//parser helpers
static void __send_usererror(const uint32_t line, const char *msg, ...);
static void parser_advance(parser *self);
static void synchronize(parser *self);
static char *to_string(char *ptr, uint32_t n);
static xerror consume(parser *self, token_type type, const char *msg);

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

//-----------------------------------------------------------------------------
//helper functions

/*******************************************************************************
 * @fn __send_usererror
 * @brief Display an error message on stderr when the user has commited a
 * grammar error.
 * @remark use send_usererror macro where possible
 ******************************************************************************/
static void __send_usererror(const uint32_t line, const char *msg, ...)
{
	assert(line > 0);
	assert(msg);

	va_list args;
	va_start(args, msg);

	fprintf(stderr, "(line %d) ", line);

	vfprintf(stderr, msg, args);

	fprintf(stderr, "\n");

	va_end(args);
}

//one of the nice benefits of using 'self' everywhere as a sort of hacky OOP
//receiver is that we can wrap the base function since the source code line is
//always accessed as self->tok.line
#define send_usererror(msg, ...) \
	__send_usererror(self->tok.line, msg, ##__VA_ARGS__)

/*******************************************************************************
 * @fn to_string
 * @brief Create a null terminated char array on heap from a non-null terminated
 * char array of length n.
 ******************************************************************************/
static char *to_string(char *ptr, uint32_t n)
{
	assert(ptr);
	assert(n);

	char *new = malloc(sizeof(char) * n);

	if (!new) {
		xerror_issue("cannot allocate memory for variable 'new'");
		return NULL;
	}

	memcpy(new, ptr, sizeof(char) * n);

	return new;
}

/*******************************************************************************
 * @fn consume
 * @brief Consume the current token held by the parser it if it matches the
 * input token type, and advance. If it doesn't match, send an error message to
 * stderr
 * and return XEPARSE.
 ******************************************************************************/
static xerror consume(parser *self, token_type type, const char *msg)
{
	assert(self);
	assert(msg);

	if (self->tok.type != type) {
		send_usererror(msg);
		return XEPARSE;
	}

	parser_advance(self);

	return XESUCCESS;
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

/*******************************************************************************
 * @fn parser_advance
 * @brief Pull a token from the scanner channel.
 * @details If the token is invalid, this function will synchronize to a new
 * sequence point.
 ******************************************************************************/
static void parser_advance(parser *self)
{
	assert(self);

	xerror err = scanner_recv(self->scn, &self->tok);

	//no release-mode check on this, its a simple bug if it triggers
	assert(!err && "attempting to read past EOF");

	if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
		token_print(self->tok);
	}

	if (self->tok.type == _INVALID) {
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
	static const char *fmt = "invalid syntax: %.*s";

	do {
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
			//needs to bypass the entire do-while loop.
			//if the token is precisely _EOF then the parser
			//will attempt a read on a closed channel.
			goto full_break;

		case _INVALID:
			self->total_invalid++;
			send_usererror(fmt, self->tok.len, self->tok.lexeme);
			fallthrough;
		default:
			//haven't yet reached a sequence point.
			//draw another token and try again.
			break;
		}

		xerror err = scanner_recv(self->scn, &self->tok);

		//no release-mode check on this, its a simple bug if it triggers
		assert(!err && "attempting to read past EOF");

		if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
			token_print(self->tok);
		}
	} while (true);

full_break:
	return;
}

//------------------------------------------------------------------------------
//recursive descent algorithm

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
static fiat __fiat(parser *self, xerror *err)
{
	assert(self);
	assert(err);

	fiat node = {0};

	switch (self->tok.type) {
	case _STRUCT:
		node.tag = NODE_DECL;

		parser_advance(self);

		node.declaration = __struct(self, err);

		if (*err == XEPARSE) {
			synchronize(self);
		} else if (*err) {
			xerror_issue("cannot create udt decl node");
			return node;
		}

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
 * @fn __struct
 * @brief Implementation of the <struct declaration> production.
 ******************************************************************************/
#define MEMBER_CAP 4

static decl __struct(parser *self, xerror *err)
{
	assert(self);
	assert(err);

	decl node = {0};
	node.tag = NODE_UDT;
	node.udt.line = self->tok.line;

	if (self->tok.type == _PUB) {
		node.udt.public = true;
		parser_advance(self);
	}

	if (self->tok.type != _IDENTIFIER) {
		send_usererror("missing identifier after struct keyword");
		*err = XEPARSE;
		return node;
	}

	node.udt.name = to_string(self->tok.lexeme, self->tok.len);

	if (!node.udt.name) {
		xerror_issue("cannot create udt name");
		*err = XENOMEM;
		return node;
	}

	parser_advance(self);

	*err = consume(self, _LEFTBRACE, "expected '{' after struct name");
	if (*err) { return node; }

	*err = member_vector_init(&node.udt.members, 0, MEMBER_CAP);

	if (*err) {
		xerror_issue("cannot init member vector");
		return node;
	}

	*err = __members(self, &node);
	if (*err) { return node; }

	*err = consume(self, _RIGHTBRACE, "expected '}' after members");
	if (*err) { return node; }

	*err = consume(self, _SEMICOLON, "expected ';' after members");
	if (*err) { return node; }


	*err = XESUCCESS;
	return node;
}

static xerror __members(parser *self, decl *node)
{
	assert(self);
	assert(node);

	xerror err = XESUCCESS;
	member attr = {0};

	while (self->tok.type != _RIGHTBRACE) {
		if (self->tok.type == _PUB) {
			attr.public = true;
			parser_advance(self);
		}

		if (self->tok.type != _IDENTIFIER) {
			send_usererror("expected member name");
			return XEPARSE;
		}

		attr.name = to_string(self->tok.lexeme, self->tok.len);

		if (!attr.name) {
			xerror_issue("cannot create member name");
			return XENOMEM;
		}

		parser_advance(self);

		err = consume(self, _COLON, "expected ':' after member name");
		if (err) { return err; }

		//TODO capture type

		err = consume(self, _SEMICOLON, "expected ';' after type");
		if (err) { return err; }

		err = member_vector_push(&node->udt.members, attr);

		if (err) {
			xerror_issue("cannot add member to vector");
			return err;
		}

		memset(&attr, 0, sizeof(member));
	}

	return err;
}
