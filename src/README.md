# src 
`lemon.h`: Common header file used by all other lemon source files. It contains compiler checks, error codes, and system headers.

`lemon.c`: Implements debugging and error functions specified in `lemon.h`.

`main.c`: Lemon entry point. Parses command line arguments and then either compiles from a file or loads the REPL.

`options.h`: API for the command line options parser.

`options.c`: Implements the options parser via GNU argp.
