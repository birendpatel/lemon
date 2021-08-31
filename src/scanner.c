/**
 * @file scanner.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Scanner implementation.
 */

#include <assert.h>
#include <pthread.h> //mutex, attr, create, detach
#include <stdbool.h>
#include <stddef.h> //ptrdiff_t
#include <stdio.h> //fprintf
#include <stdlib.h> //malloc

#include "scanner.h"
#include "defs.h" //kib, fallthrough
#include "lib/channel.h"

static void* start_routine(void *data);
static void scan(scanner *self);
static const char *get_token_name(token_type typ);
static void end_routine(scanner *self);
static void consume(scanner *self, token_type typ, uint32_t n);
static void consume_ifpeek(scanner *self, char next, token_type a, token_type b);
static char peek(scanner *self);
static void consume_comment(scanner *self);
void consume_number(scanner *self);
void consume_string(scanner *self);
static uint32_t synchronize(scanner *self);
static inline void send_invalid(scanner *self);
static xerror send_id_or_kw(scanner *self);
static bool is_letter_digit(char ch);
static bool is_letter(char ch);
static bool is_digit(char ch);
static bool is_whitespace_eof(char ch);

make_channel(token, token, static inline)

static volatile bool signal = false;

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
	[_EQUAL] = "EQUAL",
	[_EQUALEQUAL] = "EQUAL EQUAL",
	[_NOTEQUAL] = "NOT EQUAL",
	[_NOT] = "NOT",
	[_AND] = "AND",
	[_OR] = "OR",
	[_BITNOT] = "BITWISE NOT",
	[_BITAND] = "BITWISE AND",
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
	[_FALLTHROUGH] = "FALLTHROUGH",
	[_GOTO] = "GOTO",
	[_LET] = "LET",
	[_MUT] = "MUT",
	[_STRUCT] = "STRUCT",
	[_FUNC] = "FUNC",
	[_PRIV] = "PRIVATE",
	[_PUB] = "PUBLIC",
	[_RETURN] = "RETURN"
};

static const char *get_token_name(token_type typ)
{
	static const char *err = "LOOKUP ERROR";

	if (typ < _TOKEN_TYPE_COUNT) {
		return lookup[typ];
	}

	return err;
}

//note: lexemes point to regions of the in-memory source code so they are not
//guaranteed to be null-terminated strings. Therefore we use the %.*s format
//directive.
void token_print(token tok)
{
	static const char *lexfmt = "TOKEN { line %-10d: %-20s: %.*s }\n";
	static const char *nolexfmt = "TOKEN { line %-10d: %-20s }\n";
	const char *tokname = get_token_name(tok.type);

	if (tok.lexeme) {
		fprintf(stderr, lexfmt, tok.line, tokname, tok.len, tok.lexeme);
	} else {
		fprintf(stderr, nolexfmt, tok.line, tokname);
	}
}

/*******************************************************************************
 * @struct scanner
 * @var scanner::tid
 * 	@brief Thread ID of the detached thread in which the scanner executes.
 * @var scanner::mutex
 * 	@brief Safeguard to prevent the main thread from prematurely freeing
 * 	scanner resources.
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
 * 	@brief For multi-character lexemes, curr saves the position of the
 * 	first byte in the lexeme while pos advances forwards. Curr is another
 * 	temporary workspace like tok, and its value can be overridden or reset
 * 	by any function call at any time.
 ******************************************************************************/
struct scanner {
	pthread_t tid;
	pthread_mutex_t mutex;
	token_channel *const chan;
	char *src;
	char *pos;
	char *curr;
	uint32_t line;
	token tok;
};

/*******************************************************************************
This is a simple function, despite its length, but it has many potential points
of failure due to the large amount of syscalls.

The initializer performs heap allocations across three levels of indirection:
- the pointer to the scanner
- the pointer to the channel within the scanner
- the pointer to the buffer within the channel

Note that we must cast the channel pointer to remove the const qualifier so that
we can assign the result of malloc. This does not violate the C standard section
6.7.3. The original object (the memory region given to the scanner) is not
a const object. The portion of the memory region reserved for the channel pointer
is merely casted to const. Since we are casting back to its original non-const
state, there is no standards violation.

Regardless, GCC's analyser will halt compilation since the build system uses
the -Werror flag. Therefore, we temporarily disable the -Wcast-qual diagnostic.

Note that once the scanner thread is spawned, the main thread can call the
scanner_free() function and acquire the mutex first. To prevent this we busy
wait until the scanner thread signals an 'okay' on the file scope signal.
*/

xerror scanner_init(scanner **self, char *src)
{
	assert(self);
	assert(src);

	xerror err = XESUCCESS;
	pthread_attr_t attr;
	int attr_err = 0;
	scanner *tmp;

	tmp =  malloc(sizeof(scanner));

	if (!tmp) {
		err = XENOMEM;
		xerror_issue("cannot allocate scanner");
		goto fail;
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

	* (token_channel **) &tmp->chan = malloc(sizeof(token_channel));

#pragma GCC diagnostic pop

	if (!tmp->chan) {
		err = XENOMEM;
		xerror_issue("cannot allocate channel member");
		goto cleanup_scanner;
	}

	err = token_channel_init(tmp->chan, KiB(1));

	if (err) {
		xerror_issue("cannot initialize channel");
		goto cleanup_channel;
	}

	(void) pthread_mutex_init(&tmp->mutex, NULL);

	err = pthread_attr_init(&attr);

	if (err) {
		xerror_issue("cannot init attr: pthread error: %d", err);
		err = XETHREAD;
		goto cleanup_channel_init;
	}

	err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (err) {
		xerror_issue("invalid detach state: pthread error %d", err);
		goto cleanup_attribute;
	}

	tmp->src = src;
	tmp->pos = src;
	tmp->curr = NULL;
	tmp->tok = (token) { NULL, 0, 0, 0, 0 };
	tmp->line = 1;

	err = pthread_create(&tmp->tid, &attr, start_routine, tmp);

	if (err) {
		xerror_issue("cannot create thread: pthread error: %d", err);
		goto cleanup_attribute;
	}

	*self = tmp;

cleanup_attribute:
	attr_err = pthread_attr_destroy(&attr);

	if (attr_err) {
		xerror_issue("cannot destroy attr: pthread error: %d", err);
	}

	if (!err) {
		goto success;
	}

	err = XETHREAD;

cleanup_channel_init:
	//don't check return code; func will never fail because no thread has
	//used the channel yet.
	(void) token_channel_free(tmp->chan, NULL);

cleanup_channel:
	free(tmp->chan);

cleanup_scanner:
	free(tmp);

fail:
	return err;

success:
	while (!signal) { /* busy wait */ }
	return XESUCCESS;
}

/*******************************************************************************
 * @fn start_routine
 * @brief Entry point for threads initialized via pthread_create().
 ******************************************************************************/
static void* start_routine(void *data)
{
	scanner *self = (scanner *) data;
	pthread_mutex_lock(&self->mutex);

	signal = true;

	scan(self);

	end_routine(self);

	pthread_mutex_unlock(&self->mutex);
	pthread_exit(NULL);
	return NULL;
}

/*******************************************************************************
 * @fn end_routine
 * @brief Exit call for scanner thread. Send EOF token and close channel.
 * @note It is undefined behavior in release mode to call this routine on a
 * closed channel.
 ******************************************************************************/
static void end_routine(scanner *self)
{
	assert(self);
	assert(self->chan);
	assert(self->chan->flags == CHANNEL_OPEN);

	self->tok = (token) { NULL, _EOF, self->line, 0, 0};

	(void) token_channel_send(self->chan, self->tok);

	token_channel_close(self->chan);
}

/*******************************************************************************
The top-level mutex here is a safeguard to prevent the main thread (the lexer)
from shutting down the scanner prematurely. The scanner refuses to unlock its
mutex until it has sent its EOF token.
*/
xerror scanner_free(scanner *self)
{
	assert(self);

	xerror err = XESUCCESS;

	pthread_mutex_lock(&self->mutex);

	err = token_channel_free(self->chan, NULL);

	if (err) {
		xerror_issue("cannot free channel");
		pthread_mutex_unlock(&self->mutex);
		return err;
	}

	free(self->chan);

	pthread_mutex_unlock(&self->mutex);

	free(self);

	return XESUCCESS;
}

/*******************************************************************************
This function bypasses the top-level mutex. Three reasons: 1) to reduce lock
overhead 2) the channel pointer is read-only so we know the scanner thread has
not retargeted it 3) the channel has its own thread safety. All operations on
on the channel never directly access a channel member for reads or writes
unless it is through a thread-safe channel function.

Granted, this function relies on the implementation details of channels and
causes a degree of function coupling. In return, the scan() function does not
constantly have to constantly deal with threading overhead every single time
it needs to modify its temporary workspace. The overhead is murder on the
runtime speed.

All of this relies on an implicit contract; the main thread promises to never
modify or read anything in the scanner except the channel. Since this is
the only unsafe function provided by the scanner in its API, the contract is
easy to verify.
*/
xerror scanner_recv(scanner *self, token *tok)
{
	assert(self);
	assert(self->chan);
	assert(tok);

	//the one and only mutex bypass in the entirety of the scanner
	//occurs in this function call.
	xerror err = token_channel_recv(self->chan, tok);

	if (err) {
		xerror_issue("channel is closed and empty");
	}

	return err;
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

	xerror err = XESUCCESS;

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
			consume_ifpeek(self, '&', _AND, _BITAND);
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
			err = send_id_or_kw(self);

			if (err) {
				send_invalid(self);
			}
		}
	}
}

/*******************************************************************************
 * @fn send_id_or_kw
 * @brief Send the current word if it is a valid identifier or keyword
 * @return XEUNDEFINED if the current word is not valid
 ******************************************************************************/
static xerror send_id_or_kw(scanner *self)
{
	assert(self);

	self->curr = self->pos;

	if (!is_letter(*self->curr)) {
		return XEUNDEFINED;
	}

	self->curr++;

	for (;;) {
		if (is_letter_digit(*self->curr)) {
			self->curr++;
		} else if (is_whitespace_eof(*self->curr)) {
			break;
		} else {
			return XEUNDEFINED;
		}
	}

	ptrdiff_t delta = self->curr - self->pos;

	self->tok = (token) {
		.lexeme = self->pos,
		.type = _IDENTIFIER,
		.line = self->line,
		.len = (uint32_t) delta,
		.flags = TOKEN_OKAY
	};

	(void) token_channel_send(self->chan, self->tok);

	self->pos = self->curr;

	return XESUCCESS;
}

//latin alphabet, underscores, zero thru nine
static bool is_letter_digit(char ch) 
{
	return is_letter(ch) || is_digit(ch);
}

//latin alphabet, underscores
static bool is_letter(char ch)
{
	if (ch == '_') {
		return true;
	}

	if (ch >= 65 && ch <= 90) {
		return true;
	}

	if (ch >= 97 && ch <= 122) {
		return true;
	}

	return false;
}

//zero thru nine
static bool is_digit(char ch)
{
	if (ch >= 48 && ch <= 57) {
		return true;
	}

	return false;
}

//form feed, carriage return, line feed, horizontal tab, vertical tab,
//space, null character
static bool is_whitespace_eof(char ch)
{
	if (ch == '\0') {
		return true;
	}

	if (ch == ' ') {
		return true;
	}

	if (ch >= 9 && ch <= 13) {
		return true;
	}

	return false;
}

/*******************************************************************************
 * @fn send_invalid
 * @brief Send an invalid token and synchronize scanner to the next word.
 ******************************************************************************/
static inline void send_invalid(scanner *self)
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
void consume_number(scanner *self)
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
		} else if (is_digit(*self->curr)) {
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

	while (!is_whitespace_eof(*self->pos)) {
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
void consume_string(scanner *self)
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
