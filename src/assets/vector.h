/*
 * @file vector.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Templated vector data structure for shallow copies.
 */

#pragma once

#define declare_vector(type, name) 						\
	typedef struct name##_vector {						\
		int len;							\
		int cap;							\
		type *data;							\
	} name##_vector;

#define api_vector(type, name, qual)						\
	name##_vector *name##_vector_init(void);				\
	int name##_vector_push(name##_vector *self, type data);			\
	void name##_vector_free(name##_vector *self);

#define make_vector(type, name, qual)						\
	declare_vector(type, name)						\
	api_vector(type, name, qual)
