#! /usr/bin/env python3

# Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
#
# This script generates a makefile which separates installation, release, and
# debug builds into separate folders. The GNU C compiler is required in order
# to use this script.
#
# The generated makefile is never saved to disk; It is always created on demand
# and fed as standard input into 'make'. This guarantees that file dependencies
# are always up to date. 

from subprocess import run

#-------------------------------------------------------------------------------

executable_name = "lemon"
debug_dir = "./debug/"
release_dir = "./release/"

# any time a new C source file is created for the compiler, its path from the
# root project directory must be added to this list
files = [
    "./src/main.c",
    "./src/xerror.c",
    "./src/options.c",
    "./src/file.c",
    "./src/jobs.c",
    "./src/scanner.c",
    "./src/parser.c",
    "./src/symtab.c",
    "./src/assets/kmap.c",
    "./extern/cexception/CException.c"
]

# system static and dynamic libraries must be added to this list. Just add the
# name; the script will resolve the path
libraries = [
    "pthread"
]

#-------------------------------------------------------------------------------
# The start of any good makefile; the common compiler flags used by all builds
# can be added here.
#
# Do not change the compiler! Both the Lemon source code and the build system
# absolutely depend on GCC.

preamble = """\
CC = gcc

CFLAGS = -std=gnu17 -Wall -Wextra -Werror -Wpedantic
CFLAGS += -Wconversion -Wcast-qual -Wnull-dereference
CFLAGS += -Wdouble-promotion

# enable coloured error output from ./src/xerror.c
CFLAGS += -DCOLOURS

#allows for templated data structures in ./src/lib
CFLAGS += -Wno-unused-function

.PHONY: all\n
"""

#-------------------------------------------------------------------------------
# Separate build requirements are accomplished by GNU Make's target specific
# variable values. See the GNU Make manual chapter 6 section 11. Add build
# specific compiler flags and user instructions/notifications here.

debug_build = """\
.PHONY: debug debug_deps

debug: CFLAGS += -ggdb -O0 -DDEBUG
debug: debug_deps {0}lemon
\t@echo "\\nBuild finished successfully."
\t@echo "Lemon was compiled in debug mode."
\t@echo "Issue 'make release' to turn on optimisations."

debug_deps:
\t@mkdir -p {0}\n
""".format(debug_dir)

release_build = """\
.PHONY: release release_deps

release: CFLAGS += -O3 -march=native
release: release_deps {0}lemon
\t@echo "\\nBuild finished successfully."
\t@echo "Lemon was compiled in release mode."

release_deps:
\t@mkdir -p {0}\n
""".format(release_dir)

#-------------------------------------------------------------------------------
# cleanup

clean = """\
.PHONY: clean

clean:
\t@rm -rf {} {}
\t@echo "directories cleaned
""".format(debug_dir, release_dir)

#-------------------------------------------------------------------------------
# Reserved makefile rules carried over from the previous build system. Not in
# use.

memcheck = """\
.PHONY: memcheck

memcheck: debug
\t valgrind --leak-check=yes ./debug/lemon ...\n
"""

tests = """\
.PHONY: test test_scanner

TESTNAMES = test_scanner

test: $(TESTNAMES)

unity.o: unity.c unity.h unity_internals.h
\t$(CC) -c $< -o $@

test_scanner: debug
\tcd ./test; python3 ./test_scanner.py
"""

#-------------------------------------------------------------------------------
# This collection of functions creates all of the build rules. Essentially, the
# makefile functions and concepts like vpath, implicit rules, addprefix, and so
# on are all replaced by brute force python functions.

def build(directory: str) -> str:
    global files
    script = ""

    if directory == debug_dir:
        script += debug_build
    elif directory == release_dir:
        script += release_build
    else:
        ValueError("invalid target directory: {}".format(directory))

    for file in files:
        script += create_object_rule(directory, file)

    script += create_exe_rule(directory, script)

    return script

def create_object_rule(directory: str, file: str) -> str:
    command = "gcc -MM {}".format(file)

    result = run(command, check=True, shell=True, capture_output=True)
    
    target = directory
    prerequisites = result.stdout.decode()
    recipe = "\n" #implicit rule

    return target + prerequisites + recipe

def create_exe_rule(directory: str, script: str) -> str:
    global executable_name

    target = directory + executable_name + ": "
    prerequisites = get_linker_prerequisites(script)
    recipe = "\n\t($CC) -o $@ $^ {}\n\n".format(get_libraries())
    
    return target + prerequisites + recipe

def get_linker_prerequisites(script: str) -> str:
    objects = []
    pattern = ".o:"
    tokens = script.split()

    for word in tokens:
        tail = word[-3:]

        if tail == pattern:
            word_no_colon = word[:-1]

            objects.append(word_no_colon)
    
    return ' '.join(objects)

def get_libraries() -> str:
    global libraries

    flags = []

    for lib in libraries:
        flags.append('-l' + lib)

    return ' '.join(flags)

#-------------------------------------------------------------------------------

def create_makefile() -> str:
    makefile = preamble
    makefile += build(debug_dir)
    makefile += build(release_dir)
    makefile += clean

    return makefile

if __name__ == "__main__":
    makefile = create_makefile()

    print(makefile)
