# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Object orientation

This repository contains CLemon, an aggresively optimising bytecode compiler for the Lemon language. It places an emphasis on optimisation for mathematical code, particularly probability theory.

```Python
import "math"
import "io"

func main(void) -> void {
	# declares an immutable gaussian random variable mu=0, sigma=1
	let X: rvar = norm ~ (0, 1);

	# the entropy of X is reduced to a compile-time constant
	io.print(math.entropy(X));

	# declares an array of two independent bernoulli random variables
	let prob: f64 = 0.5;

	let Y: [2]rvar = [
		[0] = bern ~ (prob),
		[1] = bern ~ (prob)
	];

	# Y1 + Y2 is not simulated. The optimiser detects the convolution
	# and generates bytecode to reduce the operation down to a single 
	# random sample from a binomial distribution ~ (2, p)
	io.print(Y[1] + Y[2])

	#the same as above, more explicit and easier to use
	io.print(math.bernoulli_convolute(Y))
}
```

# Requirements

To install Lemon, you need a GNU/Linux OS with the following software installed:

- GCC 8.3.0+
- GNU Make 4.2.1+
- Python 3.8.10+

# Installation Guide

1. Clone this repository or download and extract the zip file
2. Change into the new directory with `cd ./lemon`
3. Execute the command `sudo python3 build.py install`
4. You're done! Execute `lemon [source file]` or `lemon --help`.
5. Uninstall the compiler with `sudo python3 build.py uninstall`

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Todo

- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Create a thread pool for multi-file compilation (HARD)
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add support for escape characters in string literals (HARD)
- [ ] Convert all diagnostic output to JSON (HARD)
- [ ] Refactor string implementation to not use vector.h (HARD)
- [ ] Update channel.h to use function malloc (EASY)
- [ ] Update map.h insertions to recycle removed slots before aborting (MEDIUM)
