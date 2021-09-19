/**
 * @file parser.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Recursive descent parser.
 *
 * @remark There are two classes of errors found in the parser: compiler errors
 * and user errors. User errors are not logged in the xerror buffer and they do
 * not cause the parser to terminate. User errors are not handled via normal C
 * error code propogation. Instead, they throw exceptions. Whenever an exception
 * is caught at the appropriate level, it triggers the parser to synchronize to
 * a new sequence point and the parser continues as normal.
 *
 * The use of C-style exception handling makes the recursive descent algorithm
 * implemented here readable, terse, and simple. It plays very nicely with
 * recursion, and without it we would have (and did originally have) an extra
 * 1000 lines of error checking and recovery paths to deal with. As the ancient
 * C gods once said, the supply of new lines on your screen is not a renewable
 * resource.
 *
 * The only serious limitation is that all dynamic memory allocations within a
 * try-block are not automatically released when an exception occurs. For this
 * reason the parser contains a mem_vector. All allocations push their heap
 * pointer to the mem_vector. On an exception the allocations are free'd. On
 * success, the vector is reset and the allocations are ignored. This typically
 * happens in rec_fiat().
 */

#include <assert.h>
#include <limits.h>
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
#include "xerror.h"
#include "lib/vector.h"

make_vector(void *, mem, static inline)

typedef struct parser {
	scanner *scn;
	options *opt;
	mem_vector mempool;
	token tok;
	size_t errors;
} parser;

//parser utils
static xerror parser_init(parser *self, options *opt, char *src);
static xerror parser_free(parser *self);
static void _usererror(const uint32_t line, const char *msg, ...);
static void parser_advance(parser *self);
static void synchronize(parser *self);
static void lexcpy(parser *self, char **dest, char *src, uint32_t n);
static void _check(parser *self, token_type type, char *msg, bool A, bool Z);
static size_t extract_arrnum(parser *self);

//node management
static file *file_init(char *fname);
static expr *expr_init(parser *self, exprtag tag);

//recursive descent
static file *rec_parse(parser *self, char *fname);
static fiat rec_fiat(parser *self);
static decl rec_struct(parser *self);
static member_vector rec_members(parser *self);
decl rec_func(parser *self);
static param_vector rec_params(parser *self);
type *rec_type(parser *self);
static decl rec_var(parser *self);
static stmt rec_stmt(parser *self);
static stmt rec_exprstmt(parser *self);
static expr *rec_assignment(parser *self);
static expr *rec_logicalor(parser *self);
static expr *rec_logicaland(parser *self);
static expr *rec_equality(parser *self);
static expr *rec_term(parser *self);
static expr *rec_factor(parser *self);
static expr *rec_unary(parser *self);
static expr *rec_primary(parser *self);
static expr *rec_rvar_or_ident(parser *self);
static expr *rec_rvar(parser *self, token prev);
static expr_vector rec_args(parser *self);

//parser entry point; configure parser members and launch recursive descent
xerror parse(char *src, options *opt, char *fname, file **ast)
{
	assert(src);
	assert(opt);

	parser prs = {0};
	xerror err = parser_init(&prs, opt, src);

	if (err) {
		xerror_issue("cannot initialize parser");
		return err;
	}

	*ast = rec_parse(&prs, fname);

	if (prs.errors > 1) {
		_usererror(0, "\n%d syntax errors found.", prs.errors);
	} else if (prs.errors == 1) {
		_usererror(0, "\n1 syntax error found.");
	}

	err = parser_free(&prs);

	if (err) {
		xerror_issue("cannot free parser");
		return err;
	}

	return XESUCCESS;
}

//------------------------------------------------------------------------------
// parser management

/*******************************************************************************
 * @fn parser_init
 * @brief Initialize a parser and configure the tokenizer. The magic number 8
 * is just a heurstic, a typical node often makes less than approx 8 allocations
 * before encountering a syntax error, if any.
 ******************************************************************************/
static xerror parser_init(parser *self, options *opt, char *src)
{
	assert(opt);
	assert(src);

	xerror err = scanner_init(&self->scn, src, opt);

	if (err) {
		xerror_issue("cannot initialize scanner");
		return err;
	}

	mem_vector_init(&self->mempool, 0, 8);

	self->opt = opt;
	self->tok = (token) { .type = _INVALID };
	self->errors = 0;

	return XESUCCESS;
}

/*******************************************************************************
 * @fn parser_free
 * @brief Shutdown the tokenizer and release the parser (doesn't contain AST).
 ******************************************************************************/
static xerror parser_free(parser *self)
{
	assert(self);

	xerror err = scanner_free(self->scn);

	if (err) {
		xerror_issue("cannot free scanner");
		return err;
	}

	mem_vector_free(&self->mempool, NULL);

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
static file *file_init(char *fname)
{
	file *ast = NULL;

	kmalloc(ast, sizeof(file));

	ast->name = fname;

	fiat_vector_init(&ast->fiats, 0, 128);

	return ast;
}

/*******************************************************************************
 * @fn expr_init
 * @brief Create an expression node on heap and add the pointer to the mempool.
 ******************************************************************************/
static expr *expr_init(parser *self, exprtag tag)
{
	assert(self);
	assert(tag >= NODE_ASSIGNMENT && tag <= NODE_IDENT);

	expr *new = NULL;

	kmalloc(new, sizeof(expr));

	mem_vector_push(&self->mempool, new);

	new->tag = tag;

	return new;
}

//------------------------------------------------------------------------------
//helper functions

/*******************************************************************************
 * @fn _usererror
 * @brief Send a formatted user error to stderr.
 * @param line If 0 then do not display line header.
 * @remark Use the usererror macro where possible.
 ******************************************************************************/
static void _usererror(const uint32_t line, const char *msg, ...)
{
	assert(msg);

	va_list args;
	va_start(args, msg);

	//Note, the ANSI_RED macro does not reset the colour after it is
	//applied. This allows the input msg to also be coloured red. RED()
	//cannot be applied directly to the input msg because the macro relies
	//on string concatenation.
	if (line) {
		fprintf(stderr, ANSI_RED "(line %d) ", line);
	} else {
		fprintf(stderr, ANSI_RED " \b");
	}

	vfprintf(stderr, msg, args);

	//this final RED() causes the colours to reset after the statement
	fprintf(stderr, RED("\n"));

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
 * @remark The dest pointer is added to the parser mempool
 ******************************************************************************/
static void lexcpy(parser *self, char **dest, char *src, uint32_t n)
{
	assert(src);
	assert(n);

	kmalloc(*dest, sizeof(char) * n);

	mem_vector_push(&self->mempool, *dest);

	memcpy(*dest, src, sizeof(char) * n);
}

/*******************************************************************************
 * @fn extract_arrnum
 * @brief Extract a number from the non-null terminated lexeme held in the
 * current parser token.
 * @return If the number is not a simple positive integer less than LLONG_MAX
 * then a parse exception is thrown. Otherwise a size_t representation is given.
 ******************************************************************************/
static size_t extract_arrnum(parser *self)
{
	assert(self);

	long long int candidate = 0;
	char *end = NULL;
	char *tmp = NULL;

	lexcpy(self, &tmp, self->tok.lexeme, self->tok.len);

	candidate = strtoll(tmp, &end, 10);

	if (candidate == LONG_MAX) {
		usererror("array length is too large");
		Throw(XXPARSE);
	} else if (*end != '\0') {
		usererror("array length is not a simple base-10 integer");
		Throw(XXPARSE);
	} else if (candidate == 0) {
		usererror("array length cannot be zero");
		Throw(XXPARSE);
	}

	return (size_t) candidate;
}

/*******************************************************************************
 * @fn _check
 * @brief Advance to the next token if A is true. Check the token against the
 * input type and throw XEPARSE if it doesn't match. Advance again on a
 * successful match if Z is true.
 * @remark Use the check, move_check, check_move, and move_check_move macros.
 ******************************************************************************/
static void  _check(parser *self, token_type type, char *msg, bool A, bool Z)
{
	assert(self);
	assert(msg);

	if (A) {
		parser_advance(self);
	}

	if (self->tok.type != type) {
		usererror(msg);
		Throw(XXPARSE);
	}

	if (Z) {
		parser_advance(self);
	}
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
		//reading past EOF is an off-by-one bug in the second-to-last
		//function in the call stack. Very easy fix: backtrace on gdb
		//to locate the function. At least one call to parser_advance()
		//in its body is occuring too early.
		//
		//This is relegated to an assertion as a tradeoff to improve
		//readability and simplicity of the descent algorithm.
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
 * is a sequence point.
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
			goto exit_loop;

		//non-sequence points
		case _INVALID:
			usererror(fmt, self->tok.len, self->tok.lexeme);
			self->errors++;
			fallthrough;
		default:
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

exit_loop:
	return;
}

//------------------------------------------------------------------------------
//recursive descent algorithm

/*******************************************************************************
 * @fn rec_parse
 * @brief Recursive descent entry point; configure a root file node, load the
 * first token into the parser, and descend!
 * @return The returned root file node is erroneous if the errors member is
 * greater than zero.
 ******************************************************************************/
static file *rec_parse(parser *self, char *fname)
{
	assert(self);

	file *ast = file_init(fname);

	parser_advance(self);

	while (self->tok.type != _EOF) {
		fiat node = rec_fiat(self);

		if (node.tag == NODE_INVALID) {
			self->errors++;
		} else {
			fiat_vector_push(&ast->fiats, node);
		}
	}

	return ast;
}

/*******************************************************************************
 * @fn rec_fiat
 * @brief Dispatch to decl or stmt handler and then wrap the return node. Catch
 * any parser errors and synchronize before continuing.
 *
 * @remark The fiat node doesnt need be qualified with the volatile keyword. The
 * CException library dictates if you change a stack variable in the try block
 * and require its updated values after an exception is thrown, the stack var
 * must be volatile. In this situation, any updates made to the fiat node in the
 * try block are unimportant. All we care is that the catch block overwrites the
 * tag. The union contents are unimportant.
 *
 * @return If the returned fiat node has a tag equal to NODE_INVALID, then one
 * or more syntax errors occured and the syntax tree rooted at the node is ill
 * formed.
 ******************************************************************************/
static fiat rec_fiat(parser *self)
{
	assert(self);

	CEXCEPTION_T exception;
	fiat node = { .tag = NODE_INVALID };

	Try {
		switch (self->tok.type) {
		case _STRUCT:
			node.tag = NODE_DECL;
			node.declaration = rec_struct(self);
			break;

		case _FUNC:
			node.tag = NODE_DECL;
			node.declaration= rec_func(self);
			break;

		case _LET:
			node.tag = NODE_DECL;
			node.declaration = rec_var(self);
			break;

		default:
			node.tag = NODE_STMT;
			node.statement = rec_stmt(self);
			break;
		}

		mem_vector_reset(&self->mempool, NULL);
	} Catch (exception) {
		synchronize(self);
		node.tag = NODE_INVALID;
		mem_vector_reset(&self->mempool, free);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_struct
 * @brief Implementation of the <struct declaration> production.
 * @param self The token held by the parser on invocation must be _STRUCT.
 * @decl The returned UDT node is always valid. A parse exception may be thrown.
 ******************************************************************************/
static decl rec_struct(parser *self)
{
	assert(self);

	decl node = {
		.tag = NODE_UDT,
		.udt = {
			.name = NULL,
			.members = {0},
			.public = false
		},
		.line = self->tok.line
	};

	parser_advance(self);

	if (self->tok.type == _PUB) {
		node.udt.public = true;
		parser_advance(self);
	}

	if (self->tok.type != _IDENTIFIER) {
		usererror("missing struct name");
		Throw(XXPARSE);
	}

	lexcpy(self, &node.udt.name, self->tok.lexeme, self->tok.len);

	move_check_move(self, _LEFTBRACE, "expected '{' after name");

	node.udt.members = rec_members(self);

	check_move(self, _RIGHTBRACE, "expected '}' after members");

	check_move(self, _SEMICOLON, "expected ';' after struct");

	return node;
}

/*******************************************************************************
 * @fn rec_members
 * @brief Process each scope:name:type triplet within a struct declaration.
 * @return Vector of valid member nodes. A parse exception may be thrown.
 * @remark The member capacity uses the magic number 4 as a heuristic. We assume
 * most structs are generally quite small.
 ******************************************************************************/
static member_vector rec_members(parser *self)
{
	assert(self);

	member_vector vec = {0};
	member_vector_init(&vec, 0, 4);

	//vector<member> stores members directly, not their pointers, so the
	//parser mempool only needs to track the single vector data pointer.
	mem_vector_push(&self->mempool, vec.data);

	member attr = {
		.name = NULL,
		.typ = NULL,
		.public = false
	};

	while (self->tok.type != _RIGHTBRACE) {
		if (self->tok.type == _PUB) {
			attr.public = true;
			parser_advance(self);
		}

		if (self->tok.type != _IDENTIFIER) {
			usererror("expected member name");
			Throw(XXPARSE);
		}

		lexcpy(self, &attr.name, self->tok.lexeme, self->tok.len);

		move_check_move(self, _COLON, "expected ':' after name");

		attr.typ = rec_type(self);

		check_move(self, _SEMICOLON, "expected ';' after type");

		member_vector_push(&vec, attr);

		memset(&attr, 0, sizeof(member));
	}

	return vec;
}

/*******************************************************************************
 * @fn rec_func
 * @brief Create a function declaration node.
 * @param self The token held by the parser on invocation must be _FUNC.
 * @return Function node is always valid. A parse exception may be thrown.
 ******************************************************************************/
decl rec_func(parser *self)
{
	assert(self);

	decl node = {
		.tag = NODE_FUNCTION,
		.function = {
			.name = NULL,
			.ret = NULL,
			.recv = NULL,
			.block = NULL,
			.params = {0},
			.public = false
		},
		.line = self->tok.line
	};

	parser_advance(self);

	if (self->tok.type == _PUB) {
		node.function.public = true;
		parser_advance(self);
	}

	if (self->tok.type != _IDENTIFIER) {
		usererror("missing function name");
		Throw(XXPARSE);
	}

	lexcpy(self, &node.function.name, self->tok.lexeme, self->tok.len);

	//parameter list
	move_check_move(self, _LEFTPAREN, "missing '(' after function name");

	if (self->tok.type == _VOID) {
		parser_advance(self);
	} else {
		node.function.params = rec_params(self);

		if (node.function.params.len == 0) {
			usererror("empty parameter list must state 'void'");
			Throw(XXPARSE);
		}
	}

	check_move(self, _RIGHTPAREN, "missing ')' after parameters");

	//return
	check_move(self, _RETURN, "missing 'return' after parameters");

	if (self->tok.type == _VOID) {
		parser_advance(self);
	} else {
		node.function.ret = rec_type(self);
	}

	//receiver
	if (self->tok.type == _FOR) {
		parser_advance(self);
		node.function.recv = rec_type(self);
	}

	//TODO block statement
	return node;
}

/*******************************************************************************
 * @fn rec_params
 * @brief Process each mutability:name:type triplet within a param declaration.
 * @return Vector of valid param nodes. A parse exception may be thrown.
 * @remark The param capacity uses the magic number 4 as a heuristic. We assume
 * most param lists are generally quite small.
 ******************************************************************************/
static param_vector rec_params(parser *self)
{
	assert(self);

	param_vector vec = {0};
	param_vector_init(&vec, 0, 4);

	//vector<param> stores params directly instead of pointers to params,
	//so the mempool only needs to track the vector data pointer.
	mem_vector_push(&self->mempool, vec.data);

	param attr  = {
		.name = NULL,
		.typ = NULL,
		.mutable = false
	};

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(self, _COMMA,
					"parameters must be comma separated");
		}

		if (self->tok.type == _MUT) {
			attr.mutable = true;
			parser_advance(self);
		}

		if (self->tok.type != _IDENTIFIER) {
			usererror("expected parameter name");
			Throw(XXPARSE);
		}

		lexcpy(self, &attr.name, self->tok.lexeme, self->tok.len);

		move_check_move(self, _COLON, "expected ':' after name");

		attr.typ = rec_type(self);

		param_vector_push(&vec, attr);

		memset(&attr, 0, sizeof(param));
	}

	return vec;
}

/*******************************************************************************
 * @fn rec_var
 * @brief Create a variable declaration node.
 * @param self The token held by the parser on invocation must be _VAR.
 * @return The return node is always valid. A parse exception may be thrown.
 ******************************************************************************/
static decl rec_var(parser *self)
{
	assert(self);

	decl node = {
		.tag = NODE_VARIABLE,
		.variable = {
			.name = NULL,
			.vartype = NULL,
			.value = NULL,
			.mutable = false,
			.public = false,
		},
		.line = self->tok.line
	};

	parser_advance(self);

	if (self->tok.type == _PUB) {
		node.variable.public = true;
		parser_advance(self);
	}

	if (self->tok.type == _MUT) {
		node.variable.mutable = true;
		parser_advance(self);
	}

	if (self->tok.type != _IDENTIFIER) {
		usererror("expected lvalue in variable declaration");
		Throw(XXPARSE);
	}

	lexcpy(self, &node.variable.name, self->tok.lexeme, self->tok.len);

	move_check_move(self, _COLON, "expected ':' before type");

	node.variable.vartype = rec_type(self);

	check_move(self, _EQUAL, "declaration must have an initializer");

	//TODO node.variable.value expression

	check_move(self, _SEMICOLON, "missing ';' after declaration");

	return node;
}

/*******************************************************************************
 * @fn rec_type
 * @brief Create a linked list of nodes representing a type
 * @return A singly linked list of nodes where the head represented the outer-
 * most composite type and the tail is the inner-most base type. A parse
 * exception may be thrown.
 ******************************************************************************/
type *rec_type(parser *self)
{
	assert(self);

	type *node = NULL;
	kmalloc(node, sizeof(type));
	mem_vector_push(&self->mempool, node);

	switch (self->tok.type) {
	case _IDENTIFIER:
		node->tag = NODE_BASE;
		lexcpy(self, &node->base, self->tok.lexeme, self->tok.len);
		parser_advance(self);
		break;

	case _STAR:
		node->tag = NODE_POINTER;
		parser_advance(self);
		node->pointer = rec_type(self);
		break;

	case _LEFTBRACKET:
		move_check(self, _LITERALINT, "expected array size");
		node->array.len = extract_arrnum(self);
		move_check_move(self, _RIGHTBRACKET, "expected ']'");
		node->array.base = rec_type(self);
		break;

	default:
		usererror("expected type but none found");

		Throw(XXPARSE);
		break;
	}

	return node;
}

/*******************************************************************************
 * @fn rec_stmt
 * @brief Create a statement node.
 * @return Always returns a valid node. May throw a parse exception.
 ******************************************************************************/
static stmt rec_stmt(parser *self)
{
	assert(self);

	stmt node = { 0 };

	switch(self->tok.type) {
	case _LEFTBRACKET:
		break;

	case _FOR:
		break;

	case _WHILE:
		break;

	case _SWITCH:
		break;

	case _IF:
		break;

	case _RETURN:
		break;

	case _GOTO:
		break;

	case _IMPORT:
		break;

	case _BREAK:
		break;

	case _CONTINUE:
		break;

	case _FALLTHROUGH:
		break;

	case _LABEL:
		break;

	default:
		node = rec_exprstmt(self);
		break;
	}

	return node;
}

/*******************************************************************************
 * @fn rec_exprstmt
 * @brief Create an expression statement node.
 * @param self The token held by the parser on invocation must be the first
 * token of the expression.
 * @return Always returns a valid node. May throw a parse exception.
 ******************************************************************************/
static stmt rec_exprstmt(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_EXPRSTMT,
		.exprstmt = NULL,
		.line = self->tok.line
	};

	node.exprstmt = rec_assignment(self);

	check_move(self, _SEMICOLON, "missing ';' after expression");

	return node;
}

/*******************************************************************************
 * @fn rec_assignment
 * @brief <assignment> ::= <logical or> ("=" <assignment>)?
 * @remark Note that Lemon does not allow chained assignments.
 ******************************************************************************/
static expr *rec_assignment(parser *self)
{
	assert(self);

	expr *node = rec_logicalor(self);

	if (self->tok.type == _EQUAL) {
		expr *tmp = node;
		node = expr_init(self, NODE_ASSIGNMENT);
		node->assignment.lvalue = tmp;
		node->line = self->tok.line;

		parser_advance(self);
		node->assignment.rvalue = rec_logicalor(self);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_logicalor
 * @brief <logical or> ::= <logical and> ("||" <logical and>)*
 ******************************************************************************/
static expr *rec_logicalor(parser *self)
{
	assert(self);

	expr *node = rec_logicaland(self);
	expr *tmp = NULL;

	while (self->tok.type == _OR) {
		tmp = node;
		node = expr_init(self, NODE_BINARY);

		node->binary.left = tmp;
		node->binary.operator = _OR;
		node->line = self->tok.line;

		parser_advance(self);
		node->binary.right = rec_logicaland(self);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_logicaland
 * @brief <logical and> ::= <equality> ("&&" <equality>)*
 ******************************************************************************/
static expr *rec_logicaland(parser *self)
{
	assert(self);

	expr *node = rec_equality(self);

	while (self->tok.type == _AND) {
		expr *tmp = node;
		node = expr_init(self, NODE_BINARY);

		node->binary.left = tmp;
		node->binary.operator = _AND;
		node->line = self->tok.line;

		parser_advance(self);
		node->binary.right = rec_equality(self);
	}

	return node;
}

/*******************************************************************************
 * @fn equality
 * @brief <equality> ::= <term> ((">"|"<"|">="|"<="|"=="|"!=") <term>)*
 ******************************************************************************/
static expr *rec_equality(parser *self)
{
	assert(self);

	expr *node = rec_term(self);
	expr *tmp = NULL;

	while (true) {
		switch (self->tok.type) {
		case _GREATER:
			fallthrough;

		case _LESS:
			fallthrough;

		case _GEQ:
			fallthrough;

		case _LEQ:
			fallthrough;

		case _EQUALEQUAL:
			fallthrough;

		case _NOTEQUAL:
			tmp = node;
			node = expr_init(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			parser_advance(self);
			node->binary.right = rec_term(self);
			break;

		default:
			goto exit_loop;
		}
	}

exit_loop:
	return node;
}

/*******************************************************************************
 * @fn rec_term
 * @brief249 <term> ::= <factor> (("+" | "-" | "|" | "^") <factor>)*
 ******************************************************************************/
static expr *rec_term(parser *self)
{
	assert(self);

	expr *node = rec_factor(self);
	expr *tmp = NULL;

	while (true) {
		switch (self->tok.type) {
		case _ADD:
			fallthrough;

		case _MINUS:
			fallthrough;

		case _BITOR:
			fallthrough;

		case _BITXOR:
			tmp = node;
			node = expr_init(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			parser_advance(self);
			node->binary.right = rec_factor(self);
			break;

		default:
			goto exit_loop;
		}
	}

exit_loop:
	return node;
}

/*******************************************************************************
 * @fn rec_factor
 * @brief <factor> ::= <unary> (("*"|"/"|"%"|">>"|"<<"|"&") <unary>)*
 ******************************************************************************/
static expr *rec_factor(parser *self)
{
	assert(self);

	expr *node = rec_unary(self);
	expr *tmp = NULL;

	while (true) {
		switch (self->tok.type) {
		case _STAR:
			fallthrough;

		case _DIV:
			fallthrough;

		case _MOD:
			fallthrough;

		case _LSHIFT:
			fallthrough;

		case _RSHIFT:
			fallthrough;

		case _AMPERSAND:
			tmp = node;
			node = expr_init(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			parser_advance(self);
			node->binary.right = rec_unary(self);
			break;

		default:
			goto exit_loop;
		}
	}

exit_loop:
	return node;
}

/*******************************************************************************
 * @fn rec_unary
 * @brief <unary> ::= ("-" | "+" | "'" | "!" | "*" | "&" | <cast>) <unary> 
 * | <primary>
 ******************************************************************************/
static expr *rec_unary(parser *self)
{
	assert(self);

	expr *node = NULL;

	switch (self->tok.type) {
	case _MINUS:
		fallthrough;

	case _ADD:
		fallthrough;

	case _BITNOT:
		fallthrough;

	case _NOT:
		fallthrough;

	case _STAR:
		fallthrough;

	case _AMPERSAND:
		node = expr_init(self, NODE_UNARY);
		node->unary.operator = self->tok.type;
		node->line = self->tok.line;
		parser_advance(self);
		node->unary.operand = rec_unary(self);
		break;

	case _LEFTPAREN:
		node = expr_init(self, NODE_CAST);
		node->line = self->tok.line;
		parser_advance(self);
		node->cast.casttype = rec_type(self);
		check_move(self, _RIGHTPAREN, "expected ')' after type casting");
		node->cast.operand = rec_unary(self);
		break;

	default:
		node = rec_primary(self);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_primary
 * @brief <primary> ::= <atom> (<call> | <selector> | <index>)*
 * @remark <atom> production is expanded and implemented in rec_primary
 ******************************************************************************/
static expr *rec_primary(parser *self)
{
	assert(self);

	static const char *errfmt = "expression is ill-formed at '%.*s'";
	expr *node = NULL;

	switch (self->tok.type) {
	case _IDENTIFIER:
		node = rec_rvar_or_ident(self);
		break;

	case _LITERALINT:
		fallthrough;

	case _LITERALFLOAT:
		fallthrough;

	case _LITERALSTR:
		fallthrough;

	case _NULL:
		fallthrough;

	case _SELF:
		fallthrough;

	case _TRUE:
		fallthrough;

	case _FALSE:
		node = expr_init(self, NODE_LIT);
		node->line = self->tok.line;
		lexcpy(self, &node->lit.rep, self->tok.lexeme, self->tok.len);
		node->lit.littype = self->tok.type;
		parser_advance(self);
		break;

	default:
		usererror(errfmt, self->tok.len, self->tok.lexeme);
		Throw(XXPARSE);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_rvar_or_ident
 * @brief Distinguish a leading identifier as a simple identifier node or as a
 * complex rvar literal.
 * @return Always returns a valid node. May throw a parse exception.
 ******************************************************************************/
static expr *rec_rvar_or_ident(parser *self)
{
	assert(self);

	token tmp = self->tok;

	parser_advance(self);

	if (self->tok.type == _TILDE) {
		return rec_rvar(self, tmp);
	}

	expr *node = expr_init(self, NODE_IDENT);
	node->line = tmp.line;
	lexcpy(self, &node->ident.name, tmp.lexeme, tmp.len);

	//no scanner advance due to previous tilde check
	return node;
}

/*******************************************************************************
 * @fn rec_rvar
 * @brief Create a random variable literal expression node. The current token
 * held by the parser must be the tilde.
 * @param prev The leading identifier representing the random distribution
 * @return Always returns a valid node. May throw a parse exception.
 ******************************************************************************/
static expr *rec_rvar(parser *self, token prev)
{
	assert(self);

	expr *node = expr_init(self, NODE_RVARLIT);
	
	node->line = prev.line;

	lexcpy(self, &node->rvarlit.dist, prev.lexeme, prev.len);

	parser_advance(self);

	//some random distributions are not parameterized
	if (self->tok.type == _SEMICOLON) {
		node->rvarlit.args = (expr_vector) {
			.len = 0,
			.cap = 0,
			.data = NULL
		};

		return node;
	}

	node->rvarlit.args = rec_args(self); 
	return node;
}

/*******************************************************************************
 * @fn rec_args
 * @brief Process an argument list into an expr_vector.
 * @remark Vector capacity is set heuristically.
 ******************************************************************************/
static expr_vector rec_args(parser *self)
{
	assert(self);

	expr_vector vec = {0};
	expr_vector_init(&vec, 0, 4);

	while (self->tok.type != _SEMICOLON) {
		if (vec.len > 0) {
			move_check(self, _COMMA, "expected ',' after arg");
		}

		expr_vector_push(&vec, rec_assignment(self));
	}

	return vec;
}
