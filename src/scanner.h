// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#include <stdint.h>
#include <stdio.h>

#include "options.h"
#include "xerror.h"
#include "lib/channel.h"

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

//token.lexeme is a string view of length token.len into the input source code.
//token.type is the enum token_type but it uses uint32_t to guarantee the
//struct packing behavior.
typedef struct token {
	char *lexeme;
	uint32_t type;
	uint32_t line;
	uint32_t len;
	uint32_t flags;
} token;

//token.flags
#define TOKEN_OKAY	0
#define TOKEN_BAD_NUM	1 << 0 // ill-formed number literal
#define TOKEN_BAD_STR	1 << 1 // ill-formed string literal

make_channel(token, token, static)

void TokenPrint(token tok, FILE *stream);

typedef struct scanner scanner;

//initialized in a new detached thread
xerror ScannerInit(options *opt, char *ssrc, token_channel *chan);
