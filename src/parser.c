// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Recursive descent parser. Two types of errors may be encountered: compiler
// errors and user errors. Compiler errors are treated as usual; Log the issue
// and propogate the error code.
//
// User errors are not logged, they don't propoage error codes, and they do not
// cause the parser to terminate. Instead, they throw exceptions which trigger
// the parser to synchronize to a new sequence point within the token stream.

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "defs.h"
#include "lib/channel.h"

typedef struct parser parser;

//parser management
static parser *ParserInit(const cstring *, const cstring *);
static void ParserFree(parser *);
static parser *ParserContainerOf(file *);

//utils
static void RawPointerRelease(void *);
static void Mark(parser *, void *);
static void *AllocateAndMark(parser *, const size_t);
static void RemoveGarbage(vector(Ptr) *);
static void MoveGarbage(vector(Ptr) *restrict, vector(Ptr) *restrict);
static cstring *MarkedLexeme(parser *);
static void GetNextToken(parser *);
static void GetNextValidToken(parser *);
static void ReportInvalidToken(parser *);
static size_t Synchronize(parser *);
static void CheckToken
(parser *, const token_type, const cstring *, const bool, const bool);
static intmax_t ExtractArrayIndex(parser *);
static metadata *MetadataInit(void);
static file FileInit(void);
static expr *ExprInit(parser *, const exprtag);
static stmt *CopyStmtToHeap(parser *, const stmt);
static decl *CopyDeclToHeap(parser *, const decl);

//declarations
static file *RecursiveDescent(parser *);
static cstring *RecImport(parser *);
static fiat RecFiat(parser *);
static decl RecStruct(parser *);
static vector(Member) RecParseMembers(parser *);
decl RecFunction(parser *);
static vector(Param) RecParseParameters(parser *);
type *RecType(parser *);
static decl RecVariable(parser *);

//statements
static stmt RecStmt(parser *);
static stmt RecExprStmt(parser *);
static stmt RecBlock(parser *);
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
// API implementation

file *SyntaxTreeInit(const cstring *src, const cstring *alias)
{
	assert(src);
	assert(alias);

	parser *prs = ParserInit(src, alias);

	if (!prs) {
		xerror_issue("cannot init parser");
		return NULL;
	}

	file *root = RecursiveDescent(prs);

	return root;
}

void SyntaxTreeFree(file *root)
{
	assert(root);

	parser *prs = ContainerOfRoot(root);

	ParserFree(prs);
}

//-----------------------------------------------------------------------------
// memory management
//
// The unity exception handler does not release dynamic memory allocations made
// within a try-block whenever an exception occurs. To accomodate for this,
// during recursive descent the parser tracks its dynamic allocations in the
// vector<RawPointer> container.
//
// If an exception occurs, allocations stored in the vector since the last
// successful sequence point are released to the system.
//
// The allocations made from all successful sequence points are kept when the
// parser terminates and are later referenced when the AST needs to be cleaned
// up. Since vectors use cache-dense buffers, this results in very fast AST
// destruction as compared to a post-order traversal.

typedef enum raw_pointer_tag {
	PTR_STDLIB_MALLOC,
	PTR_MEMBER_VECTOR,
	PTR_PARAM_VECTOR,
	PTR_TEST_VECTOR,
	PTR_INDEX_VECTOR,
	PTR_FIAT_VECTOR,
	PTR_DECL_VECTOR,
	PTR_STMT_VECTOR,
	PTR_EXPR_VECTOR,
} raw_pointer_tag;

typedef struct raw_pointer {
	raw_pointer_tag tag;	
	void *memory;
} raw_pointer;

make_vector(raw_pointer, RawPointer, static)

static void RawPointerRelease(void *region)
{
	const raw_pointer ptr = *((raw_pointer *) region);

	switch (ptr.tag) {
	case PTR_STDLIB_MALLOC:
		free(ptr.memory);
		break;

	case PTR_MEMBER_VECTOR:
		MemberVectorFree(ptr.memory, NULL);
		break;
	
	case PTR_PARAM_VECTOR:
		ParamvectorFree(ptr.memory, NULL);
		break;

	case PTR_TEST_VECTOR:
		TestVectorFree(ptr.memory, NULL);
		break;

	case PTR_INDEX_VECTOR:
		IndexVectorFree(ptr.memory, NULL);
		break;

	case PTR_FIAT_VECTOR:
		FiatVectorFree(ptr.memory, NULL);
		break;

	case PTR_DECL_VECTOR:
		DeclVectorFree(ptr.memory, NULL);
		break;
	
	case PTR_STMT_VECTOR:
		StmtVectorFree(ptr.memory, NULL);
		break;

	case PTR_EXPR_VECTOR:
		ExprVectorFree(ptr.memory, NULL);
		break;

	default:
		assert(0 != 0 && "invalid raw pointer tag");
	}
}

//-----------------------------------------------------------------------------
//parser management

struct parser {
	vector(RawPointer) garbage;
	channel(Token) *chan;
	const cstring *alias;
	token tok;
	file root;
};

static parser *ParserInit(const cstring *src, const cstring *alias)
{
	assert(src);
	assert(alias);

	parser *prs = AbortMalloc(sizeof(parser));

	prs->chan = AbortMalloc(sizeof(channel(Token)));
	TokenChannelInit(prs->chan, KiB(1));

	prs->garbage = RawPointerVectorInit(0, KiB(1));
	prs->alias = alias;
	prs->tok = INVALID_TOKEN;
	prs->file = FileInit();

	xerror err = ScannerInit(src, prs->chan);

	if (err) {
		const cstring *msg = XerrorDescription(err);
		xerror_issue("cannot init scanner: %s", msg);
		ParserFree(&prs);
		return NULL;
	}

	return prs;
}

static void ParserFree(parser *self)
{
	assert(self);

	(void) TokenChannelFree(self->chan, NULL);

	free(self->chan);

	RawPointerVectorFree(self->garbage, RawPointerRelease);

	free(self);
}

//@help see linux kernel containerof macro
static parser *ParserContainerOf(file *root)
{
	assert(root);

	const size_t offset = offsetof(parser, root);

	return (parser *) ((char *) root - offset);
}

//------------------------------------------------------------------------------
//node management

static file FileInit(void)
{
	file node =  {
		.imports = {0},
		.fiats = {0},
		.errors = 0
	};

	const size_t import_capacity = 8;
	node.imports = ImportVectorInit(0, import_capacity);

	const size_t fiat_capacity = 64;
	node.fiats = FiatVectorInit(0, fiat_capacity);

	return node;
}

static expr *ExprInit(parser *self, const exprtag tag)
{
	assert(self);
	assert(tag >= NODE_ASSIGNMENT && tag <= NODE_IDENT);

	expr *new = AllocateAndMark(self, sizeof(expr));

	new->tag = tag;

	return new;
}

static decl *CopyDeclToHeap(parser *self, const decl src)
{
	assert(self);

	const size_t bytes = sizeof(decl);

	decl *new = AllocateAndMark(self, bytes);

	memcpy(new, &src, bytes);

	return new;
}

static stmt *CopyStmtToHeap(parser *self, const stmt src)
{
	assert(self);

	const size_t bytes = sizeof(stmt);

	stmt *new = AllocateAndMark(self, bytes);

	memcpy(new, &src, bytes);

	return new;
}

//------------------------------------------------------------------------------
//helper functions

static void Mark(parser *self, void *region)
{
	PtrVectorPush(&self->garbage, region);
}

static void *AllocateAndMark(parser *self, const size_t bytes)
{
	assert(self);

	void *region = AbortMalloc(bytes);

	Mark(self, region);

	return region;
}

#define alloc_mark_vector(VecInit, recv, len, cap)                             \
	do {								       \
		recv = VecInit(len, cap);				       \
		Mark(self, recv.buffer);				       \
	} while(0)

static void RemoveGarbage(vector(Ptr) *garbage)
{
	assert(garbage);

	PtrVectorReset(garbage, free);
}

static void MoveGarbage(vector(Ptr) *restrict dest, vector(Ptr) *restrict src)
{
	assert(dest);
	assert(src);

	for (size_t i = 0; i < src->len; i++) {
		PtrVectorPush(dest, src->buffer[i]);
	}

	PtrVectorReset(src, NULL);
}

//the parser always uses 'self' as a OOP receiver name, so the xerror API can
//be simplified with a guaranteed access mechanism for the token file name and
//line number.
#define usererror(msg, ...) 					               \
	do {								       \
		XerrorUser(self->alias, self->tok.line, msg, ##__VA_ARGS__);   \
		self->errors++;						       \
	} while (0)

static cstring *MarkedLexeme(parser *self)
{
	assert(self);

	cstring *target = self->tok.lexeme;

	Mark(self, target);

	return target;
}

//if the extracted index is not a simple nonnegative integer less than LLONG_MAX
//then a parser exception is thrown.
static intmax_t ExtractArrayIndex(parser *self)
{
	assert(self);

	long long int candidate = 0;
	char *end = NULL;

	candidate = strtoll(self->tok.lexeme, &end, 10);

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

#define check(self, type, msg) \
	CheckToken(self, type, msg, false, false)

#define move_check(self, type, msg) \
	CheckToken(self, type, msg, true, false)

#define check_move(self, type, msg) \
	CheckToken(self, type, msg, false, true)

#define move_check_move(self, type, msg) \
	CheckToken(self, type, msg,  true, true)

//------------------------------------------------------------------------------
//channel operations

static void GetNextToken(parser *self)
{
	assert(self);

	xerror err = TokenChannelRecv(self->chan, &self->tok);

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

	const cstring *lexeme = self->tok.lexeme;

	if (self->tok.flags.bad_string == 1) {
		usererror("unterminated string literal");
	} else if (self->tok.flags.valid == 0) {
		usererror("invalid syntax: %s", lexeme);
	} else {
		usererror("unspecified syntax error: %s", lexeme);
		xerror_issue("invalid token is missing flags");
	}
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

static file *RecursiveDescent(parser *self)
{
	assert(self);

	CEXCEPTION_T exception;

	metadata *mdt = MetadataInit();

	file *root = &mdt->root;

	GetNextValidToken(self);

	while (self->tok.type == _IMPORT) {
		Try {
			cstring *path = RecImport(self);
			ImportVectorPush(&root->imports, path);
			MoveGarbage(&mdt->garbage, &self->garbage);
		} Catch (exception) {
			(void) Synchronize(self);
			RemoveGarbage(&self->garbage);
		}
	}

	while (self->tok.type != _EOF) {
		Try {
			fiat node = RecFiat(self);
			FiatVectorPush(&root->fiats, node);
			MoveGarbage(&mdt->garbage, &self->garbage);
		} Catch (exception) {
			(void) Synchronize(self);
			RemoveGarbage(&self->garbage);
		}
	}

	root->errors = self->errors;

	return root;
}

static cstring *RecImport(parser *self)
{
	assert(self);
	assert(self->tok.type == _IMPORT);

	move_check(self, _LITERALSTR, "missing import path string");

	cstring *import_path = NULL;

	if (self->tok.lexeme != NULL) {
		import_path = MarkedLexeme(self);
	}

	move_check_move(self, _SEMICOLON, "missing ';' after import path");

	return import_path;
}

static fiat RecFiat(parser *self)
{
	assert(self);

	fiat node = {0};

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

	return node;
}

//------------------------------------------------------------------------------
//declarations

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

	node.udt.name = MarkedLexeme(self);

	move_check_move(self, _LEFTBRACE, "missing '{' after struct name");

	node.udt.members = RecParseMembers(self);

	check_move(self, _RIGHTBRACE, "missing '}' after struct members");

	check_move(self, _SEMICOLON, "missing ';' after struct declaration");

	return node;
}

//<member list>
//throws error if no members found
static vector(Member) RecParseMembers(parser *self)
{
	assert(self);

	vector(Member) vec = {0};
	const size_t vec_capacity = 4;
	alloc_mark_vector(MemberVectorInit, vec, 0, vec_capacity);

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

		attr.name = MarkedLexeme(self);

		move_check_move(self, _COLON, "missing ':' after name");

		attr.typ = RecType(self);

		check_move(self, _SEMICOLON, "missing ';' after type");

		MemberVectorPush(&vec, attr);

		memset(&attr, 0, sizeof(member));
	}

	if (vec.len == 0) {
		usererror("cannot declare an empty struct");
		Throw(XXPARSE);
	}

	return vec;
}

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

	node.function.name = MarkedLexeme(self);

	//parameter list
	move_check_move(self, _LEFTPAREN, "missing '(' after function name");

	if (self->tok.type == _VOID) {
		GetNextValidToken(self);
	} else {
		node.function.params = RecParseParameters(self);
	}

	check_move(self, _RIGHTPAREN, "missing ')' after parameters");

	//return
	check_move(self, _MINUS, "missing '->' after parameter list");
	check_move(self, _GREATER, "missing ->' after parameter list");

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
static vector(Param) RecParseParameters(parser *self)
{
	assert(self);

	vector(Param) vec = {0};
	const size_t vec_capacity = 4;
	alloc_mark_vector(ParamVectorInit, vec, 0, vec_capacity);

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

		attr.name = MarkedLexeme(self);

		move_check_move(self, _COLON, "missing ':' after name");

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

	check(self, _IDENTIFIER, "missing variable name in declaration");

	node.variable.name = MarkedLexeme(self);

	move_check_move(self, _COLON, "missing ':' before type");

	node.variable.vartype = RecType(self);

	check_move(self, _EQUAL, "declared variables must be initialized");

	node.variable.value = RecAssignment(self);

	check_move(self, _SEMICOLON, "missing ';' after declaration");

	return node;
}

//guaranteed to be a singly linked list with a non-null head
type *RecType(parser *self)
{
	assert(self);

	type *node = AllocateAndMark(self, sizeof(type));

	switch (self->tok.type) {
	case _IDENTIFIER:
		node->tag = NODE_BASE;
		node->base = MarkedLexeme(self);
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

//------------------------------------------------------------------------------
//statements

//master jump table
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
	alloc_mark_vector(FiatVectorInit, node.block.fiats, 0, vec_capacity);

	GetNextValidToken(self);

	while (self->tok.type != _RIGHTBRACE) {
		FiatVectorPush(&node.block.fiats, RecFiat(self));

		if (self->tok.type == _EOF) {
			usererror("missing closing '}' at end of file");
			Throw(XXPARSE);
		}
	}

	GetNextValidToken(self);

	return node;
}

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

	node.switchstmt.tests = RecTests(self);

	check_move(self, _RIGHTBRACE, "missing '}' to close switch body");

	return node;
}

//<case statement>* within <switch statement> rule
static vector(Test) RecTests(parser *self)
{
	assert(self);

	vector(Test) vec = {0};
	const size_t vec_capacity = 4;
	alloc_mark_vector(TestVectorInit, vec, 0, vec_capacity);

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
	       self->tok.type == _RETURN);

	stmt node = {
		.line = self->tok.line
	};

	switch (self->tok.type) {
	case _GOTO:
		node.tag = NODE_GOTOLABEL;
		move_check(self, _IDENTIFIER, "missing goto target");
		node.gotolabel = MarkedLexeme(self);
		move_check_move(self, _SEMICOLON, "missing ';' after goto");
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

	node.label.name = MarkedLexeme(self);

	move_check_move(self, _COLON, "missing ':' after label name");

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

	check_move(self, _SEMICOLON, "missing ';' after expression");

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
		node->unary.operand = RecUnary(self);
		break;

	case _COLON:
		node = ExprInit(self, NODE_CAST);
		node->line = self->tok.line;
		GetNextValidToken(self);
		node->cast.casttype = RecType(self);
		check_move(self, _COLON, "missing ':' after type cast");
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
	const cstring *msg = self->tok.lexeme;

	expr *node = NULL;

	switch (self->tok.type) {
	case _IDENTIFIER:
		node = RecRvarOrIdentifier(self);
		break;

	case _LEFTBRACKET:
		node = RecArrayLiteral(self);
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
		node->lit.rep = MarkedLexeme(self);
		node->lit.littype = self->tok.type;
		GetNextValidToken(self);
		break;

	case _LEFTPAREN:
		GetNextValidToken(self);
		node = RecAssignment(self);
		check_move(self, _RIGHTPAREN, "missing ')' after grouping");
		break;

	default:
		usererror(fmt, msg);
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

		move_check(self, _IDENTIFIER, "missing attribute after '.'");

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

		check_move(self, _RIGHTBRACKET, "missing ']' after index");

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
	node->ident.name = MarkedLexeme(self);

	return node;
}

//lookahead might strip the tilde from the token channel
static expr *RecRvar(parser *self, bool seen_tilde)
{
	assert(self);
	assert(self->tok.type == _IDENTIFIER);

	expr *node = ExprInit(self, NODE_RVARLIT);

	node->line = self->tok.line;
	node->rvarlit.dist = MarkedLexeme(self);

	if (!seen_tilde) {
		move_check(self, _TILDE, "missing '~' after distribution");
	}

	GetNextValidToken(self);

	node->rvarlit.args = RecArguments(self);

	return node;
}

static vector(Expr) RecArguments(parser *self)
{
	assert(self);
	assert(self->tok.type == _LEFTPAREN);

	vector(Expr) vec = {0};
	const size_t vec_capacity = 4;
	alloc_mark_vector(ExprVectorInit, vec, 0, vec_capacity);

	GetNextValidToken(self);

	while (self->tok.type != _RIGHTPAREN) {
		if (vec.len > 0) {
			check_move(self, _COMMA, "missing ',' after arg");
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
	alloc_mark_vector(IndexVectorInit, node->arraylit.indicies, 0,idx_cap);

	const size_t expr_cap = 4;
	alloc_mark_vector(ExprVectorInit, node->arraylit.values, 0, expr_cap);

	GetNextValidToken(self);

	while (self->tok.type != _RIGHTBRACKET) {
		if (node->arraylit.values.len > 0) {
			check_move(self, _COMMA, "missing ',' after value");
		}

		if (self->tok.type == _LEFTBRACKET) {
			move_check(self, _LITERALINT, tagerr);

			intmax_t idx = ExtractArrayIndex(self);
			IndexVectorPush(&node->arraylit.indicies, idx);

			move_check_move(self, _RIGHTBRACKET, closeerr);
			check_move(self, _EQUAL, eqerr);
		} else {
			IndexVectorPush(&node->arraylit.indicies, -1);
		}

		ExprVectorPush(&node->arraylit.values, RecAssignment(self));
	}

	GetNextValidToken(self);

	assert(node->arraylit.indicies.len == node->arraylit.values.len);

	return node;
}
