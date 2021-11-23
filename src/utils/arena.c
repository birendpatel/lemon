// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "xerror.h"

#ifdef ARENA_TRACE
	#define ArenaTrace(msg, ...) xerror_trace(msg, ##__VA_ARGS__)
#else
	#define ArenaTrace(msg, ...)
#endif

typedef struct arena arena;
typedef struct header header;

static void *ArenaAllocate__private(const size_t, const bool);
static header *GetHeader(void *);
static size_t Align(const size_t);

//------------------------------------------------------------------------------

struct arena {
	void *start;
	void *top;
	size_t capacity;
	size_t remaining;
	pthread_mutex_t mutex;
};

static arena mempool =  {
	.start = NULL,
	.top = NULL,
	.capacity = 0,
	.remaining = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER
};

//------------------------------------------------------------------------------

struct header {
	size_t bytes;
};

static header *GetHeader(void *ptr)
{
	assert(ptr);

	const size_t offset = sizeof(header);
	
	return (header *) ((char *) ptr - offset);
}

//------------------------------------------------------------------------------

void ArenaInit(size_t bytes)
{
	assert(bytes);

	pthread_mutex_lock(&mempool.mutex);
	
	mempool.start = calloc(bytes, 1);

	if (!mempool.start) {
		xerror_fatal("cannot allocate memory to global arena");
		abort();
	}

	mempool.top = mempool.start;
	mempool.capacity = bytes;
	mempool.remaining = bytes;

	pthread_mutex_unlock(&mempool.mutex);

	ArenaTrace("initialized arena with %zu bytes", mempool.capacity);
}

void ArenaFree(void)
{
	pthread_mutex_lock(&mempool.mutex);
	
	free(mempool.start);

	pthread_mutex_unlock(&mempool.mutex);

	ArenaTrace("allocation at %p released", (void *) mempool.start);
}

//------------------------------------------------------------------------------

//returns input rounded up to nearest multiple of 16
static size_t Align(const size_t bytes)
{
	const size_t alignment = 0x10;
	const size_t mask = alignment - 1;

	return bytes + ((alignment - (bytes & mask)) & mask);
}

void *ArenaAllocate(size_t bytes)
{
	return ArenaAllocate__private(bytes, true);
}

static void *ArenaAllocate__private(const size_t bytes, const bool need_mutex)
{
	assert(bytes);

	ArenaTrace("user request for %zu bytes", bytes);

	const size_t user_bytes = Align(bytes);
	const size_t total_bytes = sizeof(header) + user_bytes;
	void *user_region= NULL;

	ArenaTrace("allocating %zu bytes (total %zu)", user_bytes, total_bytes);

	if (need_mutex) {
		pthread_mutex_lock(&mempool.mutex);
	}

	assert(mempool.start && "mempool not initialised (or deinitialised)");

	if (total_bytes > mempool.remaining) {
		xerror_fatal("exhausted memory in global arena");
		abort();
	}

	header *metadata = mempool.top;
	metadata->bytes = user_bytes;
	user_region = metadata + 1;

	mempool.top = (void *) ((char *) mempool.top + total_bytes);
	mempool.remaining -= total_bytes;

	ArenaTrace("done; %zu bytes remain", mempool.remaining);

	if (need_mutex) {
		pthread_mutex_unlock(&mempool.mutex);
	}

	return user_region;
}

void *ArenaReallocate(void *ptr, size_t bytes)
{
	assert(ptr);
	assert(bytes);

	void *new = NULL;
	header *metadata = GetHeader(ptr);

	ArenaTrace("reallocation of %zu bytes requested", bytes);

	pthread_mutex_lock(&mempool.mutex);

	assert(mempool.start && "mempool not initialised (or deinitialised)");

	if (metadata->bytes >= bytes) {
		ArenaTrace("no realloc; block has %zu bytes", metadata->bytes);
		new = ptr;
		goto unlock;
	}

	new = ArenaAllocate__private(bytes, false);

	memcpy(new, ptr, metadata->bytes);

	ArenaTrace("reallocated from %p to %p", ptr, new);

unlock:
	pthread_mutex_unlock(&mempool.mutex);

	return new;
}

