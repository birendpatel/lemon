# src 
`main`: This is the lemon entry point. It parses command line arguments and then either compiles from a file or loads the REPL.

`options`: This contains the command line options parser, which is implemented with GNU argp.

`xerror`: Xerror is a lightweight error handler. It include a thread-safe logging facility.

`parser`: This constructs an AST via a Pratt parser. A parse tree is not constructed as an intermediate step.

`scanner`: This is the source code tokenizer. It runs in a separate thread and communicates tokens to the parser via a thread-safe FIFO queue.

`compile`: Controller for the front and backend portions of the compiler. Raw source code goes in and a VM-executable program comes out. Start here if you're reading the source code for the first time.
