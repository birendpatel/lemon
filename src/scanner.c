/**
 * @file scanner.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Scanner implementation.
 */

#include "scanner.h"
#include "lib/channel.h"

make_channel(token, token, static inline)

//scanner entry point; configures a new scanner in the spawned thread and
//processes the source code ASAP.
void *scanner_spawn(void *data)
{
	assert(data);

	payload *pl = (payload *) data;

	char *src = pl->src;
	token_channel *chan = pl->chan;

	token t = {SEMICOLON};

	token_channel_send(chan, t);

	token_channel_close(chan);
	
	puts(src);

	return NULL;
}

xerror token_print(FILE *stream, token t)
{
	fprintf(stream, "token: %d\n", t.type);
	return XESUCCESS;
}
