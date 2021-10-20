# Contributing

Anyone is free to contribute source code for Lemon. Lemon is written in C using the GNU dialect of the ISO C17 standard.

## Programming Rules

1. All error codes must be checked or casted to void.

2. All fallthroughs must use the GCC fallthrough attribute.

3. All control flow statements must use braces.

4. Dynamic allocations must use AbortMalloc().

5. Valgrind must pass with zero detected leaks in all test cases.

6. Every global variable must be thread-safe and opaque.

7. Lemon must compile with zero errors and zero warnings.

8. No magic numbers.
