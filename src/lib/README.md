# lib
This directory contains data structures used by the Lemon compiler. Each file is standalone and has no dependencies beyond the GNU glibc library. Each data structure is written using C-style templates.

The unit tests depend on the Unity software located in `/extern/unity` or found at [ThrowtheSwitch](http://www.throwtheswitch.org/unity).

`vector.h`: Dynamic array with a doubling growth rate. It only performs shallow copying. It is not thread-safe.

`channel.h`: Thread-safe multi-producer multi-consumer FIFO queue. All read/write operations are blocking. The queue is fixed-length.

`map.h`: Associative array mapping any hashable key to any shallow-copyable value.Implemented as a linear probing hash table.
