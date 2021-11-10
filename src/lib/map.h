// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Associative array from string keys to any type T, implemented as a linear
// probing hash table.

#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../xerror.h"

#ifdef MAP_TRACE
	#define MapTrace(msg, ...) xerror_trace(msg, ##__VA_ARGS__)
#else
	#define MapTrace(msg, ...)
#endif

#ifdef __SIZEOF_INT128__
	#define uint128_t __uint128_t
#else
	#error "map.h requires 128-bit integer support"
#endif

typedef char cstring;

//-----------------------------------------------------------------------------

__attribute__((malloc)) static void *MapCalloc(const size_t bytes)
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

static bool MapMatch(const cstring *a, const cstring *b)
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

	const uint8_t *buffer = (const uint8_t *) cstr;

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

__attribute__((always_inline)) inline
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
};

//read-only
#define declare_slot(T, pfix)						       \
struct pfix##_slot {							       \
	const cstring *key;						       \
	T value;							       \
	enum slot_status status;				               \
};

#define alias_map(pfix)                                                        \
typedef struct pfix##_map pfix##_map;

//read-only
//map.len is the total closed and removed slots
#define declare_map(T, pfix)						       \
struct pfix##_map {						               \
	uint64_t len;							       \
	uint64_t cap;							       \
	pfix##_slot *buffer;						       \
};

//------------------------------------------------------------------------------

#define api_map(T, pfix, cls)					               \
cls pfix##_map pfix##MapInit(const uint64_t capacity);			       \
cls void pfix##MapFree(pfix##_map *, void (*)(T));		               \
cls bool pfix##MapInsert(pfix##_map *, const cstring *, T);		       \
cls void pfix##MapResize_private(pfix##_map *);			               \
cls bool pfix##MapProbe_private(pfix##_map *, const cstring *, T);	       \
cls bool pfix##MapRemove(pfix##_map *, const cstring *, void (*)(T));          \
cls bool pfix##MapGet(pfix##_map *, const cstring *, T *);		       \
cls bool pfix##MapSet(pfix##_map *self, const cstring *key, T value);

//------------------------------------------------------------------------------

#define MAP_DEFAULT_CAPACITY ((size_t) 16)

//use this when the hash table is going to contain a known or minimum number of
//elements; helps reduce the chances of a thread from pausing hot spot code to
//perform dynamic resizing.
#define MAP_MINIMUM_CAPACITY(capacity) MapGrow(capacity)

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
		.buffer = MapCalloc(bufsize)     			       \
	};								       \
								               \
	for (uint64_t i = 0; i < capacity; i++) {			       \
		assert(new.buffer[i].status == SLOT_OPEN);		       \
	}								       \
									       \
	MapTrace("new map initialized with %" PRIu64 "slots", capacity);       \
									       \
	return new;							       \
}

//if vfree is non-null, it is invoked on every closed or removed element.
#define impl_map_free(T, pfix, cls)					       \
cls void pfix##MapFree(pfix##_map *self, void (*vfree)(T))	               \
{									       \
	if (!self) {							       \
		MapTrace("null input; no-op");				       \
		return;							       \
	}								       \
									       \
	if (!self->buffer) {						       \
		MapTrace("null buffer; no-op");		         	       \
		return;							       \
	}								       \
								               \
	MapTrace("freeing map elements");				       \
									       \
	for (uint64_t i = 0; i < self->cap; i++) {			       \
		pfix##_slot slot = self->buffer[i];			       \
								               \
		if (slot.status == SLOT_CLOSED) {			       \
_Pragma("GCC diagnostic push")		       				       \
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")			       \
			free((void *) slot.key);			       \
_Pragma("GCC diagnostic pop")					               \
									       \
			if (vfree) {					       \
				vfree(slot.value);			       \
			}						       \
		}							       \
	}								       \
									       \
	MapTrace("freed map elements");					       \
									       \
	free(self->buffer);						       \
									       \
	MapTrace("freed map buffer");					       \
}

//abort if map.len == UINT64_MAX
//return false if key already exists; it must be removed before a new insertion
//key is duplicated when stored in the hash table; the user is responsible for
//managing memory for the input key argument on return. The value is copied.
#define impl_map_insert(T, pfix, cls)					       \
cls bool pfix##MapInsert(pfix##_map *self, const cstring *key, T value)	       \
{									       \
	assert(self);							       \
	assert(self->buffer);						       \
	assert(key);							       \
									       \
	if (self->len == UINT64_MAX) {					       \
		MapTrace("fail; map is full");				       \
		abort();						       \
	}								       \
								               \
	MapTrace("inserting '%s'", key);				       \
									       \
	const double load_factor_threshold = 0.5;			       \
	const double load_factor = (double) self->len / (double) self->cap;    \
									       \
	if (load_factor > load_factor_threshold) {			       \
		MapTrace("load factor %g exceeds threshold", load_factor);     \
		MapTrace("suspending insert of '%s'", key);		       \
		pfix##MapResize_private(self);				       \
		MapTrace("resume insert of '%s'", key);			       \
	}								       \
									       \
	return pfix##MapProbe_private(self, key, value);		       \
}

//no-op if capacity cannot expand
#define impl_map_resize_private(T, pfix, cls)			               \
cls void pfix##MapResize_private(pfix##_map *self)			       \
{								               \
	assert(self);							       \
	assert(self->buffer);						       \
									       \
	if (self->cap == UINT64_MAX) {					       \
		MapTrace("cannot resize map; maximum capacity reached");       \
		return;							       \
	}								       \
									       \
	const uint64_t new_capacity = MapGrow(self->cap);		       \
	pfix##_map new_map = pfix##MapInit(new_capacity);		       \
									       \
	for (uint64_t i = 0; i < self->cap; i++) {			       \
		const pfix##_slot slot = self->buffer[i];		       \
									       \
		if (slot.status == SLOT_CLOSED) {		               \
			const cstring *k = slot.key;			       \
			T v = slot.value;				       \
									       \
			MapTrace("new map; inserting' %s'", k);		       \
									       \
			bool ok = pfix##MapProbe_private(&new_map, k, v);      \
									       \
			assert(ok && "key already exists in new buffer");      \
		}							       \
	}								       \
								               \
	MapTrace("old map; all closed slots copied");			       \
									       \
	free(self->buffer);						       \
									       \
	MapTrace("old map; released internal buffer");	          	       \
									       \
	*self = (pfix##_map) {						       \
		.len = new_map.len,					       \
		.cap = new_map.cap,					       \
		.buffer = new_map.buffer				       \
	};								       \
									       \
	MapTrace("new map; finalized");					       \
}

//this function assumes there is at least one open slot in the map buffer.
//if the key already exists in a closed slot then do nothing and return false.
//private; see MapInsert docs; key is duplicated and value is copied.
#define impl_map_linear_probe_private(T, pfix, cls)  			       \
cls bool pfix##MapProbe_private(pfix##_map *self, const cstring *key, T value) \
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
			MapTrace("'%s' already exists in closed slot", key);   \
			return false;					       \
		}							       \
									       \
		i = (i + 1) % self->cap;				       \
		slot = self->buffer + i;				       \
	}								       \
									       \
	*slot = (pfix##_slot) {						       \
		.key = cStringDuplicate(key),				       \
		.value = value,						       \
		.status = SLOT_CLOSED					       \
	};							               \
									       \
	self->len++;							       \
									       \
	MapTrace("linear probe succeeded");			               \
									       \
	return true;							       \
}

//if vfree is not null, it is called on the value associated with the key before
//the pair is removed from the buffer.
//
//internal note; removed slots count towards the load factor, so they do not
//decrease map.len.
#define impl_map_remove(T, pfix, cls)					       \
cls bool pfix##MapRemove						       \
(									       \
	pfix##_map *self,						       \
	const cstring *key,						       \
	void (*vfree)(T))						       \
{								               \
	assert(self);							       \
	assert(self->buffer);						       \
	assert(key);							       \
									       \
	uint64_t i = MapGetSlotIndex(key, self->cap);			       \
	const uint64_t start = i;					       \
	pfix##_slot *slot = self->buffer + i;				       \
									       \
	MapTrace("searching for '%s' removal slot", key); 		       \
									       \
	do {								       \
		switch (slot->status) {					       \
		case SLOT_OPEN:						       \
			MapTrace("found open slot; '%s' cannot exist", key);   \
			return false;					       \
									       \
		case SLOT_CLOSED:					       \
			if (MapMatch(slot->key, key)) {			       \
_Pragma("GCC diagnostic push")		       				       \
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")			       \
				free((void *) slot->key);		       \
_Pragma("GCC diagnostic pop")	               				       \
								               \
				if (vfree) {				       \
					vfree(slot->value);		       \
				}					       \
								               \
				slot->status = SLOT_REMOVED;		       \
									       \
				MapTrace("'%s' removed", key);   	       \
									       \
				return true;				       \
			}						       \
									       \
			break;						       \
									       \
		case SLOT_REMOVED:					       \
			break;						       \
									       \
		default:					 	       \
			assert(0 != 0 && "bad status flag");		       \
		}							       \
									       \
		i = (i + 1) % self->cap;				       \
		slot = self->buffer + i;				       \
	} while (i != start);						       \
								               \
	/* reach here when key doesn't exist and map.len == map.cap */	       \
	MapTrace("exhaustive search; '%s' does not exist", key);	       \
	return false;							       \
}

//input value pointer may be NULL
#define impl_map_get(T, pfix, cls)					       \
cls bool pfix##MapGet(pfix##_map *self, const cstring *key, T *value)	       \
{									       \
	assert(self);							       \
	assert(self->buffer);						       \
	assert(key);							       \
									       \
	uint64_t i = MapGetSlotIndex(key, self->cap);			       \
	const uint64_t start = i;					       \
	pfix##_slot *slot = self->buffer + i;				       \
									       \
	MapTrace("searching for '%s' slot", key);			       \
									       \
	do {								       \
		switch (slot->status) {					       \
		case SLOT_OPEN:						       \
			MapTrace("found open slot; '%s' cannot exist", key);   \
			return false;					       \
									       \
		case SLOT_CLOSED:					       \
			if (MapMatch(slot->key, key)) {			       \
				MapTrace("found '%s'", key);		       \
									       \
				if (value) {				       \
					*value = slot->value;		       \
				}					       \
									       \
				return true;				       \
			}						       \
									       \
			break;						       \
								               \
		case SLOT_REMOVED:					       \
			break;						       \
									       \
		default:						       \
			assert(0 != 0 && "bad status flag");		       \
		}							       \
									       \
		i = (i + 1) % self->cap;				       \
		slot = self->buffer + i;				       \
	} while (i != start);						       \
									       \
	/* reach here when key doesn't exist and map.len == map.cap */	       \
	MapTrace("exhaustive search; '%s' does not exist", key);	       \
	return false;							       \
}

//set an existing map value associated with the input key to a new value. If
//the key doesn't exist, return false.
#define impl_map_set(T, pfix, cls)					       \
cls bool pfix##MapSet(pfix##_map *self, const cstring *key, T value)	       \
{									       \
	assert(self);							       \
	assert(self->buffer);						       \
	assert(key);							       \
									       \
	uint64_t i = MapGetSlotIndex(key, self->cap);			       \
	const uint64_t start = i;					       \
	pfix##_slot *slot = self->buffer + i;				       \
									       \
	MapTrace("searching for '%s' slot", key);			       \
								               \
	do {								       \
		switch (slot->status) {					       \
		case SLOT_OPEN:						       \
			MapTrace("found open slot; '%s' cannot exist", key);   \
			return false;					       \
									       \
		case SLOT_CLOSED:					       \
			if (MapMatch(slot->key, key)) {			       \
				MapTrace("found '%s'; exchanging value", key); \
				slot->value = value;			       \
				return true;				       \
			}						       \
									       \
			break;						       \
								               \
		case SLOT_REMOVED:					       \
			break;						       \
									       \
		default:						       \
			assert(0 != 0 && "bad status flag");		       \
		}							       \
									       \
		i = (i + 1) % self->cap;				       \
		slot = self->buffer + i;				       \
	} while (i != start);						       \
									       \
	/* reach here when key doesn't exist and map.len == map.cap */	       \
	MapTrace("exhaustive search; '%s' does not exist", key);	       \
	return false;							       \
}
//------------------------------------------------------------------------------

//make_map declares a map<T> type named pfix_map which may contain values of
//type T, and only type T, associated with cstring (char *) keys. Map operations
//have storage class cls. If sizeof(T) is not available at compile time before
//make_map then each component macro must be expanded separately.
#define make_map(T, pfix, cls)					       	       \
	alias_slot(pfix)						       \
	declare_slot(T, pfix)					               \
	alias_map(pfix)							       \
	declare_map(T, pfix)						       \
	api_map(T, pfix, cls)					               \
	impl_map_init(T, pfix, cls)				               \
	impl_map_free(T, pfix, cls)					       \
	impl_map_insert(T, pfix, cls)					       \
	impl_map_resize_private(T, pfix, cls)				       \
	impl_map_linear_probe_private(T, pfix, cls)			       \
	impl_map_remove(T, pfix, cls)				               \
	impl_map_get(T, pfix, cls)					       \
	impl_map_set(T, pfix, cls)

#define map(pfix) pfix##_map
