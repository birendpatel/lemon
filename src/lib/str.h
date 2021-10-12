// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// This header provides dynamic string data structures. The two fundamental
// data structures are the string and the view. Strings are reference-counted
// vector<char> and views are references to subsequences of strings.

#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

#ifndef STR_ESUCCESS
	#error "str.h requires user to implement STR_ESUCCESS int code"
#endif

#ifndef STR_EREF
	#error "str.h requires user to implement STR_EREF int code"
#endif

//------------------------------------------------------------------------------

typedef struct string string;
typedef struct view view;

make_vector(char, Char, static)

struct string {
	Char_vector vec;
	size_t refcount;
};

struct view {
	string *ref;
	char *data;
	size_t len;
};

//------------------------------------------------------------------------------

static void StringInit(string *const str, const size_t capacity)
{
	assert(str);
	assert(capacity);

	CharVectorInit(&str->vec, 0, capacity);
	CharVectorPush(&str->vec, '\0');

	str->refcount = 0;
}

static int StringFree(string *str)
{
	assert(str);
	assert(str->vec.len);

	if (str->refcount != 0) {
		return STR_EREF;
	}

	CharVectorFree(&str->vec, NULL);
	str->vec = (Char_vector) {0};

	return STR_ESUCCESS;
}

static size_t StringLength(const string *const str)
{
	assert(self);
	assert(self->vec.len != 0);

	return self->vec.len - 1;
}

static void StringAppend(string *const str, const char c)
{
	assert(str);
	assert(str->vec.len);

	(void) CharVectorSet(&str->vec, str->vec.len - 1, c);
	CharVectorPush(&str->vec, '\0');
}

static void StringReset(string *const str)
{
	assert(str);
	assert(str->vec.len);

	CharVectorReset(&str->vec, NULL);
	CharVectorPush(&str->vec, '\0');
}

//copy the view subsequence to a new string; the view remains valid on return.
static void StringFromView(string *const str, const view *const v)
{
	assert(str);
	assert(v);
	assert(v->data);
	assert(v->len);
	assert(v->ref);
	assert(v->ref->vec.len);

	CharVectorNewCopy(&str->vec, &v->ref->vec);

	str->refcount = 0;
}

static void StringFromCString(string *const str, char *src)
{
	assert(str);
	assert(src);

	CharVectorAdopt(&str->vec, src, strlen(src));

	str->refcount = 0;
}

//------------------------------------------------------------------------------

//views are created on stack but they must use initializer functions to ensure
//that the underlying string tracks references.
static view ViewOpen(const string src, const char *const data, const size_t len)
{
	assert(src);
	assert(src->vec.len != 0);
	assert(data);
	assert(len != 0);

	view new = {
		.ref = &src,
		.data = data,
		.len = len,
	};

	src->refcount++;

	return new;
}

static void ViewClose(view *const v)
{
	assert(v);

	v->data = NULL;
	v->len = 0;
	v->src->refcount--;
}

static void ViewToString(view *const v, string *const str)
{
	assert(str);
	assert(v);
	assert(v->data);
	assert(v->len);
	assert(v->ref);
	assert(v->ref->vec.len);

	StringFromView(str, v);

	ViewClose(v);
}

//------------------------------------------------------------------------------
//lookup table abstraction; supports at most one table per function

#define CSTRING_TABLE_BEGIN 					               \
	static const char *const str_table_lookup[] = {

#define CSTRING_TABLE_ENTRY(key, value)                                        \
	[key] = value,

#define CSTRING_TABLE_END                                                      \
	};                                                                     \
	const size_t str_table_len = sizeof(str_table_lookup) / sizeof(char*);

#define CSTRING_TABLE_FETCH(key, default)                                      \
	key < str_table_len ? str_table_lookup[key] : default
