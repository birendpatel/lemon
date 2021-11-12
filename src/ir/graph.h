// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The graph (capital T) is the final IR generated by the compiler frontend and
// represents the source code as a directed acyclic graph of abstract syntax
// trees combined with their symbol tables.
//
// The graph is represented as an adjacency list and implemented via a hash
// table using filename keys. An intrusive null-terminated linked list is 
// threaded through the verticies in topological order.
//
// The topological order is defined such that the vertex located at position i 
// in the linked list does not depend on the vertex at position j for 0 <= i < 
// j < len(list). Dependency is determined by the import directives.

#pragma once

#include "parser.h"
#include "str.h"
#include "symtable.h"

typedef struct vertex vertex;
typedef struct graph graph;

struct vertex {
	file *root;
	vertex *next;
	bool active;
};

make_map(vertex, Vertex, static)

struct graph {
	map(Vertex) table;
	vertex *head;
	symtable *global;
};

