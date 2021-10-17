// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
// 
// This header provides a dynamic string data structure. The vstring is a 
// shallow wrapper over a vector<char> whose internal buffer is guaranteed
// to be null-terminated.

#pragma once

#include <assert.h>
#include <stdint.h>

#include "vector.h"

make_vector(char, Char, static)

//-----------------------------------------------------------------------------
//cstring

typedef char cstring;

static cstring cStringDuplicate(const cstring *cstr)
{
	cstring *new = strdup(cstr);

	if (!new) {
		abort();
	}

	return new;
}

//-----------------------------------------------------------------------------
//vstring

typedef Char_vector vstring;

static vstring vStringInit(const size_t);
static void vStringTerminate__internal(string *);
static void vStringFree(vstring *);
static size_t vStringLength(vstring *);
static void vStringAppend(vstring *, const char);
static char vStringGet(const vstring *, size_t);
static void vStringTrim(vstring *, const char);
static void vStringReset(vstring *);
static const cstring *vStringRaw(vstring *);

static vstring vStringInit(const size_t capacity)
{
	const size_t default_length = 0;

	string vstr = CharVectorInit(default_length, capacity);

	vStringTerminate__internal(&vstr);

	return vstr;
}

static void vStringTerminate__internal(string *vstr)
{
	assert(vstr);

	const char null_terminator = '\0';

	CharVectorPush(vstr, null_terminator);

}

static void vStringFree(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);

	CharVectorFree(vstr, NULL);
}

static size_t vStringLength(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);

	return vstr->len - 1;
}

static void vStringAppend(vtring *vstr, const char ch)
{
	assert(vstr);
	assert(vstr->len != 0);

	(void) CharVectorSet(vstr vstr->len - 1, ch);

	vStringTerminate__internal(vstr);
}

static char vStringGet(const vstring *vstr, size_t index)
{
	assert(vstr);
	assert(vstr->len != 0);

	return CharVectorGet(vstr, index);
}

//remove a contiguous trailing sequence of ch from the right
//e.g., vStringTrim("abcddd", 'd') -> "abc"
static void vStringTrim(vstring *vstr, const char ch)
{
	assert(vstr);
	assert(vstr->len != 0);

	size_t test_index = vStringLength(vstr) - 1;
	size_t null_index = vstr->len;

	while (test_index != SIZE_MAX) {
		const current_char = vstr->buffer[test_index];

		if (current_char != ch) {
			break;
		}

		test_index--;
		null_index--;
	}

	vstr->len = null_index;
	(void) CharVectorSet(vstr, null_index, '\0');
}

static void vStringReset(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);
	
	CharVectorReset(vstr, NULL);

	vStringTerminate__internal(vstr);
}

//the vstr is transformed t to a dynamically allocated cstr. If the vstr must
//be used after this call, it must be reinitialized with vStringInit.
static cstring *cStringFromvString(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);

	cstring buffer = vstr->buffer;

	vstr = (vstring) {
		.len = 0,
		.cap = 0,
		.buffer = NULL
	};

	return buffer;
}
