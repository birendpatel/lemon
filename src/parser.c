/**
 * @file parser.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Top-down operator precedence parser (aka the Pratt Parser).
 */

#include <pthread.h>

#include "parser.h"
#include "scanner.h"
#include "lib/channel.h"

make_channel(token, token, static inline)

lemon_error parse(char *src)
{
	assert(src);

	lemon_error err = LEMON_ESUCCESS;
	int perr = 0;

	token_channel chan = {0};
	err = token_channel_init(&chan, KiB(1));
	RETURN_ERROR(err, "cannot initialize channel", err);

	payload data = {src, &chan};
	pthread_t scanner_thread;
	perr = pthread_create(&scanner_thread, NULL, scanner_spawn, &data);
	RETURN_ERROR(perr, "cannot start new thread", LEMON_ETHREAD);

	//debug
	token t = {0};
	while (err != LEMON_ECLOSED) {
		err = token_channel_recv(&chan, &t);

		if (!err) {
			(void) token_print(stderr, t);
		}
	}

	perr = pthread_join(scanner_thread, NULL);
	RETURN_ERROR(perr, "scanner join failed", LEMON_ETHREAD);

	err = token_channel_free(&chan, NULL);
	RETURN_ERROR(err, "cannot free channel", err);
	
	return err;
}
