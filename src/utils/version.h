// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#define LEMON_VERSION "alpha" " (" __DATE__ ")"

// 5 digit gcc version code; e.g., 100908 is version 10.9.8 (maj.min.patch)
#ifdef __GNUC__
	#define GCC_VERSION_MAJ (__GNUC__ * 10000)
	#define GCC_VERSION_MIN (__GNUC_MINOR__ * 100)
	#define GCC_VERSION_PCH (__GNUC_PATCHLEVEL__)
	#define GCC_VERSION GCC_VERSION_MAJ + GCC_VERSION_MIN + GCC_VERSION_PCH
#else
	#define GCC_VERSION 0
#endif
