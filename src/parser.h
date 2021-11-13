// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Compiler phase 2: syntax analysis: tokens -> parser -> abstract syntax tree

#include <stdbool.h>
#include <stdint.h>

#include "ast.h"
#include "str.h"
#include "vector.h"

//------------------------------------------------------------------------------
// Returns NULL if initialisation fails or the AST is ill-formed. On success all
// nodes and node members are valid except those which cant be fully determined 
// without semantic analysis: symtables, symbols, and module references.

module *const SyntaxTreeInit(const cstring *src, const cstring *alias);

//------------------------------------------------------------------------------
// The input AST must not be null. Only allocations made by SyntaxTreeInit are
// cleaned up. Any node modifications or deletions made to the AST after the
// SyntaxTreeInit call are not released by this function.

void SyntaxTreeFree(module *ast);

