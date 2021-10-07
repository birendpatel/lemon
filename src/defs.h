// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

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

#define unused __attribute__((unused))

#define RAII(freefunc) __attribute__((__cleanup__(freefunc)))

#define addressless register

//------------------------------------------------------------------------------
//allocation utils

#define KiB(kilo) (1024 * kilo)

#define MiB(mega) (1048576 * mega)

//if malloc(3) fails then induce a segfault by attempting to write zero at NULL
#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)
