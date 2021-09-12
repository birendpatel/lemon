/**
 * @file parser.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Top-down operator precedence parser (aka the Pratt Parser).
 */

#include <assert.h>

#include "nodes.h"
#include "scanner.h"

typedef struct parser {
	scanner *scn;
	options *opt;
	token tok;
} parser;

xerror parser_init(parser *self, char *src, options *opt);
xerror parser_free(parser *self);
void parser_advance(parser *self);

xerror parse(char *src, options *opt)
{
	assert(src);
	assert(opt);

	xerror err = XESUCCESS;
	parser prs = {0};

	err = parser_init(&prs, src, opt);

	if (err) {
		xerror_issue("cannot initialize parser");
		return err;
	}
	
	//TODO temporary
	
	while (prs.tok.type != _EOF) {
		parser_advance(&prs);
	}

	//TODO end temporary

	err = parser_free(&prs);

	if (err) {
		xerror_issue("cannot free parser");
		return err;
	}

	return err;
}

xerror parser_init(parser *self, char *src, options *opt)
{
	assert(self);
	assert(src);
	assert(opt);

	xerror err = scanner_init(&self->scn, src, opt);

	if (err) {
		xerror_issue("cannot initialize scanner");
		return err;
	}

	self->opt = opt;
	self->tok = (token) {.type = _INVALID};

	return XESUCCESS;
}

xerror parser_free(parser *self)
{
	assert(self);

	xerror err = scanner_free(self->scn);

	if (err) {
		xerror_issue("cannot free scanner");
		return err;
	}

	return XESUCCESS;
}

void parser_advance(parser *self)
{
	assert(self);

	(void) scanner_recv(self->scn, &self->tok);

	if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
		token_print(self->tok);
	}
}
