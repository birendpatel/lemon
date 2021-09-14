/**
 * @file vector.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Templated vector data structure for shallow copies.
 *
 * @details This file is plug and play, but with a few words of advice. Vectors
 * use a little memset trick to immediately induce a segementation violation
 * whenever stdlib malloc fails. You will first have to remove the kmalloc
 * wrapper and introduce errors codes to impl_vector_init and impl_vector_push
 * if you want to avoid this behavior. Vectors also intentionally crash if you 
 * attempt to push more than SIZE_MAX elements.
 *
 * Since vector.h was originally designed for the Lemon compiler, this fail
 * fast and die early approach suited the compiler requirements well. YMMV.
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//vector tracing on stderr if VECTOR_TRACE_STDERR is defined.
//Thread IDs are provided but they may collide because the opaque pthread_t
//struct is transformed to a numeric value in a portable manner.
#ifdef VECTOR_TRACE_STDERR
	#include <pthread.h>
	#include <stdio.h>

	#define TID ((void *) pthread_self())
	static const char *fmt = "vector: thread %p: %s\n";
	#define VECTOR_TRACE(msg) fprintf(stderr, fmt, TID, msg)
#else
	#define VECTOR_TRACE(msg) do { } while (0)
#endif

//kmalloc
#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)

//typedef and forward declaration
#define alias_vector(pfix)					               \
typedef struct pfix##_vector pfix##_vector;

//declares a struct with a pfixed tag and elements of type T
#define declare_vector(T, pfix) 					       \
struct pfix##_vector {					                       \
	size_t len;					                       \
	size_t cap;						               \
	T *data;						               \
};

//prototypes
#define api_vector(T, pfix, cls)					       \
cls void pfix##_vector_init(pfix##_vector *self, size_t len, size_t cap);      \
cls void pfix##_vector_free(pfix##_vector *self, void (*vfree) (T));	       \
cls void pfix##_vector_push(pfix##_vector *self, T datum);	               \
cls void pfix##_vector_get(pfix##_vector *self, const size_t i, T *datum);     \
cls T pfix##_vector_set(pfix##_vector *self, const size_t i, T datum);         \
cls void pfix##_vector_reset(pfix##_vector *self, void (*vfree) (T));

//implementation helpers
#define GROW_CAP(x) (x * (size_t) 2)
#define CAP_THRESHOLD (SIZE_MAX / 2)

/*******************************************************************************
 * @def impl_vector_init
 * @brief Initialize and zero-out the first 'len' elements.
 ******************************************************************************/
#define impl_vector_init(T, pfix, cls)				               \
cls void pfix##_vector_init(pfix##_vector *self, size_t len, size_t cap)       \
{                                                                              \
	assert(self);							       \
	assert(len <= cap);						       \
	assert(cap != 0);						       \
									       \
	self->len = len;                                                       \
	self->cap = cap;                                                       \
									       \
	size_t bytes = cap * sizeof(T);					       \
	kmalloc(self->data, bytes);           				       \
                      	 	 	 	 	                       \
	memset(self->data, 0, len * sizeof(T));				       \
									       \
	VECTOR_TRACE("vector initialized");				       \
}

/*******************************************************************************
 * @def impl_vector_free
 * @brief Release data pointer to system.
 * @details This function may be used with the GCC cleanup attribute.
 * @param vfree If vfree is a non-null function pointer then it will be called
 * on each element present in the vector.
 ******************************************************************************/
#define impl_vector_free(T, pfix, cls)					       \
cls void pfix##_vector_free(pfix##_vector *self, void (*vfree) (T))	       \
{									       \
	assert(self);							       \
									       \
	if (self->data) {						       \
		if (vfree) {						       \
			for (size_t i = 0; i < self->len; i++) {               \
				vfree(self->data[i]);			       \
			}         					       \
									       \
			VECTOR_TRACE("vfree successful on vector elements");   \
		}							       \
									       \
		free(self->data);					       \
									       \
		VECTOR_TRACE("returned system resources");		       \
									       \
		self->len = 0;						       \
		self->cap = 0;						       \
		self->data = NULL;					       \
	}                                                                      \
}

/*******************************************************************************
 * @def impl_vector_push
 * @brief Append an element to the end of the vector in amoritized O(1) time.
 ******************************************************************************/
#define impl_vector_push(T, pfix, cls)					       \
cls void pfix##_vector_push(pfix##_vector *self, T datum)	               \
{									       \
	assert(self);							       \
                                                                               \
	if (self->len == SIZE_MAX) {                                           \
		* (int *) 0 = 0; /* segfault */                                \
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
		memcpy(tmp, self->data, self->len * sizeof(T));                \
									       \
		free(self->data);					       \
  									       \
		self->cap = new_cap;					       \
		self->data = tmp;					       \
									       \
		VECTOR_TRACE("grow succeeded");				       \
	}								       \
									       \
	self->data[self->len] = datum;					       \
	self->len++;							       \
    									       \
	VECTOR_TRACE("push successful");				       \
}

/*******************************************************************************
 * @def impl_vector_get
 * @brief Array indexing on vector, O(1).
 * @details This function is intended for situations where array indexing
 * checks are required in debug mode or where the vector struct is opaque.
 ******************************************************************************/
#define impl_vector_get(T, pfix, cls)					       \
cls void pfix##_vector_get(pfix##_vector *self, const size_t i, T *datum)      \
{									       \
	assert(self);                                                          \
	assert(i < self->len);                                                 \
									       \
	*datum = self->data[i];						       \
									       \
	VECTOR_TRACE("get; data copied");				       \
}

/*******************************************************************************
 * @def impl_vector_set
 * @brief Set an element at the specified index to a new element.
 * @return The old element is returned.
 ******************************************************************************/
#define impl_vector_set(T, pfix, cls)					       \
cls T pfix##_vector_set(pfix##_vector *self, const size_t i, T datum)          \
{									       \
	assert(self);							       \
	assert(i < self->len);						       \
									       \
	T old = self->data[i];						       \
	self->data[i] = datum;						       \
									       \
	VECTOR_TRACE("set; data swapped");				       \
									       \
	return old;							       \
}

/*******************************************************************************
 * @def impl_vector_reset
 * @brief Remove all elements from the vector but retain its capacity. After
 * reset, the vector will have a length of zero.
 * @param vfree If vfree is not null, it will be called on each element that
 * is present in the vector before initiating a reset.
 ******************************************************************************/
#define impl_vector_reset(T, pfix, cls)					       \
cls void pfix##_vector_reset(pfix##_vector *self, void (*vfree) (T))	       \
{									       \
	assert(self);							       \
									       \
	if (vfree) {							       \
		for (size_t i = 0; i < self->len; i++) {		       \
			vfree(self->data[i]);				       \
		}							       \
	}								       \
									       \
	VECTOR_TRACE("reset successful");				       \
									       \
	self->len = 0;							       \
}

/*******************************************************************************
 * @def make_vector
 * @brief Create a generic vector named pfix_vector which contains elements
 * of type T and calls methods with storage class cls.
 ******************************************************************************/
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
