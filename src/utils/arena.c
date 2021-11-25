// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <stdint.h>
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

//configurable to any power of two
#define ALIGNMENT ((size_t) 0x10)

static size_t Align(const size_t bytes)
{
	assert(ALIGNMENT != 0 && "degenerate alignment");
	assert(SIZE_MAX % ALIGNMENT == 0 && "potential overflow");
	assert(!(ALIGNMENT & (ALIGNMENT - 1)) && "not a power of 2");

	const size_t mask = ALIGNMENT - 1;
	const size_t offset = (ALIGNMENT- (bytes & mask)) & mask;

	const size_t adjustment = bytes + offset;

	if (bytes != adjustment) {
		ArenaTrace("aligned %zu to %zu", bytes, adjustment);
	}

	return adjustment;
}

//------------------------------------------------------------------------------
//an arena is a contiguous block of calloced memory with an sbrk() style pointer
//that progressively bumps in multiples of 16. If the top pointer reaches the
//location at (char*) arena.top + capacity, then the arena is out of memory. The
//GCC storage class __thread (_Thread_local in C11) substitutes in for a more
//cumbersome pthread_key_t implementation.

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
//a header hides just in front of each memory region returned to the user; it is
//the primary mechanism that enables block reallocation.

struct header {
	size_t bytes;
};

static header *GetHeader(void *user_region)
{
	assert(user_region);

	const size_t offset = sizeof(header);

	return (header *) ((char *) user_region - offset);
}

//------------------------------------------------------------------------------

bool ArenaInit(size_t bytes)
{
	ArenaTrace("request for arena with %zu bytes", bytes);

	bytes = Align(bytes);

	arena_tls.capacity = bytes;
	arena_tls.remaining = bytes;
	arena_tls.top = calloc(bytes, 1);

	if (!arena_tls.top) {
		xerror_fatal("stdlib calloc; out of memory");
		return false;
	}

	ArenaTrace("initialised arena at %p", arena_tls.top);
	
	return true;
}

void ArenaFree(void)
{
	if (!arena_tls.top) {
		xerror_issue("thread local arena not initialised");
		return;
	}

	const size_t offset = arena_tls.capacity - arena_tls.remaining;
	void *start = (char *) arena_tls.top - offset;
	free(start);

	__attribute__((unused)) double total = (double) arena_tls.capacity;
	__attribute__((unused)) double used = (double) arena_tls.remaining;
	__attribute__((unused)) double percent = used / total;

	ArenaTrace("allocation at %p released (%g%%)", start, percent);
}

void *ArenaAllocate(size_t bytes)
{
	if (!arena_tls.top) {
		xerror_fatal("thread local arena not initialised");
		return NULL;
	}

	ArenaTrace("request for new block with %zu bytes", bytes);

	const size_t user_bytes = Align(bytes);
	const size_t total_bytes = sizeof(header) + user_bytes;

	if (total_bytes < user_bytes) {
		xerror_fatal("region + header causes overflow");
		return NULL;
	}

	if (total_bytes > arena_tls.remaining) {
		xerror_fatal("arena; out of memory");
		return NULL;
	}

	header *metadata = arena_tls.top;
	metadata->bytes = user_bytes;

	arena_tls.top = (void *) ((char *) arena_tls.top + total_bytes);
	arena_tls.remaining -= total_bytes;

	ArenaTrace("request fulfilled; block header at %p", (void *) metadata);
	ArenaTrace("arena; %zu bytes remain", arena_tls.remaining);

	void *user_region = metadata + 1;
	return user_region;
}

void *ArenaReallocate(void *old, size_t bytes)
{
	if (!arena_tls.top) {
		xerror_fatal("thread local arena not initialised");
		return NULL;
	}

	if (!old) {
		xerror_fatal("request to reallocate NULL block");
		return NULL;
	}

	void *new = NULL;
	header *metadata = GetHeader(old);

	ArenaTrace("request; realloc %p to %zu bytes", (void *) metadata, bytes);

	if (metadata->bytes >= bytes) {
		ArenaTrace("denied; block has %zu bytes", metadata->bytes);
		return old;
	}

	new = ArenaAllocate(bytes);

	if (!new) {
		xerror_fatal("new block request failed");
		return NULL;
	}

	memcpy(new, old, metadata->bytes);

	ArenaTrace("data copied from user block at %p to %p", old, new);

	return new;
}

