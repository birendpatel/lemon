// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#include <stdlib.h>

//------------------------------------------------------------------------------
// versioning

#define LEMON_VERSION "Alpha"

//5 digit version code; e.g., 100908 is version 10.9.8 (maj.min.patch)
#ifdef __GNUC__
	#define GCC_VERSION_MAJ (__GNUC__ * 10000)
	#define GCC_VERSION_MIN (__GNUC_MINOR__ * 100)
	#define GCC_VERSION_PCH (__GNUC_PATCHLEVEL__)
	#define GCC_VERSION GCC_VERSION_MAJ + GCC_VERSION_MIN + GCC_VERSION_PCH
#else
	#define GCC_VERSION 0
#endif

//------------------------------------------------------------------------------
//language extensions

#define fallthrough __attribute__((fallthrough))

#define RAII(free) __attribute__((__cleanup__(free)))

//------------------------------------------------------------------------------
//allocation utils

static size_t KiB(const double kilobytes)
{
	return (size_t) (1024 * kilobytes);
}

static size_t MiB(const double megabytes)
{
	return (size_t) (1048576 * megabytes);
}

__attribute__((malloc)) static void *AbortMalloc(const size_t bytes)
{
	void *region = malloc(bytes);

	if (!region) {
		abort();
	}

	return region;
}
