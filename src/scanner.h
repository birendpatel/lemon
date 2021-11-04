// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The scanner API initializes lexical analysis in a separate detached thread.
// Tokens generated by the scanner are communicated to the parent thread by
// value via a thread-safe, buffered, and blocking FIFO queue. 

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

	//sequence point keywords
	_STRUCT,
	_FUNC,
	_METHOD,
	_LET,
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
	cstring *lexeme; //either dynamically allocated or NULL
	token_type type;
	size_t line; //starts at 1
	token_flags flags;
} token;

#define INVALID_TOKEN (token) {NULL, _INVALID, 0, {0, 0}}

//scanner sends tokens on this communication channel in the order that they
//are found. When lexical analysis is complete, the scanner sends a final _EOF
//token and then closes the channel.
make_channel(token, Token, static)

//initialize scanner in a new detached thread. The channel must be initialized 
//prior to this call and must not be freed until the final _EOF has been 
//received or else a segmentation violation may occur. If a thread other than 
//the scanner thread closes the channel, then the scanner thread will hang 
//itself. Returns XETHREAD on failure.
xerror ScannerInit(const cstring *src, Token_channel *chan);
