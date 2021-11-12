// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The parser API provides functionality for creating, manipulating, and
// destroying abstract syntax trees. Lexical analysis is subsumed within the
// parsing process.

#include "ast.h"
#include "str.h"

//returns NULL if initialisation fails or the AST associated with the input src
//is ill formed.
file *SyntaxTreeInit(const cstring *src, const cstring *alias);

//input ast must not be null
void SyntaxTreeFree(file *ast);

