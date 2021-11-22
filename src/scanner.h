// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Compiler phase 1: lexical analysis: source code -> scanner -> tokens

#pragma once

#include "xerror.h"
#include "channel.h"
#include "str.h"

typedef enum token_type {
	//markers
	_INVALID = 0,
	_EOF,
	_IDENTIFIER,

	//atomic literals
	_LITERALINT,
	_LITERALFLOAT,
	_LITERALSTR,

	//punctuation
	_SEMICOLON,	// ;
	_LEFTBRACKET,	// [
	_RIGHTBRACKET,	// ]
	_LEFTPAREN,	// (
	_RIGHTPAREN,	// )
	_LEFTBRACE,	// {
	_RIGHTBRACE,	// }
	_DOT,		// .
	_TILDE,		// ~
	_COMMA,		// ,
	_COLON,		// :

	//operators
	_EQUAL,		// =
	_EQUALEQUAL,	// ==
	_NOTEQUAL,	// !=
	_NOT,		// !
	_AND,		// &&
	_OR,		// ||
	_BITNOT,	// '
	_AMPERSAND,	// &
	_BITOR,		// |
	_BITXOR,	// ^
	_LSHIFT,	// <<
	_RSHIFT,	// >>
	_GREATER,	// >
	_GEQ,		// >=
	_LESS,		// <
	_LEQ,		// <=
	_ADD,		// +
	_MINUS,		// -
	_STAR,		// *
	_DIV,		// /
	_MOD,		// %

	//sequence point keywords; module level
	_STRUCT,
	_FUNC,
	_METHOD,
	_LET,

	//sequence point keywords; block level
	_RETURN,
	_BREAK,
	_CONTINUE,
	_GOTO,
	_FOR,
	_WHILE,
	_IF,
	_SWITCH,
	
	//regular keywords
	_ELSE,
	_CASE,
	_DEFAULT,
	_FALLTHROUGH,
	_LABEL,
	_MUT,
	_NULL,
	_TRUE,
	_FALSE,
	_PUB,
	_SELF,
	_VOID,
	_IMPORT,

	//total
	_TOKEN_TYPE_COUNT
} token_type;

typedef struct token_flags {
	unsigned int valid : 1;
	unsigned int bad_string : 1; //valid when type == _LITERALSTR
} token_flags;

typedef struct token {
	struct {
		char *view; //pointer into in-memory source code, or null
		size_t len; //may be zero
	} lexeme;
	token_type type;
	size_t line; //starts at 1
	token_flags flags;
} token;

#define INVALID_TOKEN (token) {{NULL, 0}, _INVALID, 0, {0, 0}}

//------------------------------------------------------------------------------
//Tokens are sent on the channel in the order that they are found. On completion
//a final _EOF token is sent and the channel is closed.

make_channel(token, Token, static)

//------------------------------------------------------------------------------
//Execute lexical analysis in a new detached thread. The input channel must be
//initialised prior to this call and not freed until the final _EOF is received.
//The scanner has the exclusive right to close the channel. On failure XETHREAD
//is returned.

xerror ScannerInit(const cstring *src, Token_channel *chan);
