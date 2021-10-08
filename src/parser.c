// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Recursive descent parser. Two types of errors may be encountered: compiler
// errors and user errors. Compiler errors are treated as usual; Log the issue
// and propogate the error code.
//
// User errors are not logged, they don't propoage error codes, and they do not
// cause the parser to terminate. Instead, they throw exceptions which trigger
// the parser to synchronize to a new sequence point within the token stream.
//
// The setjmp/longjmp exception handler does not release the dynamic memory
// allocations made within a try-block when an exception occurs. The parser
// contains a vector<void*> used to track all memory allocations requested since
// the last successful sequence point. Garbage collection is triggered after
// synchronisation.

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"

#include "lib/channel.h"
#include "lib/vector.h"

typedef struct parser parser;

//parser utils
static parser *ParserInit(options *opt, string src);
static void ParserFree(parser **self);
static void Mark(parser *self, void *region);
static void *AllocateAndMark(parser *self, size_t bytes);
static void CollectGarbage(parser *self, bool flag);
static string StringFromLexeme(parser *self);
static void GetNextToken(parser *self);
static void GetNextValidToken(parser *self);
static void ReportInvalidToken(parser *self);
static size_t Synchronize(parser *self);
static void CheckToken(parser *self, token_type type, char *msg, bool, bool);
static intmax_t ExtractArrayIndex(parser *self);
static file *FileInit(string alias);
static expr *ExprInit(parser *self, exprtag tag);
static stmt *CopyStmtToHeap(parser *self, stmt src);
static decl *CopyDeclToHeap(parser *self, decl src);

//declarations
static file *RecursiveDescent(parser *self, string alias);
static fiat RecFiat(parser *self);
static decl RecStruct(parser *self);
static member_vector RecParseMembers(parser *self);
decl RecFunction(parser *self);
static param_vector RecParseParameters(parser *self);
type *RecType(parser *self);
static decl RecVariable(parser *self);

//statements
static stmt RecStmt(parser *self);
static stmt RecExprStmt(parser *self);
static stmt RecBlock(parser *self);
static stmt RecLabel(parser *self);
static stmt RecAnonymousTarget(parser *self);
static stmt RecNamedTarget(parser *self);
static stmt RecForLoop(parser *self);
static stmt RecWhileLoop(parser *self);
static stmt RecBranch(parser *self);
static stmt RecSwitch(parser *self);
static test_vector rec_tests(parser *self);

//expressions
static expr *RecAssignment(parser *self);
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

file *SyntaxTreeInit(options *opt, string src, string alias)
{
	assert(opt);
	assert(src);
	assert(alias);

	RAII(ParserFree) parser *prs = parser_init(opt, src);

	if (!prs) {
		xerror_issue("cannot init parser");
		return NULL;
	}

	file *syntax_tree = RecursiveDescent(prs, alias);

	return syntax_tree;
}

void SyntaxTreeFree(file *ast)
{
	free(ast);
	//TODO
}

//------------------------------------------------------------------------------
// parser management

make_vector(void *, ptr, static)

struct parser {
	options *opt;
	token_channel *chan;
	ptr_vector garbage;
	token tok;
	size_t errors;
};

static parser *ParserInit(options *opt, string src)
{
	assert(opt);
	assert(src);

	parser *prs = NULL;
	kmalloc(prs, sizeof(parser));

	kmalloc(prs->chan, sizeof(token_channel));
	token_channel_init(prs->chan, KiB(1));

	xerror err = ScannerInit(opt, src, prs->chan);

	if (err) {
		xerror_issue("cannot init scanner: %s", XerrorDescription(err));
		free(prs);
		return NULL;
	}

	const size_t garbage_capacity = 32;
	ptr_vector_init(&prs->garbage, 0, garbage_capacity);

	prs->opt = opt;
	prs->tok = (token) { .type = _INVALID };
	prs->errors = 0;

	return prs;
}

//intended for use with gcc cleanup
static void ParserFree(parser **self)
{
	assert(self);

	parser *prs = *self;

	if (!prs) {
		return;
	}

	(void) token_channel_free(prs->chan, NULL);

	free(prs->chan);

	ptr_vector_free(&prs->garbage, NULL);

	free(prs);

	return XESUCCESS;
}

//------------------------------------------------------------------------------
//node management

static file *FileInit(string alias)
{
	file *syntax_tree = NULL;

	kmalloc(syntax_tree, sizeof(file));

	syntax-tree->alias = alias;

	const size_t fiat_capacity = 64;
	fiat_vector_init(&syntax_tree->fiats, 0, fiat_capacity);

	return syntax_tree;
}

static expr *ExprInit(parser *self, exprtag tag)
{
	assert(self);
	assert(tag >= NODE_ASSIGNMENT && tag <= NODE_IDENT);

	expr *new = AllocateAndMark(self, sizeof(expr));

	new->tag = tag;

	return new;
}

static decl *CopyDeclToHeap(parser *self, decl src)
{
	assert(self);

	decl *new = AllocateAndMark(self, sizeof(decl);

	memcpy(new, &src, sizeof(decl));

	return new;
}

static stmt *CopyStmtToHeap(parser *self, stmt src)
{
	assert(self);

	stmt *new = AllocateAndMark(self, sizeof(stmt));

	memcpy(new, &src, sizeof(stmt));

	return new;
}

//------------------------------------------------------------------------------
//helper functions

static void Mark(parser *self, void *region)
{
	ptr_vector_push(&self->garbage, region);
}

static void *AllocateAndMark(parser *self, size_t bytes)
{
	assert(self);

	void *region = kmalloc(dest, bytes);

	Mark(self, region);

	return region;
}

static void CollectGarbage(parser *self, bool flag)
{
	void (*freefunc)(void *) = NULL;

	if (flag) {
		freefunc = free;
	}

	ptr_vector_reset(&self->garbage, freefunc);
}

//OOP in Lemon always uses 'self' as a convention, so the xerror API can be
//simplified with a guaranteed access mechanism for the token line number
#define usererror(msg, ...) XerrorUser(self->tok.line, msg, ##__VA_ARGS__)

static string StringFromLexeme(parser *self)
{
	assert(self);

	string new = StringFromView(self->tok.lexeme);

	Mark(self, new);

	return new;
}

//if the extracted index is not a simple nonnegative integer less than LLONG_MAX
//then a parser exception ins thrown.
static intmax_t ExtractArrayIndex(parser *self)
{
	assert(self);

	long long int candidate = 0;
	char *end = NULL;

	char *tmp = StringFromLexeme(self);

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

static void CheckToken
(
	parser *self,
	token_type type,
	string msg,
	bool move_before,
	bool move_after
)
{
	assert(self);
	assert(type >= _INVALID && type < _TOKEN_TYPE_COUNT);
	assert(msg);

	if (move_before) {
		GetNextValidToken(self);
	}

	if (self->tok.type != type) {
		usererror(msg);
		Throw(XXPARSE);
	}

	if (move_after) {
		GetNextValidToken(self);
	}
}

#define check(self, type, msg) \
	CheckToken(self, type, msg, false, false)

#define move_check(self, type, msg) \
	CheckToken(self, type, msg, true, false)

#define check_move(self, type, msg) \
	CheckToken(self, type, msg, false, true)

#define move_check_move(self, type, msg) \
	CheckToken(self, type, msg,  true, true)

static void GetNextToken(parser *self)
{
	assert(self);

	xerror err = token_channel_recv(self->chan, &self->tok);

	//TODO: parser was refactored to use exceptions; this assertion can
	//be removed and replaced with Throw()

	//channel status is relegated to an assertion as a tradeoff to improve
	//readability and simplicity of the recursive descent implementation.
	//
	//reading past EOF is a simple off-by-one bug. The final call to
	//GetNextToken before the assertion triggered is occuring too early.
	//The call stack backtrace will quickly locate the problem.
	assert(!err && "attempted to read past EOF");
}

static void GetNextValidToken(parser *self)
{
	assert(self);

	GetNextToken(self);

	if (self->tok.type == _INVALID) {
		(void) Synchronize(self);
	}
}

static void ReportInvalidToken(parser *self)
{
	assert(self);
	assert(self->tok.type == _INVALID);

	char *msg = self->tok.lexeme.data;
	size_t msg_len = self->tok.lexeme.len;

	switch (self->tok.flags) {
	case TOKEN_OKAY:
		usererror("invalid syntax: %.*s", msg_len, msg);
		break;

	case TOKEN_BAD_STR:
		usererror("unterminated string literal");
		break;

	default:
		assert(0 == 0 && "token flag combination is not valid");
	}

	self->errors++;
}

static size_t Synchronize(parser *self)
{
	size_t tokens_skipped = 0;

	do {
		switch(self->tok.type) {
		case _EOF:
			fallthrough;

		case _LEFTBRACE:
			fallthrough;

		case _STRUCT ... _SWITCH:
			goto found_sequence_point;

		case _INVALID:
			ReportInvalidToken(self);
			fallthrough;

		default:
			break;
		}

		GetNextToken(self);
		tokens_skipped++;
	} while (true);

found_sequence_point:
	return tokens_skipped;
}

//------------------------------------------------------------------------------
//parsing algorithm

//always returns a file node pointing to a valid region of memory, but the tree
//itself is ill-formed if self.errors > 0 on return.
static file *RecursiveDescent(parser *self, string alias)
{
	assert(self);

	file *syntax_tree = FileInit(alias);

	GetNextValidToken(self);

	while (self->tok.type != _EOF) {
		fiat node = RecFiat(self);

		if (node.tag == NODE_INVALID) {
			self->errors++;
		} else {
			fiat_vector_push(&syntax_tree->fiats, node);
		}
	}

	return syntax_tree;
}

//remark on unity exceptions: the fiat node doesn't need to be qualified with
//volatile. The CException library dictates that stack variables must be marked
//volatile if the updated value of the variable is required after an exception
//is thrown. In this situation, any updates made to the fiat node are not
//relevant since all information we need is within the garbage vector. Only
//the tag overwrite is necessary.
static fiat RecFiat(parser *self)
{
	assert(self);

	CEXCEPTION_T exception;
	fiat node = {0};
	bool gc_flag = false;

	Try {
		switch (self->tok.type) {
		case _STRUCT:
			node.tag = NODE_DECL;
			node.declaration = RecStruct(self);
			break;

		case _FUNC:
			node.tag = NODE_DECL;
			node.declaration = RecFunction(self);
			break;

		case _LET:
			node.tag = NODE_DECL;
			node.declaration = RecVariable(self);
			break;

		default:
			node.tag = NODE_STMT;
			node.statement = RecStmt(self);
			break;
		}
	} Catch (exception) {
		(void) Synchronize(self);
		node.tag = NODE_INVALID;
		gc_flag = true;
	}

	CollectGarbage(gc_flag);

	return node;
}

//<struct declaration>
static decl RecStruct(parser *self)
{
	assert(self);
	assert(self->tok.type == _STRUCT);

	decl node = {
		.tag = NODE_UDT,
		.udt = {
			.name = NULL,
			.members = {0},
			.public = false
		},
		.line = self->tok.line
	};

	GetNextValidToken(self);

	if (self->tok.type == _PUB) {
		node.udt.public = true;
		GetNextValidToken(self);
	}

	check(self, _IDENTIFIER, "missing struct name after 'struct' keyword");

	node.udt.name = StringFromLexeme(self);

	move_check_move(self, _LEFTBRACE, "missing '{' after struct name");

	node.udt.members = RecParseMembers(self);

	check_move(self, _RIGHTBRACE, "missing '}' after struct members");

	check_move(self, _SEMICOLON, "missing ';' after struct declaration");

	return node;
}

//<member list>
//throws error if no members found
static member_vector RecParseMembers(parser *self)
{
	assert(self);

	member_vector vec = {0};
	const size_t vec_capacity = 4;
	member_vector_init(&vec, 0, vec_capacity);
	Mark(self, vec.data);

	member attr = {
		.name = NULL,
		.typ = NULL,
		.public = false
	};

	while (self->tok.type != _RIGHTBRACE) {
		if (self->tok.type == _PUB) {
			attr.public = true;
			GetNextValidToken(self);
		}

		check(self, _IDENTIFIER, "missing struct member name");

		attr.name = StringFromLexeme(self);

		move_check_move(self, _COLON, "missing ':' after name");

		attr.typ = RecType(self);

		check_move(self, _SEMICOLON, "missing ';' after type");

		member_vector_push(&vec, attr);

		memset(&attr, 0, sizeof(member));
	}

	if (vec.len == 0) {
		usererror("cannot declare an empty struct");
		Throw(XXPARSE);
	}

	return vec;
}

//<function declaration>
decl RecFunction(parser *self)
{
	assert(self);
	assert(self->tok.type == _FUNC);

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

	GetNextValidToken(self);

	if (self->tok.type == _PUB) {
		node.function.public = true;
		GetNextValidToken(self);
	}

	check(self, _IDENTIFIER, "missing function name in declaration");

	node.function.name = StringFromLexeme(self);

	//parameter list
	move_check_move(self, _LEFTPAREN, "missing '(' after function name");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.function.params = RecParseParameters(self);
	}

	check_move(self, _RIGHTPAREN, "missing ')' after parameters");

	//return
	check_move(self, _RETURN, "missing 'return' after parameters");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.function.ret = RecType(self);
	}

	//receiver
	if (self->tok.type == _FOR) {
		GetNextValidToken(self);
		node.function.recv = RecType(self);
	}

	//body
	check(self, _LEFTBRACE, "cannot declare function without a body");
	node.function.block = CopyStmtToHeap(self, RecBlock(self));

	return node;
}

//<parameter list>
//throws error if no parameters found
static param_vector RecParseParameters(parser *self)
{
	assert(self);

	param_vector vec = {0};
	const size_t vec_capacity = 4;
	param_vector_init(&vec, 0, vec_capacity);
	Mark(self, vec.data);

	param attr  = {
		.name = NULL,
		.typ = NULL,
		.mutable = false
	};

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(self, _COMMA, "missing ',' after parameter");
		}

		if (self->tok.type == _MUT) {
			attr.mutable = true;
			GetNextValidToken(self);
		}

		check(self, _IDENTIFIER, "missing function parameter name");

		attr.name = StringFromLexeme(self);

		move_check_move(self, _COLON, "missing ':' after name");

		attr.typ = RecType(self);

		param_vector_push(&vec, attr);

		memset(&attr, 0, sizeof(param));
	}

	if (vec.len == 0) {
		usererror("a function declaration without parameters must be"
			  "marked explicitly with 'void'");
		Throw(XXPARSE);
	}

	return vec;
}

//<variable declaration>
static decl RecVariable(parser *self)
{
	assert(self);
	assert(self->tok.type == _LET);

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

	GetNextValidToken(self);

	if (self->tok.type == _PUB) {
		node.variable.public = true;
		GetNextValidToken(self);
	}

	if (self->tok.type == _MUT) {
		node.variable.mutable = true;
		GetNextValidToken(self);
	}

	check(self, _IDENTIFIER, "missing variable name in declaration");

	node.variable.name = StringFromLexeme(self);

	move_check_move(self, _COLON, "missing ':' before type");

	node.variable.vartype = RecType(self);

	check_move(self, _EQUAL, "declared variables must be initialized");

	check(self, _SEMICOLON, "missing value after '=' in declaration");

	node.variable.value = RecAssignment(self);

	check_move(self, _SEMICOLON, "missing ';' after declaration");

	return node;
}

//<type>
type *RecType(parser *self)
{
	assert(self);

	type *node = AllocateAndMark(self, sizeof(type));

	switch (self->tok.type) {
	case _IDENTIFIER:
		node->tag = NODE_BASE;
		node->base = StringFromLexeme(self);
		GetNextValidToken(self);
		break;

	case _STAR:
		node->tag = NODE_POINTER;
		GetNextValidToken(self);
		node->pointer = RecType(self);
		break;

	case _LEFTBRACKET:
		move_check(self, _LITERALINT, "missing array size");
		node->array.len = ExtractArrayIndex(self);
		move_check_move(self, _RIGHTBRACKET, "missing ']'");
		node->array.base = RecType(self);
		break;

	default:
		usererror("missing a data type");
		Throw(XXPARSE);
		break;
	}

	return node;
}

//<statement>; jump table
static stmt RecStmt(parser *self)
{
	assert(self);

	stmt node = { 0 };

	switch(self->tok.type) {
	case _LEFTBRACE:
		node = RecBlock(self);
		break;

	case _FOR:
		node = RecForLoop(self);
		break;

	case _WHILE:
		node = RecWhileLoop(self);
		break;

	case _SWITCH:
		node = RecSwitch(self);
		break;

	case _IF:
		node = RecBranch(self);
		break;

	case _RETURN:
		fallthrough;

	case _GOTO:
		fallthrough;

	case _IMPORT:
		node = RecNamedTarget(self);
		break;

	case _BREAK:
		fallthrough;

	case _CONTINUE:
		fallthrough;

	case _FALLTHROUGH:
		node = RecAnonymousTarget(self);
		break;

	case _LABEL:
		node = RecLabel(self);
		break;

	default:
		node = RecExprStmt(self);
		break;
	}

	return node;
}

//<block statement>
static stmt RecBlock(parser *self)
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

	const size_t vec_capacity = 4;
	fiat_vector_init(&node.block.fiats, 0, vec_capacity);
	Mark(self, node.block.fiats.data);

	GetNextValidToken(self);

	while (self->tok.type != _RIGHTBRACE) {
		fiat_vector_push(&node.block.fiats, RecFiat(self));
	}

	GetNextValidToken(self);

	return node;
}

//<for statement>
static stmt RecForLoop(parser *self)
{
	assert(self);
	assert(self->tok.type == _FOR);

	stmt node = {
		.tag = NODE_FORLOOP,
		.line = self->tok.line
	};

	move_check_move(self, _LEFTPAREN, "missing '(' after 'for' keyword");

	//initial condition
	if (self->tok.type == _LET) {
		node.forloop.tag = FOR_DECL;
		node.forloop.shortvar = CopyDeclToHeap(self, RecVariable(self));
		//RecVariable consumes the terminating semicolon
	} else {
		node.forloop.tag = FOR_INIT;
		node.forloop.init = RecAssignment(self);
		check_move(self, _SEMICOLON, "missing ';' after init");
	}

	node.forloop.cond = RecAssignment(self);
	check_move(self, _SEMICOLON, "missing ';' after condition");

	node.forloop.post = RecAssignment(self);
	check_move(self, _RIGHTPAREN, "missing ')' after post expression");

	node.forloop.block = CopyStmtToHeap(self, RecBlock(self));

	return node;
}

//<while statement>
static stmt RecWhileLoop(parser *self)
{
	assert(self);
	assert(self->tok.type == _WHILE);

	stmt node = {
		.tag = NODE_WHILELOOP,
		.line = self->tok.line
	};

	move_check_move(self, _LEFTPAREN, "missing'(' after 'while'");

	node.whileloop.cond = RecAssignment(self);

	check_move(self, _RIGHTPAREN, "missing ')' after while condition");

	if (self->tok.type == _LEFTBRACE) {
		node.whileloop.block = CopyStmtToHeap(self, RecBlock(self));
	} else {
		usererror("missing block statement after while loop");
		Throw(XXPARSE);
	}

	return node;
}

//<switch statement>
static stmt RecSwitch(parser *self)
{
	assert(self);
	assert(self->tok.type == _SWITCH);

	stmt node = {
		.tag = NODE_SWITCHSTMT,
		.line = self->tok.line
	};

	move_check_move(self, _LEFTPAREN, "missing '(' after 'switch'");

	node.switchstmt.controller = RecAssignment(self);

	check_move(self, _RIGHTPAREN, "missing ')' after switch condition");

	check_move(self, _LEFTBRACE, "missing '{' to open switch body");

	node.switchstmt.tests = rec_tests(self);

	check_move(self, _RIGHTBRACE, "missing '}' to close switch body");

	return node;
}

//<case statement>* within <switch statement>
static test_vector rec_tests(parser *self)
{
	assert(self);

	test_vector vec = {0};
	const size_t vec_capacity = 4;
	test_vector_init(&vec, 0, vec_capacity);
	Mark(self, vec.data);

	test t = {
		.cond = NULL,
		.pass = NULL
	};

	while (self->tok.type == _CASE || self->tok.type == _DEFAULT) {
		switch (self->tok.type) {
		case _CASE:
			GetNextValidToken(self);
			t.cond = RecAssignment(self);
			break;

		case _DEFAULT:
			GetNextValidToken(self);
			t.cond = NULL;
			break;

		default:
			assert(0 != 0);
		}

		if (self->tok.type != _LEFTBRACE) {
			usererror("switch cases must be block statements");
			Throw(XXPARSE);
		}

		t.pass = CopyStmtToHeap(self, RecBlock(self));
		test_vector_push(&vec, t);
		memset(&t, 0, sizeof(test));
	}

	if (vec.len == 0) {
		usererror("switch statement cannot be empty");
		Throw(XXPARSE);
	}

	return vec;
}

//<branch statement>
static stmt RecBranch(parser *self)
{
	assert(self);
	assert(self->tok.type == _IF);

	stmt node = {
		.tag = NODE_BRANCH,
		.line = self->tok.line
	};

	//condition
	move_check_move(self, _LEFTPAREN, "missing '(' after 'if'");

	if (self->tok.type == _LET) {
		node.branch.shortvar = CopyDeclToHeap(self, RecVariable(self));
		//RecVariable consumes terminating semicolon
	} else {
		node.branch.shortvar = NULL;
	}

	node.branch.cond = RecAssignment(self);

	check_move(self, _RIGHTPAREN, "missing ')' after if condition");

	//if branch
	if (self->tok.type == _LEFTBRACE) {
		node.branch.pass = CopyStmtToHeap(self, RecBlock(self));
	} else {
		usererror("missing block statement after if condition");
		Throw(XXPARSE);
	}

	//else branch
	if (self->tok.type != _ELSE) {
		node.branch.fail = NULL;
		return node;
	}

	GetNextValidToken(self);

	switch (self->tok.type) {
	case _IF:
		node.branch.fail = CopyStmtToHeap(self, RecBranch(self));
		break;

	case _LEFTBRACE:
		node.branch.fail = CopyStmtToHeap(self, RecBlock(self));
		break;

	default:
		usererror("expected 'else if' or 'else {...}' after block");
		Throw(XXPARSE);
	}

	return node;
}

static stmt RecNamedTarget(parser *self)
{
	assert(self);
	assert(self->tok.type == _GOTO ||
	       self->tok.type == _IMPORT ||
	       self->tok.type == _RETURN);

	stmt node = {
		.line = self->tok.line
	};

	switch (self->tok.type) {
	case _GOTO:
		node.tag = NODE_GOTOLABEL;
		move_check(self, _IDENTIFIER, "missing goto target");
		node.gotolabel = StringFromLexeme(self);
		move_check_move(self, _SEMICOLON, "missing ';' after goto");
		break;

	case _IMPORT:
		node.tag = NODE_IMPORT;
		move_check(self, _IDENTIFIER, "missing import name");
		node.import = StringFromlexeme(self);
		move_check_move(self, _SEMICOLON, "missing ';' after import");
		break;

	case _RETURN:
		node.tag = NODE_RETURNSTMT;
		GetNextValidToken(self);

		if (self->tok.type == _SEMICOLON) {
			node.returnstmt = NULL;
			GetNextValidToken(self);
			break;
		}

		node.returnstmt = RecAssignment(self);
		check_move(self, _SEMICOLON, "missing ';' after return");
		break;

	default:
		assert(0 != 0);
	}

	return node;
}

static stmt RecAnonymousTarget(parser *self)
{
	assert(self);
	assert(self->tok.type == _BREAK ||
	       self->tok.type == _CONTINUE ||
	       self->tok.type == _FALLTHROUGH);

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
		assert(0 != 0); 
	}

	return node;
}

//<label statement>
static stmt RecLabel(parser *self)
{
	assert(self);
	assert(self->tok.type == _LABEL);

	stmt node = {
		.tag = NODE_LABEL,
		.label = {
			.name = NULL,
			.target = NULL
		},
		.line = self->tok.line
	};

	move_check(self, _IDENTIFIER, "label name must be an identifier");

	node.label.name = StringFromLexeme(self);

	move_check_move(self, _COLON, "missing ':' after label name");

	node.label.target = CopyStmtToHeap(self, RecStmt(self));

	return node;
}

//<expression statement>; the token held by the parser on invocation must be the
//first token of the expression.
static stmt RecExprStmt(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_EXPRSTMT,
		.exprstmt = NULL,
		.line = self->tok.line
	};

	node.exprstmt = RecAssignment(self);

	check_move(self, _SEMICOLON, "missing ';' after expression");

	return node;
}

//<assignment>; note that lemon does not allow chained assignments
static expr *RecAssignment(parser *self)
{
	assert(self);

	expr *node = rec_logicalor(self);

	if (self->tok.type == _EQUAL) {
		expr *tmp = node;
		node = ExprInit(self, NODE_ASSIGNMENT);
		node->assignment.lvalue = tmp;
		node->line = self->tok.line;

		GetNextValidToken(self);
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
		node = ExprInit(self, NODE_BINARY);

		node->binary.left = tmp;
		node->binary.operator = _OR;
		node->line = self->tok.line;

		GetNextValidToken(self);
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
		node = ExprInit(self, NODE_BINARY);

		node->binary.left = tmp;
		node->binary.operator = _AND;
		node->line = self->tok.line;

		GetNextValidToken(self);
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
			node = ExprInit(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			GetNextValidToken(self);
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
			node = ExprInit(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			GetNextValidToken(self);
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
			node = ExprInit(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			GetNextValidToken(self);
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
		node = ExprInit(self, NODE_UNARY);
		node->unary.operator = self->tok.type;
		node->line = self->tok.line;
		GetNextValidToken(self);
		node->unary.operand = rec_unary(self);
		break;

	case _COLON:
		node = ExprInit(self, NODE_CAST);
		node->line = self->tok.line;
		GetNextValidToken(self);
		node->cast.casttype = RecType(self);
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
		node = ExprInit(self, NODE_LIT);
		node->line = self->tok.line;
		node->lit.rep = StringFromLexeme(self);
		node->lit.littype = self->tok.type;
		GetNextValidToken(self);
		break;

	case _LEFTPAREN:
		GetNextValidToken(self);
		node = RecAssignment(self);
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
		node = ExprInit(self, NODE_SELECTOR);
		node->line = self->tok.line;

		move_check(self, _IDENTIFIER, "missing attribute after '.'");

		node->selector.name = prev;
		node->selector.attr = rec_ident(self, self->tok);

		GetNextValidToken(self);
		break;

	case _LEFTPAREN:
		node = ExprInit(self, NODE_CALL);
		node->line = self->tok.line;

		node->call.name = prev;
		node->call.args = rec_args(self);

		break;

	case _LEFTBRACKET:
		node = ExprInit(self, NODE_INDEX);
		node->line = self->tok.line;

		GetNextValidToken(self);

		node->index.name = prev;
		node->index.key = RecAssignment(self);

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

	GetNextValidToken(self);

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

	expr *node = ExprInit(self, NODE_IDENT);
	node->line = tok.line;
	node->ident.name = StringFromLexeme(self);

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

	expr *node = ExprInit(self, NODE_RVARLIT);

	node->line = prev.line;

	token tmp = self->tok;
	self->tok = prev;
	node->rvarlit.dist = StringFromLexeme(self);
	self->tok = tmp;

	GetNextValidToken(self);

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

	GetNextValidToken(self); //move off left parenthesis

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(self, _COMMA, "expected ',' after arg");
		}

		expr_vector_push(&vec, RecAssignment(self));
	}

	GetNextValidToken(self); //move off right parenthesis

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

	expr *node = ExprInit(self, NODE_ARRAYLIT);

	node->line = self->tok.line;
	idx_vector_init(&node->arraylit.indicies, 0, 4);
	expr_vector_init(&node->arraylit.values, 0, 4);

	GetNextValidToken(self); //move off left bracket

	while (self->tok.type != _RIGHTBRACKET) {
		if (node->arraylit.values.len > 0) {
			check_move(self, _COMMA, "expected ',' after value");
		}

		if (self->tok.type == _LEFTBRACKET) {
			move_check(self, _LITERALINT, tagerr);

			intmax_t idx = ExtractArrayIndex(self);
			idx_vector_push(&node->arraylit.indicies, idx);

			move_check_move(self, _RIGHTBRACKET, closeerr);
			check_move(self, _EQUAL, eqerr);
		} else {
			idx_vector_push(&node->arraylit.indicies, -1);
		}

		expr_vector_push(&node->arraylit.values, RecAssignment(self));
	}

	GetNextValidToken(self); //move off right bracket

	assert(node->arraylit.indicies.len == node->arraylit.values.len);

	return node;
}
