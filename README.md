# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Matrix types

This repository implements Lemon in C, nicknamed CLemon. CLemon is an interpreter and has a bytecode compiler that performs high-level optimisations. In the future, CLemon's optimised IR will funnel to LLVM.

# Requirements

Lemon has only been tested on a 64-bit Linux OS with GCC 9.3.0. It makes heavy use of GCC extensions and may refuse to build on Clang and will almost surely refuse on MSVC.

# Install

First, ensure you have GCC with version 9.3.0 or higher. Then, clone this repository and issue `make release`. The executable `lemon` will be placed in the newly created `release` directory. The executable is standalone so you may install it wherever you prefer, such as `/usr/local/bin`.

You can issue `make docs` to generate the source code documentation via Doxygen.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in the root directory.

# Todo

- [X] Refactor vector library to use lemon error codes
- [X] Refactor vector free to allow NULL vector for RAII
- [X] Refactor REPL to use updated vector mechanisms
- [X] Refactor main to propogate info up call chain on errors
- [X] Simplify error handling in main
- [ ] Consider adding C11 Generic for RAII idioms
- [X] Add i flag to optionally drop run_file into run_repl
- [ ] Add locale support to main
- [X] Add GNU Compiler info and copyright to REPL header
- [X] Add ability to invoke shell commands on REPL
- [ ] Add 60 second tutorial on root readme
- [ ] Make C lemon compatible with Clang.
