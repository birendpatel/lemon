// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Thread-safe arena memory allocator. Each arena is created in thread local
// storage to eliminate contention. Like the DMD and 8cc compilers, this
// "set it and forget it" allocation policy results in simpler application
// code, a faster compiler, and fewer use-after-free & double-free bugs.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define KiB(kibibytes) ((size_t) (1024 * kibibytes))
#define MiB(mebibytes) ((size_t) (1048576 * mebibytes))
#define GiB(gibibytes) ((size_t) (1073741824 * gibibytes))

//initialise a thread-local arena with a minimum fixed capacity of 'bytes';
//returns false on failure
bool ArenaInit(size_t bytes);

//returns an aligned and zeroed memory block; returns NULL on failure
void *ArenaAllocate(size_t bytes);

//reallocate the data at 'ptr' to an aligned block of memory; if the new size
//is larger than the old size, then the additional memory is zeroed; returns
//NULL on failure
void *ArenaReallocate(void *ptr, size_t bytes);

//releases system resources acquired by ArenaInit; returns false if arena was
//not initialised prior to this call
void ArenaFree(void);

__attribute__((always_inline))
static inline void *allocate(size_t bytes)
{
	void *ptr = ArenaAllocate(bytes);

	if (!ptr) {
		abort();
	}

	return ptr;
}

__attribute__((always_inline))
static inline void *reallocate(void *ptr, size_t bytes)
{
	void *new = ArenaReallocate(ptr, bytes);

	if (!new) {
		abort();
	}

	return new;
}
