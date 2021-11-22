// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "xerror.h"

#ifdef ARENA_TRACE
	#define ArenaTrace(msg, ...) xerror_trace(msg, ##__VA_ARGS__)
#else
	#define ArenaTrace(msg, ...)
#endif

//------------------------------------------------------------------------------

typedef struct arena {
	void *top;
	size_t capacity;
	size_t remaining;
	pthread_mutex_t mutex;
} arena;

static arena mempool =  {
	.start = NULL,
	.top = NULL,
	.capacity = 0,
	.remaining = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER
};

//------------------------------------------------------------------------------

typedef struct header {
	size_t bytes;
} header;

static inline header *GetHeader(void *ptr)
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

	mempool.top = start;
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

//returns input rounded up to nearest multiple of 16
size_t Align(const size_t bytes)
{
	const size_t alignment = 0x10;
	const size_t lo_mask = alignment - 1;
	const size_t hi_mask = ~lo_mask;

	return (bytes + lo_mask) & ~hi_mask;
}

void *ArenaAllocate(size_t bytes)
{
	assert(bytes);

	const size_t user_bytes = Align(bytes);
	const size_t total_bytes = sizeof(header) + user_bytes;
	void *user_region= NULL;

	ArenaTrace("allocating %zu bytes (total %zu)", user_bytes, total_bytes);

	pthread_mutex_lock(&mempool.mutex);

	if (total_bytes > mempool.remaining) {
		xerror_fatal("exhausted memory in global arena");
		abort();
	}

	header *metadata = mempool.top;
	metadata->bytes = user_bytes;
	user_region = metadata + 1;

	mempool.top = (void *) ((char *) mempool.top + total_bytes);
	mempool.remaining -= total_bytes;

	ArenaTrace("%done; %zu bytes remain", mempool.remaining);

	pthread_mutex_unlock(&mempool.mutex);

	return user_region;
}

void *ArenaReallocate(void *ptr, size_t bytes)
{
	assert(ptr);
	assert(bytes);

	void *new = NULL;
	header *metadata = Getheader(ptr);

	ArenaTrace("reallocation of %zu bytes requested", bytes);

	pthread_mutex_lock(&mempool.mutex);

	if (metadata->bytes >= bytes) {
		ArenaTrace("no realloc; block has %zu bytes", metadata->bytes);
		return ptr;
	}

	new = ArenaAllocate(bytes);

	memcpy(region, ptr, metadata->bytes);

	pthread_mutex_unlock(&mempool.mutex);

	ArenaTrace("reallocated from %p to %p", ptr, new);

	return new;
}

