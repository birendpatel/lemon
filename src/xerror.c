// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "xerror.h"

#define BUFFER_CAPACITY ((uint8_t) 64)
#define MESSAGE_MAX_LENGTH ((uint8_t) 128)

struct xqueue {
	pthread_mutex_t mutex;
	char buf[BUFFER_CAPACITY][MESSAGE_MAX_LENGTH];
	uint8_t len;
};

static struct xqueue xq = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.len = 0
};

static const char *GetLevelName(const int level)
{
	static const char *lookup[] = {
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
	assert(err >= 0);

	const size_t code = (size_t) err;
	
	STRING_TABLE_BEGIN
	
	STRING_TABLE_ENTRY(XESUCCESS, "function terminated successfully")
	STRING_TABLE_ENTRY(XENOMEM, "dynamic allocation failed")
	STRING_TABLE_ENTRY(XEOPTION, "options parsing failed")
	STRING_TABLE_ENTRY(XEFULL, "data structure is at capacity")
	STRING_TABLE_ENTRY(XEFILE, "IO failure")
	STRING_TABLE_ENTRY(XEBUSY, "thread is waiting on a condition")
	STRING_TABLE_ENTRY(XECLOSED, "communication channel is closed")
	STRING_TABLE_ENTRY(XETHREAD, "multithreading failure")
	STRING_TABLE_ENTRY(XESHELL, "shell error")
	STRING_TABLE_ENTRY(XEPARSE, "parsing to AST failed")
	STRING_TABLE_ENTRY(XEUNDEFINED, "unspecified error")
	
	STRING_TABLE_END

	return STRING_TABLE_FETCH(code, "no error description available");
}

static void FlushBuffer(bool need_mutex)
{
	if (need_mutex) {
		//mutex is a fast variant; return code is useless
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

	pthread_mutex_lock(&xq.mutex);

	if (xq.len == BUFFER_CAPACITY) {
		FlushBuffer(false);
	}

	int total_printed = snprintf(
		xq.buf[xq.len],
		MESSAGE_MAX_LENGTH,
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

	int remaining_chars = (int) MESSAGE_MAX_LENGTH - total_printed;
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

void XerrorUser(const size_t line, const string msg, ...)
{
	assert(msg);

	va_list args;
	va_start(args, msg);

	//Note, the ANSI_RED macro does not reset the colour after it is
 	//applied. This allows the input msg to also be coloured red. RED()
 	//cannot be applied directly to the input msg because the macro relies
        //on string concatenation.
	if (line) {
                fprintf(stderr, ANSI_RED "(line %zu) ", line);
        } else {
                fprintf(stderr, ANSI_RED " \b");
        }

        vfprintf(stderr, msg, args);

        //this final RED() causes the colours to reset after the statement
        fprintf(stderr, RED("\n"));

        va_end(args);
}
