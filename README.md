# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Matrix types

This repository implements Lemon in C, henceforth called CLemon. CLemon is an interpreter and has a bytecode compiler that performs high-level optimisations. In the future, CLemon's optimised IR will funnel to LLVM.

# Install

Clone this repository and issue `make release`. The executable `lemon` will be placed in the newly created `release` directory.

You can issue `make docs` to generate the source code documentation via Doxygen.

Check out the makefile if you want to build Lemon in debug mode or issue any of the unit and integration tests.

# Philosophy

The focus of Lemon is its native support for random variables. This allows us to engineer a variety of interesting high level optimisations on the IR.

For example, we can simplify probability convolutions. The sum of 1 million bernoulli samples will reduce to 1 binomial sample at compile time. Or, we can leverage CPU vectorisation for parallel sampling. We can precalculate the entropy of a density function and shift the value into the program text segment as an immediate value.

These types of optimisations are tremendously hard to perform in dynamic languages like Python which depend heavily on runtime information.

In the distant future, Lemon will hopefully take a form similar to the Glasgow Haskell Compiler. In that it will perform high level IR modifications before funneling to a low level optimiser like LLVM.

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in the root directory.
