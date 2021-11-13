// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The resolver recursively locates all names in the source code and generates
// a corresponding object for each name. Names fall into one of two categories:
// directives and identifiers. Directives generate abstract syntax trees while
// identifiers generate symbols and symbol tables. These generated objects are
// brought together into a directed acyclic graph.

#pragma once

#include "ast.h"
#include "symtable.h"
#include "graph.h"
#include "str.h"

//------------------------------------------------------------------------------
// The graph verticies contain an intrusive null-terminated linked list located
// on module.next. This list is threaded through the verticies in topological
// order. The topological order is defined such that the vertex located at
// position i in the linked list does not depend on the vertex at position j for
// 0 <= i < j < len(list), where X is dependent on Y if X imports Y.

make_graph(module *, Module, static)

//returns the zero graph on error; check with ModuleGraphIsZero()
graph(Module) ResolverInit(const cstring *modname);

//input graph must not be NULL
void ResolverFree(graph(Module) *graph);

