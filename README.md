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

# Installation Guide

1. Clone this repository
2. Change into the new directory
3. Execute the command `sudo make install`.
4. You're done! Execute `lemon [options] [files]` or `lemon --help`.
5. Uninstall with `sudo make uninstall`.

By default, Lemon will be installed in the `/usr/local/bin` directory. You can change this path by using the `INSTALL_PATH` makefile variable.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Todo

- [ ] Add a 60 second language tutorial on this readme (EASY)
- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Create a thread pool for multi-file compilation (HARD)
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add support for escape characters in string literals (HARD)
- [ ] Refactor the makefile (MEDIUM)
- [ ] Convert all diagnostic output to JSON (HARD)
- [ ] Refactor string implementation to not use vector.h (HARD)
- [ ] Update channel.h to use function malloc (EASY)
- [ ] Update map.h insertions to recycle removed slots before aborting (MEDIUM)
- [ ] Add 'as' keyword to grammar and scanner
- [ ] implement 'as' keyword for import nicknames
