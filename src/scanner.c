// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.

#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "scanner.h"
#include "defs.h"
#include "assets/kmap.h"
#include "lib/channel.h"

static void* StartRoutine(void *data);
static void scan(scanner *self);
static const char *GetTokenName(token_type typ);
static void EndRoutine(scanner *self);
static void consume(scanner *self, token_type typ, uint32_t n);
static void consume_ifpeek(scanner *self, char next, token_type a, token_type b);
static char peek(scanner *self);
static void consume_comment(scanner *self);
static void consume_number(scanner *self);
static void consume_string(scanner *self);
static uint32_t synchronize(scanner *self);
static inline void consume_invalid(scanner *self);
static void consume_ident_kw(scanner *self);
static bool IsLetterDigit(char ch);
static bool IsLetter(char ch);
static bool IsSpaceEOF(char ch);
static void make_id_or_kw_token(scanner *self, uint32_t len);

//busy-wait comm between parent and scanner thread
static _Atomic volatile bool signal = false;

static const char *GetTokenName(token_type typ)
{
	static const char *lookup[] = {
		[_INVALID] = "INVALID",
		[_EOF] = "EOF",
		[_IDENTIFIER] = "IDENTIFIER",
		[_LITERALINT] = "INT LITERAL",
		[_LITERALFLOAT] = "FLOAT LITERAL",
		[_LITERALSTR] = "STRING LITERAL",
		[_SEMICOLON] = "SEMICOLON",
		[_LEFTBRACKET] = "LEFT BRACKET",
		[_RIGHTBRACKET] = "RIGHT BRACKET",
		[_LEFTPAREN] = "LEFT PARENTHESIS",
		[_RIGHTPAREN] = "RIGHT PARENTHESIS",
		[_LEFTBRACE] = "LEFT BRACE",
		[_RIGHTBRACE] = "RIGHT BRACE",
		[_DOT] = "DOT",
		[_TILDE] = "TILDE",
		[_COMMA] = "COMMA",
		[_COLON] = "COLON",
		[_EQUAL] = "EQUAL",
		[_EQUALEQUAL] = "EQUAL EQUAL",
		[_NOTEQUAL] = "NOT EQUAL",
		[_NOT] = "NOT",
		[_AND] = "AND",
		[_OR] = "OR",
		[_BITNOT] = "BITWISE NOT",
		[_AMPERSAND] = "AMPERSAND",
		[_BITOR] = "BITWISE OR",
		[_BITXOR] = "BITWISE XOR",
		[_LSHIFT] = "LEFT SHIFT",
		[_RSHIFT] = "RIGHT SHIFT",
		[_GREATER] = "GREATER THAN",
		[_GEQ] = "GREATER OR EQUAL",
		[_LESS] = "LESS THAN",
		[_LEQ] = "LESS OR EQUAL",
		[_ADD] = "ADD",
		[_MINUS] = "MINUS",
		[_STAR] = "STAR",
		[_DIV] = "DIVISION",
		[_MOD] = "MODULO",
		[_FOR] = "FOR LOOP",
		[_WHILE] = "WHILE LOOP",
		[_BREAK] = "BREAK",
		[_CONTINUE] = "CONTINUE",
		[_IF] = "IF BRANCH",
		[_ELSE] = "ELSE BRANCH",
		[_SWITCH] = "SWITCH",
		[_CASE] = "CASE",
		[_DEFAULT] = "DEFAULT",
		[_FALLTHROUGH] = "FALLTHROUGH",
		[_GOTO] = "GOTO",
		[_LABEL] = "LABEL",
		[_LET] = "LET",
		[_MUT] = "MUT",
		[_STRUCT] = "STRUCT",
		[_IMPORT] = "IMPORT",
		[_SELF] = "SELF",
		[_FUNC] = "FUNC",
		[_PRIV] = "PRIVATE",
		[_PUB] = "PUBLIC",
		[_RETURN] = "RETURN",
		[_VOID] = "VOID",
		[_NULL] = "NULL",
		[_TRUE] = "BOOL TRUE",
		[_FALSE] = "BOOL FALSE"
	};

	if (typ < _TOKEN_TYPE_COUNT) {
		return lookup[typ];
	}

	return "LOOKUP ERROR";
}

void TokenPrint(token tok, FILE *stream)
{
	const char *lexfmt = "TOKEN { line %-10d: %-20s: %.*s }\n";
	const char *nolexfmt = "TOKEN { line %-10d: %-20s }\n";

	const char *tokname = GetTokenName(tok.type);

	if (tok.lexeme) {
		fprintf(stream, lexfmt, tok.line, tokname, tok.len, tok.lexeme);
	} else {
		fprintf(stream, nolexfmt, tok.line, tokname);
	}
}

/*******************************************************************************
 * @struct scanner
 * @var scanner::chan
 * 	@brief Communication channel between scanner and main thread. See the
 * 	scanner_recv() documentation.
 * @var scanner::src
 * 	@brief Raw input source
 * @var scanner::tok
 * 	@brief Since the scanner only needs to process one token at a time, it
 * 	maintains this embedded token as its temporary workspace.
 * @var scanner::line
 * 	@brief The current line in the source code.
 * @var scanner::pos
 * 	@brief The position of the current byte being analysed by the scanner.
 * @var scanner::curr
 * 	@brief Used to help process multi-character lexemes in tandem with pos.
 ******************************************************************************/
struct scanner {
	token_channel *chan;
	char *src;
	char *pos;
	char *curr;
	uint32_t line;
	token tok;
};

xerror ScannerInit(options *opt, char *ssrc, token_channel *chan)
{
	assert(opt);
	assert(ssrc);
	assert(chan);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	scanner *scn = NULL;
	kmalloc(scn, sizeof(scanner));

	scn->chan = chan;
	scn->src = ssrc;
	scn->pos = ssrc;
	scn->curr = NULL;
	scn->tok = (token) { NULL, 0, 0, 0, 0 };
	scn->line = 1;

	pthread_t thread;
	int err = pthread_create(&thread, &attr, StartRoutine, scn);

	if (err) {
		xerror_issue("cannot create thread: pthread error: %d", err);
		return XETHREAD;	
	}

	if (opt->diagnostic & DIAGNOSTIC_THREAD) {
		xerror_trace("scanner running in detached thread");
		XerrorFlush();
	}

	return XESUCCESS;
}

/*******************************************************************************
 * @fn StartRoutine
 * @brief Entry point for threads initialized via pthread_create().
 ******************************************************************************/
static void *StartRoutine(void *data)
{
	assert(data);

	scanner *self = (scanner *) data;

	scan(self);

	EndRoutine(self);

	pthread_exit(NULL);

	return NULL;
}

/*******************************************************************************
 * @fn EndRoutine
 * @brief Exit call for scanner thread. Send EOF token and close channel.
 * @note It is undefined behavior in release mode to call this routine on a
 * closed channel.
 ******************************************************************************/
static void EndRoutine(scanner *self)
{
	assert(self);

	self->tok = (token) { NULL, _EOF, self->line, 0, 0};

	token_channel_send(self->chan, self->tok);

	token_channel_close(self->chan);

	free(self);
}

/*******************************************************************************
 * @fn scan
 * @brief This function initiates the actual lexical analysis once the scanner
 * and its thread are configured.
 ******************************************************************************/
static void scan(scanner *self)
{
	assert(self);
	assert(self->chan);
	assert(self->chan->flags == CHANNEL_OPEN);

	while (*self->pos) {
		switch (*self->pos) {
		//whitespace
		case '\n':
			self->line++;
			fallthrough;

		case ' ':
			fallthrough;

		case '\r':
			fallthrough;

		case '\t':
			fallthrough;

		case '\v':
			fallthrough;

		case '\f':
			self->pos++;
			break;

		case '#':
			consume_comment(self);
			break;

		//single symbols
		case ';':
			consume(self, _SEMICOLON, 1);
			break;

		case '[':
			consume(self, _LEFTBRACKET, 1);
			break;

		case ']':
			consume(self, _RIGHTBRACKET, 1);
			break;

		case '(':
			consume(self, _LEFTPAREN, 1);
			break;

		case ')':
			consume(self, _RIGHTPAREN, 1);
			break;

		case '{':
			consume(self, _LEFTBRACE, 1);
			break;

		case '}':
			consume(self, _RIGHTBRACE, 1);
			break;

		case '.':
			consume(self, _DOT, 1);
			break;

		case '~':
			consume(self, _TILDE, 1);
			break;

		case ',':
			consume(self, _COMMA, 1);
			break;

		case ':':
			consume(self, _COLON, 1);
			break;

		case '*':
			consume(self, _STAR, 1);
			break;

		case '\'':
			consume(self, _BITNOT, 1);
			break;

		case '^':
			consume(self, _BITXOR, 1);
			break;

		case '+':
			consume(self, _ADD, 1);
			break;

		case '-':
			consume(self, _MINUS, 1);
			break;

		case '/':
			consume(self, _DIV, 1);
			break;

		case '%':
			consume(self, _MOD, 1);
			break;

		//single or double symbols
		case '=':
			consume_ifpeek(self, '=', _EQUALEQUAL, _EQUAL);
			break;

		case '!':
			consume_ifpeek(self, '=', _NOTEQUAL, _NOT);
			break;

		case '&':
			consume_ifpeek(self, '&', _AND, _AMPERSAND);
			break;

		case '|':
			consume_ifpeek(self, '|', _OR, _BITOR);
			break;

		case '<':
			if (peek(self) == '<') {
				consume(self, _LSHIFT, 2);
			} else {
				consume_ifpeek(self, '=', _LEQ, _LESS);
			}

			break;

		case '>':
			if (peek(self) == '>') {
				consume(self, _RSHIFT, 2);
			} else {
				consume_ifpeek(self, '=', _GEQ, _GREATER);
			}

			break;

		//literals
		case '0':
			fallthrough;
		case '1':
			fallthrough;
		case '2':
			fallthrough;
		case '3':
			fallthrough;
		case '4':
			fallthrough;
		case '5':
			fallthrough;
		case '6':
			fallthrough;
		case '7':
			fallthrough;
		case '8':
			fallthrough;
		case '9':
			consume_number(self);
			break;

		case '"':
			consume_string(self);
			break;

		//identifiers and keywords
		default:
			if (IsLetter(*self->pos)) {
				consume_ident_kw(self);
			} else {
				consume_invalid(self);
			}
		}
	}
}

/*******************************************************************************
 * @fn consume_ident_kw
 * @brief Consume the current word as either a keyword or an identifier
 ******************************************************************************/
static void consume_ident_kw(scanner *self)
{
	assert(self);
	assert(IsLetter(*self->pos));

	self->curr = self->pos + 1;

	for (;;) {
		if (IsLetterDigit(*self->curr)) {
			self->curr++;
		} else {
			break;
		}
	}

	ptrdiff_t delta = self->curr - self->pos;
	assert(delta >= 0);

	make_id_or_kw_token(self, (uint32_t) delta);
	(void) token_channel_send(self->chan, self->tok);
	self->pos = self->curr;
}

//determine if the word with len chars at the current position is a keyword
//or an identifier, and construct its token. Like punctuation, we don't
//really need the lexeme for keywords but we capture them anyways (in case
//parser requirements change in the future or we add aliasing keywords).
static void make_id_or_kw_token(scanner *self, uint32_t len)
{
	assert(self);
	assert(len);

	self->tok.lexeme = self->pos;
	self->tok.line = self->line;
	self->tok.len = len;
	self->tok.flags = TOKEN_OKAY;

	const kv_pair *kv = kmap_lookup(self->pos, len);

	if (kv) {
		self->tok.type = kv->typ;
	} else {
		self->tok.type = _IDENTIFIER;
	}
}

static bool IsLetterDigit(char ch)
{
	return IsLetter(ch) || isdigit(ch);
}

static bool IsLetter(char ch)
{
	if (isalpha(ch) || ch == '_') {
		return true;
	}

	return false;
}

static bool IsSpaceEOF(char ch)
{
	if (isspace(ch) || ch == '\0') {
		return true;
	}

	return false;
}

/*******************************************************************************
 * @fn consume_invalid
 * @brief Consume an invalid token and attempt to recover the scanner to a valid
 position.
 ******************************************************************************/
static inline void consume_invalid(scanner *self)
{
	assert(self);

	char *start = self->pos;
	uint32_t n = synchronize(self);

	self->tok = (token) {
		.lexeme = start,
		.type = _INVALID,
		.line = self->line,
		.len = n,
		.flags = TOKEN_OKAY
	};

	(void) token_channel_send(self->chan, self->tok);
}

/*******************************************************************************
 * @fn consume
 * @brief Create a token for the next n characters at the current position and
 * advance the scanner to consume the lexeme.
 ******************************************************************************/
static void consume(scanner *self, token_type typ, uint32_t n)
{
	assert(self);
	assert(n > 0);
	assert(typ < _TOKEN_TYPE_COUNT);

	self->tok = (token) {
		.lexeme = self->pos,
		.type = typ,
		.line = self->line,
		.len = n,
		.flags = TOKEN_OKAY
	};

	(void) token_channel_send(self->chan, self->tok);

	self->pos += n;
}

/*******************************************************************************
 * @fn consume_ifpeek
 * @brief Peek the next character in the source. If it matches next, then
 * consume token a. Otherwise, consume token b.
 * @param a Token with a two-character lexeme
 * @param b Token with a one-character lexeme
 ******************************************************************************/
static void consume_ifpeek(scanner *self, char next, token_type a, token_type b)
{
	assert(self);
	assert(next);
	assert(a < _TOKEN_TYPE_COUNT && a != _INVALID);
	assert(b < _TOKEN_TYPE_COUNT && b != _INVALID);

	char ch = peek(self);

	assert(ch);

	if (ch == next) {
		consume(self, a, 2);
	} else {
		consume(self, b, 1);
	}
}

/*******************************************************************************
 * @fn consume_comment
 * @brief Throw away source characters until '\0' or '\n' is encountered.
 ******************************************************************************/
static void consume_comment(scanner *self)
{
	assert(self);

	self->pos++;

	while (*self->pos != '\0' && *self->pos != '\n') {
		self->pos++;
	}
}

/*******************************************************************************
 * @fn consume_number
 * @brief Tokenize a suspected number.
 * @details This function is a weak consumer and will stop early at the first
 * sight of a non-digit. For example, 3.14e3 will be scanned as two tokens;
 * a float 3.14 and an identifier e3.
 * @return If the string is not a valid float or integer form then an invalid
 * token will be sent with the TOKEN_BAD_NUM flag set.
 ******************************************************************************/
static void consume_number(scanner *self)
{
	assert(self);

	bool seen_dot = false;
	uint32_t guess = _LITERALINT;
	ptrdiff_t delta = 0;
	self->curr = self->pos + 1;

	for (;;) {
		if (*self->curr == '.') {
			if (seen_dot) {
				break;
			}

			guess = _LITERALFLOAT;
			seen_dot = true;
			self->curr++;
		} else if (isdigit(*self->curr)) {
			self->curr++;
		} else {
			break;
		}
	}

	delta = self->curr - self->pos;

	self->tok = (token) {
		.lexeme = self->pos,
		.type = guess,
		.line = self->line,
		.len = (uint32_t) delta,
		.flags = TOKEN_OKAY
	};

	(void) token_channel_send(self->chan, self->tok);
	self->pos = self->curr;
	return;
}

/*******************************************************************************
 * @fn synchronize
 * @brief Advance scanner position to the next whitespace or EOF token.
 * @returns Total characters consumed.
 ******************************************************************************/
static uint32_t synchronize(scanner *self)
{
	assert(self);

	uint32_t i = 0;

	while (!IsSpaceEOF(*self->pos)) {
		i++;
		self->pos++;
	}

	return i;
}

/*******************************************************************************
 * @fn consume_string
 * @brief Tokenize a suspected string.
 * @return If the string literal is not terminated with a quote then an invalid
 * token will be sent with the TOKEN_BAD_STR flag set.
 ******************************************************************************/
static void consume_string(scanner *self)
{
	assert(self);

	self->curr = self->pos + 1;

	while (*self->curr != '"') {
		if (*self->curr == '\0') {
			self->tok = (token) {
				.lexeme = NULL,
				.type = _INVALID,
				.line = self->line,
				.len = 0,
				.flags = TOKEN_BAD_STR
			};

			(void) token_channel_send(self->chan, self->tok);

			self->pos = self->curr;

			return;
		}

		self->curr++;
	}

	ptrdiff_t delta = (self->curr - self->pos) - 1;
	assert(delta >= 0 && "conversion to uint32 will fail");

	self->tok = (token) {
		.lexeme = self->pos + 1,
		.type = _LITERALSTR,
		.line = self->line,
		.len = (uint32_t) delta,
		.flags = TOKEN_OKAY
	};

	(void) token_channel_send(self->chan, self->tok);

	self->pos = self->curr + 1;
}

/*******************************************************************************
 * @fn peek
 * @brief Look at the next character in the source buffer but do not move to
 * its position.
 ******************************************************************************/
static char peek(scanner *self)
{
	assert(self);

	if (*self->pos == '\0') {
		return '\0';
	}

	return *(self->pos + 1);
}
