/**
 * @file vector.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Templated vector data structure for shallow copies.
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//error codes
#define VECTOR_ESUCCESS	0	/**< @brief Function returned successfully */
#define VECTOR_ENOMEM	1	/**< @brief Cannot allocate memory */
#define VECTOR_EFULL	2	/**< @brief Cannot append, vector is full */

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
cls int pfix##_vector_init(pfix##_vector *self, size_t len, size_t cap);       \
cls void pfix##_vector_free(pfix##_vector *self);			       \
cls int pfix##_vector_push(pfix##_vector *self, const T datum);	               \
cls void pfix##_vector_get(pfix##_vector *self, const size_t i, T *datum);     \
cls T pfix##_vector_set(pfix##_vector *self, const size_t i, const T datum);   \

//implementation helpers
#define GROW_CAP(x) (x * (size_t) 2)
#define CAP_THRESHOLD (SIZE_MAX / 2)

/*******************************************************************************
 * @def impl_vector_init
 * @brief Initialize and zero-out the first 'len' elements.
 * @return VECTOR_ENOMEM if dynamic memory allocation fails
 ******************************************************************************/
#define impl_vector_init(T, pfix, cls)				               \
cls int pfix##_vector_init(pfix##_vector *self, size_t len, size_t cap)        \
{                                                                              \
	assert(self);							       \
	assert(len <= cap);						       \
	assert(cap != 0);						       \
									       \
	self->len = len;                                                       \
	self->cap = cap;                                                       \
									       \
	self->data = malloc(cap * sizeof(T));                                  \
									       \
	if (!self->data) {                                                     \
		return VECTOR_ENOMEM;                                          \
	}                                                                      \
                      	 	 	 	 	                       \
	memset(self->data, 0, len * sizeof(T));				       \
									       \
	return VECTOR_ESUCCESS;						       \
}

/*******************************************************************************
 * @def impl_vector_free
 * @brief Release data pointer to system.
 * @details This function may be used with the GCC cleanup attribute.
 ******************************************************************************/
#define impl_vector_free(T, pfix, cls)					       \
cls void pfix##_vector_free(pfix##_vector *self)			       \
{									       \
	assert(self);							       \
									       \
	free(self->data);						       \
									       \
	self->len = 0;							       \
	self->cap = 0;							       \
	self->data = NULL;						       \
}

/*******************************************************************************
 * @def impl_vector_push
 * @brief Append an element to the end of the vector in amoritized O(1) time.
 * @return VECTOR_EFULL if vector is already at max capacity or VECTOR_ENOMEM
 * if dynamic memory allocation fails.
 ******************************************************************************/
#define impl_vector_push(T, pfix, cls)					       \
cls int pfix##_vector_push(pfix##_vector *self, const T datum)                 \
{									       \
	assert(self);							       \
                                                                               \
	if (self->len == SIZE_MAX) {                                           \
		return VECTOR_EFULL;                                           \
	}                                                                      \
									       \
	if (self->len == self->cap) {					       \
		size_t new_cap = 0;					       \
      									       \
		if (self->len >= CAP_THRESHOLD) {			       \
			new_cap = SIZE_MAX;				       \
		} else {						       \
			new_cap = GROW_CAP(self->cap);			       \
		}							       \
									       \
		T *tmp = malloc(new_cap * sizeof(T));			       \
									       \
		if (!tmp) {						       \
			return VECTOR_ENOMEM;				       \
		}							       \
									       \
		memcpy(tmp, self->data, self->len * sizeof(T));                \
									       \
		free(self->data);					       \
  									       \
		self->cap = new_cap;					       \
		self->data = tmp;					       \
	}								       \
									       \
	self->data[self->len] = datum;					       \
	self->len++;							       \
    									       \
	return VECTOR_ESUCCESS;						       \
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
}

/*******************************************************************************
 * @def impl_vector_set
 * @brief Set an element at the specified index to a new element.
 * @return The old element is returned.
 ******************************************************************************/
#define impl_vector_set(T, pfix, cls)					       \
cls T pfix##_vector_set(pfix##_vector *self, const size_t i, const T datum)    \
{									       \
	assert(self);							       \
	assert(i < self->len);						       \
									       \
	T old = self->data[i];						       \
	self->data[i] = datum;						       \
									       \
	return old;							       \
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
	impl_vector_set(T, pfix, cls)
