//Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
//Compiler phase 2: syntactic analysis: tokens -> parser -> abstract syntax tree

#pragma once

#include "ast.h"
#include "str.h"

module *SyntaxTreeInit(const cstring *filename);

//------------------------------------------------------------------------------
// The input AST must not be null. Only allocations made by SyntaxTreeInit are
// cleaned up. The user may choose to prune the tree in application node; this
// will not prevent correct behavior from SyntaxTreeInit as all SyntaxTreeInit
// allocations are doubly recorded in an opaque region of memory.
//
// The memory for those allocations made by SyntaxTreeInit (all node pointer
// members in any node) is solely the responsibiliy of SyntaxTreeFree to manage
// and release. If you call stdlib free() or any memory management function
// on these pointers then a segfault may occur (if you're lucky).

void SyntaxTreeFree(module *ast);

