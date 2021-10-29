// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The ImportGraph API exposes functions for creating a directed dependency
// graph based on file import statements. The graph nodes represent the imported
// files and directed edges represent dependencies. (A -> B :=  A depends on B)

#pragma once

#include <stdlib.h>

#include "xerror.h"
#include "parser.h"
#include "lib/str.h"

typedef struct {
	cstring *alias;
	file *ast;
} tagged_ast;

//creates a directed-acyclic graph and returns the graph vertices in topological
//order as an array of tagged_ast structs. The array is guaranteed to be NULL
//terminated, even if an error is returned.
xerror ImportGraphInit(const cstring *input_file, tagged_ast **list);
