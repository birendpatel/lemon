# Lemon

Lemon is a programming language. It includes features such as:

- Static typing
- Garbage collection
- Closures
- Random variable types
- Object orientation

```python
import "io"

func main(void) -> void {
	let message: str = "hello world";
	io.print(message);
}
```

# Requirements

To install Lemon, you need a GNU/Linux OS with the following software installed:

- GCC 8.3.0+
- GNU Make 4.2.1+
- Python 3.8.10+

# Installation Guide

1. Clone this repository or download and extract the zip file
2. Change into the new directory with `cd lemon`
3. Execute the command `sudo python3 build install`
4. You're done! Lemon is ready to use. 
5. Uninstall the compiler with `sudo python3 build uninstall`

# License

This project is licensed under the GNU General Public License v3.0. You may find a copy of this license in this project's root directory.

# Todo

- [ ] Write vector unit tests (MEDIUM)
- [ ] Write channel unit tests (HARD)
- [ ] Write map unit tests (HARD)
- [ ] Write str unit tests (EASY)
- [ ] Write graph unit tests (EASY) 
- [ ] Add a lookup table for pthreads errors (EASY)
- [ ] Implement the pthreads lookup table for all pthreads error paths (MEDIUM)
- [ ] Add support for escape characters in string literals (HARD)
- [ ] Convert all diagnostic output to JSON (HARD)
- [ ] Update map.h insertions to recycle removed slots before aborting (MEDIUM)
- [ ] Sweep files and remove superfluous includes (EASY)
