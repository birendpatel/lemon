// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Graph data structure represented via an adjacency list. Implemented as a hash
// table from cstring keys to verticies with a generic type T. The graph is just
// a shallow wrapper over map.h.

#pragma once

#include <stdbool.h>
#include <string.h>

#include "map.h"
#include "xerror.h"

#ifdef GRAPH_TRACE
	#define GraphTrace(msg, ...) xerror_trace(msg, ##__VA_ARGS__)
#else
	#define GraphTrace(msg, ...)
#endif

//------------------------------------------------------------------------------

#define alias_graph(pfix)						       \
typedef map(pfix) pfix##_graph;

#define api_graph(T, pfix, cls)						       \
cls pfix##_graph pfix##GraphInit(void);					       \
cls void pfix##GraphFree(pfix##_graph *, void (*)(T));		               \
cls bool pfix##GraphInsert(pfix##_graph *, const cstring *, T);                \
cls bool pfix##GraphSearch(pfix##_graph *, const cstring *, T *);              \
cls bool pfix##GraphModify(pfix##_graph *, const cstring *, T );	       \

#define impl_graph_init(T, pfix, cls)					       \
cls pfix##_graph pfix##GraphInit(void)                                         \
{									       \
	return pfix##MapInit(MAP_DEFAULT_CAPACITY);			       \
}

//if gfree is non-null then it is called on every vertex
#define impl_graph_free(T, pfix, cls)					       \
cls void pfix##GraphFree(pfix##_graph *self, void (*gfree)(T))	               \
{									       \
	assert(self);							       \
	pfix##MapFree(self, gfree);					       \
}

//return false if the vertex already exists
#define impl_graph_insert(T, pfix, cls)					       \
cls bool pfix##GraphInsert(pfix##_graph *self, const cstring *key, T vertex)   \
{									       \
	assert(self);							       \
	assert(key);							       \
									       \
	return pfix##MapInsert(self, key, vertex);			       \
}

//return false if the vertex does not exist; input vertex pointer may be null
#define impl_graph_search(T, pfix, cls)					       \
cls bool pfix##GraphSearch(pfix##_graph *self, const cstring *key, T *vertex)  \
{									       \
	assert(self);							       \
	assert(key);							       \
									       \
	return pfix##MapGet(self, key, vertex);				       \
}

//return false if the vertex does not exist
#define impl_graph_modify(T, pfix, cls)					       \
cls bool pfix##GraphModify(pfix##_graph *self, const cstring *key, T vertex)   \
{									       \
	assert(self);							       \
	assert(key);							       \
									       \
	return pfix##MapSet(self, key, vertex);				       \
}

//------------------------------------------------------------------------------

//make_graph declares a graph<T> type named pfix_graph which may contain vertex
//values of type T, and only type T, associated with cstring (char *) keys. The
//graph operations have storage clas cls. If sizeof(T) is not available at
//compile time before make_graph then each component macro must be expanded
//separately.
#define make_graph(T, pfix, cls)		                               \
	make_map(T, pfix, cls)					               \
	alias_graph(pfix)					               \
	api_graph(T, pfix, cls)					               \
	impl_graph_init(T, pfix, cls)					       \
	impl_graph_free(T, pfix, cls)					       \
	impl_graph_insert(T, pfix, cls)				               \
	impl_graph_search(T, pfix, cls)	 			               \
	impl_graph_modify(T, pfix, cls)

#define graph(pfix) pfix##_graph
