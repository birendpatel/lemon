# Data Structures Library

Each header file located within this directory is standalone and has no dependencies beyond the GNU glibc library. Like Lemon, each file is licensed under the GNU General Public License v3.0.

If you wish to use any of these data structures in your own projects, just copy over the header file and you're good to go! If you're missing anything a preprocessor error will let you know what to do when you attempt to compile.

All of these data structures abort the application program whenever a dynamic allocation request fails. Since they are templated using preprocessor token pasting, the underlying structs are not opaque. However, you should never read and write directly to struct members. Use only the provided API.
