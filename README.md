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

# Contribute

First, welcome! I'm genuinely surprised that another human being has stumbled upon this project!

If you'd like to contribute, the todo list below has a list of tasks that need to be done, but don't let that stop you from working on whatever you like. Raise an issue first to make sure its something we can agree on.

C code style rules for the project are pretty relaxed. All I really ask is that you check all error paths, add braces on control flow, verify memory leaks with valgrind, and document your code. Happy hacking!

# Todo

- [ ] Implement addressof, rvar literal, bool literal, array literal, self, void, null in scanner
- [ ] Add self, void, null to grammar
- [ ] Add rvar literal to var declaration in grammar
- [ ] Fix the diagnostic-all flag  which doesn't set the interactive flag (EASY)
- [ ] Add locale support to main.c (MEDIUM)
- [ ] Add a 60 second language tutorial on this readme (EASY)
- [ ] Make the project compatible with Clang (MEDIUM)
- [ ] Add a diagnostic flag for multithreading (EASY)
- [ ] Add tracing to the vector template (EASY)
- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Create a thread pool for multi-file compilation (HARD)
- [ ] Add full installation support in the makefile (EASY)
- [ ] Add a macro called XERROR_DEBUG to xerror.h. Default it to OFF (TRIVIAL)
- [ ] Update xerror trace to cause a flush in debug mode (EASY)
- [ ] Add thread ID tracing to the channel template (EASY)
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add arrow key support for REPL (MEDIUM)
- [ ] Automate gperf modifications and build system (see todo in kmap.c) (HARD)
- [ ] Clean up, refactor, and finish documentation for scanner.c (EASY)
