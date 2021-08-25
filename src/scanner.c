/**
 * @file scanner.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Scanner implementation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h> //printer
#include <stdlib.h> //alloc

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
//embedded struct token as its temporary workspace.
struct scanner {
	pthread_t tid;
	token_channel *chan;
	char *src;
	token tok;
	uint32_t line;
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

	tmp->chan = malloc(sizeof(token_channel));

	if (!tmp->chan) {
		err = XENOMEM;
		xerror_issue("cannot allocate channel member");
		goto cleanup_scanner;
	}

	xerror err = token_channel_init(tmp->chan, KiB(1));

	if (err) {
		xerror_issue("cannot init the allocated channel");
		goto cleanup_chan;
	}

	tmp->src = src;
	tmp->tok = {NULL, 0, 0, 0, 0};
	tmp->line = 1; //match the fact that most IDEs start at line 1
	
	//From this point, in this parent thread, the scanner members can no
	//longer be read or written to since there is no top level mutex. The
	//only exception is the channel member via scanner_recv().
	//
	//The tmp pointer itself is passed along as the thread payload. In OOP
	//terms, it means this very instance of the scanner object will now
	//exist in tandem in both threads
	err = pthread_create(&tmp->tid, NULL, scanner_spawn, &tmp);

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
	free(tmp->chan);

cleanup_scanner:
	free(tmp);

fail:
	return err;
}

//------------------------------------------------------------------------------
static void* scanner_spawn(void *payload)
{
	scanner *self = (scanner *) payload;

	xerror_trace("scanner thread spawned");

	scan(self);

	return NULL;
}

//the actual lexical analysis of the target source code is initialized from this
//function.
static void scan(scanner *self)
{
	return;
}
