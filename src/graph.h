// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The Graph API exposes an interface for creating a directed dependency graph
// based on file import statements and topologically sorting the graph to
// facilitate symbol table construction.

#pragma once

#include <stdlib.h>

#include "xerror.h"
#include "parser.h"
#include "lib/str.h"

//starting from the input fname, create a directed acyclic graph of file nodes
//according to their import dependencies. On return the ast parameter points
//to a null-terminated dynamically allocated array of nodes in topological
//order. 
xerror GraphSort(const cstring *fname, file *ast[])
