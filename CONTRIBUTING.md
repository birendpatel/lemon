# Contributing

Anyone is free to contribute source code for Lemon. Check out the todo list in the root README for tasks that definitely need to be done. If you have an idea of your own, reach out first to make sure we agree on it.

## C Programming Rules

1. Error codes must be either checked or casted to void with a comment.

2. Fallthroughs must use the fallthrough macro defined in defs.h

3. Control flow statements must use braces.

4. Dynamic allocations must use AbortMalloc() defined in defs.h

5. Valgrind must pass with zero detected leaks in all test cases.

6. Every global variable must be thread-safe and opaque.

7. Lemon must compile with zero errors and zero warnings.

8. No magic numbers.

The compiler is written entirely in C using the GNU dialect of the ISO C17 standard. Lemon is not intended to be portable to Clang, MSVC, or ICC so you are encouraged to leverage whatever C11 and GNU extensions help you get the job done.

