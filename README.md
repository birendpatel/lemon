# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Object orientation

This repository contains CLemon, an aggressively optimising bytecode compiler for the Lemon language. It places an emphasis on optimisation for mathematical code, particularly for probability theory.

```Python
import "math"
import "io"

func main(void) -> void {
	# define a standard guassian random variable
	let X: rvar = norm ~ (0, 1);

	# we can sample directly from X and receive a new random value every time
	let sample_1 = X.sample();
	let sample_2 = X.sample();
	
	if (sample_1 == sample_2) {
		io.print("this code is unreachable; the samples are random!");
	}

	# the entropy of X is reduced to a compile-time constant and stored directly
	# as a machine instruction operand.
	io.print(math.entropy(X));

	# fills an array with 10,000 bernoulli variables parameterised by probability 0.5
	let Y: [2]rvar = [
		[0] = bern ~ (0.5)
	];
	
	# all variables are immutable by default unless the 'mut' qualifier is specified
	let mut sum: f64 = 0.0;
	
	# the optimiser detects that the sum is a probability convolution and reduces it
	# to a single random sample from a binomial distribution with n = 10000 and
	# p = 0.5. The entire for loop is optimised out.
	for (let i: i16 = 0; i < 10000; i = i + 1) {
		sum += Y[i].sample()
	}

	# the same as above; sometimes the programmer needs to do mathematical programming
	# but may not necessarily know what a specific theory is. When they don't know, the
	# optimiser is there to help. When they do know, the standard library is there to
	# make life simple.
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
