# Lemon
Lemon is a dynamic programming language. It includes features such as:

- Garbage collection
- First class functions
- Closures

It also includes uncommon features such as:

- Random variables
- Built-in linear algebra

This repository implements Lemon in C with GNU extensions and is licensed under the GNU General Public License v3.0. You may find a copy of this license in the root directory.

# Install

Clone this repository and issue `make release`. If successful, you should see `Build finished successfully. Lemon was compiled in release mode.` The executable `lemon` will be placed in the newly created `release` directory.

You can issue `make docs` to generate the source code documentation via Doxygen.

# FAQ

## Why is it called Lemon?

It was picked completely at random from a dictionary.

## Is this a real programming language?

Yes, but I have not yet implemented a standard library or an OS interface.

## Why did you build it?

I wanted to play around with the idea of incorporating probability theory directly into a language. It allows the compiler to perform interesting high-level optimisations. For example, Lemon can detect probability convolutions. If the user sums 1 million IID bernoulli variables, Lemon will optimize the bytecode and reduce it to a single binomial sample.

On modern CPUs Lemon can also perform extreme low-level optimisations. A stream of bernoulli random samples can be optimised into a bit trie. This can be transformed into a tiny virtual machine that leverages AVX512 for absolutely massive throughputs.

I was inspired by the crazy high-level optimisations that Haskell compilers can perform before dropping into LLVM IR.

## Are these actually frequently asked questions?

No, the repository has zero stars and one contributor. I'm basically talking to myself out loud.
