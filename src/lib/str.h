// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
// 
// This header provides a dynamic string data structure. The vstring is a 
// shallow wrapper over a vector<char> whose internal buffer is guaranteed
// to be null-terminated.

#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "vector.h"

make_vector(char, Char, static)

//-----------------------------------------------------------------------------
//cstring

typedef char cstring;

//no strdup, will cause memory leak without arena
static cstring *cStringDuplicate(const cstring *src)
{
	cstring *dest = allocate(sizeof(char) * (strlen(src) + 1));

	strcpy(dest, src);

	return dest;
}

static cstring *cStringFromView(const char *data, const size_t len)
{
	const size_t bytes = sizeof(char) * len;

	char *new = allocate(bytes + 1);

	memcpy(new, data, bytes);

	new[len] = '\0';

	return new;
}

//-----------------------------------------------------------------------------
//vstring

typedef Char_vector vstring;

static vstring vStringInit(const size_t);
static void vStringTerminate__internal(vstring *);
static size_t vStringLength(vstring *);
static void vStringAppend(vstring *, const char);
static char vStringGet(const vstring *, size_t);
static void vStringTrim(vstring *, const char);
static void vStringReset(vstring *);
static cstring *cStringFromvString(vstring *);
static void vStringAppendcString(vstring *, const cstring *);
static void vStringAppendIntMax(vstring *, intmax_t);

static vstring vStringInit(const size_t capacity)
{
	const size_t default_length = 0;

	vstring vstr = CharVectorInit(default_length, capacity);

	vStringTerminate__internal(&vstr);

	return vstr;
}

static void vStringTerminate__internal(vstring *vstr)
{
	assert(vstr);

	const char null_terminator = '\0';

	CharVectorPush(vstr, null_terminator);

}

static size_t vStringLength(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);

	return vstr->len - 1;
}

static void vStringAppend(vstring *vstr, const char ch)
{
	assert(vstr);
	assert(vstr->len != 0);

	size_t null_index = vstr->len - 1;

	(void) CharVectorSet(vstr, null_index, ch);

	vStringTerminate__internal(vstr);
}

//faster than vStringAppend for bulk copying
static void vStringAppendcString(vstring *vstr, const cstring *cstr)
{
	assert(vstr);
	assert(vstr->len != 0);
	assert(cstr);

	//synthetically remove the null terminator
	vstr->len -= 1;

	while (*cstr) {
		CharVectorPush(vstr, *cstr);
		cstr++;
	}

	vStringTerminate__internal(vstr);
}

//append the string representation of the input number to vstr
static void vStringAppendIntMax(vstring *vstr, intmax_t number)
{
	assert(vstr);
	assert(vstr->len != 0);

	const int length = snprintf(NULL, 0, "%" PRIdMAX "", number);
	assert(length > 0);

	cstring *digits = allocate((size_t) length * sizeof(char) + 1);
	digits[length] = '\0';
	snprintf(digits, (size_t) length, "%" PRIdMAX "", number);

	vStringAppendcString(vstr, digits);	
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

	size_t null_index = vStringLength(vstr);
	size_t test_index = null_index - 1;

	assert(null_index == vstr->len - 1);

	while (test_index != SIZE_MAX) {
		const char current_char = vstr->buffer[test_index];

		if (current_char != ch) {
			break;
		}

		test_index--;
		null_index--;
	}

	vstr->len = null_index + 1;
	(void) CharVectorSet(vstr, null_index, '\0');
}

static void vStringReset(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);
	
	CharVectorReset(vstr);

	vStringTerminate__internal(vstr);
}

//the vstr is transformed to a dynamically allocated cstr. If the vstr must
//be used after this call, it must be reinitialized with vStringInit.
static cstring *cStringFromvString(vstring *vstr)
{
	assert(vstr);
	assert(vstr->len != 0);

	cstring *buffer = vstr->buffer;

	*vstr = (vstring) {
		.len = 0,
		.cap = 0,
		.buffer = NULL
	};

	return buffer;
}
