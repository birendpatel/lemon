// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
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

static header *GetHeader(void *);
static size_t Align(const size_t);

//------------------------------------------------------------------------------
//an arena is just a contiguous block of stdlib malloced memory with an sbrk()
//style increment that progressively bumps the top pointer. Once the top pointer
//is at (char*) arena.top + capacity, the arena is out of memory. An arena is 
//created for each thread in its TLS; this mitigates thread contention that 
//would occur if we instead implemented a single shared arena with mutex locks.

struct arena {
	void *top;
	size_t capacity;
	size_t remaining;
};

static __thread arena arena_tls =  {
	.top = NULL,
	.capacity = 0,
	.remaining = 0
};

//------------------------------------------------------------------------------
//a header hides just in front of each memory region returned to the user and is
//the primary mechanism that enables block reallocation.

struct header {
	size_t bytes;
};

static header *GetHeader(void *user_region)
{
	assert(user_regiion);

	const size_t offset = sizeof(header);
	return (header *) ((char *) user_region - offset);
}

//------------------------------------------------------------------------------
//note; since arena size can be modified via compiler arguments, the bounds
//check must not be asserted since it will reduce to a no-op in release mode.

void ArenaInit(size_t bytes)
{
	if (!bytes) {
		xerror_fatal("requested empty arena");
		abort();
	}

	arena_tls.top = calloc(bytes, 1);

	if (!arena_tls.top) {
		xerror_fatal("cannot allocate memory for new arena");
		abort();
	}

	arena_tls.capacity = bytes;
	arena_tls.remaining = bytes;

	const cstring *msg = "initialised arena at %p with %zu bytes";
	ArenaTrace(msg, arena_tls.top, bytes);
}

void ArenaFree(void)
{
	assert(arena_tls.top);

	const size_t offset = arena_tls.capacity - arena_tls.remaining;
	void *start = (char *) arena_tls.top - offset;

	assert(start);
	free(start);

	const double used = arena_tls.remaining / (double) arena_tls.capacity;
	ArenaTrace("allocation at %p released (%g%)", start, used);
}

//------------------------------------------------------------------------------

//returns input rounded up to nearest multiple of 16
static size_t Align(const size_t bytes)
{
	const size_t alignment = 0x10;
	const size_t mask = alignment - 1;
	const size_t newsize = bytes + ((alignment - (bytes & mask)) & mask);

	assert(newsize >= bytes && "alignment overflow");

	return newsize;
}

void *ArenaAllocate(size_t bytes)
{
	assert(arena_tls.top);
	assert(bytes);

	ArenaTrace("request for %zu bytes", bytes);

	const size_t user_bytes = Align(bytes);
	const size_t total_bytes = sizeof(header) + user_bytes;
	assert(total_bytes > user_bytes && "region + header causes overflow");

	ArenaTrace("allocating %zu bytes (total %zu)", user_bytes, total_bytes);

	if (total_bytes > arena_tls.remaining) {
		xerror_fatal("arena is out of memory");
		abort();
	}

	header *metadata = arena_tls.top;
	metadata->bytes = user_bytes;

	arena_tls.top = (void *) ((char *) arena_tls.top + total_bytes);
	arena_tls.remaining -= total_bytes;

	ArenaTrace("done; %zu bytes remain", arena_tls.remaining);

	void *user_region = metadata + 1;
	return user_region;
}

void *ArenaReallocate(void *ptr, size_t bytes)
{
	assert(arena_tls.top);
	assert(ptr);
	assert(bytes);

	void *new = NULL;
	header *metadata = GetHeader(ptr);

	ArenaTrace("reallocation of %zu bytes requested", bytes);

	if (metadata->bytes >= bytes) {
		ArenaTrace("no realloc; block has %zu bytes", metadata->bytes);
		return ptr;
	}

	new = ArenaAllocate(bytes);

	memcpy(new, ptr, metadata->bytes);

	ArenaTrace("reallocated from %p to %p", ptr, new);

	return new;
}

