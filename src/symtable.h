// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

typedef struct symbol symbol;
typedef struct symtable symtable;

// this function must be called before any other in order to register the
// predeclared types and functions. 
bool SymTableGlobalConfig(void);
