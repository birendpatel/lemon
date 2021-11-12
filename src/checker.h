// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#pragma once

typedef struct symbol symbol;
typedef struct symtable symtable;

//register the predeclared native types and functions in a thread-safe table
void SymTableGlobalInit(void);
void SymTableGlobalFree(void);
