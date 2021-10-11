// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

#ifndef kmalloc
	#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)
#endif

typedef struct view {
	char *data;
	size_t len;
} view;

//------------------------------------------------------------------------------
//static strings; null terminated fixed-length char arrays

typedef char* string;

static string StringFromArray(char *src, size_t len)
{
	assert(src);

	string dest;
	kmalloc(dest, sizeof(char) * len + 1);
	memcpy(dest, src, sizeof(char) * len);
	dest[len] = '\0';

	return dest;
}

static string StringFromView(view v)
{
	assert(v.data);

	string dest;
	kmalloc(dest, sizeof(char) * v.len + 1);
	memcpy(dest, v.data, sizeof(char) * v.len);
	dest[v.len] = '\0';

	return dest;
}

static string StringInit(size_t len)
{
	string dest;
	kmalloc(dest, sizeof(char) * len + 1);
	dest[len] = '\0';

	return dest;
}

static void StringFree(string s)
{
	if (s) {
		free(s);
	}
}

//use with gcc cleanup
static void StringPtrFree(string *sptr)
{
	if (sptr && *sptr) {
		free(*sptr);
	}
}

//------------------------------------------------------------------------------
//lookup table abstraction; supports at most one table per function

#define STRING_TABLE_BEGIN \
	static const string const str_table_lookup[] = {

#define STRING_TABLE_ENTRY(key, value) \
	[key] = value,

#define STRING_TABLE_END \
	}; \
	const size_t str_table_len = sizeof(str_table_lookup) / sizeof(char*);

#define STRING_TABLE_FETCH(key, default) \
	key < str_table_len ? str_table_lookup[key] : default

//------------------------------------------------------------------------------
//dynamic strings

make_vector(char, string, static)

typedef string_vector dynamic_string;

static void DynamicStringInit(dynamic_string *self, size_t cap)
{
	assert(self);
	string_vector_init(self, 0, cap);
	string_vector_push(self, '\0');
}
static void DynamicStringFree(dynamic_string *self)
{
	assert(self);
	string_vector_free(self, NULL);
}

static size_t DynamicStringLength(dynamic_string *self)
{
	assert(self);
	assert(self->len != 0 && "dynamic string not initialized");

	return self->len - 1;
}

static void DynamicStringAppend(dynamic_string *self, char c)
{
	assert(self);
	assert(self->len != 0 && "dynamic string not initialized");

	(void) string_vector_set(self, self->len - 1, c);
	string_vector_push(self, '\0');
}

static void DynamicStringReset(dynamic_string *self)
{
	string_vector_reset(self, NULL);
	string_vector_push(self, '\0');
}

static view ViewFromDynamicString(dynamic_string *self)
{
	assert(self);
	assert(self->len != 0 && "dynamic string not initialized");

	view v = {
		.data = self->data,
		.len = self->len - 1
	};

	return v;
}
