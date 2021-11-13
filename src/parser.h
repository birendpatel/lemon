//Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
//Compiler phase 2: syntactic analysis: tokens -> parser -> abstract syntax tree

module *const SyntaxTreeInit(const cstring *filename);

//------------------------------------------------------------------------------
// The input AST must not be null. Only allocations made by SyntaxTreeInit are
// cleaned up. Any node modifications or deletions made to the AST after the
// SyntaxTreeInit call are not released by this function.

void SyntaxTreeFree(module *ast);

