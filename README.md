# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Matrix types

This repository implements Lemon in C. C-Lemon is an interpreter and has a bytecode compiler that performs high-level optimisations.

# Requirements

To build Lemon, you should have the GCC compiler version 9.3.0 or greater installed. You'll also need GNU Make and a 64-bit machine running the Linux OS. To build the documentation, you need to install Doxygen.

# Install

Clone this repository and issue `make release`. The executable `lemon` will be placed in the newly created `release` directory. You can issue `make docs` to generate the source code documentation via Doxygen.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Todo

- [ ] Fix diagnostic-all bug which doesn't set the interactive flag
- [X] Consider adding C11 Generic for RAII idioms
- [ ] Add locale support to main
- [ ] Add 60 second tutorial on root readme
- [ ] Make C lemon compatible with Clang
- [X] Add help info when REPL fires up
- [ ] Add diagnostic flag for multithreading
- [ ] Add vector tracing
- [ ] Write vector unit tests
- [ ] Write channel unit tests
- [ ] Create a thread pool for multi-file compilation
- [ ] Add full installation support in the makefile
- [X] Refactor file reader in main
- [ ] Encapsulate thread and channel within scanner API
- [X] Update src readme
- [ ] Display instructions for user when program end in fatal state
