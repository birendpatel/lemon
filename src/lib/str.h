// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// This header provides dynamic string data structures. The two fundamental
// data structures are the string and the view. Strings are reference-counted
// vector<char> and views are references to subsequences of strings. Some
// minor abstractions over C-style strings are also provided.

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

#ifndef STR_ESUCCESS
	#error "str.h requires user to implement STR_ESUCCESS int code"
#endif

#ifndef STR_EREF
	#error "str.h requires user to implement STR_EREF int code"
#endif

typedef char cstring;
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

static string StringInit(const size_t capacity)
{
	const size_t default_length = 0;
	const char null_terminator = '\0';

	string s = {
		.vec = {0},
		.refcount = 0
	};

	CharVectorInit(&s.vec, default_length, capacity);
	CharVectorPush(&s.vec, null_terminator);

	return s;
}

static int StringFree(string *s)
{
	assert(s);
	assert(str->vec.len);

	if (s->refcount != 0) {
		return STR_EREF;
	}

	CharVectorFree(&s->vec, NULL);
	s->vec = (Char_vector) {0};

	return STR_ESUCCESS;
}

static size_t StringLength(const string *s)
{
	assert(self);
	assert(s->vec.len != 0);

	return s->vec.len - 1;
}

static void StringAppend(string *s, const char c)
{
	assert(s);
	assert(s->vec.len);

	(void) CharVectorSet(&s->vec, s->vec.len - 1, c);
	CharVectorPush(&s->vec, '\0');
}

static char StringGet(const string *s, const size_t index)
{
	assert(s);
	assert(index < StringLength(s));

	char c = '\0';

	CharVectorGet(s, index, &c);

	return c;
}

static void StringTrim(string *s, char c)
{
	assert(s);
	assert(c);

	size_t pos = StringLength(s) - 1;

	while (pos != SIZE_MAX) {
		if (s->vec.buffer[pos] != c) {
			break;
		}

		pos--;
		s->vec.len--;
	}

	(void) CharVectorSet(&s->vec, s->vec.len, '\0');	
}

static void StringReset(string *s)
{
	assert(s);
	assert(s->vec.len);

	CharVectorReset(&s->vec, NULL);
	CharVectorPush(&s->vec, '\0');
}

//copy the view subsequence to a new string; the view remains valid on return.
static string StringFromView(const view *v)
{
	assert(v);
	assert(v->data);
	assert(v->len);
	assert(v->ref);
	assert(v->ref->vec.len);

	string s = {
		.vec = {0},
		.refcount = 0
	};

	CharVectorNewCopy(&s.vec, &v->ref->vec);

	return s;
}

//CString gets subsumed by string, so if cstring exists on heap then it must
//not be freed unless by StringFree. i.e., the cstring resource is given to the
//string, not borrowed or copied.
static string StringFromHeapCString(const cstring *src)
{
	assert(src);
	assert(src[strlen(src) - 1] == '\0');

	string s = {
		.vec = {0},
		.refcount = 0
	};

	CharVectorAdopt(&s.vec, src, strlen(src));

	return s;
}

//------------------------------------------------------------------------------

//view are created on stack but the initializer functions guarantee correct
//reference tracking.
static view ViewOpen(const string *src, const char *data, const size_t len)
{
	assert(src);
	assert(src->vec.len != 0);
	assert(data);
	assert(len != 0);

	view v = {
		.ref = src,
		.data = data,
		.len = len,
	};

	src->refcount++;

	return v;
}

static void ViewClose(view *v)
{
	assert(v);

	v->data = NULL;
	v->len = 0;
	v->src->refcount--;
}

static string ViewToString(view *v)
{
	assert(str);
	assert(v);
	assert(v->data);
	assert(v->len);
	assert(v->ref);
	assert(v->ref->vec.len);

	string s = StringFromView(v);

	ViewClose(v);

	return s;
}
