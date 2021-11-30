// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xerror.h"

static const cstring *GetLevelName(const int);
static void XerrorFlush__unsafe(void);
static const cstring *RemoveFilePath(const cstring *);
static void NewThreadID__unsafe(void);
static size_t GetThreadID__unsafe(void);

//colours provided by @gon1332 at stackoverflow.com/questions/2616906/
#ifdef COLOURS
	#define COLOUR_RESET	"\x1B[0m"
	#define ANSI_RED  	"\x1B[31m"
	#define ANSI_GREEN  	"\x1B[32m"
	#define ANSI_YELLOW 	"\x1B[33m"
	#define ANSI_BLUE  	"\x1B[34m"
	#define ANSI_MAGENTA	"\x1B[35m"
	#define ANSI_CYAN  	"\x1B[36m"
	#define ANSI_WHITE      "\x1B[37m"

	#define RED(str)	(ANSI_RED str COLOUR_RESET)
	#define GREEN(str)	(ANSI_GREEN str COLOUR_RESET)
	#define YELLOW(str)	(ANSI_YELLOW str COLOUR_RESET)
	#define BLUE(str)	(ANSI_BLUE str COLOUR_RESET)
	#define MAGENTA(str)	(ANSI_MAGENTA str COLOUR_RESET)
	#define CYAN(str)	(ANSI_CYAN str COLOUR_RESET)
	#define WHITE(str)	(ANSI_WHITE str COLOUR_RESET)

	#define BOLD(str) 	"\x1B[1m" str COLOUR_RESET
	#define UNDERLINE(str) 	"\x1B[4m" str COLOUR_RESET
#else
	#define COLOUR_RESET
	#define ANSI_RED
	#define ANSI_GREEN
	#define ANSI_YELLOW
	#define ANSI_BLUE
	#define ANSI_MAGENTA
	#define ANSI_CYAN
	#define ANSI_WHITE

	#define RED(str) str
	#define GREEN(str) str
	#define YELLOW(str) str
	#define BLUE(str) str
	#define MAGENTA(str) str
	#define CYAN(str) str
	#define WHITE(str) str

	#define BOLD(str) str
	#define UNDERLINE(str) str

#endif

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
//the compiler is multithreaded so the logger needs to report thread IDs. But,
//pthread_self() is opaque and the gettid syscall results in a large integer
//that is difficult on your eyes when reading a long trace log. So, instead we 
//manually create thread IDs in the TLS.

static __thread size_t thread_id = 0;

static void NewThreadID__unsafe(void)
{
	assert(thread_id == 0 && "attempted to reconfigure thread ID");

	static size_t key = 1;
	thread_id = key++;
}

static size_t GetThreadID__unsafe(void)
{
	if (thread_id == 0) {
		NewThreadID__unsafe();
	}

	return thread_id;
}

//------------------------------------------------------------------------------

void XerrorFlush(void)
{
	pthread_mutex_lock(&xqueue.mutex);

	XerrorFlush__unsafe();

	pthread_mutex_unlock(&xqueue.mutex);
}

static void XerrorFlush__unsafe(void)
{
	//colour of the header changes when the thread id of the next message
	//is different than the previous message.
	static size_t prev_thread_id = 0;
	size_t curr_thread_id = 0;

	for (size_t i = 0; i < xqueue.len; i++) {
		message *msg = &xqueue.buffer[i];

		//use sscanf because the input is santised, well-defined, and
		//created by a trusted source.
		(void) sscanf(msg->header, "%zu", &curr_thread_id);

		cstring *fmt = "0x%s";

		if (curr_thread_id != prev_thread_id && prev_thread_id) {
			fmt = YELLOW("0x%s");
		} 
		
		(void) fprintf(stderr, fmt, msg->header);
		(void) fprintf(stderr, CYAN("\n\t-> %s\n"), msg->body);

		prev_thread_id = curr_thread_id;

	}

	xqueue.len = 0;
}

static const cstring *GetLevelName(const int level)
{
	assert(level >= XTRACE && level <= XFATAL);

	static const cstring *lookup[] = {
		[XTRACE] = "TRACE",
		[XWARN]  = "WARN",
		[XERROR] = "ERROR",
		[XFATAL] = "FATAL"
		
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
	assert(level >= XTRACE && level <= XFATAL);
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

	const cstring *fmt = "%zu %s %s %s";
	char *header = msg->header;
	char *body = msg->body;
	const cstring *fname = RemoveFilePath(file);
	const size_t tid = GetThreadID__unsafe();
	const cstring *lname = GetLevelName(level);

	(void) snprintf(header, HEADER_LIMIT, fmt, tid, lname, fname, func);
	(void) vsnprintf(body, BODY_LIMIT, txt, args);

	xqueue.len++;

#ifdef XERROR_DEBUG
	const int threshold = XTRACE;
#else
	const int threshold = XFATAL;
#endif
	assert(XFATAL > XTRACE);
	
	if (level >= threshold) {
		XerrorFlush__unsafe();
	}

	pthread_mutex_unlock(&xqueue.mutex);

	va_end(args);
}

//------------------------------------------------------------------------------

void XerrorUser
(
	const cstring *fname,
	const size_t line,
	const int level,
	const cstring *msg,
	...
)
{
	assert(level <= XUSERHELP && level >= XUSERERROR);
	assert(msg);

	static const cstring *lookup[] = {
		[XUSERERROR] = ANSI_RED "ERROR: ",
		[XUSERWARN] = ANSI_YELLOW "WARN: ",
		[XUSERHELP] = ANSI_GREEN "ADVICE: "
	};

	va_list args;
	va_start(args, msg);

	//table lookup triggers all subsequent output to be coloured
	fprintf(stderr, "%s", lookup[level]);

	if (fname) {
		fprintf(stderr, "%s: ", fname);
	}

	if (line) {
                fprintf(stderr, "%zu: ", line);
        } 

        vfprintf(stderr, msg, args);

	//default gets restored by piggybacking on the invisible newline
        fprintf(stderr, "\n" COLOUR_RESET);

	va_end(args);
}
