# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Matrix types

# Requirements

To install Lemon, you need a POSIX compliant system or POSIX comptability layer with the following software installed:

- GCC 8.3.0+
- GNU Make 4.2.1+

If you want to hack around on the source code or become a contributor, you also need to install:

- Doxygen 1.8.13+
- Python 3.8.10

# Installation Guide

1. Clone this repository
2. Change into the new directory
3. Execute the command `sudo make install`.
4. You're done! Execute `lemon [options] [files]` or `lemon --help`.
5. Uninstall with `sudo make uninstall`.

By default, Lemon will be installed in the `/usr/local/bin` directory. You can change this path by using the `INSTALL_PATH` makefile variable.

The Lemon compiler can highlight user errors and warnings in a variety of colours.If your terminal supports colour output, you can install Lemon with `sudo make install CFLAGS+=-DCOLOURS`.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Internals

The Lemon bytecode compiler and virtual machine are written in C with the GNU dialect of the ISO C17 standard. Auxiliary programs, such as for unit testing and metaprogramming, are written in Python 3.

# Todo

- [ ] Refactor shell handler to include waitpid and robust error handling (EASY)
- [ ] Add a 60 second language tutorial on this readme (EASY)
- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Create a thread pool for multi-file compilation (HARD)
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add arrow key support for REPL (MEDIUM)
- [ ] Add support for escape characters in string literals (HARD)
- [ ] Remove the private keyword from the scanner, grammar, and kmap (MEDIUM)
- [ ] Rename 'for loop' token name to 'for' due to func declarations (EASY)
- [ ] Refactor the makefile (MEDIUM)
- [ ] Update makefile dependencies for headers (EASY)
- [ ] Convert all diagnostic output to JSON (HARD)
