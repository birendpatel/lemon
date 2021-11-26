// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "scanner.h"
#include "options.h"
#include "assets/kmap.h"
#include "lib/channel.h"

//------------------------------------------------------------------------------
//@pos current byte being analysed
//@curr used with pos to help process multi-char lexemes

typedef struct scanner scanner;

static void* StartRoutine(void *);
static void Scan(scanner *);
static void Consume(scanner *, token_type, size_t);
static void ConsumeIfPeek(scanner *, char, token_type, token_type);
static void ConsumeComment(scanner *);
static void ConsumeNumber(scanner *);
static void ConsumeString(scanner *);
static void ConsumeSpace(scanner *);
static void ConsumeInvalid(scanner *, token_flags);
static void ConsumeIdentOrKeyword(scanner *);
static size_t GetIdentOrKeywordLength(scanner *);
static size_t Synchronize(scanner *);
static char Peek(scanner *);
static bool IsLetterDigit(char);
static bool IsLetter(char);
static bool IsSpaceEOF(char);
static void SendToken(scanner *);
static void SendEOF(scanner *);
static const cstring *GetTokenName(token_type);
static void TokenPrint(scanner *);
static _Noreturn void Hang(void);

//------------------------------------------------------------------------------

struct scanner {
	Token_channel *chan;
	char *pos;
	char *curr;
	cstring *src;
	size_t line;
	token tok;
};

xerror ScannerInit(cstring *src, channel(Token) *chan)
{
	assert(src);
	assert(chan);

	//allocated in the parent arena. this avoids sending the src and chan
	//as a payload into the new thread, which means having to block the
	//parent thread until the data is copied. The scanner makes no other
	//dynamic allocations so this also mitigates the overhead of creating
	//a new arena.
	scanner *scn = allocate(sizeof(scanner));

	*scn = (scanner) {
		.chan = chan,
		.pos = src,
		.curr = NULL,
		.src = src,
		.line = 1,
		.tok = {
			.lexeme = {
				.view = NULL,
				.len = 0
			},
			.type = _INVALID,
			.line = 0,
			.flags = {
				.valid = 0,
				.bad_string = 0
			}
		},
	};

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thread;
	int err = pthread_create(&thread, &attr, StartRoutine, scn);

	if (err) {
		xerror_issue("cannot create thread: pthread error: %d", err);
		return false;
	}

	return true;
}

//pthread_init argument 
static void *StartRoutine(void *pthread_payload)
{
	scanner *self = (scanner *) pthread_payload;

	const bool trace = OptionsGetFlag(DIAGNOSTIC_MULTITHREADING);

	if (trace) {
		xerror_trace("scanner running in detached thread");
		XerrorFlush();
	}

	Scan(self);

	if (trace) {
		xerror_trace("scanner shutting down");
		XerrorFlush();
	}

	pthread_exit(NULL);

	return NULL;
}

static const cstring *GetTokenName(token_type type)
{
	static const cstring *lookup[] = {
		[_INVALID] = "INVALID",
		[_EOF] = "EOF",
		[_IDENTIFIER] = "IDENTIFIER",
		[_LITERALINT] = "INT LITERAL",
		[_LITERALFLOAT] = "FLOAT LITERAL",
		[_LITERALSTR] = "STRING LITERAL",
		[_SEMICOLON] = "SEMICOLON",
		[_LEFTBRACKET] = "LEFTBRACKET",
		[_RIGHTBRACKET] = "RIGHTBRACKET",
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
		[_LESS] = "LESS THAN",
		[_GEQ] = "GREATER OR EQUAL",
		[_LEQ] = "LESS OR EQUAL",
		[_ADD] = "ADD",
		[_MINUS] = "MINUS",
		[_STAR] = "STAR",
		[_DIV] = "DIVISION",
		[_MOD] = "MODULO",
		[_FOR] = "FOR",
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
		[_MUT] = "MUTABLE",
		[_STRUCT] = "STRUCT",
		[_IMPORT] = "IMPORT",
		[_SELF] = "SELF",
		[_FUNC] = "FUNCTION",
		[_METHOD] = "METHOD",
		[_PUB] = "PUBLIC",
		[_RETURN] = "RETURN",
		[_VOID] = "VOID",
		[_NULL] = "NULL",
		[_TRUE] = "TRUE",
		[_FALSE] = "FALSE",
	};

	if (type <= _INVALID || type >= _TOKEN_TYPE_COUNT) {
		return "LOOKUP ERROR";
	}

	return lookup[type];
}

static void TokenPrint(scanner *self)
{
	const cstring *lexfmt = "TOKEN { line %-10zu: %-20s: %.*s: %d %d }\n";
	const cstring *nolexfmt = "TOKEN { line %-10zu: %-20s: %d %d }\n";

	const size_t line = self->tok.line;
	const cstring *name = GetTokenName(self->tok.type);
	const cstring *view = self->tok.lexeme.view;
	const size_t len = self->tok.lexeme.len;
	const int valid = self->tok.flags.valid;
	const int badstr = self->tok.flags.bad_string;

	if (view) {
		fprintf(stderr, lexfmt, line, name, view, len, valid, badstr);
	} else {
		fprintf(stderr, nolexfmt, line, name, valid, badstr);
	}
}

//------------------------------------------------------------------------------

static void Scan(scanner *self)
{
	assert(self);

	const token_flags invalid_state = {
		.valid = 0,
		.bad_string = 0
	};

	while (true) {

/* enable switch range statements */
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wpedantic\"")

		switch (*self->pos) {
		case '\0':
			goto exit;

		case '\t' ... '\r':
			__attribute__((fallthrough));

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
			__attribute__((fallthrough));

		case 'a' ... 'z':
			__attribute__((fallthrough));

		case '_':
			ConsumeIdentOrKeyword(self);
			break;

		default:
			ConsumeInvalid(self, invalid_state);
		}

/* disable switch range statements */
_Pragma("GCC diagnostic pop")

	}

exit:
	SendEOF(self);
	TokenChannelClose(self->chan);
	return;
}

//------------------------------------------------------------------------------

static _Noreturn void Hang(void)
{
	for (;;) asm volatile ("") ;
}

static void SendToken(scanner *self)
{
	assert(self);
	assert(self->chan->flags & CHANNEL_OPEN);

	if (OptionsGetFlag(DIAGNOSTIC_LEXICAL_TOKENS)) {
		TokenPrint(self);
	}

	xerror err = TokenChannelSend(self->chan, self->tok);

	if (err) {
		xerror_fatal("cannot send token: %s", XerrorDescription(err));
		xerror_fatal("cannot fulfill EOF contract on token channel");
		xerror_fatal("hanging");
		Hang();
	}
}

static void SendEOF(scanner *self)
{
	assert(self);

	self->tok = (token) {
		.lexeme = {
			.view = NULL,
			.len = 0
		},
		.type = _EOF,
		.line = self->line,
		.flags = {
			.valid = 1,
		}
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
			.view = self->pos,
			.len = word_length
		},
		.type = kv ? kv->typ : _IDENTIFIER,
		.line = self->line,
		.flags = {
			.valid = 1,
		}
	};

	SendToken(self);

	self->pos = self->curr;
}

//on return self->curr is set to the first unscanned character
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

static void Consume(scanner *self, token_type type, size_t n)
{
	assert(self);
	assert(n > 0);
	assert(type < _TOKEN_TYPE_COUNT);

	self->tok = (token) {
		.lexeme = {
			.view = self->pos,
			.len = n
		},
		.type = type,
		.line = self->line,
		.flags = {
			.valid = 1,
		}
	};

	SendToken(self);
	self->pos += n;
}

static void ConsumeInvalid(scanner *self, token_flags flags)
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
			.view = start,
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
	token_type guess = _LITERALINT;
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
			.view = self->pos,
			.len = (size_t) delta
		},
		.type = guess,
		.line = self->line,
		.flags = {
			.valid = 1,
		}
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

//if the string is ill-formed an invalid token with the bad_string flag is sent.
//otherwise, a _LITERALSTR token is sent, but if the string is an empty string
//then the token lexeme view will be set to NULL.
static void ConsumeString(scanner *self)
{
	assert(self);
	assert(*self->pos == '"');

	self->curr = self->pos + 1;

	while (*self->curr != '"') {
		if (*self->curr == '\0') {
			const token_flags flags = {
				.valid = 0,
				.bad_string = 1
			};

			ConsumeInvalid(self, flags);
			self->pos = self->curr;
			return;
		}

		self->curr++;
	}

	//-1 to remove terminating quotation mark
	size_t delta = (size_t) ((self->curr - self->pos) - 1);

	self->tok = (token) {
		.lexeme = {
			.view = delta ? self->pos + 1 : NULL,
			.len = delta
		},
		.type = _LITERALSTR,
		.line = self->line,
		.flags = {
			.valid = 1,
			.bad_string = 0
		}
	};

	SendToken(self);
	self->pos = self->curr + 1;
}

static char Peek(scanner *self)
{
	assert(self);
	assert(*self->pos != '\0' && "buffer over-read");

	return *(self->pos + 1);
}
