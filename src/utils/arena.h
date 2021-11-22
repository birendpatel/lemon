// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#include <stddef.h>

#define KiB(kibibytes) ((size_t) (1024 * kibibytes))
#define MiB(mebibytes) ((size_t) (1048576 * mebibytes))

//initialize a thread-safe memory pool with maximum input capacity
void ArenaInit(size_t bytes);

//returns a zeroed region of memory; if the requested allocation is larger than
//the total remaining free bytes then the program will abort.
void *ArenaAllocate(size_t bytes);

//returns a zeroed region of memory which might be the same as the input region;
//if the requested allocation is larger than the total remaining free bytes then 
//the program will abort.
void *ArenaReallocate(void *ptr, size_t bytes);

//releases system resources acquired by ArenaInit
void ArenaFree(void);
