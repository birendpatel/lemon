# Contributing

Anyone is free to contribute source code for Lemon.

Lemon is written in C using the GNU dialect of the ISO C17 standard. Meaning, you are free to write lemon code which uses GCC attributes and constructs which violate -Wpedantic. C11+ features such as _Generic, _Noreturn, atomics, and so on are all allowed.

## Programming Rules 

1. All error codes except printf must be checked or casted to void.

2. No implicit fallthroughs; Use the GCC fallthrough attribute.

3. All control flow statements must use braces.

4. Dynamic allocations must use AbortMalloc().

5. Valgrind must pass with zero detected leaks.

6. Global variables must be encapsulated and thread-safe.

7. Inline assembly and bytecode must comment every instruction.

8. Lemon must compile with zero errors and zero warnings.

9. No goto unless for error handling, nested loops, or tail call optimisation.

10. No setjmp and longjmp other than the Unity CException library.

11. No magic numbers.

12. No plagarism. Cite all sources including StackOverflow authors.

13. Have fun because all said and done it's just a useless compiler for a unused unknown unloved language. So you might as well enjoy the ride.
