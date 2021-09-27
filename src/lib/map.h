/**
 * @file map.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Templated associative array implemented via a hash table.
 *
 * @details This file is plug and play, but with a few words of advice. Maps use
 * a memset trick to immediately induce a segmentation violation whenever stdlib
 * allocs fail. You must remove the kmalloc wrapper and introduce error codes
 * where appropriate if you want to avoid this behavior.
 *
 * It is recommended that you run initially with assertions enabled as most
 * API calls use assertions to validate preconditions.
 */

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//tracing provides Thread IDs but they may collide. The opaque pthread_t struct
//uses a void cast for portability, but this is not an injective function.
#ifdef MAP_TRACE_STDERR
	#include <pthread.h>
	#include <stdio.h>

	#define TID ((void *) pthread_self())
	static const char *fmt = "map: thread %p: %s\n";
	#define MAP_TRACE(msg) fprintf(stderr, fmt, TID, msg)
#else
	#define MAP_TRACE(msg) do { } while (0)
#endif

//kmalloc; induce a segfault if malloc fails
#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)

#ifndef MAP_ESUCCESS
	#error "user must implement the MAP_ESUCCESS integer error code"
#endif

//insertion error; key already exists in map.
#ifndef MAP_EEXISTS
	#error "user must implement the MAP_EEXISTS integer error code"
#endif

#ifndef MAP_EFULL
	#error "user must implement the MAP_EFULL integer error code"
#endif

#define alias_map(pfix)							       \
typedef struct pfix##_map pfix##_map;

//hash table slot
#define declare_pair(K, V, pfix)					       \
struct pfix##_pair {							       \
	K key;								       \
	V value;							       \
	bool closed;							       \
};

//the user should not write directly to the map members. Doing so will corrupt
//either the load factor or the linear probe.
#define declare_map(pfix, K, V)						       \
struct pfix##_map {							       \
	uint64_t len;							       \
	uint64_t cap;							       \
	pfix##_pair *data;						       \
};

#ifndef __SIZEOF_INT128__
	#error "map.h requires 128-bit integer support"
#endif

//range reduction wrapper over the user-provided hash function.
//
//this uses a bit hack typically used in embedded devices and some compilers
//to optimize a multiply + divide. It uses far fewer CPU cycles than modulo.
//See comments from Jacob Vecht and Bill Holland at:
//
//https://embeddedgurus.com/stack-overflow/2011/02/
//efficient-c-tip-13-use-the-modulus-operator-with-caution/#comment-29259
#define impl_hash(K, pfix, cls, hfuncptr)				       \
cls uint64_t pfix##_hash(const K key, uint64_t m)			       \
{									       \
	uint64_t hash = hfuncptr (key);					       \
	return (uint64_t) (((__uint128_t) hash * (__uint128_t) m) >> 64);      \
}

#define impl_equal(K, pfix, cls, eqptr)					       \
cls bool pfix##_equal(K key_1, K key_2)					       \
{									       \
	return eqptr(key_1, key_2);					       \
}

//at compile time, optimizations will remove the branch when kfptr is null and
//reduce the entire function to a no-op.
#define impl_kfree(K, pfix, cls, kfptr)					       \
cls void pfix##_kfree(K value)						       \
{									       \
	if (kfptr) {							       \
		kfptr(K);						       \
	}								       \
}

#define impl_vfree(V, pfix, cls, vfptr)					       \
cls void pfix##_vfree(V value)						       \
{									       \
	if (vfptr) {							       \
		vfptr(V);						       \
	}								       \
}

//prototypes
#define api_map(K, V, pfix, cls)					       \
cls uint64_t pfix##_hash(const K key, uint64_t m);			       \
cls bool pfix##_equal(K key_1, K key_2);				       \
cls void pfix##_kfree(K value);					               \
cls void pfix##_vfree(V value);						       \
cls void pfix##_map_init(pfix##_map *self, uint64_t cap);		       \
cls void pfix##_map_free(pfix##_map *self);				       \
cls int pfix##_map_insert(pfix##_map *self, const K key, const V value);       \
cls int pfix##_map_probe(pfix##_map *self, const K key, const V value);        \
cls void pfix##_map_resize(pfix##_map *self);				       \
cls void pfix##_map_search(pfix##_map *self, const K key, V *value);

/*******************************************************************************
 * @def impl_map_init
 * @brief Initialize a new map with a starting capacity.
 ******************************************************************************/
#define impl_map_init(K, V, pfix, cls)					       \
cls void pfix##_map_init(pfix##_map *self, uint64_t cap)		       \
{									       \
	assert(self);							       \
	assert(cap != 0);						       \
									       \
	self->len = 0;							       \
	self->cap = cap;						       \
									       \
	size_t bytes = sizeof(pfix##_pair) * cap;	 		       \
	kmalloc(self->data, bytes);					       \
	memset(self->data, 0, bytes);		        		       \
									       \
	MAP_TRACE("map initialized");					       \
}

/*******************************************************************************
 * @def impl_map_free
 * @brief Release map resources to system. This function is safe to call if the
 * map has not been initialized. This function may be used with the GCC cleanup
 * attribute.
 ******************************************************************************/
#define impl_map_free(K, V, pfix, cls)					       \
cls void pfix##_map_free(pfix##_map *self)                                     \
{									       \
	assert(self);							       \
									       \
	if (!self->data) {						       \
		MAP_TRACE("map not initialized; nothing to free.");            \
		return;							       \
	}								       \
									       \
	MAP_TRACE("freeing map elements");				       \
	for (uint64_t i = 0; i < self->cap; i++) {			       \
		if (self->data[i].closed) { 		                       \
			pfix##_kfree(self->data[i].key);		       \
			pfix##_vfree(self->data[i].value);		       \
		}							       \
	}							               \
									       \
	MAP_TRACE("freeing map data pointer");				       \
	free(self->data);						       \
}

#define LOAD_FACTOR_THRESHOLD 0.5

/*******************************************************************************
 * @def impl_map_insert
 * @brief Insert a key-value pair into the map. If the map has reached its
 * critical load factor, a resize will occur before the insertion can continue.
 * @return If the key already exists in the map, then the insertion fails and
 * the MAP_EEXISTS error code is returned. Otherwise MAP_ESUCCESS is returned.
 ******************************************************************************/
#define impl_map_insert(K, V, pfix, cls)				       \
cls int pfix##_map_insert(pfix##_map *self, const K key, const V value)        \
{									       \
	assert(self);							       \
	assert(self->data);						       \
									       \
	if (self->len == UINT64_MAX) {					       \
		return MAP_EFULL;					       \
	}								       \
									       \
	const double load_factor = self->len / (double) self->cap;	       \
									       \
	if (self->cap != UINT64_MAX && load_factor > LOAD_FACTOR_THRESHOLD) {  \
		pfix##_map_resize(self);				       \
	}							 	       \
									       \
	return pfix##_map_probe(self, K, V);				       \
}

/*******************************************************************************
 * @def impl_map_probe
 * @brief Insertion via linear probing. Assumes there is at least one empty
 * slot in the hash table. Do not call directly from user code.
 * @return MAP_EEXISTS if K is already in the hash table regardless of V.
 ******************************************************************************/
#define impl_map_probe(K, V, pfix, cls)					       \
cls int pfix##_map_probe(pfix##_map *self, const K key, const V value)         \
{									       \
	assert(self);							       \
	assert(self->data);						       \
								               \
	uint64_t pos = pfix##_hash(K, self->cap);			       \
									       \
	while (true) {							       \
		if (!self->data[pos].closed) {				       \
			 break;						       \
		}							       \
									       \
		if (pfix##_equal(K, self->data[pos].key)) {		       \
			return MAP_EEXISTS;				       \
		}							       \
									       \
		pos = pos + 1 >= self->cap ? 0 : pos + 1;		       \
	}								       \
								               \
	self->data[pos].key = K;					       \
	self->data[pos].value = V;					       \
	self->data[pos].closed = true;					       \
	self->len++;						               \
									       \
	return MAP_ESUCCESS;						       \
}

/*******************************************************************************
 * @def impl_map_resize
 * @brief Internal function; Do not call directly from user code.
 ******************************************************************************/
#define impl_map_resize(K, V, pfix, cls)				       \
cls void pfix##_map_resize(pfix##_map *self)
{
	assert(self);
	assert(self->data);

	pfix##_map new = {0};
	uint64_t cap = 0;

	if (self->len >= (UINT64_MAX / 2)) {
		cap = UINT64_MAX;
	} else {
		cap = self->len * 2;
	}

	pfix##_map_init(&curr, cap);

	for (uint64_t i = 0; i < self->cap; i++) {
		if (self->data[i].closed) {
			K key = self->data[i].key;
			V value = self->data[i].value;

			(void) pfix##_map_probe(&curr, key, value);
		}
	}

	pfix##_map_free(self);

	*self = new;
}

/*******************************************************************************
 * @def make_map
 * @brief Create a generic map named pfix_map which maps hashable elements of
 * type K to shallow-copyable values of type V. Methods have storage class cls.
 *
 * @details elements are hashed via the provided function pointer hfuncptr. Its
 * signature must be uint64_t (*)(K). If the provided function pointer violates
 * this signature, there are no guarantees that the map will work as intended.
 *
 * The output of the hash function will be mapped to a smaller range [0, m) for
 * some m < UINT64_MAX. Your choice of hash function should not assume that the
 * mapping is performed via modulo.
 *
 * The eqptr is a function pointer used to compare two keys. Its signature must
 * be bool (*)(K, K) and should return true only if the two keys are identical.
 *
 * vfptr and kfptr have signatures void (*)(K) and void(*)(V), respectively.
 * They may be NULL. These functions are invoked on keys and/or values when
 * the map is free'd or when it performs cleanup operations during resizing.
 ******************************************************************************/
#define make_map(K, V, pfix, cls, hfuncptr, eqptr, vfptr, kfptr)	       \
	alias_map(pfix)							       \
	declare_pair(K, V, pfix)					       \
	declare_map(pfix, K, V)						       \
	api_map(K, V, pfix, cls)					       \
	impl_hash(K, pfix, cls, hfuncptr)				       \
	impl_equal(K, pfix, cls, eqptr)					       \
	impl_kfree(K, pfix, cls, kfptr)					       \
	impl_vfree(V, pfix, cls, vfptr)					       \
	impl_map_init(K, V, pfix, cls)					       \
	impl_map_free(K, V, pfix, cls)					       \
	impl_map_insert(K, V, pfix, cls)		                       \
	impl_map_probe(K, V, pfix, cls)					       \
	impl_map_resize(K, V, pfix, cls)
