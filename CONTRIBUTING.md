# Contributing

Anyone is free to contribute source code for Lemon. 

The compiler is written entirely in C using the GNU dialect of the ISO C17 standard. Lemon is not intended to be portable to Clang, MSVC, or ICC so you are encouraged to leverage whatever C11 and GNU extensions help you get the job done.

## C Programming Rules

1. Error codes must be either checked or casted to void with a comment.

2. Fallthroughs on switch cases must use the GCC fallthrough attribute.

3. Control flow statements must use braces.

4. Dynamic allocations must use the arena allocator from ./src/utils/arena.h

5. Valgrind must pass with zero detected leaks in all test cases.

6. No global variables. 

7. File scoped variables must be thread-safe and opaque.

8. Lemon must compile with zero errors and zero warnings in all modes.

9. No magic numbers.
