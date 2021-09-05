# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Matrix types

Lemon is compiled to bytecode and executed on a virtual machine written in C.

# Requirements

To build Lemon, you need a POSIX compliant system or comptability layer with the following software installed:

- GCC 8.3.0+
- GNU Make 4.2.1+
- Doxygen 1.8.13+ (optional)

# Installation Guide

Clone this repository and issue the command `sudo make install` from within the new directory. By default, the Lemon executable will be placed in `/usr/local/bin`.

If you want to install Lemon in a different directory, just use `sudo make install INSTALL_PATH=...` instead. You may need to export the path. If so, add `export PATH=$PATH:...` to your `~/.profile` and then reload with `. ~/.profile`.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Contribute

First, welcome! I'm genuinely surprised that another human being has stumbled upon this project!

If you'd like to contribute, the todo list below has a list of tasks that need to be done, but don't let that stop you from working on whatever you like. Raise an issue first to make sure its something we can agree on.

C code style rules for the project are pretty relaxed. All I really ask is that you check all error paths, add braces on control flow, verify memory leaks with valgrind, and document your code. Happy hacking!

# Todo

- [X] Add null, void, self, addressof, rvar literal, bool literal, to scanner (MEDIUM)
- [X] Add null, void, self to grammar (EASY)
- [ ] Refactor shell handler to include waitpid and robust error handling
- [ ] Add locale support to main.c (MEDIUM)
- [ ] Add a 60 second language tutorial on this readme (EASY)
- [ ] Make the project compatible with Clang (MEDIUM)
- [X] Implement diagnostic flag for multithreading on scanner (EASY)
- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Create a thread pool for multi-file compilation (HARD)
- [X] Add full installation support in the makefile (EASY)
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add arrow key support for REPL (MEDIUM)
- [ ] Clean up, refactor, and finish documentation for scanner.c (EASY)
