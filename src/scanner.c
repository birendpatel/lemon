/**
 * @file scanner.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Scanner implementation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h> //printer
#include <stdlib.h> //alloc, exit
#include <string.h> //memcpy

#include "defs.h"
#include "lib/channel.h"
#include "scanner.h"

//scanner sends tokens by value across a thread-safe channel
make_channel(token, token, static inline)

//printing is used by certain diagnostic options flags
static const char *token_name_lookup[] = {
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

void token_print(token tok)
{
	static const char *lex_fmt = "line %-10d: %-17s: %.*s\n";
	static const char *nolex_fmt = "line %-10d: %-17s\n";
	char *tokname = "LOOKUP ERROR";

	if (tok.token_type >= _INVALID && tok.token_type < _TOKEN_TYPE_COUNT) {
		tokname = token_name_lookup[tok.token_type];
	}

	if (tok.lexeme) {
		fprintf(stderr, lex_fmt, tok.line, tokname, tok.len, tok.lexeme);
	} else {
		fprintf(stderr, nolex_fmt, tok.line, tokname);
	}
}

//since the scanner only needs to process one token at a time, it maintains an
//embedded struct token as its temporary workspace. This same token is passed
//by value to the channel when ready.
//
//the scanner acquires the top-level mutex when its thread is created and only
//unlocks it when tokenization is complete. This prevents the main thread from
//prematurely freeing scanner resources. The scanner thread runs detached.
//
//However, the chan itself (see the scanner_recv() implementation) bypasses 
//the mutex. One, because it reduces lock overhead, two because the chan ref
//is read-only, and three because chan resources are protected by their own 
//mutex and its struct is encapsulated enough to avoid direct R/W from either 
//thread.
struct scanner {
	pthread_t tid;
	pthread_mutex_t mutex;
	token_channel *const chan;
	char *src;
	token tok;
	uint32_t line;
	bool failed; //signal to parent thread on any issue
};

//the scanner needs to perform heap allocations across three levels of indirection:
// - the pointer to the scanner
// - the pointer to the channel within the scanner
// - the pointer to the buffer within the channel
xerror scanner_init(scanner **self, char *src)
{
	assert(self);
	assert(src);

	xerror err = XESUCCESS;

	scanner *tmp =  malloc(sizeof(scanner));

	if (!tmp) {
		err = XENOMEM;
		xerror_issue("cannot allocate scanner");
		goto fail;
	}

	//the read-only channel pointer is bypassed via memcpy.
	token_channel *tmp_chan = malloc(sizeof(token_channel));

	if (!tmp_chan) {
		err = XENOMEM;
		xerror_issue("cannot allocate channel member");
		goto cleanup_scanner;
	}

	memcpy(&(tmp->chan), &tmp_chan, sizeof(void *));

	xerror err = token_channel_init(tmp->chan, KiB(1));

	if (err) {
		xerror_issue("cannot init the allocated channel");
		goto cleanup_chan;
	}

	tmp->src = src;
	tmp->tok = {NULL, 0, 0, 0, 0};
	tmp->line = 1; //match the fact that most IDEs start at line 1
	tmp->failed = false;

	pthread_mutex_init(&tmp->mutex, NULL);

	pthread_attr_t attr;
	err = pthread_attr-setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (err) {
		xerror_issue("invalid detach state: pthread error %d", err);
		goto cleanup_thread;
	}
	
	//scanner needs to acquire the top-level mutex as soon as it can.
	//theoretically, the main thread could call scanner_free() before
	//the scanner thread can acquire its mutex. Practically, this is
	//this is a non-issue since the scanner signals task completion with
	//an EOF token. The top-level mutex is a basic safeguard against the
	//lexer but the lexer also  has too many safeguards of its own to
	//actually trigger the issue.
	err = pthread_create(&tmp->tid, &attr, scanner_spawn, &tmp);

	if (err) {
		xerror_issue("cannot create thread: pthread error: %d", err);
		goto cleanup_thread;
	}

	*self = tmp;

	return XESUCCESS;

cleanup_thread:
	//don't check error, function will always success because the thread
	//creation failed, so we already know its never been used.
	(void) token_channel_free(tmp, NULL);

cleanup_chan:
	free(tmp_chan);

cleanup_scanner:
	free(tmp);

fail:
	return err;
}

//entry point for the detached scanner thread
static void* scanner_spawn(void *payload)
{
	xerror err = XESUCCESS;

	scanner *self = (scanner *) payload;

	pthread_mutex_lock(&self->tid);

	scan(self);

	pthread_mutex_unlock(&self->tid);

	pthread_exit(NULL);

	return NULL;
}

//the actual lexical analysis of the target source code is initialized from this
//function.
static xerror scan(scanner *self)
{
	self->tok = {NULL, _EOF, 1, 2, 3}; //placeholder

	(void) token_channel_send(self->chan, self->tok);

	return XESUCCESS;
}
