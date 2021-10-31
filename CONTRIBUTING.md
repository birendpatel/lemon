# Contributing

Anyone is free to contribute source code for Lemon. Lemon is written in C using the GNU dialect of the ISO C17 standard.

## C Programming Rules

1. Error codes must be either checked or casted to void with a comment.

2. Fallthroughs must use the fallthrough macro defined in ./src/defs.h

3. Control flow statements must use braces.

4. Dynamic allocations must use AbortMalloc() defined in ./src/defs.h

5. Valgrind must pass with zero detected leaks in all test cases.

6. Every global variable must be thread-safe and opaque.

7. Lemon must compile with zero errors and zero warnings.

8. No magic numbers.

9. Goto statements may only be used for error handing and nested loops.
