// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/syscall.h>

#include "xerror.h"

static const cstring *GetLevelName(const int);
static void XerrorFlush__unsafe(void);
static const cstring *RemoveFilePath(const cstring *);

//------------------------------------------------------------------------------
//A single queue is shared between all threads, rather than one queue per thread
//local storage. Although there may be some mutex contention, a single queue 
//means we can deliver the messages to stderr in psuedo-chronological order.

#define HEADER_LIMIT	64
#define BODY_LIMIT      128
#define BUFFER_CAPACITY 64

typedef struct message {
	char header[HEADER_LIMIT];
	char body[BODY_LIMIT];
} message;

typedef struct xerror_queue {
	pthread_mutex_t mutex;
	size_t len;
	message buffer[BUFFER_CAPACITY];
} xerror_queue;

static xerror_queue xqueue = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.len = 0,
	.buffer = {{{0}, {0}}}
};

//------------------------------------------------------------------------------
//note; mutex is not recursive. Naked calls to __unsafe functions may deadlock

void XerrorFlush(void)
{
	pthread_mutex_lock(&xqueue.mutex);

	XerrorFlush__unsafe();

	pthread_mutex_unlock(&xqueue.mutex);
}

static void XerrorFlush__unsafe(void)
{
	size_t i = 0;

	while (i < xqueue.len) {
		message msg = xqueue.buffer[i];

		(void) fprintf(stderr, "%s " , msg.header);

		(void) fprintf(stderr, CYAN("\n\t-> %s\n"), msg.body);

		i++;
	}

	xqueue.len = 0;
}

static const cstring *GetLevelName(const int level)
{
	assert(level >= XFATAL && level <= XTRACE);

	static const cstring *lookup[] = {
		[XFATAL] = "FATAL",
		[XERROR] = "ERROR",
		[XTRACE] = "TRACE"
	};

	return lookup[level];
}

//a file path from the root project directory provide almost zero value so this
//function strips it out and return a pointer to the first char of the filename
static const cstring *RemoveFilePath(const cstring *path)
{
	assert(path);

	char *last_slash = strrchr(path, '/');

	if (last_slash) {
		return last_slash + 1;
	}

	return path;
}

void XerrorLog
(
	const cstring *file,
	const cstring *func,
	const int level,
	const cstring *txt,
	...
)
{
	assert(file);
	assert(func);
	assert(level >= XFATAL && level <= XTRACE);
	assert(txt);

	va_list args;
	va_start(args, txt);

	pthread_mutex_lock(&xqueue.mutex);

	if (xqueue.len == BUFFER_CAPACITY) {
		XerrorFlush__unsafe();
	}

	message *msg = &xqueue.buffer[xqueue.len];
	assert(msg->header[HEADER_LIMIT - 1] == '\0' && "check xqueue init");
	assert(msg->body[BODY_LIMIT - 1] == '\0' && "check xqueue init");

	const cstring *fmt = "%p %s %s %s";
	char *header = msg->header;
	char *body = msg->body;
	const cstring *fname = RemoveFilePath(file);
	void *tid = (void *) syscall(SYS_gettid);
	const cstring *lname = GetLevelName(level);

	(void) snprintf(header, HEADER_LIMIT, fmt, tid, lname, fname, func);
	(void) vsnprintf(body, BODY_LIMIT, txt, args);

	xqueue.len++;

#ifdef XERROR_DEBUG
	const int threshold = XTRACE;
#else
	const int threshold = XFATAL;
#endif
	
	if (level >= threshold) {
		XerrorFlush__unsafe();
	}

	pthread_mutex_unlock(&xqueue.mutex);

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
	fprintf(stderr, ANSI_RED "ERROR: ");

	if (fname) {
		fprintf(stderr, "%s: ", fname);
	}

	if (ln) {
                fprintf(stderr, "%zu: ", ln);
        } 

        vfprintf(stderr, msg, args);

        //this final RED() causes the colours to reset after the statement
        fprintf(stderr, RED("\n"));

        va_end(args);
}

void XwarnUser(const cstring *fname, const size_t ln, const cstring *msg, ...)
{
	assert(msg);

	va_list args;
	va_start(args, msg);

	fprintf(stderr, ANSI_YELLOW "WARNING: ");

	if (fname) {
		fprintf(stderr, "%s: ", fname);
	}

	if (ln) {
                fprintf(stderr, "%zu: ", ln);
        } 

        vfprintf(stderr, msg, args);

        fprintf(stderr, YELLOW("\n"));

        va_end(args);
}

void XhelpUser(const cstring *fname, const size_t ln, const cstring *msg, ...)
{
	assert(msg);

	va_list args;
	va_start(args, msg);

	fprintf(stderr, ANSI_GREEN "ADVICE: ");

	if (fname) {
		fprintf(stderr, "%s: ", fname);
	}

	if (ln) {
                fprintf(stderr, "%zu: ", ln);
        } 

        vfprintf(stderr, msg, args);

        fprintf(stderr, GREEN("\n"));

        va_end(args);
}
