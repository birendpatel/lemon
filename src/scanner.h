// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The scanner API initializes lexical analysis in a separate detached thread.
// Tokens generated by the scanner are communicated to the parent thread by
// value via a thread-safe, buffered, and blocking FIFO queue provided by the
// parent.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "options.h"
#include "xerror.h"

#include "lib/channel.h"
#include "lib/str.h"

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

	//control flow
	_FOR,
	_WHILE,
	_BREAK,
	_CONTINUE,
	_IF,
	_ELSE,
	_SWITCH,
	_CASE,
	_DEFAULT,
	_FALLTHROUGH,
	_GOTO,
	_LABEL,

	//assignment
	_LET,
	_MUT,
	_NULL,
	_TRUE,
	_FALSE,

	//composition
	_STRUCT,
	_IMPORT,

	//procedures
	_FUNC,
	_PRIV,
	_PUB,
	_RETURN,
	_SELF,
	_VOID,

	//total
	_TOKEN_TYPE_COUNT
} token_type;

typedef struct token {
	view lexeme;
	token_type type;
	size_t line;
	uint8_t flags;
} token;

//token.flags
#define TOKEN_OKAY	0
#define TOKEN_BAD_STR	1 << 0 // ill-formed string literal

//scanner sends tokens on this communication channel in the order that they
//are found. When lexical analysis is complete, the scanner sends a final _EOF
//token and then closes the channel.
make_channel(token, token, static)

//initialized in a new detached thread.
//channel must be initialized prior to this call.
xerror ScannerInit(options *opt, string src, token_channel *chan);
