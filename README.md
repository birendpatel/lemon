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

To install Lemon in the default directory /usr/local/bin:

1. Clone this repository
2. Change into the new directory
3. Issue `sudo make install`.
4. Uninstall with `sudo make uninstall`.

To install Lemon in a custom directory:

1. Clone this repository
2. Change into the new directory
3. Create a custom installation directory with `mkdir /my/new/path`
4. Issue `sudo make install INSTALL_PATH=/my/new/path`
5. Export the path by adding `export PATH=$PATH:/my/new/path` to `~/.profile`
6. Reload with `source ~/.profile`
4. Uninstall with `sudo make uninstall INSTALL_PATH=/my/new/path`

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Contribute

First, welcome! I'm genuinely surprised that another human being has stumbled upon this project!

If you'd like to contribute, the todo list below has a list of tasks that need to be done, but don't let that stop you from working on whatever you like. Raise an issue first to make sure its something we can agree on.

C code style rules for the project are pretty relaxed. All I really ask is that you check all error paths, add braces on control flow, verify memory leaks with valgrind, and document your code. Happy hacking!

# Todo

- [ ] Refactor shell handler to include waitpid and robust error handling (EASY)
- [ ] Add a 60 second language tutorial on this readme (EASY)
- [ ] Make the project compatible with Clang (MEDIUM)
- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Create a thread pool for multi-file compilation (HARD)
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add arrow key support for REPL (MEDIUM)
- [ ] Add support for escape characters in string literals (HARD)
- [ ] Remove the private keyword from the scanner, grammar, and kmap (MEDIUM)
