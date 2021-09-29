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

#include "xerror.h"

#define BUFLEN ((uint8_t) 64)
#define STRLEN ((uint8_t) 128)

//xqueue is an in-memory queue. xq is global because it simplifies application
//code. There is no loss of safety because a) xqueue is thread safe b) xqueue
//is completely encapsulated.
struct xqueue {
	pthread_mutex_t mutex;
	char buf[BUFLEN][STRLEN];
	uint8_t len;
};

static struct xqueue xq = {
	.mutex = PTHREAD_MUTEX_INITIALIZER
};

//level codes lookup
static const char *level_lookup[] = {
	[XFATAL] = "FATAL",
	[XERROR] = "ERROR",
	[XTRACE] = "TRACE"
};

const char *get_level_name(const int level)
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
	[XECLOSED]    = "communication channel is closed",
	[XETHREAD]    = "multithreading failure",
	[XESHELL]     = "shell error",
	[XEUNDEFINED] = "unspecified error"
};

const char *xerror_str(const xerror err)
{
	if (err >= XESUCCESS && err <= XEUNDEFINED) {
		return error_lookup[err];
	}

	return "no error description available";
}

//this private function is a thread unsafe buffer flush which lets us
//avoid using a recursive mutex. Since locking has a large overhead, any
//call stack that has already acquired the xqueue mutex along its chain
//should call this function.
//
//In particular, __xerror_log will deadlock if we use the public variant.
static void __xerror_flush(void)
{
	for (uint8_t i = 0; i < xq.len; i++) {
		fprintf(stderr, "%s\n", xq.buf[i]);
	}

        xq.len = 0;
}


//lock return codes are not checked during a flush operation. The mutex is
//initialized statically as a fast variant, so we can be sure that
//EINVAL and EDEADLK will not occur.
void xerror_flush(void)
{
	(void) pthread_mutex_lock(&xq.mutex);

	__xerror_flush();

	(void) pthread_mutex_unlock(&xq.mutex);
}

//logger doesn't check for printf errors. If such an error occurs then the
//the logger will fail silently. At that point, the application program is
//still able to pass error codes up the call chain, so the program will at
//least exit with a status code.
//
//This design choice means that the logger is not a necessary element of
//the application. It's convenient, but if it breaks then the compiler
//will still be able to finish successfully.
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
	assert(func);
	assert(line >= 0);
	assert(level >= XFATAL && level <= XTRACE);
	assert(msg);

	va_list args;
	va_start(args, msg);

	const char *fmt = "(%p) %s %s:%s:%d ";
	const char *lname = get_level_name(level);
	const void *tid = (void *) pthread_self();
	int n = 0;

	pthread_mutex_lock(&xq.mutex);
	
	if (xq.len == BUFLEN) {
		__xerror_flush();
	}

	n = snprintf(xq.buf[xq.len], STRLEN, fmt, tid, lname, file, func, line);
	
	//buf + n points to the null char index or to the start of the buf row
	//if snprintf failed
	n = n < 0 ? 0 : n;

	assert(STRLEN - n >= 0);

	(void) vsnprintf(xq.buf[xq.len] + n, (size_t) (STRLEN - n), msg, args);

	xq.len++;

#ifdef XERROR_DEBUG
	__xerror_flush();
#else
	if (level == XFATAL) {
		__xerror_flush();
	}
#endif

	pthread_mutex_unlock(&xq.mutex);

	va_end(args);
}
