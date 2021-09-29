// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
// Miscellaneous utilities and definitions

#pragma once

#define LEMON_VERSION "Alpha"

#ifdef __GNUC__
	#define GCC_VERSION_MAJ (__GNUC__ * 10000)
	#define GCC_VERSION_MIN (__GNUC_MINOR__ * 100)
	#define GCC_VERSION_PCH (__GNUC_PATCHLEVEL__)
	#define GCC_VERSION GCC_VERSION_MAJ + GCC_VERSION_MIN + GCC_VERSION_PCH
#else
	#define GCC_VERSION 0
#endif

#define fallthrough __attribute__((fallthrough))

#define unused __attribute__((unused))

#define KiB(kilos) (1024 * kilos)

//induce a segementation violation if the allocation request failed. The Lemon
//compiler uses a "fail fast and fail early" design philosophy.
#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)
