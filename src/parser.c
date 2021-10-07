//Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
//The recursive descent may encounter two types of errors: compiler errors and
//user errors. Compiler errors are treated in the same fashion as the rest of
//the program; Log the issue via xerror and propogate the error code up the 
//call chain.
//
//User errors are very different. They are not logged via xerror. They do not
//cause the parser to terminate. They do not propogate error codes. Instead,
//they throw exceptions. Whenever an exception is caught, the parser will
//synchronize to a new sequence point within the token stream. Sequence points
//are, generally speaking, any tokens which identify or hint at the start of a
//new statement or declaration.
//
//The only serious limitation to setjmp/longjmp exception handling is that
//dynamic memory allocations made within a try-block are not released when an
//exception occurs. For this reason, the parser contains a vector<void*> used
//to track and release all memory allocations that were requested since the
//last successful sequence point. The parser cannot walk the erroneous subtree
//in post-order to release the allocations because sometimes an exception is
//thrown before a parent and child are connected. This results in a dangling
//pointer. So, every kmalloc call must be coupled to an immediate vector push.

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "scanner.h"

#include "lib/channel.h"
#include "lib/vector.h"

make_vector(void *, mem, static)

typedef struct parser {
	options *opt;
	token_channel *chan;
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
static intmax_t extract_arrnum(parser *self);

//node management
static file *file_init(char *fname);
static expr *expr_init(parser *self, exprtag tag);
static stmt *stmt_init(parser *self, stmt src);
static decl *decl_init(parser *self, decl src);

//declarations
static file *rec_parse(parser *self, char *fname);
static fiat rec_fiat(parser *self);
static decl rec_struct(parser *self);
static member_vector rec_members(parser *self);
decl rec_func(parser *self);
static param_vector rec_params(parser *self);
type *rec_type(parser *self);
static decl rec_var(parser *self);

//statements
static stmt rec_stmt(parser *self);
static stmt rec_exprstmt(parser *self);
static stmt rec_block(parser *self);
static stmt rec_label(parser *self);
static stmt rec_anonymous_target(parser *self);
static stmt rec_named_target(parser *self);
static stmt rec_forloop(parser *self);
static stmt rec_whileloop(parser *self);
static stmt rec_branch(parser *self);
static stmt rec_switch(parser *self);
static test_vector rec_tests(parser *self);

//expressions
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
static expr *rec_arraylit(parser *self);
static expr *rec_arraylit(parser *self);
static expr *rec_ident(parser *self, token tok);
static expr *rec_access(parser *self, expr *prev);

xerror parse(options *opt, char *src, char *fname, file **ast)
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

static xerror parser_init(parser *self, options *opt, char *src)
{
	assert(opt);
	assert(src);

	kmalloc(self->chan, sizeof(token_channel));
	token_channel_init(self->chan, KiB(1));

	xerror err = ScannerInit(opt, src, self->chan);

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

	xerror err = token_channel_free(self->chan, NULL);

	if (err) {
		xerror_issue("cannot free channel");
		return err;
	}

	free(self->chan);

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

/*******************************************************************************
 * @fn decl_init
 * @brief Create a decl node on heap and copy the contents of the input decl
 * node to the new heap node. The new heap pointer is added to the parser
 * mempool.
 ******************************************************************************/
static decl *decl_init(parser *self, decl src)
{
	assert(self);

	decl *new = NULL;

	kmalloc(new, sizeof(decl));

	mem_vector_push(&self->mempool, new);

	memcpy(new, &src, sizeof(decl));

	return new;
}

/*******************************************************************************
 * @fn stmtcpy
 * @brief Create a statment node on heap and copy the contents of the input
 * statement node to the new heap node. The new heap pointer is added to the
 * parser mempool.
 ******************************************************************************/
static stmt *stmt_init(parser *self, stmt src)
{
	assert(self);

	stmt *new = NULL;

	kmalloc(new, sizeof(stmt));

	mem_vector_push(&self->mempool, new);

	memcpy(new, &src, sizeof(stmt));

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

	kmalloc(*dest, sizeof(char) * n + 1);

	mem_vector_push(&self->mempool, *dest);

	memcpy(*dest, src, sizeof(char) * n);

	(*dest)[n] = '\0';
}

/*******************************************************************************
 * @fn extract_arrnum
 * @brief Extract a number from the non-null terminated lexeme held in the
 * current parser token.
 * @return If the number is not a simple nonnegitive integer less than LLONG_MAX
 * then a parse exception is thrown. Else, an intmax representation is given.
 ******************************************************************************/
static intmax_t extract_arrnum(parser *self)
{
	assert(self);

	long long int candidate = 0;
	char *end = NULL;
	char *tmp = NULL;

	lexcpy(self, &tmp, self->tok.lexeme, self->tok.len);

	candidate = strtoll(tmp, &end, 10);

	if (candidate == LONG_MAX) {
		usererror("array index is too large");
		Throw(XXPARSE);
	} else if (*end != '\0') {
		usererror("array index is not a simple base-10 integer");
		Throw(XXPARSE);
	}

	assert(candidate >= 0 && "scanner int literal has minus token");

	return (intmax_t) candidate;
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

	if (token_channel_recv(self->chan, &self->tok)) {
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
		TokenPrint(self->tok, stderr);
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
		if (token_channel_recv(self->chan, &self->tok)) {
			assert(0 != 0 && "attempted to read past EOF");
		}

		if (self->opt->diagnostic & DIAGNOSTIC_TOKENS) {
			TokenPrint(self->tok, stderr);
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

	//body
	check(self, _LEFTBRACE, "missing body in function declaration");
	node.function.block = stmt_init(self, rec_block(self));

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

	if (self->tok.type == _SEMICOLON) {
		usererror("missing rvalue in variable declaration");
		Throw(XXPARSE);
	}

	node.variable.value = rec_assignment(self);

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

		//array length should not be zero, but this check is
		//relegated to the context sensitive analyser.
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
 * @brief Jump table for statement handlers.
 * @return Always returns a valid node. May throw a parse exception.
 ******************************************************************************/
static stmt rec_stmt(parser *self)
{
	assert(self);

	stmt node = { 0 };

	switch(self->tok.type) {
	case _LEFTBRACE:
		node = rec_block(self);
		break;

	case _FOR:
		node = rec_forloop(self);
		break;

	case _WHILE:
		node = rec_whileloop(self);
		break;

	case _SWITCH:
		node = rec_switch(self);
		break;

	case _IF:
		node = rec_branch(self);
		break;

	case _RETURN:
		fallthrough;

	case _GOTO:
		fallthrough;

	case _IMPORT:
		node = rec_named_target(self);
		break;

	case _BREAK:
		fallthrough;

	case _CONTINUE:
		fallthrough;

	case _FALLTHROUGH:
		node = rec_anonymous_target(self);
		break;

	case _LABEL:
		node = rec_label(self);
		break;

	default:
		node = rec_exprstmt(self);
		break;
	}

	return node;
}

/*******************************************************************************
 * @fn rec_block
 * @brief <block statement> ::= "{" <fiat>* "}" . The current token held in the
 * parser must be a _LEFTBRACE token.
 * @return Always returns a valid block node. May throw a parse exception. At
 * return the parser will have consumed the terminating right brace.
 ******************************************************************************/
static stmt rec_block(parser *self)
{
	assert(self);
	assert(self->tok.type == _LEFTBRACE);

	stmt node = {
		.tag = NODE_BLOCK,
		.block = {
			.fiats = {0}
		},
		.line = self->tok.line
	};

	fiat_vector_init(&node.block.fiats, 0, 4);

	parser_advance(self); //move off left brace 

	while (self->tok.type != _RIGHTBRACE) {
		fiat_vector_push(&node.block.fiats, rec_fiat(self));
	}

	parser_advance(self); //move off right brace

	return node;
}

/*******************************************************************************
 * @fn rec_forloop
 * @brief <for statement> ::= "for" "(" (<short declaration> | <expression>) ";"
 * <expression> ";" <expression> ")" <block statement> . The token held
 * in the parser should be the _FOR keyword.
 ******************************************************************************/
static stmt rec_forloop(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_FORLOOP,
		.line = self->tok.line
	};

	move_check_move(self, _LEFTPAREN, "missing '(' after 'for' keyword");

	//initial condition
	if (self->tok.type == _LET) {
		node.forloop.tag = FOR_DECL;
		node.forloop.shortvar = decl_init(self, rec_var(self));
		//rec_var consumes the terminating semicolon
	} else {
		node.forloop.tag = FOR_INIT;
		node.forloop.init = rec_assignment(self);
		check_move(self, _SEMICOLON, "missing ';' after init");
	}

	node.forloop.cond = rec_assignment(self);
	check_move(self, _SEMICOLON, "missing ';' after condition");

	node.forloop.post = rec_assignment(self);
	check_move(self, _RIGHTPAREN, "missing ')' after post expression");

	node.forloop.block = stmt_init(self, rec_block(self));

	return node;
}

/*******************************************************************************
 * @fn rec_whileloop
 * @brief <while statement> ::= "while" "(" <expression> ")" <block statement> .
 * The current token in the parser should be the _WHILE keyword.
 ******************************************************************************/
static stmt rec_whileloop(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_WHILELOOP,
		.line = self->tok.line
	};

	move_check_move(self, _LEFTPAREN, "expected '(' after 'while'");

	node.whileloop.cond = rec_assignment(self);

	check_move(self, _RIGHTPAREN, "expected ')' after while condition");

	if (self->tok.type == _LEFTBRACE) {
		node.whileloop.block = stmt_init(self, rec_block(self));
	} else {
		usererror("expected block statement after while loop");
		Throw(XXPARSE);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_switch
 * @brief <switch statement> ::= "switch" "(" <expression> ")" "{" 
 * <case statement>* "}"
 * The token currently held by the parser should be the _SWITCH keyword.
 ******************************************************************************/
static stmt rec_switch(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_SWITCHSTMT,
		.line = self->tok.line
	};

	move_check_move(self, _LEFTPAREN, "missing '(' after 'switch'");

	node.switchstmt.controller = rec_assignment(self);

	check_move(self, _RIGHTPAREN, "missing ')' after switch condition");

	check_move(self, _LEFTBRACE, "missing '{' to open switch body");

	node.switchstmt.tests = rec_tests(self);

	check_move(self, _RIGHTBRACE, "missing '}' to close switch body");

	return node;
}

/*******************************************************************************
 * @fn rec_tests
 * @brief <case statement> ::= ("case" <expression> | "default") <block stmt>
 ******************************************************************************/
static test_vector rec_tests(parser *self)
{
	assert(self);

	test_vector vec = {0};
	test_vector_init(&vec, 0, 4);

	test t = {
		.cond = NULL,
		.pass = NULL
	};

	while (self->tok.type == _CASE || self->tok.type == _DEFAULT) {
		switch (self->tok.type) {
		case _CASE:
			parser_advance(self);
			t.cond = rec_assignment(self);
			break;

		case _DEFAULT:
			parser_advance(self);
			t.cond = NULL;
			break;

		default:
			assert(0 != 0);
		}

		if (self->tok.type != _LEFTBRACE) {
			usererror("switch cases must use block statements");
			Throw(XXPARSE);
		}

		t.pass = stmt_init(self, rec_block(self));
		test_vector_push(&vec, t);
		memset(&t, 0, sizeof(test));
	}

	return vec;
}

/*******************************************************************************
 * @fn rec_branch
 * @brief <branch statement> ::=  "if" "(" <short declaration>? <expression> ")"
 * <block statement> ("else" (<branch statement> | <block statement>))? . The
 * current token in the parser should be the _IF keyword.
 ******************************************************************************/
static stmt rec_branch(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_BRANCH,
		.line = self->tok.line
	};

	//condition test
	move_check_move(self, _LEFTPAREN, "missing '(' after 'if'");

	if (self->tok.type == _LET) {
		node.branch.shortvar = decl_init(self, rec_var(self));
		//rec_var consumes terminating semicolon
	} else {
		node.branch.shortvar = NULL;
	}

	node.branch.cond = rec_assignment(self);

	check_move(self, _RIGHTPAREN, "missing ')' after if condition");

	//if branch
	if (self->tok.type == _LEFTBRACE) {
		node.branch.pass = stmt_init(self, rec_block(self));
	} else {
		usererror("expected block statement after if condition");
		Throw(XXPARSE);
	}

	//else branch
	if (self->tok.type != _ELSE) {
		node.branch.fail = NULL;
		return node;
	}

	parser_advance(self);

	switch (self->tok.type) {
	case _IF:
		node.branch.fail = stmt_init(self, rec_branch(self));
		break;

	case _LEFTBRACE:
		node.branch.fail = stmt_init(self, rec_block(self));
		break;
	
	default:
		usererror("expected 'else if' or 'else {...}' after block");
		Throw(XXPARSE);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_named_target
 * @brief goto, import, and return statments. The current token in the parser
 * should be the leading token of the statement.
 ******************************************************************************/
static stmt rec_named_target(parser *self)
{
	assert(self);

	stmt node = {
		.line = self->tok.line
	};

	switch (self->tok.type) {
	case _GOTO:
		node.tag = NODE_GOTOLABEL;
		move_check(self, _IDENTIFIER, "missing goto target");
		lexcpy(self, &node.gotolabel, self->tok.lexeme, self->tok.len);
		move_check_move(self, _SEMICOLON, "missing ';' after goto");
		break;

	case _IMPORT:
		node.tag = NODE_IMPORT;
		move_check(self, _IDENTIFIER, "missing import name");
		lexcpy(self, &node.import, self->tok.lexeme, self->tok.len);
		move_check_move(self, _SEMICOLON, "missing ';' after import");
		break;

	case _RETURN:
		node.tag = NODE_RETURNSTMT;
		parser_advance(self);

		//func returns nothing
		if (self->tok.type == _SEMICOLON) {
			node.returnstmt = NULL;
			parser_advance(self);
			break;
		}

		node.returnstmt = rec_assignment(self);
		check_move(self, _SEMICOLON, "missing ';' after return");
		break;
	
	default:
		assert(0 != 0);
	}

	return node;
}

/*******************************************************************************
 * @fn rec_anonymous_target
 * @brief break, continue, and fallthrough statements. The current token in the
 * parser should be the leading token of the statement.
 ******************************************************************************/
static stmt rec_anonymous_target(parser *self)
{
	assert(self);
	
	stmt node = {
		.line = self->tok.line
	};

	switch (self->tok.type) {
	case _BREAK:
		node.tag = NODE_BREAKSTMT;
		move_check_move(self, _SEMICOLON, "missing ';' after break");
		break;
	
	case _CONTINUE:
		node.tag = NODE_CONTINUESTMT;
		move_check_move(self, _SEMICOLON, "missing ';' after continue");
		break;

	case _FALLTHROUGH:
		node.tag = NODE_FALLTHROUGHSTMT;
		move_check_move(self, _SEMICOLON, "missing ';' after fall");
		break;

	default:
		assert(0 != 0 && "func called on non-anonymous token");
	}

	return node;
}

/*******************************************************************************
 * @fn rec_label
 * @brief <label statement> ::= "label" IDENTIFIER ":" <statement> . The current
 * token in the parser must be the _LABEL keyword.
 ******************************************************************************/
static stmt rec_label(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_LABEL,
		.label = {
			.name = NULL,
			.target = NULL
		},
		.line = self->tok.line
	};

	move_check(self, _IDENTIFIER, "label name is not an identifier");

	lexcpy(self, &node.label.name, self->tok.lexeme, self->tok.len);

	move_check_move(self, _COLON, "missing ':' after label name");

	node.label.target = stmt_init(self, rec_stmt(self));

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

	case _COLON:
		node = expr_init(self, NODE_CAST);
		node->line = self->tok.line;
		parser_advance(self);
		node->cast.casttype = rec_type(self);
		check_move(self, _COLON, "expected ':' after type casting");
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
 * @remark <atom> production is expanded and implemented in rec_primary while
 * optional calls, selectors, and indicies are implemented in rec_access.
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

	case _LEFTBRACKET:
		node = rec_arraylit(self);
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

	case _LEFTPAREN:
		parser_advance(self);
		node = rec_assignment(self);
		check_move(self, _RIGHTPAREN, "expected ')' after grouping");
		break;

	default:
		usererror(errfmt, self->tok.len, self->tok.lexeme);
		Throw(XXPARSE);
	}

	return rec_access(self, node);
}

/*******************************************************************************
 * @fn rec_access
 * @brief <call> | <selector> | <index>
 ******************************************************************************/
static expr *rec_access(parser *self, expr *prev)
{
	assert(self);
	assert(prev);

	expr *node = NULL;

	switch (self->tok.type) {
	case _DOT:
		node = expr_init(self, NODE_SELECTOR);
		node->line = self->tok.line;
		
		move_check(self, _IDENTIFIER, "missing attribute after '.'");
		
		node->selector.name = prev;
		node->selector.attr = rec_ident(self, self->tok);
		
		parser_advance(self);
		break;

	case _LEFTPAREN:
		node = expr_init(self, NODE_CALL);
		node->line = self->tok.line;

		node->call.name = prev;
		node->call.args = rec_args(self);

		break;

	case _LEFTBRACKET:
		node = expr_init(self, NODE_INDEX);
		node->line = self->tok.line;

		parser_advance(self);

		node->index.name = prev;
		node->index.key = rec_assignment(self);

		check_move(self, _RIGHTBRACKET, "missing ']' after index");

		break;

	//recursion base case
	default:
		return prev;
	}

	return rec_access(self, node);
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
	} else {
		return rec_ident(self, tmp);
	}
}

/*******************************************************************************
 * @fn rec_ident
 * @brief Simple identifier nodes. Creates an identifier from the token input
 * rather than from the parser.
 ******************************************************************************/
static expr *rec_ident(parser *self, token tok)
{
	assert(self);

	expr *node = expr_init(self, NODE_IDENT);
	node->line = tok.line;
	lexcpy(self, &node->ident.name, tok.lexeme, tok.len);

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
	assert(self->tok.type == _TILDE);

	expr *node = expr_init(self, NODE_RVARLIT);
	
	node->line = prev.line;

	lexcpy(self, &node->rvarlit.dist, prev.lexeme, prev.len);

	parser_advance(self);

	node->rvarlit.args = rec_args(self); 

	return node;
}

/*******************************************************************************
 * @fn rec_args
 * @brief Process an argument list into an expr_vector. The current token held
 * in the parser must be a _LEFTPAREN.
 * @return On return the node is always valid and the right parenthesis will
 * have been consumed by the parser. A parse exception may be thrown.
 * @remark Vector capacity is set heuristically.
 ******************************************************************************/
static expr_vector rec_args(parser *self)
{
	assert(self);
	assert(self->tok.type == _LEFTPAREN);

	expr_vector vec = {0};
	expr_vector_init(&vec, 0, 4);

	parser_advance(self); //move off left parenthesis

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(self, _COMMA, "expected ',' after arg");
		}

		expr_vector_push(&vec, rec_assignment(self));
	}

	parser_advance(self); //move off right parenthesis

	return vec;
}

/*******************************************************************************
 * @fn rec_arraylit
 * @brief Process an array literal into an expression node. The current token
 * held by the parser must be a _LEFTBRACKET.
 * @return Always returns a valid node. May throw a parse exception. Every
 * value in the values vector has an associated value in the indicies vector
 * at the same vector index. When the associated value in the indicies vector
 * is -1, then the value in the values vector does not have a tagged index.
 ******************************************************************************/
static expr *rec_arraylit(parser *self)
{
	assert(self);
	assert(self->tok.type == _LEFTBRACKET);

	static char *tagerr = "tagged array index must be an integer";
	static char *closeerr = "missing ']' after tagged index";
	static char *eqerr = "missing '=' after tagged index";

	expr *node = expr_init(self, NODE_ARRAYLIT);

	node->line = self->tok.line;
	idx_vector_init(&node->arraylit.indicies, 0, 4);
	expr_vector_init(&node->arraylit.values, 0, 4);

	parser_advance(self); //move off left bracket

	while (self->tok.type != _RIGHTBRACKET) {
		if (node->arraylit.values.len > 0) {
			check_move(self, _COMMA, "expected ',' after value");
		}

		if (self->tok.type == _LEFTBRACKET) {
			move_check(self, _LITERALINT, tagerr);

			intmax_t idx = extract_arrnum(self);
			idx_vector_push(&node->arraylit.indicies, idx);
			
			move_check_move(self, _RIGHTBRACKET, closeerr);
			check_move(self, _EQUAL, eqerr);
		} else {
			idx_vector_push(&node->arraylit.indicies, -1);
		}

		expr_vector_push(&node->arraylit.values, rec_assignment(self));
	}

	parser_advance(self); //move off right bracket

	assert(node->arraylit.indicies.len == node->arraylit.values.len);

	return node;
}
