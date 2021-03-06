// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Encapsulation for the gperf auto-generated C code.

#pragma once

#include "scanner.h"

#include <stddef.h>

typedef struct kv_pair {
	char *name;
	token_type typ;
} kv_pair;

const struct kv_pair *kmap_lookup(const char *str, size_t len);
