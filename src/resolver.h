// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The resolver locates all directives and identifiers in the source code. It
// uses them to generate a variety of digraph structures composed of abstract
// syntax trees and symbol tables. These digraphs are interwoven into a large
// sparse network which unifies all aspects of the compiler front-end. This IR
// serves as the foundation for subsequent semantic and optimisation passes.

#pragma once

#include "graph.h"
#include "parser.h"
#include "str.h"
#include "symtable.h"

make_graph(module *, Module, static)

//------------------------------------------------------------------------------
// @dependencies: Rooted directed acyclic graph. Edges are given by the module
// import member where X -> Y if and only if X imports Y.
//
// @head: Intrusive linked list threaded through the verticies of the dependency
// graph in topological order. The topological order is defined such that the 
// vertex located at position i in the linked list does not import the vertex 
// at position j for 0 <= i < j < len(list). Traverse the list via module->next
// and cycle back via module->imports.
//
// @global: predeclared identifiers such as native types and native functions.
// Points to module->public. Like network.head it weaves its own parent pointer
// tree through the module's AST.

typedef struct network {
	graph(Module) dependencies;
	module *head;
	symtable *global;
} network;

//------------------------------------------------------------------------------
// API

//returns NULL on failure
network *ResolverInit(const cstring *filename);
