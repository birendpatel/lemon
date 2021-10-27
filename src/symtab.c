// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include "symtab.h"
#include "lib/map.h"

//insert things into a symbol table
//- functions
//- types
//- variables
//
//if something already found, throw a re-declared sort of error
//
//do all types of things sit in the same hash table at the same scope?
//namespacing!

//lookup things in a symbol table
//- functions
//- types
//- variables
//
//if nothing found, throw an error
//
//nodes should store location of slot in the hash table to allow future
//faster accesses instead of having to lookup hash again, but this can only
//be done when the hash table is finished growing b/c hash resizes will change
//references. this is just for optimisation.
//
//instead you can store the specific hash table rather than the slot, this
//solves the issue of a redeclaration within the scope. You can also
//traverse the tree first to count up the total decls and then on the second
//pass create a hash table with a capacity large enough to not trigger
//a growth.

//pop and push hash tables off a stack
//- whenever a new lexical scope is encountered or left
//
//store references to each hash table in block statement nodes

//need some sort of global root hash table where predeclared identifiers reside.
//this is not stuff that the user has declared but stuff that is provided by
//Lemon beforehand, like ints and floats.

//symbol table should include number of times a variable, function, or UDT is
//referenced so as to trigger -Wunused errors
//
//how to handle import? what if a PROGRAM recursively calls itself to handle
//import dependencies? I'm thinking main.c should only take one input file,
//which solves the first issue of which file is the main file when multiple
//files contain file-scoped statements. From there, Lemon needs to figure out
//how to resolve import dependencies from this main file. How can the main
//file get access to a symbol table.
void SymTabInit(void) {
	return;
}
