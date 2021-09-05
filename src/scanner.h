/**
 * @file scanner.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Scanner API.
 */

#pragma once

#include <stdint.h>

#include "options.h"
#include "xerror.h"

/*******************************************************************************
 * @enum token_type 
 * @brief Tagged union type indicator for the token struct.
 ******************************************************************************/
typedef enum token_type {
	//markers
	_INVALID = 0,
	_EOF,
	_IDENTIFIER,
	
	//literals
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

	//keywords, control flow
	_FOR,
	_WHILE,
	_BREAK,
	_CONTINUE,
	_IF,
	_ELSE,
	_SWITCH,
	_CASE,
	_FALLTHROUGH,
	_GOTO,

	//keywords, assignment
	_LET,
	_MUT,
	_NULL,
	_TRUE,
	_FALSE,
	
	//keywords, composition
	_STRUCT,

	//keywords, procedures
	_FUNC,
	_PRIV,
	_PUB,
	_RETURN,
	_SELF,
	_VOID,

	//token count
	_TOKEN_TYPE_COUNT,
} token_type;

/*******************************************************************************
 * @struct token
 * @brief Tokens are words generated by the scanner during its lexical analysis
 * of the raw source code.
 * @remark The token_type is specified as u32 to guarantee a packed struct.
 * @var token::lexeme
 * 	@brief Pointer into the input src provided on scanner_init
 * @var token::len
 * 	@brief Lexeme byte length
 * @var token::flags
 * 	@brief May contain set bits for any of the flags macro definitions.
 * @note shallow memcmp and memcpy are valid operations on this struct.
 ******************************************************************************/
typedef struct token {
	char *lexeme;
	uint32_t type;
	uint32_t line;
	uint32_t len;
	uint32_t flags;
} token;

//flag bits
#define TOKEN_OKAY	0
#define TOKEN_BAD_NUM	1 << 0 /**< @brief Ill-formed number literal */
#define TOKEN_BAD_STR	1 << 1 /**< @brief Ill-formed string literal */

/*******************************************************************************
 * @fn token_print
 * @brief Pretty printer to stderr
 ******************************************************************************/
void token_print(token tok);

/*******************************************************************************
 * @typedef scanner
 * @brief Since the scanner uses a channel, we make its representation opaque.
 * Otherwise we risk the parent thread accidentally modifying the channel
 * attributes without a mutex.
 ******************************************************************************/
typedef struct scanner scanner;

/*******************************************************************************
 * @fn scanner_init
 * @brief Initialize a scanner in a new thread
 ******************************************************************************/
xerror scanner_init(scanner **self, char *src, options *opt);

/*******************************************************************************
 * @fn scanner_recv
 * @brief Fetch a token, if any, from the front of the communication channel.
 * @details If no token is available and the channel is in a valid open state,
 * then the caller thread will suspend and wait until a token is available. The
 * wait period will not timeout. If the caller receives a token of type _EOF,
 * then this indicates and guarantees that the scanner will no longer send
 * tokens.
 ******************************************************************************/
xerror scanner_recv(scanner *self, token *tok);

/*******************************************************************************
 * @fn scanner_free
 * @brief Terminate the scanner thread, shutdown its communication channel, and
 * release the scanner resource itself.
 * @details This function may fail if the channel is busy.
 ******************************************************************************/
xerror scanner_free(scanner *self);
