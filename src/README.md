# File Descriptions

`main`: Contains the REPL and all disk IO management. 

`def`: Contains helper macros and GCC extension wrappers.

`options`: Command line options parser implemented via GNU argp.

`xerror`: Lightweight error handler with a thread-safe logging facility.

`nodes`: Contains definitions for all abstract syntax tree nodes.

`parser`: Constructs an abstract syntax tree via recursive descent.

`scanner`: Multithreaded Source code tokenizer.

`compile`: Orchestrates parsing, semantic analysis, and IR optimisation.
