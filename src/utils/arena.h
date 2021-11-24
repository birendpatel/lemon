// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Thread-safe arena allocator with 16-byte alignment. Like compilers such as
// DMD and 8cc, C Lemon's allocator does not provide a mechanism to free the
// allocated blocks. This means that memory does not need to be managed, and so
// application code can be written as if a garbage collector is present. This
// also eliminates use-after-free and double-free bugs.

#pragma once

#include <stddef.h>

#define KiB(kibibytes) ((size_t) (1024 * kibibytes))
#define MiB(mebibytes) ((size_t) (1048576 * mebibytes))
#define GiB(gibibytes) ((size_t) (1073741824 * gibibytes))

//initialize the arena with maximum capacity == bytes; will abort() if the 
//internal stdlib allocation call fails or bytes == 0.
void ArenaInit(size_t bytes);

//The call will abort if bytes == 0, if any internal adjustments to the input
//request cause an overflow, or if the request is more than the remaining free 
//bytes. Otherwise returns a zeroed region of memory.
void *ArenaAllocate(size_t bytes);

//The call will abort if bytes == 0 or if the request is more than the remaining
//free bytes. The call is equivalent to ArenaAllocate() if ptr == NULL. Returns 
//a zeroed region of memory which might be identical to the input region.
void *ArenaReallocate(void *ptr, size_t bytes);

//releases system resources acquired by ArenaInit
void ArenaFree(void);
