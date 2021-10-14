// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// A simple dynamic array data structure implemented via C-style templating.
// Some operations performed on vectors may cause the application program to
// abort. Enable vector tracing to locate the root cause.
//
// If application code covertly modifies vector struct members it is highly
// recommended to first compile the code with stdlib assertions enabled. 

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

//thread IDs may collide; the pthread_t transformation is not injective
#ifdef VECTOR_TRACE_STDERR
	#include <pthread.h>
	#include <stdio.h>

	static void VectorTrace(const char *msg)
	{
		const void *thread_id = ((void *) pthread_self());
		const char *fmt = "vector: thread %p: %s\n";

		fprintf(stderr, fmt, thread_id, msg);
	}
#else
	static void VectorTrace(__attribute__((unused)) const char *msg)
	{
		return;
	}
#endif

static void *VectorMalloc(size_t bytes)
{
	void *region = malloc(bytes);

	if (!region) {
		VectorTrace("malloc failed; aborting program");
		abort();
	}

	return region;
}

static void *VectorRealloc(void *ptr, size_t bytes)
{
	assert(ptr);
	assert(bytes != 0);

	void *region = realloc(ptr, bytes);

	if (!region) {
		VectorTrace("realloc failed; aborting program");
		abort();
	}

	return region;
}

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

//application code may directly modify vector members provided sizeof(buffer) ==
//cap, len <= cap, and buffer is a non-null pointer compatible with stdlib free
//and (m)(re)alloc calls. Provided these three restrictions are held, any covert 
//modifications or reassignments will not hinder subsequent vector operations.
//
//these restrictions are enforced during vector operations by stdlib assertions.
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
cls void pfix##VectorFree(pfix##_vector *, void (*)(T));           	       \
cls void pfix##VectorPush(pfix##_vector *, const T);	                       \
cls T pfix##VectorGet(pfix##_vector *, const size_t index);                    \
cls T pfix##VectorSet(pfix##_vector *, const size_t , const T);                \
cls void pfix##VectorReset(pfix##_vector *, void (*) (T));	               \

#define impl_vector_init(T, pfix, cls)				               \
cls pfix##_vector pfix##VectorInit(const size_t len, const size_t cap)         \
{                                                                              \
	assert(len <= cap);						       \
									       \
	pfix##_vector v = {						       \
		.len = len,						       \
		.cap = cap,						       \
		.buffer = NULL						       \
	};								       \
									       \
	const size_t bytes = cap * sizeof(T);				       \
	v.buffer = VectorMalloc(bytes);  				       \
									       \
	VectorTrace("initialized");				               \
}

//this function may be used with GCC __attribute__((cleanup(f))). If the pointer
//passed to this function was dynamically allocated then the members are cleaned 
//up but the memory allocated to the struct itself is not released.
//
//if the vfree parameter is non-null, it is called on each element in the vector
//buffer before the buffer itself is freed.
#define impl_vector_free(T, pfix, cls)					       \
cls void pfix##VectorFree(pfix##_vector *self, void (*vfree) (T))	       \
{									       \
	assert(self);							       \
	assert(self->len <= self->cap);					       \
	assert(sizeof(self->buffer)/sizeof(T) == self->cap);		       \
									       \
	if (self->buffer) {						       \
		if (vfree) {						       \
			for (size_t i = 0; i < self->len; i++) {               \
				vfree(self->buffer[i]);			       \
			}         					       \
									       \
			VectorTrace("buffer elements released");	       \
		}							       \
									       \
		free(self->buffer);					       \
									       \
		VectorTrace("internal buffer released");                       \
									       \
		self->len = 0;						       \
		self->cap = 0;						       \
		self->buffer= NULL;					       \
	} else {							       \
		VectorTrace("vector not initialized; nothing to free");	       \
	}								       \
}

//elements pushed to a vector are always copied to the internal buffer by value
#define impl_vector_push(T, pfix, cls)					       \
cls void pfix##VectorPush(pfix##_vector *self, const T datum)                  \
{									       \
	assert(self);							       \
	assert(self->len <= self->cap);                                        \
	assert(sizeof(self->buffer)/sizeof(T) == self->cap);	               \
	assert(self->buffer);						       \
                                                                               \
	if (self->len == SIZE_MAX) {                                           \
		VectorTrace("reached absolute capacity; aborting program");    \
		abort();			                               \
	}                                                                      \
									       \
	if (self->len == self->cap) {					       \
		VectorTrace("maximum capacity; reallocating...");	       \
									       \
		self->cap = VectorGrow(self->cap);                             \
		self->buffer = VectorRealloc(self->buffer, self->cap);	       \
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
cls T pfix##VectorGet(pfix##_vector *self, const size_t index)                 \
{									       \
	assert(self);                                                          \
	assert(self->len <= self->cap);					       \
	assert(sizeof(self->buffer)/sizeof(T) == self->cap);		       \
	assert(index < self->len);                                             \
									       \
	return self->buffer[index];					       \
}

#define impl_vector_set(T, pfix, cls)					       \
cls T pfix##VectorSet(pfix##_vector *self, const size_t index, const T datum)  \
{									       \
	assert(self);                                                          \
	assert(self->len <= self->cap);					       \
	assert(sizeof(self->buffer)/sizeof(T) == self->cap);		       \
	assert(index < self->len);                                             \
									       \
	T old_element = self->buffer[index];				       \
								               \
	self->buffer[index] = datum;					       \
									       \
	return old_element;						       \
}

//if the vfree parameter is non-null, it is called on each element in the vector
//buffer before the reset operation is performed.
#define impl_vector_reset(T, pfix, cls)					       \
cls void pfix##VectorReset(pfix##_vector *self, void (*vfree) (T))	       \
{									       \
	assert(self);                                                          \
	assert(self->len <= self->cap);					       \
	assert(sizeof(self->buffer)/sizeof(T) == self->cap);		       \
									       \
	if (vfree) {							       \
		for (size_t i = 0; i < self->len; i++) {		       \
			vfree(self->buffer[i]);				       \
		}							       \
									       \
		VectorTrace("buffer elements released");		       \
	}								       \
									       \
	self->len = 0;							       \
									       \
	VectorTrace("reset successful");				       \
}

//------------------------------------------------------------------------------

//make_vector declares a vector<T> type named pfix_vector which may contain
//elements of type T and only type T. Vector operations have storage class cls.
//
//like any C code, the abstraction has its limitations. make_vector may only
//be used where sizeof(T) is available at compile time before make_vector. When
//this is not possible each component macro must be expanded separately.
#define make_vector(T, pfix, cls)					       \
	alias_vector(pfix)  					               \
	declare_vector(T, pfix)					               \
	api_vector(T, pfix, cls)					       \
	impl_vector_init(T, pfix, cls)				               \
	impl_vector_free(T, pfix, cls)					       \
	impl_vector_push(T, pfix, cls)					       \
	impl_vector_get(T, pfix, cls)					       \
	impl_vector_set(T, pfix, cls)					       \
	impl_vector_reset(T, pfix, cls)

//where the application code requires functions names to be PascalCase, the
//make_vector macro creates type names as 'Type_vector' not 'type_vector'. The
//awkward capital letter can be obscured with this macro. The macro also
//provides some similarity to templating syntax in other languages like C++.
#define vector(pfix) pfix##_vector
