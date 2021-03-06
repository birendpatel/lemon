// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Recursive descent parser.

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "file.h"
#include "parser.h"
#include "scanner.h"
#include "options.h"
#include "xerror.h"
#include "channel.h"
#include "vector.h"

typedef struct parser parser;

//parser management
static parser *ParserInit(cstring *);
static module *RecursiveDescent(parser *, const cstring *);

//node management
static module ModuleInit(parser *, const cstring *);
static expr *ExprInit(parser *, const exprtag);
static stmt *CopyStmtToHeap(parser *, const stmt);
static decl *CopyDeclToHeap(parser *, const decl);

//utils
static void GetNextToken(parser *);
static void GetNextValidToken(parser *);
static void ReportInvalidToken(parser *);
static size_t Synchronize(parser *, bool);
static void CheckToken
(parser *, const token_type, const cstring *, const bool, const bool);
static intmax_t ExtractArrayIndex(parser *);
static cstring *cStringFromLexeme(parser *);

//directives
static import RecImport(parser *);

//declarations
static decl RecDecl(parser *);
static fiat RecFiat(parser *);
static decl RecStruct(parser *);
static vector(Member) RecParseMembers(parser *);
static decl RecFunction(parser *);
static decl RecMethod(parser *self);
static vector(Param) RecParseParameters(parser *);
static type *RecType(parser *);
static decl RecVariable(parser *);

//statements
static stmt RecStmt(parser *);
static stmt RecExprStmt(parser *);
static stmt RecBlock(parser *);
static void ParseFiats(parser *, vector(Fiat) *const);
static stmt RecLabel(parser *);
static stmt RecAnonymousTarget(parser *);
static stmt RecNamedTarget(parser *);
static stmt RecForLoop(parser *);
static stmt RecWhileLoop(parser *);
static stmt RecBranch(parser *);
static stmt RecSwitch(parser *);
static vector(Test) RecTests(parser *);

//expressions
static expr *RecAssignment(parser *);
static expr *RecLogicalOr(parser *);
static expr *RecLogicalAnd(parser *);
static expr *RecEquality(parser *);
static expr *RecTerm(parser *);
static expr *RecFactor(parser *);
static expr *RecUnary(parser *);
static expr *RecPrimary(parser *);
static expr *RecRvarOrIdentifier(parser *);
static expr *RecRvar(parser *, bool);
static vector(Expr) RecArguments(parser *);
static expr *RecArrayLiteral(parser *);
static expr *RecArrayLiteral(parser *);
static expr *RecIdentifier(parser *);
static expr *RecAccess(parser *, expr *);

//-----------------------------------------------------------------------------
// parser management
//
// User errors are not logged, they don't propoage error codes, and they do not
// cause the parser to terminate. Instead, they throw exceptions which trigger
// the parser to synchronize to a new sequence point within the token stream.
//
// If an exception occurs, the unity exception handler does not release dynamic
// memory allocations that were made within the try-block. To accomodate for
// this limitation, during recursive descent the parser will track its dynamic
// allocations by pushing them to a vector<void *>.
//
// Allocations to this vector must be pushed in the order that they have been
// requested from the system. This ensures that a backwards traversal of the
// vector buffer always encounters child node allocations before parent node
// allocations. In effect, a reverse linear traversal of the buffer mimics a
// post-order right-left-parent tree traversal.
//
// As a secondary benefit, vector buffers are cache-friendly and as a result
// the AST destruction process is much faster than a manual tree traversal.

struct parser {
	channel(Token) *chan;
	token tok;
	module root;
	size_t errors;
};

//returns NULL on failure; does not initialize the root member
static parser *ParserInit(cstring *src)
{
	assert(src);

	parser *prs = allocate(sizeof(parser));

	prs->chan = allocate(sizeof(channel(Token)));
	TokenChannelInit(prs->chan, KiB(1));

	prs->tok = INVALID_TOKEN;

	prs->errors = 0;

	bool ok = ScannerInit(src, prs->chan);

	if (!ok) {
		xerror_issue("cannot init scanner");
		(void) TokenChannelShutdown(prs->chan);
		return NULL;
	}

	return prs;
}

//-----------------------------------------------------------------------------
// API implementation

module *SyntaxTreeInit(const cstring *filename)
{
	assert(filename);

	cstring *src = FileLoad(filename);

	if (!src) {
		return NULL;
	}

	parser *prs = ParserInit(src);

	if (!prs) {
		xerror_fatal("cannot init parser");
		return NULL;
	}

	module *root = RecursiveDescent(prs, filename);

	(void) TokenChannelShutdown(prs->chan);

	if (prs->errors) {
		xerror_fatal("tree is ill-formed");
		return NULL;
	}

	return root;
}

//------------------------------------------------------------------------------
//node management

static module ModuleInit(parser *self, const cstring *alias)
{
	assert(self);
	assert(alias);

	cstring *copy = cStringDuplicate(alias);

	module node =  {
		.imports = {0},
		.declarations = {0},
		.alias = copy,
		.next = NULL,
		.table = NULL,
		.flag = false 
	};

	const size_t import_capacity = 8;
	node.imports = ImportVectorInit(0, import_capacity);

	const size_t decl_capacity = 16;
	node.declarations = DeclVectorInit(0, decl_capacity);

	return node;
}

static expr *ExprInit(parser *self, const exprtag tag)
{
	assert(self);
	assert(tag >= NODE_ASSIGNMENT && tag <= NODE_IDENT);

	expr *new = allocate(sizeof(expr));

	new->tag = tag;

	return new;
}

static decl *CopyDeclToHeap(parser *self, const decl src)
{
	assert(self);

	const size_t bytes = sizeof(decl);

	decl *new = allocate(bytes);

	memcpy(new, &src, bytes);

	return new;
}

static stmt *CopyStmtToHeap(parser *self, const stmt src)
{
	assert(self);

	const size_t bytes = sizeof(stmt);

	stmt *new = allocate(bytes);

	memcpy(new, &src, bytes);

	return new;
}

//returns a dynamically allocated cstring copy of the token lexeme
static cstring *cStringFromLexeme(parser *self)
{
	assert(self);
	assert(self->tok.lexeme.view != NULL);

	return cStringFromView(self->tok.lexeme.view, self->tok.lexeme.len);
}

//------------------------------------------------------------------------------
//helper functions

//the parser always uses 'self' as a OOP receiver name, so the xerror API can
//be simplified with a guaranteed access mechanism for the token file name and
//line number.
#define usererror(msg, ...) 					               \
do {								               \
	xuser_error(self->root.alias, self->tok.line, msg, ##__VA_ARGS__);     \
	self->errors++;					                       \
} while (0)

//if the extracted index is not a simple nonnegative integer less than LLONG_MAX
//then a parser exception is thrown.
static intmax_t ExtractArrayIndex(parser *self)
{
	assert(self);

	long long int candidate = 0;
	char *end = NULL;

	const cstring *digits = cStringFromLexeme(self);
	candidate = strtoll(digits, &end, 10);

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
	const token_type type,
	const cstring *msg,
	const bool move_before,
	const bool move_after
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

#define check(type, msg) \
	CheckToken(self, type, msg, false, false)

#define move_check(type, msg) \
	CheckToken(self, type, msg, true, false)

#define check_move(type, msg) \
	CheckToken(self, type, msg, false, true)

#define move_check_move(type, msg) \
	CheckToken(self, type, msg,  true, true)


//------------------------------------------------------------------------------
//channel operations

//the lexeme associated with the token, if any, will be added to the garbage
static void GetNextToken(parser *self)
{
	assert(self);

	int err = TokenChannelRecv(self->chan, &self->tok);

	if (err) {
		assert(0 != 0 && "attempted to read past EOF");
		xerror_fatal("attempted to read past EOF");
		abort();
	}
}

//synchronize at the block level if the immediate next token is invalid
static void GetNextValidToken(parser *self)
{
	assert(self);

	GetNextToken(self);

	if (self->tok.type == _INVALID) {
		(void) Synchronize(self, false);
	}
}

static void ReportInvalidToken(parser *self)
{
	assert(self);
	assert(self->tok.type == _INVALID);

	cstring *name = cStringFromLexeme(self);

	if (self->tok.flags.bad_string == 1) {
		usererror("unterminated string literal");
	} else if (self->tok.flags.valid == 0) {
		usererror("invalid syntax: %s", name);
	} else {
		usererror("unspecified syntax error: %s", name);
		xerror_issue("invalid token is missing flags");
	}
}

//if module-level then only declarations are sequence points
static size_t Synchronize(parser *self, bool module_level)
{
	size_t tokens_skipped = 0;

	do {

/* enable switch range statements */
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wpedantic\"")

		switch(self->tok.type) {
		case _EOF:
			__attribute__((fallthrough));

		case _STRUCT ... _LET:
			goto found_sequence_point;

		//'{' is for all intents and purposes the blockstmt keyword
		case _LEFTBRACE:
			__attribute__((fallthrough));

		case _RETURN ... _SWITCH:
			if (module_level) {
				break;
			}

			goto found_sequence_point;

		case _INVALID:
			ReportInvalidToken(self);
			break;

		default:
			break;
		}

/* disable switch range statements */
_Pragma("GCC diagnostic pop")

		GetNextToken(self);
		tokens_skipped++;
	} while (true);

found_sequence_point:
	return tokens_skipped;
}

//------------------------------------------------------------------------------
//parsing algorithm

static module *RecursiveDescent(parser *self, const cstring *alias)
{
	assert(self);

	CEXCEPTION_T exception;

	self->root = ModuleInit(self, alias);

	GetNextValidToken(self);

	while (self->tok.type == _IMPORT) {
		Try {
			import node = RecImport(self);
			ImportVectorPush(&self->root.imports, node);
		} Catch (exception) {
			(void) Synchronize(self, true);
		}
	}

	while (self->tok.type != _EOF) {
		Try {
			decl node = RecDecl(self);
			DeclVectorPush(&self->root.declarations, node);
		} Catch (exception) {
			(void) Synchronize(self, true);
		}
	}

	return &self->root;
}

//-----------------------------------------------------------------------------
//directives

static import RecImport(parser *self)
{
	assert(self);
	assert(self->tok.type == _IMPORT);

	move_check(_LITERALSTR, "missing import path string");

	import node = {
		.alias = cStringFromLexeme(self),
		.line = self->tok.line
	};

	GetNextValidToken(self);

	return node;
}

//------------------------------------------------------------------------------
//declarations

static decl RecDecl(parser *self)
{
	assert(self);

	const cstring *msg = "'%.*s' is not the start of a valid declaration";
	const char *view = self->tok.lexeme.view;
	const size_t len = self->tok.lexeme.len;

	switch (self->tok.type) {
	case _STRUCT:
		return RecStruct(self);

	case _FUNC:
		return RecFunction(self);

	case _METHOD:
		return RecMethod(self);

	case _LET:
		return RecVariable(self);

	default:
		usererror(msg, len, view);
		Throw(XXPARSE);
		__builtin_unreachable();
	}
}

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

	check(_IDENTIFIER, "missing struct name after 'struct' keyword");

	node.udt.name = cStringFromLexeme(self);

	move_check_move(_LEFTBRACE, "missing '{' after struct name");

	node.udt.members = RecParseMembers(self);

	check_move(_RIGHTBRACE, "missing '}' after struct members");

	check_move(_SEMICOLON, "missing ';' after struct declaration");

	return node;
}

//<member list>
//throws error if no members found
//member line number corresponds to the leading identifier
static vector(Member) RecParseMembers(parser *self)
{
	assert(self);

	const size_t vec_capacity = 4;
	vector(Member) vec = MemberVectorInit(0, vec_capacity);

	member attr = {
		.name = NULL,
		.typ = NULL,
		.entry = NULL,
		.line = 0,
		.public = false
	};

	while (self->tok.type != _RIGHTBRACE) {
		if (self->tok.type == _PUB) {
			attr.public = true;
			GetNextValidToken(self);
		}

		check(_IDENTIFIER, "missing struct member name");

		attr.name = cStringFromLexeme(self);
		attr.line = self->tok.line;

		move_check_move(_COLON, "missing ':' after name");

		attr.typ = RecType(self);

		check_move(_SEMICOLON, "missing ';' after type");

		MemberVectorPush(&vec, attr);

		memset(&attr, 0, sizeof(member));
	}

	if (vec.len == 0) {
		usererror("cannot declare an empty struct");
		Throw(XXPARSE);
	}

	return vec;
}

static decl RecFunction(parser *self)
{
	assert(self);
	assert(self->tok.type == _FUNC);

	decl node = {
		.tag = NODE_FUNCTION,
		.function = {
			.name = NULL,
			.ret = NULL,
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

	check(_IDENTIFIER, "missing function name in declaration");

	node.function.name = cStringFromLexeme(self);

	//parameter list
	move_check_move(_LEFTPAREN, "missing '(' after function name");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.function.params = RecParseParameters(self);
	}

	check_move(_RIGHTPAREN, "missing ')' after parameters");

	//return
	check_move(_MINUS, "missing '->' after parameter list");
	check_move(_GREATER, "missing ->' after parameter list");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.function.ret = RecType(self);
	}

	//body
	check(_LEFTBRACE, "cannot declare function without a body");
	node.function.block = CopyStmtToHeap(self, RecBlock(self));

	return node;
}

static decl RecMethod(parser *self)
{
	assert(self);
	assert(self->tok.type == _METHOD);

	decl node = {
		.tag = NODE_METHOD,
		.method= {
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
		node.method.public = true;
		GetNextValidToken(self);
	}

	//receiver-name pair
	check_move(_LEFTPAREN, "missing '(' before receiver");

	node.method.recv = RecType(self);

	check_move(_RIGHTPAREN, "missing closing ')' after receiver");

	check(_IDENTIFIER, "missing method name in declaration");

	node.method.name = cStringFromLexeme(self);

	//parameter list
	move_check_move(_LEFTPAREN, "missing '(' after method name");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.method.params = RecParseParameters(self);
	}

	check_move(_RIGHTPAREN, "missing ')' after parameters");

	//return
	check_move(_MINUS, "missing '->' after parameter list");
	check_move(_GREATER, "missing ->' after parameter list");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.function.ret = RecType(self);
	}

	//body
	check(_LEFTBRACE, "cannot declare method without a body");
	node.method.block = CopyStmtToHeap(self, RecBlock(self));

	return node;
}

//<parameter list>
//throws error if no parameters found
//line number corresponds to leading identifier
static vector(Param) RecParseParameters(parser *self)
{
	assert(self);

	const size_t vec_capacity = 4;
	vector(Param) vec = ParamVectorInit(0, vec_capacity);

	param attr  = {
		.name = NULL,
		.typ = NULL,
		.entry = NULL,
		.line = 0,
		.mutable = false
	};

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(_COMMA, "missing ',' after parameter");
		}

		if (self->tok.type == _MUT) {
			attr.mutable = true;
			GetNextValidToken(self);
		}

		check(_IDENTIFIER, "missing function parameter name");

		attr.name = cStringFromLexeme(self);
		attr.line = self->tok.line;

		move_check_move(_COLON, "missing ':' after name");

		attr.typ = RecType(self);

		ParamVectorPush(&vec, attr);

		memset(&attr, 0, sizeof(param));
	}

	if (vec.len == 0) {
		usererror("empty parameter list; did you mean 'void'?");
		Throw(XXPARSE);
	}

	return vec;
}

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

	check(_IDENTIFIER, "missing variable name in declaration");

	node.variable.name = cStringFromLexeme(self);

	move_check_move(_COLON, "missing ':' before type");

	node.variable.vartype = RecType(self);

	check_move(_EQUAL, "declared variables must be initialized");

	node.variable.value = RecAssignment(self);

	check_move(_SEMICOLON, "missing ';' after declaration");

	return node;
}

//guaranteed to be a singly linked list with a non-null head
static type *RecType(parser *self)
{
	assert(self);

	type *node = allocate(sizeof(type));
	cstring *prev_name = NULL;

	switch (self->tok.type) {
	case _IDENTIFIER:
		node->line = self->tok.line;

		prev_name = cStringFromLexeme(self);

		GetNextValidToken(self);

		if (self->tok.type != _DOT) {
			node->tag = NODE_BASE;
			node->base.name = prev_name;
			break;
		}

		node->tag = NODE_NAMED;
		node->named.name = prev_name;

		move_check(_IDENTIFIER, "missing type after '.'");
		node->named.reference = RecType(self);

		if (node->named.reference->tag != NODE_BASE) {
			usererror("nested named types are not allowed");
			Throw(XXPARSE);
		}

		break;

	case _STAR:
		node->tag = NODE_POINTER;
		node->line = self->tok.line;
		GetNextValidToken(self);
		node->pointer.reference = RecType(self);

		break;

	case _LEFTBRACKET:
		node->tag = NODE_ARRAY;
		node->line = self->tok.line;

		move_check(_LITERALINT, "missing array size");
		node->array.len = ExtractArrayIndex(self);
		move_check_move(_RIGHTBRACKET, "missing ']'");

		node->array.element = RecType(self);

		break;

	default:
		usererror("missing data type");
		Throw(XXPARSE);
		__builtin_unreachable();
	}

	return node;
}

//------------------------------------------------------------------------------
//statements

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
		__attribute__((fallthrough));

	case _GOTO:
		node = RecNamedTarget(self);
		break;

	case _BREAK:
		__attribute__((fallthrough));

	case _CONTINUE:
		__attribute__((fallthrough));

	case _FALLTHROUGH:
		node = RecAnonymousTarget(self);
		break;

	case _LABEL:
		node = RecLabel(self);
		break;

	case _SEMICOLON:
		usererror("empty statement has no effect");
		Throw(XXPARSE);
		__builtin_unreachable();

	default:
		node = RecExprStmt(self);
		break;
	}

	return node;
}

//blocks essentially "restart" recursive descent and are even more expressive
//than module nodes because they allow for statements. A new try block is used
//here so that we can synchronise from errors with as little information loss
//as possible. If we remove the try block and let a function higher in the call
//stack catch the exception, that function might or might not synchronize at
//the module level. This would drop the remaining statements within the block.
//Catching here means that we can present more errors to the user in a single
//compliation attempt.
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
	node.block.fiats = FiatVectorInit(0, vec_capacity);

	GetNextValidToken(self);

	ParseFiats(self, &node.block.fiats);

	GetNextValidToken(self);

	return node;
}

static void ParseFiats(parser *self, vector(Fiat) *const fiats)
{
	assert(self);
	assert(fiats);

	CEXCEPTION_T e;

	while (self->tok.type != _RIGHTBRACE) {
		Try {
			FiatVectorPush(fiats, RecFiat(self));
		} Catch (e) {
			(void) Synchronize(self, false);
		}

		if (self->tok.type == _EOF) {
			usererror("missing closing '}' at end of module");
			Throw(XXPARSE);
		}
	}
}

static fiat RecFiat(parser *self)
{
	assert(self);

	switch (self->tok.type) {
	case _STRUCT:
		__attribute__((fallthrough));

	case _FUNC:
		__attribute__((fallthrough));

	case _METHOD:
		__attribute__((fallthrough));

	case _LET:
		return (fiat) {
			.tag = NODE_DECL,
			.declaration = RecDecl(self)
		};

	default:
		return (fiat) {
			.tag = NODE_STMT,
			.statement = RecStmt(self)
		};
	}
}

static stmt RecForLoop(parser *self)
{
	assert(self);
	assert(self->tok.type == _FOR);

	stmt node = {
		.tag = NODE_FORLOOP,
		.line = self->tok.line
	};

	move_check_move(_LEFTPAREN, "missing '(' after 'for' keyword");

	//initial condition
	if (self->tok.type == _LET) {
		node.forloop.tag = FOR_DECL;
		node.forloop.shortvar = CopyDeclToHeap(self, RecVariable(self));
		//RecVariable consumes the terminating semicolon
	} else {
		node.forloop.tag = FOR_INIT;
		node.forloop.init = RecAssignment(self);
		check_move(_SEMICOLON, "missing ';' after init");
	}

	node.forloop.cond = RecAssignment(self);
	check_move(_SEMICOLON, "missing ';' after condition");

	node.forloop.post = RecAssignment(self);
	check_move(_RIGHTPAREN, "missing ')' after post expression");

	node.forloop.block = CopyStmtToHeap(self, RecBlock(self));

	return node;
}

static stmt RecWhileLoop(parser *self)
{
	assert(self);
	assert(self->tok.type == _WHILE);

	stmt node = {
		.tag = NODE_WHILELOOP,
		.line = self->tok.line
	};

	move_check_move(_LEFTPAREN, "missing'(' after 'while'");

	node.whileloop.cond = RecAssignment(self);

	check_move(_RIGHTPAREN, "missing ')' after while condition");

	if (self->tok.type == _LEFTBRACE) {
		node.whileloop.block = CopyStmtToHeap(self, RecBlock(self));
	} else {
		usererror("missing block statement after while loop");
		Throw(XXPARSE);
	}

	return node;
}

static stmt RecSwitch(parser *self)
{
	assert(self);
	assert(self->tok.type == _SWITCH);

	stmt node = {
		.tag = NODE_SWITCHSTMT,
		.line = self->tok.line
	};

	move_check_move(_LEFTPAREN, "missing '(' after 'switch'");

	node.switchstmt.controller = RecAssignment(self);

	check_move(_RIGHTPAREN, "missing ')' after switch condition");

	check_move(_LEFTBRACE, "missing '{' to open switch body");

	node.switchstmt.tests = RecTests(self);

	check_move(_RIGHTBRACE, "missing '}' to close switch body");

	return node;
}

//<case statement>* within <switch statement> rule
static vector(Test) RecTests(parser *self)
{
	assert(self);

	const size_t vec_capacity = 4;
	vector(Test) vec = TestVectorInit(0, vec_capacity);

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
		TestVectorPush(&vec, t);
		memset(&t, 0, sizeof(test));
	}

	if (vec.len == 0) {
		usererror("switch statement cannot be empty");
		Throw(XXPARSE);
	}

	return vec;
}

static stmt RecBranch(parser *self)
{
	assert(self);
	assert(self->tok.type == _IF);

	stmt node = {
		.tag = NODE_BRANCH,
		.line = self->tok.line
	};

	//condition
	move_check_move(_LEFTPAREN, "missing '(' after 'if'");

	if (self->tok.type == _LET) {
		node.branch.shortvar = CopyDeclToHeap(self, RecVariable(self));
		//RecVariable consumes terminating semicolon
	} else {
		node.branch.shortvar = NULL;
	}

	node.branch.cond = RecAssignment(self);

	check_move(_RIGHTPAREN, "missing ')' after if condition");

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
	       self->tok.type == _RETURN);

	stmt node = {
		.line = self->tok.line
	};

	switch (self->tok.type) {
	case _GOTO:
		node.tag = NODE_GOTOLABEL;
		move_check(_IDENTIFIER, "missing goto target");
		node.gotostmt.name = cStringFromLexeme(self);
		move_check_move(_SEMICOLON, "missing ';' after goto");
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
		check_move(_SEMICOLON, "missing ';' after return");
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
		move_check_move(_SEMICOLON, "missing ';' after break");
		break;

	case _CONTINUE:
		node.tag = NODE_CONTINUESTMT;
		move_check_move(_SEMICOLON, "missing ';' after continue");
		break;

	case _FALLTHROUGH:
		node.tag = NODE_FALLTHROUGHSTMT;
		move_check_move(_SEMICOLON, "missing ';' after fall");
		break;

	default:
		assert(0 != 0);
	}

	return node;
}

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

	move_check(_IDENTIFIER, "label name must be an identifier");

	node.label.name = cStringFromLexeme(self);

	move_check_move(_COLON, "missing ':' after label name");

	node.label.target = CopyStmtToHeap(self, RecStmt(self));

	return node;
}

static stmt RecExprStmt(parser *self)
{
	assert(self);

	stmt node = {
		.tag = NODE_EXPRSTMT,
		.exprstmt = NULL,
		.line = self->tok.line
	};

	node.exprstmt = RecAssignment(self);

	check_move(_SEMICOLON, "missing ';' after expression");

	return node;
}

//------------------------------------------------------------------------------
//expressions

static expr *RecAssignment(parser *self)
{
	assert(self);

	expr *node = RecLogicalOr(self);

	if (self->tok.type == _EQUAL) {
		expr *tmp = node;
		node = ExprInit(self, NODE_ASSIGNMENT);
		node->assignment.lvalue = tmp;
		node->line = self->tok.line;

		GetNextValidToken(self);
		node->assignment.rvalue = RecLogicalOr(self);
	}

	return node;
}

static expr *RecLogicalOr(parser *self)
{
	assert(self);

	expr *node = RecLogicalAnd(self);
	expr *tmp = NULL;

	while (self->tok.type == _OR) {
		tmp = node;
		node = ExprInit(self, NODE_BINARY);

		node->binary.left = tmp;
		node->binary.operator = _OR;
		node->line = self->tok.line;

		GetNextValidToken(self);
		node->binary.right = RecLogicalAnd(self);
	}

	return node;
}

static expr *RecLogicalAnd(parser *self)
{
	assert(self);

	expr *node = RecEquality(self);

	while (self->tok.type == _AND) {
		expr *tmp = node;
		node = ExprInit(self, NODE_BINARY);

		node->binary.left = tmp;
		node->binary.operator = _AND;
		node->line = self->tok.line;

		GetNextValidToken(self);
		node->binary.right = RecEquality(self);
	}

	return node;
}

static expr *RecEquality(parser *self)
{
	assert(self);

	expr *node = RecTerm(self);
	expr *tmp = NULL;

	while (true) {
		switch (self->tok.type) {
		case _GREATER:
			__attribute__((fallthrough));

		case _LESS:
			__attribute__((fallthrough));

		case _GEQ:
			__attribute__((fallthrough));

		case _LEQ:
			__attribute__((fallthrough));

		case _EQUALEQUAL:
			__attribute__((fallthrough));

		case _NOTEQUAL:
			tmp = node;
			node = ExprInit(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			GetNextValidToken(self);
			node->binary.right = RecTerm(self);
			break;

		default:
			goto exit_loop;
		}
	}

exit_loop:
	return node;
}

static expr *RecTerm(parser *self)
{
	assert(self);

	expr *node = RecFactor(self);
	expr *tmp = NULL;

	while (true) {
		switch (self->tok.type) {
		case _ADD:
			__attribute__((fallthrough));

		case _MINUS:
			__attribute__((fallthrough));

		case _BITOR:
			__attribute__((fallthrough));

		case _BITXOR:
			tmp = node;
			node = ExprInit(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			GetNextValidToken(self);
			node->binary.right = RecFactor(self);
			break;

		default:
			goto exit_loop;
		}
	}

exit_loop:
	return node;
}

static expr *RecFactor(parser *self)
{
	assert(self);

	expr *node = RecUnary(self);
	expr *tmp = NULL;

	while (true) {
		switch (self->tok.type) {
		case _STAR:
			__attribute__((fallthrough));

		case _DIV:
			__attribute__((fallthrough));

		case _MOD:
			__attribute__((fallthrough));

		case _LSHIFT:
			__attribute__((fallthrough));

		case _RSHIFT:
			__attribute__((fallthrough));

		case _AMPERSAND:
			tmp = node;
			node = ExprInit(self, NODE_BINARY);

			node->binary.left = tmp;
			node->binary.operator = self->tok.type;
			node->line = self->tok.line;

			GetNextValidToken(self);
			node->binary.right = RecUnary(self);
			break;

		default:
			goto exit_loop;
		}
	}

exit_loop:
	return node;
}

static expr *RecUnary(parser *self)
{
	assert(self);

	expr *node = NULL;

	switch (self->tok.type) {
	case _MINUS:
		__attribute__((fallthrough));

	case _ADD:
		__attribute__((fallthrough));

	case _BITNOT:
		__attribute__((fallthrough));

	case _NOT:
		__attribute__((fallthrough));

	case _STAR:
		__attribute__((fallthrough));

	case _AMPERSAND:
		node = ExprInit(self, NODE_UNARY);
		node->unary.operator = self->tok.type;
		node->line = self->tok.line;
		GetNextValidToken(self);
		node->unary.operand = RecUnary(self);
		break;

	case _COLON:
		node = ExprInit(self, NODE_CAST);
		node->line = self->tok.line;
		GetNextValidToken(self);
		node->cast.casttype = RecType(self);
		check_move(_COLON, "missing ':' after type cast");
		node->cast.operand = RecUnary(self);
		break;

	default:
		node = RecPrimary(self);
	}

	return node;
}

//the <atom> grammar rule is expanded and implemented within RecPrimary while
//the optional call, selector, and index rules are relegated to RecAccess.
static expr *RecPrimary(parser *self)
{
	assert(self);

	static const cstring *fmt = "expression is ill-formed at '%s'";
	const char *view = self->tok.lexeme.view;
	const size_t len = self->tok.lexeme.len;

	assert(view);

	expr *node = NULL;

	switch (self->tok.type) {
	case _IDENTIFIER:
		node = RecRvarOrIdentifier(self);
		break;

	case _LEFTBRACKET:
		node = RecArrayLiteral(self);
		break;

	case _LITERALINT:
		__attribute__((fallthrough));

	case _LITERALFLOAT:
		__attribute__((fallthrough));

	case _LITERALSTR:
		__attribute__((fallthrough));

	case _NULL:
		__attribute__((fallthrough));

	case _SELF:
		__attribute__((fallthrough));

	case _TRUE:
		__attribute__((fallthrough));

	case _FALSE:
		node = ExprInit(self, NODE_LIT);
		node->line = self->tok.line;
		node->lit.rep = cStringFromLexeme(self);
		node->lit.littype = self->tok.type;
		GetNextValidToken(self);
		break;

	case _LEFTPAREN:
		GetNextValidToken(self);
		node = RecAssignment(self);
		check_move(_RIGHTPAREN, "missing ')' after grouping");
		break;

	//imports cannot exist other than at the start of a module. The default
	//case message is too generic to make sense for most end-users in this
	//situation.
	case _IMPORT:
		usererror("import directives must occur before all other "
			  "statements and declarations");
		Throw(XXPARSE);
		break;

	default:
		usererror(fmt, len, view);
		Throw(XXPARSE);
	}

	return RecAccess(self, node);
}

//RecAccess recursively wraps the previous expression within another expression
//tree. The previous expression always binds tighter than the expression it is
//wrapped in, and this is enforced by a post-order traversal.
//
//example: x.y.z wraps the atom node x in a selector node for y. the selector
//node for y is again wrapped in another selector node for z.
//
//example: foo().bar wraps the atom node foo in a call node. The call node is
//wrapped again in a selector node for bar.
static expr *RecAccess(parser *self, expr *prev)
{
	assert(self);
	assert(prev);

	expr *node = NULL;

	switch (self->tok.type) {
	case _DOT:
		node = ExprInit(self, NODE_SELECTOR);
		node->line = self->tok.line;

		move_check(_IDENTIFIER, "missing attribute after '.'");

		node->selector.name = prev;
		node->selector.attr = RecIdentifier(self);

		GetNextValidToken(self);
		break;

	case _LEFTPAREN:
		node = ExprInit(self, NODE_CALL);
		node->line = self->tok.line;

		node->call.name = prev;
		node->call.args = RecArguments(self);

		break;

	case _LEFTBRACKET:
		node = ExprInit(self, NODE_INDEX);
		node->line = self->tok.line;

		GetNextValidToken(self);

		node->index.name = prev;
		node->index.key = RecAssignment(self);

		check_move(_RIGHTBRACKET, "missing ']' after index");

		break;

	//recursion base case
	default:
		return prev;
	}

	return RecAccess(self, node);
}

static expr *RecRvarOrIdentifier(parser *self)
{
	assert(self);
	assert(self->tok.type == _IDENTIFIER);

	expr *node = NULL;
	token prev = self->tok;

	GetNextValidToken(self);

	if (self->tok.type == _TILDE) {
		self->tok = prev;
		node = RecRvar(self, true);
	} else {
		token tmp = self->tok;
		self->tok = prev;
		node = RecIdentifier(self);
		self->tok = tmp;
	}

	return node;
}

static expr *RecIdentifier(parser *self)
{
	assert(self);
	assert(self->tok.type == _IDENTIFIER);

	expr *node = ExprInit(self, NODE_IDENT);
	node->line = self->tok.line;
	node->ident.name = cStringFromLexeme(self);

	return node;
}

//lookahead might strip the tilde from the token channel
static expr *RecRvar(parser *self, bool seen_tilde)
{
	assert(self);
	assert(self->tok.type == _IDENTIFIER);

	expr *node = ExprInit(self, NODE_RVARLIT);

	node->line = self->tok.line;
	node->rvarlit.dist = cStringFromLexeme(self);

	if (!seen_tilde) {
		move_check(_TILDE, "missing '~' after distribution");
	}

	GetNextValidToken(self);

	node->rvarlit.args = RecArguments(self);

	return node;
}

static vector(Expr) RecArguments(parser *self)
{
	assert(self);
	assert(self->tok.type == _LEFTPAREN);

	const size_t vec_capacity = 4;
	vector(Expr) vec = ExprVectorInit(0, vec_capacity);

	GetNextValidToken(self);

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(_COMMA, "missing ',' after arg");
		}

		ExprVectorPush(&vec, RecAssignment(self));
	}

	GetNextValidToken(self);

	return vec;
}

//for each key-value pair in the array literal the key is kept in an idx_vector
//and the value is kept in an expr_vector at the same index. If a value in the
//array literal is not tagged with a key, then the associated value in the
//idx_vector is -1.
static expr *RecArrayLiteral(parser *self)
{
	assert(self);
	assert(self->tok.type == _LEFTBRACKET);

	static cstring *tagerr = "tagged array index must be an integer";
	static cstring *closeerr = "missing ']' after tagged index";
	static cstring *eqerr = "missing '=' after tagged index";

	expr *node = ExprInit(self, NODE_ARRAYLIT);

	node->line = self->tok.line;

	const size_t idx_cap = 4;
	node->arraylit.indicies = IndexVectorInit(0, idx_cap);

	const size_t expr_cap = 4;
	node->arraylit.values = ExprVectorInit(0, expr_cap);

	GetNextValidToken(self);

	while (self->tok.type != _RIGHTBRACKET) {
		if (node->arraylit.values.len > 0) {
			check_move(_COMMA, "missing ',' after value");
		}

		if (self->tok.type == _LEFTBRACKET) {
			move_check(_LITERALINT, tagerr);

			intmax_t idx = ExtractArrayIndex(self);
			IndexVectorPush(&node->arraylit.indicies, idx);

			move_check_move(_RIGHTBRACKET, closeerr);
			check_move(_EQUAL, eqerr);
		} else {
			IndexVectorPush(&node->arraylit.indicies, -1);
		}

		ExprVectorPush(&node->arraylit.values, RecAssignment(self));
	}

	GetNextValidToken(self);

	assert(node->arraylit.indicies.len == node->arraylit.values.len);

	return node;
}
