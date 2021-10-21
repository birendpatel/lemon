// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Associative array from string keys to any type T, implemented as a linear 
// probing hash table.

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

#ifdef __SIZEOF_INT128__
	#define uint128_t __uint128_t
#else
	#error "map.h requires 128-bit integer support"
#endif

#ifndef MAP_ESUCCESS
	#error "map.h requires user to implement the MAP_ESUCCESS int code"
#endif

#ifndef MAP_EFULL
	#error "map.h requires user to implement the MAP_EFULL int code"
#endif

#ifndef MAP_EEXISTS
	#error "map.h requires user to implement the MAP_EEXISTS int code"
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

static uint64_t MapGrow(uint64_t curr_capacity)
{
	const uint64_t overflow_threshold = UINT64_MAX / 2;
	const uint64_t growth_rate = 2;

	if (curr_capacity == 0) {
		return 1;
	}

	if (curr_capacity >= overflow_threshold) {
		return UINT64_MAX;
	}

	return curr_capacity * growth_rate;
}

static bool MapMatch(cstring *a, cstring *b)
{
	return !strcmp(a, b);
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

//tranform value to [0, upper_bound) via an optimized multiply + divide.
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
cls pfix##_map pfix##MapInit(const uint64_t capacity);			       \
cls void pfix##MapFree(pfix##_map *, void (*)(void *));			       \
cls int pfix##MapInsert(pfix##_map *, const cstring *, T);		       \
cls void pfix##MapResize_internal(pfix##_map *);			       \
cls int pfix##MapProbe_internal(pfix##_map *, const cstring *, T);	       \
cls int pfix##MapRemove(pfix##_map *, cstring *, void (*)(void *));

//------------------------------------------------------------------------------

#define impl_map_init(T, pfix, cls)					       \
cls pfix##_map pfix##MapInit(const uint64_t capacity)			       \
{								               \
	assert(SLOT_OPEN == 0 && "MapInit uses calloc to set SLOT_OPEN");      \
									       \
	const size_t bufsize = capacity * sizeof(pfix##_slot);	               \
									       \
	struct pfix##_map new = {					       \
		.len = 0,						       \
		.cap = capacity,					       \
		.buffer = MapCalloc(bufsize);     			       \
	}								       \
								               \
	for (uint64_t i = 0; i < capacity; i++) {			       \
		assert(new.buffer[i].status == SLOT_OPEN)		       \
	}								       \
									       \
	MapTrace("map initialized");					       \
									       \
	return new;							       \
}

//if vfree is non-null, it is invoked on every closed or removed element.
#define impl_map_free(T, pfix, cls)					       \
cls void pfix##MapFree(pfix##_map *self, void (*vfree)(void *ptr))             \
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
	if (vfree) {							       \
		MapTrace("freeing map elements");			       \
									       \
		const int conditions = SLOT_CLOSED | SLOT_OPEN;		       \
									       \
		for (uint64_t i = 0; i < self->cap; i++) {		       \
			const pfix##_slot slot = self->buffer[i];	       \
									       \
			if (slot.status & conditions) {			       \
				vfree(slot.value);			       \
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

#define impl_map_insert(T, pfix, cls)					       \
cls int pfix##MapInsert(pfix##_map *self, const cstring *key, T value)	       \
{									       \
	assert(self);							       \
	assert(self->buffer);						       \
	assert(key);							       \
									       \
	if (self->len == UINT64_MAX) {					       \
		MapTrace("fail; map is full");				       \
		return MAP_EFULL;					       \
	}								       \
									       \
	const double load_factor_threshold = 0.5;			       \
	const double load_factor = self->len / (double) self->cap;	       \
									       \
	if (load_factor > load_factor_threshold) {			       \
		MapTrace("load factor exceeds threshold");		       \
		pfix##MapResize_internal(self);				       \
	}								       \
								               \
	MapTrace("inserting key value pair");			               \
									       \
	return pfix##MapProbe_internal(self);				       \
}

//no-op if capacity cannot expand 
#define impl_map_resize_internal(T, pfix, cls)			               \
cls void pfix##MapResize_internal(pfix##_map *self)			       \
{								               \
	assert(self);							       \
	assert(self->buffer);						       \
									       \
	if (self->cap == UINT64_MAX) {					       \
		MapTrace("fail; cannot resize map; maximum capacity reached"); \
		return;							       \
	}								       \
									       \
	const uint64_t new_capacity = MapGrow(self->cap);		       \
	pfix##_map new_map = pfix##MapInit(new_capacity);		       \
									       \
	MapTrace("resized map");					       \
									       \
	for (uint64_t i = 0; i < self->cap; i++) {			       \
		const pfix##_slot slot = self->buffer[i];		       \
									       \
		if (slot.status == SLOT_CLOSED) {		               \
			cstring *k = slot.key;				       \
			T v = slot.value;				       \
			int err = pfix##MapProbe_internal(new_map, k, v);      \
									       \
			assert(!err && "key already exists in new buffer");    \
		}							       \
	}								       \
								               \
	MapTrace("inserted closed slots from old map");			       \
									       \
	free(self->buffer);						       \
									       \
	MapTrace("released internal buffer from old map");		       \
									       \
	*self = (pfix##_map) {						       \
		.len = new_map.len,					       \
		.cap = new_map.cap,					       \
		.buffer = new_map.buffer				       \
	};								       \
									       \
	MapTrace("copied new map members to old map");			       \
}

//this function assumes there is at least one open slot in the map buffer.
//if the key already exists in a closed slot (but not removed) then do nothing
//and return MAP_EEXISTS.
#define impl_map_linear_probe_internal(T, pfix, cls)  			       \
cls int pfix##MapProbe_internal(pfix##_map *self, const cstring *key, T value) \
{									       \
	assert(self);							       \
	assert(self->buffer);						       \
	assert(self->len < self->cap);					       \
	assert(key);							       \
									       \
	uint64_t i = MapGetSlotIndex(key, self->cap);			       \
	pfix##_slot *slot = self->buffer + i;				       \
									       \
	while (slot->status != SLOT_OPEN) {				       \
		if (slot->status == SLOT_CLOSED && MapMatch(key, slot->key)) { \
			MapTrace("fail; key already exists in closed slot");   \
			return MAP_EEXISTS;				       \
		}							       \
									       \
		i = (i + 1) % self->cap;				       \
		slot = self->buffer + i;				       \
	}								       \
									       \
	*slot = (pfix##_slot) {						       \
		.key = key,						       \
		.value = value,						       \
		.status = SLOT_CLOSED					       \
	};							               \
									       \
	self->len++;							       \
									       \
	MapTrace("linear probe succeeded");			               \
									       \
	return MAP_ESUCCESS;						       \
}

#define impl_map_remove(T, pfix, cls)					       \
cls int pfix##MapRemove(pfix##_map *self, cstring *key, void (*vfree)(void *))
{
	;
}

//------------------------------------------------------------------------------

//make_map declares a map<T> type named pfix_map which may contain values of
//type T, and only type T, associated with cstring (char *) keys. Map operations
//have storage class cls. If sizeof(T) is not available at compile time before 
//make_map then each component macro must be expanded separately.
#define make_map(T, pfix, cls, vfree)					       \
	alias_slot(pfix)						       \
	declare_slot(pfix)					               \
	alias_map(pfix)							       \
	declare_map(T, pfix)						       \
	api_map(T, pfix, cls)					               \
	impl_map_init(T, pfix, cls)				               \
	impl_map_free(T, pfix, cls)					       \
	impl_map_insert(T, pfix, cls)					       \
	impl_map_resize_internal(T, pfix, cls)				       \
	impl_map_linear_probe_internal(T, pfix, cls)			       \
	impl_map_remove(T, pfix, cls)

#define map(pfix) pfix##_map
