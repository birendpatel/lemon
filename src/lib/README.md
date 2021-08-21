# lib 
This directory contains source code used by, but completely decoupled from, the Lemon compiler. This includes data structures and generic functions. 

The unit tests depend on the Unity software located in `/extern/unity` or found at [ThrowtheSwitch](http://www.throwtheswitch.org/unity).

In general, `/extern` contains decoupled 3rd party software while `/src/lib` contains decoupled in-house software.

`vector.h`: Templated dynamic array which appends and sets shallow copies. Lemon uses vectors to implement the REPL and for program sections like .text and .data.

`channel.h`: Templated thread-safe FIFO queue. It supports multiple producers and consumers. All read/write operations are blocking. Lemon uses a channel to enable the scanner and parser to run concurrently. The scanner sends tokens onto the channel and the parser collects them on an as-needed basis.
