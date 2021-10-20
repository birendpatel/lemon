// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Associative array from string keys to any type T, implemented as a linear 
// probing hash table.

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "str.h"

#ifdef __SIZEOF_INT128__
	#define uint128_t __uint128_t
#else
	#error "map.h requires 128-bit integer support"
#endif

//-----------------------------------------------------------------------------

//thread IDs may collide; the pthread_t transformation is not injective
#ifdef MAP_TRACE_STDERR
	#include <pthread.h>
	#include <stdio.h>

	static void MapTrace(const cstring *msg)
	{
		const void *thread_id = ((void *) pthread_self());
		const cstring *fmt = "map: thread %p: %s\n";

		(void) fprintf(stderr, fmt, thread_id, msg);
	}
#else
	static void MapTrace(__attribute__((unused)) const cstring *msg)
	{
		return;
	}
#endif

__attribute__((malloc)) static void *MapMalloc(const size_t bytes)
{
	void *region = malloc(bytes);

	if (!region) {
		MapTrace("malloc failed; aborting program");
		abort();
	}

	return region;
}

__attribute__((calloc)) static void *MapCalloc(const size_t bytes)
{
	void *region = calloc(bytes, sizeof(char));

	if (!region) {
		MapTrace("calloc failed; aborting program");
		abort();
	}

	return region;
}

//------------------------------------------------------------------------------

//the public domain Fowler-Noll-Vo 1-Alternate 64-bit hash function.
//reference: http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-1a
static uint64_t MapFNV1a(const cstring *cstr)
{
	assert(cstr);
	
	const uint64_t fnv_offset = (uint64_t) 14695981039346656037ULL;
	const uint64_t fnv_prime  = (uint64_t) 1099511628211ULL;

	uint64_t hash = fnv_offset;

	const uint8_t *buffer = (uint8_t *) cstr;

	while (*buffer) {
		hash ^= (uint64_t) *buffer;
		hash *= fnv_prime;
		buffer++;
	}

	return hash;
}

//tranform value to [0, upper_bound) via an optimized multiply + divide
static uint64_t MapScale(const uint64_t value, const uint64_t upper_bound)
{
	const uint128_t mult = (uint128_t) value * (uint128_t) upper_bound;

	return (uint64_t) (mult >> 64);
}

__attribute__((always_inline))
static uint64_t MapGetSlotIndex(const cstring *cstr, const uint64_t upper_bound)
{
	return MapScale(MapFNV1a(cstr), upper_bound);
}

//------------------------------------------------------------------------------

#define alias_slot(pfix)						       \
typedef struct pfix##_slot pfix##_slot;

enum slot_status {
	SLOT_OPEN = 0,
	SLOT_CLOSED = 1,
	SLOT_REMOVED = 2,
}

//read-only
#define declare_slot(T, pfix)						       \
struct pfix##_slot {							       \
	cstring *key;						               \
	T value;							       \
	enum slot_status status;				               \
}

#define alias_map(pfix)                                                        \
typedef struct pfix##_map pfix##_map;

//read-only
//map.len is the total closed and removed slots
#define declare_map(T, pfix)						       \
struct pfix##_map {						               \
	uint64_t len;							       \
	uint64_t cap;							       \
	pfix##_slot *buffer;						       \
}

//------------------------------------------------------------------------------

#define api_map(T, pfix, cls)					               \
cls pfix##_map pfix##MapInit(void);				               \
cls void pfix##MapFree(pfix##_map *, void (*)(void *));			       \

//------------------------------------------------------------------------------

#define impl_map_init(T, pfix, cls)					       \
cls pfix##_map pfix##MapInit(void)					       \
{								               \
	assert(SLOT_OPEN == 0 && "MapInit uses calloc to set SLOT_OPEN");      \
									       \
	const size_t initial_capacity = 8;				       \
	const size_t bufsize = initial_capacity * sizeof(pfix##_slot);	       \
									       \
	struct pfix##_map new = {					       \
		.len = 0,						       \
		.cap = initial_capacity,				       \
		.buffer = MapCalloc(bufsize);     			       \
	}								       \
								               \
	for (uint64_t i = 0; i < initial_capacity; i++) {		       \
		assert(new.buffer[i].status == SLOT_OPEN)		       \
	}								       \
									       \
	MapTrace("map initialized");					       \
									       \
	return new;							       \
}

//if mfree is non-null, it is invoked on every closed or removed element.
#define impl_map_free(T, pfix, cls)					       \
cls void pfix##MapFree(pfix##_map *self, void (*mfree)(void *ptr))             \
{									       \
	if (!self) {							       \
		MapTrace("null input; nothing to free");		       \
		return;							       \
	}								       \
									       \
	if (!self->buffer) {						       \
		MapTrace("map buffer is null; nothing to free");	       \
		return;							       \
	}								       \
									       \
	if (!mfree) {							       \
		MapTrace("freeing map elements");			       \
									       \
		const int conditions = SLOT_CLOSED | SLOT_OPEN;		       \
									       \
		for (uint64_t i = 0; i < self->cap; i++) {		       \
			const pfix##_slot slot = self->buffer[i];	       \
									       \
			if (slot.status & conditions) {			       \
				mfree(slot.value);			       \
			}						       \
		}							       \
									       \
		MapTrace("freed map elements");				       \
	}								       \
									       \
	free(self->buffer);						       \
									       \
	MapTrace("freed map buffer");					       \
}

//------------------------------------------------------------------------------

//make_map declares a map<T> type named pfix_map which may contain values of
//type T, and only type T, associated with cstring (char *) keys. Map operations
//have storage class cls. If sizeof(T) is not available at compile time before 
//make_map then each component macro must be expanded separately. 
#define make_map(T, pfix, cls)						       \
	alias_slot(pfix)						       \
	declare_slot(pfix)					               \
	alias_map(pfix)							       \
	declare_map(T, pfix)						       \
	api_map(T, pfix, cls)					               \
	impl_map_init(T, pfix, cls)

#define map(pfix) pfix##_map

//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

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

