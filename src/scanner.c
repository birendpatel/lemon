/**
 * @file scanner.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Scanner implementation.
 */

#include <assert.h>
#include <pthread.h> //mutex, attr, create, detach
#include <stdbool.h>
#include <stdio.h> //fprintf
#include <stdlib.h> //malloc

#include "scanner.h"
#include "defs.h" //kib
#include "lib/channel.h"

static void* start_routine(void *data);
static void scan(scanner *self);
static const char *get_token_name(token_type typ);

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
	[_DOT] = "DOT",
	[_TILDE] = "TILDE",
	[_COMMA] = "COMMA",
	[_STAR] = "STAR",
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
	[_MULT] = "MULTIPLICATION",
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

void token_print(token tok)
{
	static const char *lexfmt = "TOKEN { line %-10d: %-17s: %.*s }\n";
	static const char *nolexfmt = "TOKEN { line %-10d: %-17s }\n";
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
 * 	first byte in the lexeme while pos advances forwards.
 ******************************************************************************/
struct scanner {
	pthread_t tid;
	pthread_mutex_t mutex;
	token_channel *const chan;
	char *src;
	uint32_t line;
	uint32_t pos;
	uint32_t curr;
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
	tmp->tok = (token) { NULL, 0, 0, 0, 0 };
	tmp->line = 1;
	tmp->pos = 0;
	tmp->curr = 0;
	
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

	pthread_mutex_unlock(&self->mutex);
	pthread_exit(NULL);
	return NULL;
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
	//placeholder testing
	self->tok = (token) { "hello", _IDENTIFIER, 42, 5, 123456};

	(void) token_channel_send(self->chan, self->tok);

	self->tok = (token) { NULL, _EOF, 1, 2, 3 };

	(void) token_channel_send(self->chan, self->tok);
}
