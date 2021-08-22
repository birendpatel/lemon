/**
 * @file xerror.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Error handling implementation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "xerror.c"

#define BUF_CAPACITY ((uint8_t) 64)
#define STR_CAPACITY ((uint8_t) 64)

//xqueue is an in-memory error buffer.
//It is a queue data structure but it does not need head and tail pointers
//because the only push-pop operation is xerror_flush, which wipes out all of
//the buffer contents at once.
static struct xqueue {
	pthread_mutex_t mutex;
	char buf[BUF_CAPACITY][STR_CAPACITY];
	uint8_t len;
} xq = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.buf = {0},
	.len = 0
};

//level codes lookup
static const char *level_lookup[] = {
	[XFATAL] = "FATAL",
	[XERROR] = "ERROR",
	[XTRACE] = "TRACE"
};

const char *get_level_name(int level)
{
	if (level >= XFATAL && level <= XTRACE) {
		return level_lookup[level];
	}

	return "N/A";
}

//error codes lookup
static const char *error_lookup[] = {
	[XESUCCESS]   = "function terminated successfully",
	[XENOMEM]     = "dynamic allocation failed",
	[XEOPTION]    = "options parsing failed",
	[XEFULL]      = "container is at capacity",
	[XEFILE]      = "IO failure",
	[XEBUSY]      = "thread waiting on condition",
	[XECLOSED]    = "attempted to use closed channel",
	[XETHREAD]    = "multithreading failure",
	[XEUNDEFINED] = "unspecified error"
};

const char *xerror_str(xerror err)
{
	if (err >= XESUCCESS && err <= XEUNDEFINED) {
		return error_lookup[err];
	}

	return "no error description available";
}

//lemon will never flush manually during compilation or in any spawned threads.
//in general, manual flushes only occur in main.c outside of any compilation
//context.
void xerror_flush(void)
{
	//don't check lock return code. The mutex was initialized statically
	//as a fast variant, so it won't fail with EINVAL or EDEADLK.
	(void) pthread_mutex_lock(&xq.mutex);

	for (uint8_t i = 0; i < xq.len; i++) {
		fprintf(stderr, buf[i];
	}

	xq.len = 0;

	(void) pthread_mutex_unlock(&xq.mutex);
}

//logger doesn't check for printf errors. If such an error occurs then the
//the logger will fail silently. At that point, the application program is
//still able to pass error codes up the call chain, so the program will at
//least exit with a status code.
void __xerror_log
(
	const char *file,
	const char *func,
	const int line,
	const int level,
	const char *msg,
	...
)
{
	assert(file);
	asesrt(func);
	assert(line >= 0);
	assert(level >= XFATAL && level <= XTRACE);
	assert(msg);

	va_list args;
	va_start(args, msg);

	const char *level_name = get_level_name(level);

	pthread_mutex_lock(&xq.mutex);

	pthread_mutex_unlock(&xq.mutex);

	va_end(args);
}
