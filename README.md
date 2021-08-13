# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Matrix types

This repository implements Lemon in C, nicknamed CLemon. CLemon is an interpreter and has a bytecode compiler that performs high-level optimisations. In the future, CLemon's optimised IR will funnel to LLVM.

# Install

Clone this repository and issue `make release`. The executable `lemon` will be placed in the newly created `release` directory.

You can issue `make docs` to generate the source code documentation via Doxygen.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in the root directory.
