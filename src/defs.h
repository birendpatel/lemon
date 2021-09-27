// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
// Miscellaneous utilities and definitions

#pragma once

#define LEMON_VERSION "(alpha)"

static inline size_t KiB(const size_t kilos) {
	const size_t multiplier = 1024;
	return kilos * multiplier;
}

#define fallthrough __attribute__((fallthrough))

//induce a segementation violation if the allocation request failed. The Lemon
//compiler uses a "fail fast and fail early" design philosophy.
#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)
