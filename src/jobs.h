// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// The jobs API exposes an interface for creating and manipulating a directed
// dependency graph of abstract syntax trees based on file import statements.
// The process of creating a job graph subsumes lexical analysis and parsing.

#pragma once

#include "xerror.h"
#include "parser.h"
#include "lib/vector.h"
#include "lib/str.h"

make_vector(file *, File, static)

//returns a topologically sorted vector of dynamically allocated abstract syntax
//trees such that the tree located at vector.buffer[i] does not depend on the 
//tree located at vector.buffer[j] for 0 <= i < j < vector.len. If an error 
//occurs, vector.len will be set to 0 and the error code will be one of XEFILE, 
//XEPARSE, or XEUSER.
vector(File) JobsCreate(const cstring *filename, xerror *err);
