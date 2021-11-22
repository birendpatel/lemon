// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.  //
// A simple dynamic array data structure implemented via C-style templating.  // Some operations performed on vectors may cause the application program to
// abort. Enable vector tracing to locate the root cause.
//
// If application code covertly modifies vector struct members it is highly
// recommended to first compile the code with stdlib assertions enabled. 

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "xerror.h"

#ifdef VECTOR_TRACE
	#define VectorTrace(msg, ...) xerror_trace(msg, ##__VA_ARGS__)
#else
	#define VectorTrace(msg, ...)
#endif

static size_t VectorGrow(size_t curr_capacity)
{
	const size_t overflow_threshold = SIZE_MAX / 2;
	const size_t growth_rate = 2;

	if (curr_capacity == 0) {
		return 1;
	}

	if (curr_capacity >= overflow_threshold) {
		return SIZE_MAX;
	}

	return curr_capacity * growth_rate;
}

//------------------------------------------------------------------------------

//application code may directly modify vector members. After any modification
//the vector must be equal to the default vector {0, 0, NULL} or the following 
//restrictions must be met:
//
//1. buffer != NULL
//2. maximum elements in buffer == cap
//3. len <= cap
//4. buffer is compatible with the utils arena allocator 
//
//Provided these restrictions are met, any convert modification or reassignment
//will not hinder subsequent vector operations. If the vector is the default
//then no subsequent operation is well-defined other than VectorInit.
#define declare_vector(T, pfix) 					       \
struct pfix##_vector {					                       \
	size_t len;					                       \
	size_t cap;						               \
	T *buffer;						               \
};

#define alias_vector(pfix)					               \
typedef struct pfix##_vector pfix##_vector;

#define api_vector(T, pfix, cls)					       \
cls pfix##_vector pfix##VectorInit(const size_t, const size_t);	               \
cls void pfix##VectorPush(pfix##_vector *, T);   	                       \
cls T pfix##VectorGet(const pfix##_vector *, const size_t);                    \
cls void pfix##VectorReset(pfix##_vector *);				       \
cls T pfix##VectorSet(pfix##_vector *, const size_t , T);

#define VECTOR_DEFAULT_CAPACITY ((size_t) 8)

#define impl_vector_init(T, pfix, cls)				               \
cls pfix##_vector pfix##VectorInit(const size_t len, const size_t cap)         \
{                                                                              \
	assert(len <= cap);						       \
									       \
	VectorTrace("initializing vector");				       \
									       \
	pfix##_vector v = {						       \
		.len = len,						       \
		.cap = cap,						       \
		.buffer = NULL						       \
	};								       \
									       \
	const size_t bytes = cap * sizeof(T);				       \
	v.buffer = ArenaAllocate(bytes);				       \
									       \
	VectorTrace("initialized");				               \
								               \
	return v;							       \
}

//elements pushed to a vector are always copied to the internal buffer by value
//
//the datum paramater cannot be qualified with 'const' because if T is a pointer
//then the macro expands such that a pointer to a const will be pushed to the
//buffer. But the buffer contains non-const elements. Therefore the compiler may
//trigger a -Wdiscarded-qualifiers warning.
#define impl_vector_push(T, pfix, cls)					       \
cls void pfix##VectorPush(pfix##_vector *self, T datum)                        \
{									       \
	assert(self);							       \
	assert(self->len <= self->cap);                                        \
	assert(self->buffer);						       \
                                                                               \
	if (self->len == SIZE_MAX) {                                           \
		VectorTrace("reached absolute capacity; aborting program");    \
		abort();			                               \
	}                                                                      \
									       \
	if (self->len == self->cap) {					       \
		VectorTrace("maximum capacity %zu reached", self->cap);	       \
		VectorTrace("(%zu bytes)", self->cap * sizeof(T));             \
									       \
		self->cap = VectorGrow(self->cap);                             \
									       \
		VectorTrace("realloc buffer to %zu elements", self->cap);      \
		VectorTrace("(%zu bytes)", self->cap * sizeof(T));             \
								               \
		const size_t bytes = self->cap * sizeof(T);		       \
		self->buffer = ArenaReallocate(self->buffer, bytes);           \
									       \
		VectorTrace("reallocated");                                    \
	}								       \
									       \
	self->buffer[self->len] = datum;				       \
	self->len++;							       \
    									       \
	VectorTrace("push successful");					       \
}

#define impl_vector_get(T, pfix, cls)					       \
cls T pfix##VectorGet(const pfix##_vector *self, const size_t index)           \
{									       \
	assert(self);                                                          \
	assert(self->buffer);                                                  \
	assert(self->len <= self->cap);					       \
	assert(index < self->len);                                             \
									       \
	return self->buffer[index];					       \
}

#define impl_vector_set(T, pfix, cls)					       \
cls T pfix##VectorSet(pfix##_vector *self, const size_t index, T datum)        \
{									       \
	assert(self);                                                          \
	assert(self->buffer);						       \
	assert(self->len <= self->cap);					       \
	assert(index < self->len);                                             \
									       \
	T old_element = self->buffer[index];				       \
								               \
	self->buffer[index] = datum;					       \
									       \
	return old_element;						       \
}

#define impl_vector_reset(T, pfix, cls)					       \
cls void pfix##VectorReset(pfix##_vector *self)		         	       \
{									       \
	assert(self);                                                          \
	assert(self->buffer);                                                  \
	assert(self->len <= self->cap);					       \
									       \
	self->len = 0;							       \
									       \
	VectorTrace("reset successful");				       \
}

//------------------------------------------------------------------------------

//make_vector declares a vector<T> type named pfix_vector which may contain
//values of type T, and only type T. Vector operations have storage class cls.
//If sizeof(T) is not available at compile time before make_vector then each
//component macro must be expanded separately.
#define make_vector(T, pfix, cls)					       \
	alias_vector(pfix)  					               \
	declare_vector(T, pfix)					               \
	api_vector(T, pfix, cls)					       \
	impl_vector_init(T, pfix, cls)				               \
	impl_vector_push(T, pfix, cls)					       \
	impl_vector_get(T, pfix, cls)					       \
	impl_vector_set(T, pfix, cls)					       \
	impl_vector_reset(T, pfix, cls)

#define vector(pfix) pfix##_vector
