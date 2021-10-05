// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "scanner.h"
#include "defs.h"
#include "assets/kmap.h"
#include "lib/channel.h"

typedef struct scanner {
	token_channel *chan;
	char *src;
	char *pos; //current byte being analysed
	char *curr; //helps process multi-char lexemes in tandem with pos
	uint32_t line; //current line in source code; starts at 1
	token tok; //temp workspace
} scanner;

static void* StartRoutine(void *data);
static void Scan(scanner *self);
static const string GetTokenName(token_type type);
static void Consume(scanner *self, token_type type, uint32_t n);
static void ConsumeIfPeek(scanner *self, char next, token_type a, token_type b);
static void ConsumeComment(scanner *self);
static void ConsumeNumber(scanner *self);
static void ConsumeString(scanner *self);
static void ConsumeSpace(scanner *self)
static void ConsumeInvalid(scanner *self, uint8_t flags);
static void ConsumeIdentOrKeyword(scanner *self);
static size_t GetIdentOrKeywordLength(scanner *self)
static size_t Synchronize(scanner *self);
static char Peek(scanner *self);
static bool IsLetterDigit(char ch);
static bool IsLetter(char ch);
static bool IsSpaceEOF(char ch);
static void SendToken(scanner *self);
static void SendEOF(scanner *self);

static const string GetTokenName(token_type type)
{
	STRING_TABLE_BEGIN
		STRING_TABLE_ENTRY(_INVALID, "INVALID")
		STRING_TABLE_ENTRY(_EOF, "EOF")
		STRING_TABLE_ENTRY(_IDENTIFIER, "IDENTIFIER")
		STRING_TABLE_ENTRY(_LITERALINT, "INT LITERAL")
		STRING_TABLE_ENTRY(_LITERALFLOAT, "FLOAT LITERAL")
		STRING_TABLE_ENTRY(_LITERALSTR, "STRING LITERAL")
		STRING_TABLE_ENTRY(_SEMICOLON, "SEMICOLON")
		STRING_TABLE_ENTRY(_LEFTBRACKET, "LEFTBRACKET")
		STRING_TABLE_ENTRY(_RIGHTBRACKET, "RIGHTBRACKET")
		STRING_TABLE_ENTRY(_LEFTPAREN, "LEFT PARENTHESIS")
		STRING_TABLE_ENTRY(_RIGHTPAREN, "RIGHT PARENTHESIS")
		STRING_TABLE_ENTRY(_LEFTBRACE, "LEFT BRACE")
		STRING_TABLE_ENTRY(_RIGHTBRACE, "RIGHT BRACE")
		STRING_TABLE_ENTRY(_DOT, "DOT")
		STRING_TABLE_ENTRY(_TILDE, "TILDE")
		STRING_TABLE_ENTRY(_COMMA, "COMMA")
		STRING_TABLE_ENTRY(_COLON, "COLON")
		STRING_TABLE_ENTRY(_EQUAL, "EQUAL")
		STRING_TABLE_ENTRY(_EQUALEQUAL, "EQUAL EQUAL")
		STRING_TABLE_ENTRY(_NOTEQUAL, "NOT EQUAL")
		STRING_TABLE_ENTRY(_NOT, "NOT")
		STRING_TABLE_ENTRY(_AND, "AND")
		STRING_TABLE_ENTRY(_OR, "OR")
		STRING_TABLE_ENTRY(_BITNOT, "BITWISE NOT")
		STRING_TABLE_ENTRY(_AMPERSAND, "AMPERSAND")
		STRING_TABLE_ENTRY(_BITOR, "BITWISE OR")
		STRING_TABLE_ENTRY(_BITXOR, "BITWISE XOR")
		STRING_TABLE_ENTRY(_LSHIFT, "LEFT SHIFT")
		STRING_TABLE_ENTRY(_RSHIFT, "RIGHT SHIFT")
		STRING_TABLE_ENTRY(_GREATER, "GREATER THAN")
		STRING_TABLE_ENTRY(_LESS, "LESS THAN")
		STRING_TABLE_ENTRY(_GEQ, "GREATER OR EQUAL")
		STRING_TABLE_ENTRY(_LEQ, "LESS OR EQUAL")
		STRING_TABLE_ENTRY(_ADD, "ADD")
		STRING_TABLE_ENTRY(_MINUS, "MINUS")
		STRING_TABLE_ENTRY(_STAR, "STAR")
		STRING_TABLE_ENTRY(_DIV, "DIVISION")
		STRING_TABLE_ENTRY(_MOD, "MODULO")
		STRING_TABLE_ENTRY(_FOR, "FOR")
		STRING_TABLE_ENTRY(_WHILE, "WHILE LOOP")
		STRING_TABLE_ENTRY(_BREAK, "BREAK")
		STRING_TABLE_ENTRY(_CONTINUE, "CONTINUE")
		STRING_TABLE_ENTRY(_IF, "IF BRANCH")
		STRING_TABLE_ENTRY(_ELSE, "ELSE BRANCH")
		STRING_TABLE_ENTRY(_SWITCH, "SWITCH")
		STRING_TABLE_ENTRY(_CASE, "CASE")
		STRING_TABLE_ENTRY(_DEFAULT, "DEFAULT")
		STRING_TABLE_ENTRY(_FALLTHROUGH, "FALLTHROUGH")
		STRING_TABLE_ENTRY(_GOTO, "GOTO")
		STRING_TABLE_ENTRY(_LABEL, "LABEL")
		STRING_TABLE_ENTRY(_LET, "LET")
		STRING_TABLE_ENTRY(_MUT, "MUTABLE")
		STRING_TABLE_ENTRY(_STRUCT, "STRUCT")
		STRING_TABLE_ENTRY(_IMPORT, "IMPORT")
		STRING_TABLE_ENTRY(_SELF, "SELF")
		STRING_TABLE_ENTRY(_FUNC, "FUNCTION")
		STRING_TABLE_ENTRY(_PUB, "PUBLIC")
		STRING_TABLE_ENTRY(_PRIV, "PRIVATE")
		STRING_TABLE_ENTRY(_RETURN, "RETURN")
		STRING_TABLE_ENTRY(_VOID, "VOID")
		STRING_TABLE_ENTRY(_NULL, "NULL")
		STRING_TABLE_ENTRY(_TRUE, "TRUE")
		STRING_TABLE_ENTRY(_FALSE, "FALSE")
	STRING_TABLE_END

	return STRING_TABLE_FETCH(type, "LOOKUP ERROR");
}

void TokenPrint(token tok, FILE *stream)
{
	const string lexfmt = "TOKEN { line %-10d: %-20s: %.*s }\n";
	const string nolexfmt = "TOKEN { line %-10d: %-20s }\n";

	const uint32_t line = tok.line;
	const string name = GetTokenName(tok.type);
	const size_t len = tok.lexeme.len;
	const char *data = tok.lexeme.data;

	if (data) {
		fprintf(stream, lexfmt, line, name, len, data);
	} else {
		fprintf(stream, nolexfmt, line, name);
	}
}

xerror ScannerInit(options *opt, string src, token_channel *chan)
{
	assert(opt);
	assert(src);
	assert(chan);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	scanner *scn = NULL;
	kmalloc(scn, sizeof(scanner));

	scn->chan = chan;
	scn->src = src;
	scn->pos = src;
	scn->curr = NULL;
	scn->line = 1;
	scn->tok = {0};

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

static void *StartRoutine(void *data)
{
	assert(data);

	scanner *self = (scanner *) data;

	Scan(self);

	free(self);

	pthread_exit(NULL);

	return NULL;
}

static void Scan(scanner *self)
{
	assert(self);

	while (true) {
		switch (*self->pos) {
		case '\0':
			goto exit;

		case '\t' ... '\r':
			fallthrough;

		case ' ':
			ConsumeSpace(self);
			break;

		case '#':
			ConsumeComment(self);
			break;

		case ';':
			Consume(self, _SEMICOLON, 1);
			break;

		case '[':
			Consume(self, _LEFTBRACKET, 1);
			break;

		case ']':
			Consume(self, _RIGHTBRACKET, 1);
			break;

		case '(':
			Consume(self, _LEFTPAREN, 1);
			break;

		case ')':
			Consume(self, _RIGHTPAREN, 1);
			break;

		case '{':
			Consume(self, _LEFTBRACE, 1);
			break;

		case '}':
			Consume(self, _RIGHTBRACE, 1);
			break;

		case '.':
			Consume(self, _DOT, 1);
			break;

		case '~':
			Consume(self, _TILDE, 1);
			break;

		case ',':
			Consume(self, _COMMA, 1);
			break;

		case ':':
			Consume(self, _COLON, 1);
			break;

		case '*':
			Consume(self, _STAR, 1);
			break;

		case '\'':
			Consume(self, _BITNOT, 1);
			break;

		case '^':
			Consume(self, _BITXOR, 1);
			break;

		case '+':
			Consume(self, _ADD, 1);
			break;

		case '-':
			Consume(self, _MINUS, 1);
			break;

		case '/':
			Consume(self, _DIV, 1);
			break;

		case '%':
			Consume(self, _MOD, 1);
			break;

		case '=':
			ConsumeIfPeek(self, '=', _EQUALEQUAL, _EQUAL);
			break;

		case '!':
			ConsumeIfPeek(self, '=', _NOTEQUAL, _NOT);
			break;

		case '&':
			ConsumeIfPeek(self, '&', _AND, _AMPERSAND);
			break;

		case '|':
			ConsumeIfPeek(self, '|', _OR, _BITOR);
			break;

		case '<':
			if (Peek(self) == '<') {
				Consume(self, _LSHIFT, 2);
			} else {
				ConsumeIfPeek(self, '=', _LEQ, _LESS);
			}

			break;

		case '>':
			if (Peek(self) == '>') {
				Consume(self, _RSHIFT, 2);
			} else {
				ConsumeIfPeek(self, '=', _GEQ, _GREATER);
			}

			break;

		case '0' ... '9':
			ConsumeNumber(self);
			break;

		case '"':
			ConsumeString(self);
			break;

		case 'A' ... 'Z':
			fallthrough;

		case 'a' ... 'z':
			fallthrough;

		case '_':
			ConsumeIdentOrKeyword(self);
		
		default:
			ConsumeInvalid(self, TOKEN_OKAY);
		}
	}

exit:
	SendEOF(self);
	token_channel_close(self->chan);
	return;
}

static void SendToken(scanner *self)
{
	assert(self);
	assert(self->chan->flags & CHANNEL_OPEN);

	(void) token_channel_send(self->chan, self->tok);
}

static void SendEOF(scanner *self)
{
	assert(self);

	self->tok = (token) {
		.lexeme = {
			.data = NULL,
			.len = 0
		},
		.type = _EOF,
		.line = self->line,
		.flags = TOKEN_OKAY
	};

	SendToken(self);
}

static void ConsumeIdentOrKeyword(scanner *self)
{
	assert(self);
	assert(IsLetter(*self->pos));
	
	const size_t word_length = GetIdentOrKeywordLength(self);

	const kv_pair *kv = kmap_lookup(self->pos, word_length);

	self->tok = (token) {
		.lexeme = {
			.data = self->pos,
			.len = word_length
		},
		.type = kv ? kv->type : _IDENTIFER,
		.line = self->line,
		.flags = TOKEN_OKAY
	};

	SendToken(self);
	
	self->pos = self->curr;
}

static size_t GetIdentOrKeywordLength(scanner *self)
{
	assert(self);

	self->curr = self->pos + 1;

	while (true) {
		if (IsLetterDigit(*self->curr)) {
			self->curr++;
		} else {
			break;
		}
	}

	const ptrdiff_t len = self->curr - self->pos;

	assert(len > 0 && "curr pointer is traveling backward");

	return (size_t) len;
}

static bool IsLetterDigit(char ch)
{
	return IsLetter(ch) || isdigit(ch);
}

static bool IsLetter(char ch)
{
	return isalpha(ch) || ch == '_';
}

static bool IsSpaceEOF(char ch)
{
	return isspace(ch) || ch == '\0';
}

static void Consume(scanner *self, token_type type, uint32_t n)
{
	assert(self);
	assert(n > 0);
	assert(type < _TOKEN_TYPE_COUNT);

	self->tok = (token) {
		.lexeme = {
			.data = self->pos,
			.len = n
		},
		.type = type,
		.line = self->line,
		.len = n,
		.flags = TOKEN_OKAY
	};

	SendToken(self);
	self->pos += n;
}

static void ConsumeInvalid(scanner *self, uint8_t flags)
{
	assert(self);

	char *start = self->pos;

	//synchronization implies that valid chars will be lost if they are
	//next to the invalid char without intermediate whitespace. Any grammar
	//errors in the lost valid region won't be detected in later compiler
	//passes until the invalid token is rectified.
	size_t total_invalid_chars = Synchronize(self);

	self->tok = (token) {
		.lexeme = {
			.data = start,
			.len = total_invalid_chars
		},
		.type = _INVALID,
		.line = self->line,
		.flags = flags
	};

	SendToken(self);
}

static void ConsumeSpace(scanner *self)
{
	assert(self);

	if (*self->pos == '\n') {
		self->line++;
	}

	self->pos++;
}

static void ConsumeIfPeek(scanner *self, char next, token_type a, token_type b)
{
	assert(self);
	assert(next);
	assert(a < _TOKEN_TYPE_COUNT && a != _INVALID);
	assert(b < _TOKEN_TYPE_COUNT && b != _INVALID);

	char ch = Peek(self);

	assert(ch);

	if (ch == next) {
		Consume(self, a, 2);
	} else {
		Consume(self, b, 1);
	}
}

static void ConsumeComment(scanner *self)
{
	assert(self);
	assert(*self->pos == '#');

	char next = '\0';

	do {
		next = *(++self->pos);
	} while (next != '\0' && next != '\n');
}

//This function is a weak consumer and will stop early at the first
//sight of a non-digit. For example, 3.14e3 will be scanned as two tokens;
//a float 3.14 and an identifier e3.
static void ConsumeNumber(scanner *self)
{
	assert(self);

	bool seen_dot = false;
	uint32_t guess = _LITERALINT;
	ptrdiff_t delta = 0;
	self->curr = self->pos + 1;

	while(true) {
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
		.lexeme = {
			.data = self->pos,
			.len = (size_t) delta
		},
		.type = guess,
		.line = self->line,
		.flags = TOKEN_OKAY
	};

	SendToken(self);
	self->pos = self->curr;
}

//advance scanner to the next whitespace or null char. return total characters
//consumed.
static size_t Synchronize(scanner *self)
{
	assert(self);

	size_t total_consumed = 0;

	while (!IsSpaceEOF(*self->pos)) {
		total_consumed++;
		self->pos++;
	}

	return total_consumed;
}

static void ConsumeString(scanner *self)
{
	assert(self);
	assert(*self->pos == '"');

	self->curr = self->pos + 1;

	while (*self->curr != '"') {
		if (*self->curr == '\0') {
			ConsumeInvalid(self, TOKEN_BAD_STR);
			self->pos = self->curr;
			return;
		}

		self->curr++;
	}

	//-1 to remove terminating quotation mark
	size_t delta = (self->curr - self->pos) - 1;

	self->tok = (token) {
		.lexeme = {
			.data = delta ? self->pos + 1 : NULL,
			.len = delta
		},
		.type = _LITERALSTR,
		.line = self->line,
		.flags = TOKEN_OKAY
	};

	SendToken(self);
	self->pos = self->curr + 1;
}

static char Peek(scanner *self)
{
	assert(self);
	assert(*self->pos != '\0' && "attempted buffer over-read");

	return *(self->pos + 1);
}
