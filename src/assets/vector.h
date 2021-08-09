/*
 * @file vector.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Templated vector data structure for shallow copies.
 */

#pragma once

#include "../lemon.h"

#include <stdlib.h>

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
cls int pfix##_vector_init(pfix##_vector *self);			       \
cls void pfix##_vector_free(pfix##_vector *self);			       \
cls int pfix##_vector_push(pfix##_vector *self, const T data);	               \
cls int pfix##_vector_get(pfix##_vector *self, const size_t i, T *data);       \
cls int pfix##_vector_set(pfix##_vector *self, const size_t i, const T data);  \

//implementation helpers
#define START_CAP ((size_t) 8)
#define GROW_CAP(x) (x * (size_t) 2)

/*******************************************************************************
 * @def vector_init
 * @brief Initialize a vector with cap = 8 and a doubling growth rate.
 ******************************************************************************/
#define impl_vector_init(T, pfix, cls)				               \
									       \
cls int pfix##_vector_init(pfix##_vector *self)                                \
{                                                                              \
	assert(self);							       \
									       \
	self->data = malloc(START_CAP * sizeof(T));                            \
									       \
	if (!self->data) {                                                     \
		return LEMON_ENOMEM;                                           \
	}                                                                      \
                      	 	 	 	 	                       \
	self->len = 0;                                                         \
	self->cap = START_CAP;                                                 \
                                   					       \
	return LEMON_ESUCCESS;						       \
}

/*******************************************************************************
 * @def vector_free
 * @brief Release data pointer to system.
 ******************************************************************************/
#define impl_vector_free(T, pfix, cls)					       \
									       \
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
 * @def make_vector
 * @brief Create a generic vector named pfix_vector which contains elements
 * of type T and calls methods with storage class cls. 
 ******************************************************************************/
#define make_vector(T, pfix, cls)					       \
	alias_vector(pfix)  					               \
	declare_vector(T, pfix)					               \
	api_vector(T, pfix, cls)					       \
	impl_vector_init(T, pfix, cls)				               \
	impl_vector_free(T, pfix, cls)
