// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Simple dynamic arrays implemented via C-style templates

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//tracing provides thread IDs but two different threads may have an ID collision
//because the pthread_t transformation is not an injective function.
#ifdef VECTOR_TRACE_STDERR
	#include <pthread.h>
	#include <stdio.h>
	#define TID ((void *) pthread_self())
	static const char *fmt = "vector: thread %p: %s\n";
	#define VECTOR_TRACE(msg) fprintf(stderr, fmt, TID, msg)
#else
	#define VECTOR_TRACE(msg) do { } while (0)
#endif

#define kmalloc(target, bytes)                                                 \
        do {                                                                   \
                target = malloc(bytes);                                        \
                                                                               \
                if (!target) {                                                 \
                        abort();                                               \
                }                                                              \
        } while (0)

#define alias_vector(pfix)					               \
typedef struct pfix##_vector pfix##_vector;

//buffer has a maximum capacity of cap and current size len.
#define declare_vector(T, pfix) 					       \
struct pfix##_vector {					                       \
	size_t len;					                       \
	size_t cap;						               \
	T *buffer;						               \
};

#define api_vector(T, pfix, cls)					       \
cls void pfix##VectorInit(pfix##_vector *self, size_t len, size_t cap);        \
cls void pfix##VectorFree(pfix##_vector *self, void (*vfree) (T));	       \
cls void pfix##VectorPush(pfix##_vector *self, T datum);	               \
cls void pfix##VectorGet(pfix##_vector *self, const size_t i, T *datum);       \
cls T pfix##VectorSet(pfix##_vector *self, const size_t i, T datum);           \
cls void pfix##VectorReset(pfix##_vector *self, void (*vfree) (T));	       \
cls void pfix##VectorNewCopy(pfix##_vector *dest, pfix##_vector *src);	       \
cls void pfix##VectorAdopt(pfix##vector *dest, void *src, size_t bytes);

#define impl_vector_init(T, pfix, cls)				               \
cls void pfix##VectorInit(pfix##_vector *self, size_t len, size_t cap)         \
{                                                                              \
	assert(self);							       \
	assert(len <= cap);						       \
	assert(cap != 0);						       \
									       \
	self->len = len;                                                       \
	self->cap = cap;                                                       \
									       \
	size_t bytes = cap * sizeof(T);					       \
	kmalloc(self->buffer, bytes);           		               \
                      	 	 	 	 	                       \
	memset(self->buffer, 0, len * sizeof(T));			       \
									       \
	VECTOR_TRACE("vector initialized");				       \
}

//may be used with GCC cleanup if the vector is located on stack.
//vfree is called on each element present in the vector before the vector buffer
//is freed, unless vfree is NULL.
#define impl_vector_free(T, pfix, cls)					       \
cls void pfix##VectorFree(pfix##_vector *self, void (*vfree) (T))	       \
{									       \
	assert(self);							       \
									       \
	if (self->buffer) {						       \
		if (vfree) {						       \
			for (size_t i = 0; i < self->len; i++) {               \
				vfree(self->buffer[i]);			       \
			}         					       \
									       \
			VECTOR_TRACE("vfree successful on vector elements");   \
		}							       \
									       \
		free(self->buffer);					       \
									       \
		VECTOR_TRACE("returned system resources");		       \
									       \
		self->len = 0;						       \
		self->cap = 0;						       \
		self->buffer= NULL;					       \
	}                                                                      \
}

#define GROW_CAP(x) (x * (size_t) 2)

#define CAP_THRESHOLD (SIZE_MAX / 2)

#define impl_vector_push(T, pfix, cls)					       \
cls void pfix##VectorPush(pfix##_vector *self, T datum)	                       \
{									       \
	assert(self);							       \
                                                                               \
	if (self->len == SIZE_MAX) {                                           \
		abort();			                               \
	}                                                                      \
									       \
	if (self->len == self->cap) {					       \
									       \
		VECTOR_TRACE("vector at capacity; growing");		       \
									       \
		size_t new_cap = 0;					       \
      									       \
		if (self->len >= CAP_THRESHOLD) {			       \
			new_cap = SIZE_MAX;				       \
		} else {						       \
			new_cap = GROW_CAP(self->cap);			       \
		}							       \
									       \
		T *tmp = NULL;					               \
		size_t bytes = new_cap * sizeof(T);			       \
		kmalloc(tmp, bytes);		                               \
									       \
		memcpy(tmp, self->buffer, self->len * sizeof(T));              \
									       \
		free(self->buffer);					       \
  									       \
		self->cap = new_cap;					       \
		self->buffer= tmp;					       \
									       \
		VECTOR_TRACE("grow succeeded");				       \
	}								       \
									       \
	self->buffer[self->len] = datum;				       \
	self->len++;							       \
    									       \
	VECTOR_TRACE("push successful");				       \
}

#define impl_vector_get(T, pfix, cls)					       \
cls void pfix##VectorGet(pfix##_vector *self, const size_t i, T *datum)      \
{									       \
	assert(self);                                                          \
	assert(i < self->len);                                                 \
									       \
	*datum = self->buffer[i];					       \
									       \
	VECTOR_TRACE("get; data copied");				       \
}

//return the old element
#define impl_vector_set(T, pfix, cls)					       \
cls T pfix##VectorSet(pfix##_vector *self, const size_t i, T datum)          \
{									       \
	assert(self);							       \
	assert(i < self->len);						       \
									       \
	T old = self->buffer[i];					       \
	self->buffer[i] = datum;					       \
									       \
	VECTOR_TRACE("set; data swapped");				       \
									       \
	return old;							       \
}

//vfree is called on each element in the vector before initiating a reset, unless
//vfree is NULL.
#define impl_vector_reset(T, pfix, cls)					       \
cls void pfix##VectorReset(pfix##_vector *self, void (*vfree) (T))	       \
{									       \
	assert(self);							       \
									       \
	if (vfree) {							       \
		for (size_t i = 0; i < self->len; i++) {		       \
			vfree(self->buffer[i]);				       \
		}							       \
	}								       \
									       \
	VECTOR_TRACE("reset successful");				       \
									       \
	self->len = 0;							       \
}

//init the dest vector as a copy of the src
#define impl_vector_new_copy(T, pfix, cls)				       \
cls void pfix##VectorNewCopy(pfix##_vector *dest, pfix##_vector *src)	       \
{									       \
	assert(dest);							       \
	assert(src);						 	       \
									       \
	pfix##VectorInit(dest, src->len, src->cap);			       \
									       \
	size_t bytes = sizeof(T) * src->len;				       \
						   			       \
	memcpy(dest->buffer, src->buffer, bytes);			       \
}

//init the dest vector using the heap buffer src as the underlying vec buffer
#define impl_vector_new_adopt(T, pfix, cls)				       \
cls void pfix##VectorAdopt(pfix##vector *dest, void *src, size_t bytes)        \
{									       \
	assert(dest);							       \
	assert(src);							       \
									       \
	dest->buffer = src;						       \
	dest->len = bytes;						       \
	dest->cap = bytes;						       \
}

//create a generic vector<T> named pfix_vector containing elements of type T and
//functions with storage class cls.
#define make_vector(T, pfix, cls)					       \
	alias_vector(pfix)  					               \
	declare_vector(T, pfix)					               \
	api_vector(T, pfix, cls)					       \
	impl_vector_init(T, pfix, cls)				               \
	impl_vector_free(T, pfix, cls)					       \
	impl_vector_push(T, pfix, cls)					       \
	impl_vector_get(T, pfix, cls)					       \
	impl_vector_set(T, pfix, cls)					       \
	impl_vector_reset(T, pfix, cls)					       \
	impl_vector_new_copy(T, pfix, cls)				       \
	impl_vector_new_adopt(T, pfix, cls)
