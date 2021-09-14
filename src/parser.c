/**
 * @file parser.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Recursive descent parser.
 *
 * @remark There are two classes of errors found in the parser: compiler errors
 * and user errors. User errors are signaled by the xerror XEPARSE error code.
 * User errors are never complier errors, so they are not logged and they do not
 * cause the parser to terminate. In all XEPARSE situations, the parser sends
 * the user error on stderr and attempts to synchronize to a new sequence point.
 */

#include <assert.h>
#include <stdarg.h> 
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "nodes.h"
#include "options.h"
#include "scanner.h"

typedef struct parser {
	scanner *scn;
	options *opt;
	token tok;
	bool ill_formed;
} parser;

//parser utils
static void _usererror(const uint32_t line, const char *msg, ...);
static void parser_advance(parser *self);
static void synchronize(parser *self);
static xerror lexcpy(char *dest, char *src, uint32_t n);
static xerror _check(parser *self, token_type type, char *msg, bool A, bool Z);

//node management
static xerror file_init(file *ast, char *fname);

//recursive descent
static xerror rec_parse(parser *self, char *fname, file *ast);
static fiat rec_fiat(parser *self, xerror *err);
static decl rec_struct(parser *self, xerror *err);
static xerror rec_members(parser *self, decl *node);

//parser entry point; configure parser members and launch recursive descent
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

	err = rec_parse(&prs, fname, ast);

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

//------------------------------------------------------------------------------
//node management

/*******************************************************************************
 * @fn file_init
 * @brief Initialize the fiat vector and add an import tag.
 * @remark The fiat vector needs an initial capacity, but if it is too small we
 * will dip into kernel mode too often. The magic number 128 is a heuristic.
 * For many REPL programs, it is large enough to never trigger a vector resize.
 ******************************************************************************/
static xerror file_init(file *ast, char *fname)
{
	xerror err = fiat_vector_init(&ast->fiats, 0, 128);

	if (err) {
		xerror_issue("could not initalize fiat vector");
	};

	ast->name = fname;

	return err;
}

//-----------------------------------------------------------------------------
//helper functions

/*******************************************************************************
 * @fn _usererror
 * @brief Send a formatted user error to stderr.
 * @remark Use the usererror macro where possible.
 ******************************************************************************/
static void _usererror(const uint32_t line, const char *msg, ...)
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

/*******************************************************************************
 * @def usererror
 * @brief Wrapper over _usererror.
 * @remark Since the Lemon source code uses 'self' everywhere as a hacky OOP
 * receiver, we can guarantee that the source code line is always accessed as
 * self->tok.line.
 ******************************************************************************/
#define usererror(msg, ...) _usererror(self->tok.line, msg, ##__VA_ARGS__)

/*******************************************************************************
 * @fn lexcpy
 * @brief Copy a src lexeme of length n to the dest pointer and reformat it as
 * a valid C string.
 * @return XENOMEM if dynamic allocation fails.
 ******************************************************************************/
static xerror lexcpy(char *dest, char *src, uint32_t n)
{
	assert(src);
	assert(n);

	dest = malloc(sizeof(char) * n);

	if (!dest) {
		return XENOMEM;
	}

	memcpy(dest, src, sizeof(char) * n);

	return XESUCCESS;
}

/*******************************************************************************
 * @fn _check
 * @brief Advance to the next token if A is true. Check the token against the
 * input type and return XEPARSE if it doesn't match. Advance again on a
 * successfuly match if Z is true.
 * @remark Use the check, move_check, check_move, and move_check_move macros.
 ******************************************************************************/
static xerror _check(parser *self, token_type type, char *msg, bool A, bool Z)
{
	assert(self);
	assert(msg);

	if (A) {
		parser_advance(self);
	}

	if (self->tok.type != type) {
		usererror(msg);
		return XEPARSE;
	}

	if (Z) {
		parser_advance(self);
	}

	return XESUCCESS;
}

//don't move to a new token, check the current one, and don't move afterwards
#define check(self, type, msg) _check(self, type, msg, false, false)

//move to a new token, check it, but don't move again
#define move_check(self, type, msg) _check(self, type, msg, true, false)

//check the current token, move on success
#define check_move(self, type, msg) _check(self, type, msg, false, true)

//move to a new token, check it, move again on success
#define move_check_move(self, type, msg) _check(self, type, msg,  true, true)

/*******************************************************************************
 * @fn parser_advance
 * @brief Receive a token from the scanner channel.
 * @details If the token is invalid, this function will synchronize to a new
 * sequence point before it returns.
 ******************************************************************************/
static void parser_advance(parser *self)
{
	assert(self);

	if (scanner_recv(self->scn, &self->tok)) {
		assert(0 != 0 && "attempted to read past EOF");
	}

	if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
		token_print(self->tok);
	}

	if (self->tok.type == _INVALID) {
		synchronize(self);
	}
}

/*******************************************************************************
 * @fn synchronize
 * @brief If the current token held by the parser is not a sequence point, then
 * throw it away. Receive new tokens from the scanner channel until a sequence
 * point is found.
 * @return This function guarantees on return that the token held by the parser
 * is a sequence point. If at least one token was thrown away, the ill_formed
 * flag on the parser will be set.
 ******************************************************************************/
static void synchronize(parser *self)
{
	static const char *fmt = "invalid syntax: %.*s";

	do {
		switch(self->tok.type) {
		
		//sequence points
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
			goto success;

		//non-sequence points
		case _INVALID:
			usererror(fmt, self->tok.len, self->tok.lexeme);
			fallthrough;
		default:
			self->ill_formed = true;
			break;
		}

		//draw another token and try again
		if (scanner_recv(self->scn, &self->tok)) {
			assert(0 != 0 && "attempted to read past EOF");
		}

		if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
			token_print(self->tok);
		}
	} while (true);

success:
	return;
}

//------------------------------------------------------------------------------
//recursive descent algorithm

/*******************************************************************************
 * @fn rec_parse
 * @brief Recursive descent entry point
 ******************************************************************************/
static xerror rec_parse(parser *self, char *fname, file *ast)
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
		fiat node = rec_fiat(self, &err);

		if (err) {
			xerror_issue("could not create fiat node");
			return err;
		}

		fiat_vector_push(&ast->fiats, node);
	}

	return XESUCCESS;
}

/*******************************************************************************
 * @fn rec_fiat
 * @brief Dispatch to decl or stmt handler and then wrap the return node.
 ******************************************************************************/
static fiat rec_fiat(parser *self, xerror *err)
{
	assert(self);
	assert(err);

	fiat node = {0};

	switch (self->tok.type) {
	case _STRUCT:
		node.tag = NODE_DECL;

		parser_advance(self);

		node.declaration = rec_struct(self, err);

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
 * @fn rec_struct
 * @brief Implementation of the <struct declaration> production.
 ******************************************************************************/
#define MEMBER_CAP 4

static decl rec_struct(parser *self, xerror *err)
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
		usererror("missing identifier after struct keyword");
		*err = XEPARSE;
		return node;
	}
	
	*err = lexcpy(node.udt.name, self->tok.lexeme, self->tok.len);

	if (*err) {
		xerror_issue("cannot create udt name");
		return node;
	}

	*err = move_check_move(self, _LEFTBRACE, "expected '{' after name");
	if (*err) { return node; }

	*err = member_vector_init(&node.udt.members, 0, MEMBER_CAP);

	if (*err) {
		xerror_issue("cannot init member vector");
		return node;
	}

	*err = rec_members(self, &node);
	if (*err) { return node; }

	*err = check_move(self, _RIGHTBRACE, "expected '}' after members");
	if (*err) { return node; }

	*err = check_move(self, _SEMICOLON, "expected ';' after struct");
	if (*err) { return node; }


	*err = XESUCCESS;
	return node;
}

/*******************************************************************************
 * @fn rec_members
 * @brief Process each scope:name:type triplet within a struct declaration.
 ******************************************************************************/
static xerror rec_members(parser *self, decl *node)
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
			usererror("expected member name");
			return XEPARSE;
		}

		err = lexcpy(attr.name, self->tok.lexeme, self->tok.len);

		if (err) {
			xerror_issue("cannot create member name");
			return XENOMEM;
		}

		err = move_check_move(self, _COLON, "expected ':' after name");
		if (err) { return err; }

		//TODO capture type

		err = check_move(self, _SEMICOLON, "expected ';' after type");
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
