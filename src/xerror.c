// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "xerror.h"

static const cstring *GetLevelName(const int);
static void FlushBuffer(bool);

enum xqueue_properties {
	buffer_capacity = (uint8_t) 64,
	message_max_length = (uint8_t) 128
};

struct xqueue {
	pthread_mutex_t mutex;
	char buf[buffer_capacity][message_max_length];
	uint8_t len;
};

static struct xqueue xq = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.len = 0
};

static const cstring *GetLevelName(const int level)
{
	static const cstring *lookup[] = {
		[XFATAL] = "FATAL",
		[XERROR] = "ERROR",
		[XTRACE] = "TRACE"
	};

	if (level >= XFATAL && level <= XTRACE) {
		return lookup[level];
	}

	return "N/A";
}

const char *XerrorDescription(const xerror err)
{
	static const cstring *lookup[] = {
		[XESUCCESS] = "function terminated successfully",
		[XENOMEM] = "dynamic allocation failed",
		[XEOPTION] = "options parsing failed",
		[XEFULL] = "data structure reached capacity",
		[XEFILE] = "IO failure",
		[XEBUSY] = "thread is waiting on a condition",
		[XECLOSED] = "communication channel is closed",
		[XETHREAD] = "multithreading failure",
		[XESHELL] = "shell failure",
		[XEUSER] = "issues detected in user source code",
		[XEUNDEFINED] = "unspecified error"
	};

	if (err >= XESUCCESS && err <= XEUNDEFINED) {
		return lookup[err];
	}

	return "no error description available";	
}

static void FlushBuffer(bool need_mutex)
{
	if (need_mutex) {
		//the mutex is a fast variant; return code is not useful
		(void) pthread_mutex_lock(&xq.mutex);
	}

	for (uint8_t i = 0; i < xq.len; i++) {
		(void) fprintf(stderr, "%s\n", xq.buf[i]);
	}

	xq.len = 0;

	if (need_mutex) {
		(void) pthread_mutex_unlock(&xq.mutex);
	}
}

void XerrorFlush(void)
{
	FlushBuffer(true);
}

//snprint and vsnprintf may fail in this function, but ignoring the errors
//simplifies the API. Since the logger isn't a core compiler requirement, the
//source code might still execute perfectly even if the logger breaks.
void XerrorLog 
(
	const cstring *file,
	const cstring *func,
	const int line,
	const int level,
	const cstring *msg,
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

	pthread_mutex_lock(&xq.mutex);

	if (xq.len == buffer_capacity) {
		FlushBuffer(false);
	}

	int total_printed = snprintf(
		xq.buf[xq.len],
		message_max_length,
		"(%p) %s %s:%s:%d ",
		(void *) pthread_self(),
		GetLevelName(level),
		file,
		func,
		line
	);

	if (total_printed < 0) {
		total_printed = 0;
	}

	int remaining_chars = (int) message_max_length - total_printed;
	assert(remaining_chars >= 0);

	char *msg_offset = xq.buf[xq.len] + total_printed;

	(void) vsnprintf(msg_offset, (size_t) remaining_chars, msg, args);

	xq.len++;

#ifdef XERROR_DEBUG
	FlushBuffer(false);
#else
	if (level == XFATAL) {
		FlushBuffer(false);
	}
#endif

	pthread_mutex_unlock(&xq.mutex);

	va_end(args);
}

void XerrorUser(const cstring *fname, const size_t ln, const cstring *msg, ...)
{
	assert(msg);

	va_list args;
	va_start(args, msg);

	//Note, the ANSI_RED macro does not reset the colour after it is
 	//applied. This allows the inputs to also be coloured red. RED()
 	//cannot be applied directly to the inputs because the ANSI_RED macro
	//relies on string concatenation.
	fprintf(stderr, ANSI_RED "");

	if (fname) {
		fprintf(stderr, "%s: ", fname);
	}

	if (ln) {
                fprintf(stderr, "line %zu: ", ln);
        } 

        vfprintf(stderr, msg, args);

        //this final RED() causes the colours to reset after the statement
        fprintf(stderr, RED("\n"));

        va_end(args);
}
