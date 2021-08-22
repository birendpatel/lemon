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

xerror parse(char *src)
{
	assert(src);

	xerror err = XESUCCESS;

	token_channel chan = {0};
	err = token_channel_init(&chan, KiB(1));

	if (err) {
		xerror_issue("cannot initialize channel");
		return err;
	}

	payload data = {src, &chan};
	pthread_t scanner_thread;
	err = pthread_create(&scanner_thread, NULL, scanner_spawn, &data);
	
	if (err) {
		xerror_issue("cannot create thread, pthread error: %d", err);
		return XETHREAD;
	}

	//debug
	token t = {0};
	while (err != XECLOSED) {
		err = token_channel_recv(&chan, &t);

		if (!err) {
			(void) token_print(stderr, t);
		}
	}

	err = pthread_join(scanner_thread, NULL);
	
	if (err) {
		xerror_issue("cannot join thread, pthread error: %d", err);
		return XETHREAD;
	}

	err = token_channel_free(&chan, NULL);

	if (err) {
		xerror_issue("cannot free channel");
		return err;
	}
	
	return err;
}
