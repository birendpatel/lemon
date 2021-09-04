#! /usr/bin/env python3

# kmap.py
# Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
#
# The lemon scanner (../scanner.c) uses a perfect hash function to detect
# keywords during its lexical analysis. This perfect hash is built using
# gperf and then hidden behind the kmap.h API.
#
# Unfortunately, the generated C code has a few issues. First, it
# triggers a -Wconversion error during compilation. Second, it declares
# and defines the kv_pair, but this struct needs to be made public in the
# kmap.h API. Finally, the lookup function uses strcmp but strcmp requires
# two null-terminated char arrays. The scanner's pointer into the in-memory
# source code does not point to strings. Gperf also fails to include the
# string.h header.
#
# This python script does three things to the gperf generated code. It 
# introduces GCC pragmas to bypass the -Wconversion error. It removes the 
# kv_pair struct definition. And it includes string.h and then swaps strcmp
# for strncmp. This script is then triggered by the build system whenever
# keywords.txt is updated.

from subprocess import run

# execute gperf on the shell and capture the generated C code from stdout.
# stdout is captured as a python bytes object so it is decoded to a string.
def exec_gperf() -> str:
	options = "-t -C --null-strings"
	hashname = "--hash-function-name=kmap_hash"
	input = "keywords.txt"
	cmd = "gperf {} {} {}".format(options, hashname, input)

	proc_status = run(cmd, check=True, shell=True, capture_output=True)

	return proc_status.stdout.decode()

# include the strings.h header which is required by strncmp and the kmap.h
# API that the ../scanner.c file requires.
def add_include(text: str) -> str:
	str_directive = "#include <string.h>"
	api_directive = "#include \"kmap.h\""

	return "\n\n".join([str_directive, api_directive, text])

# add the pragma preprocessor directives to bypass the -Wconversion error.
def add_diagnostic(text: str) -> str:
	push = "#pragma GCC diagnostic push"
	flag = "#pragma GCC diagnostic ignored \"-Wconversion\""
	pop  = "#pragma GCC diagnostic pop"

	return "\n\n".join([push, flag, text, pop])

# remove struct kv_pair to avoid redefinition from kmap.h.
def remove_struct(text: str) -> str:
	struct = "struct kv_pair { char *name; token_type typ; };"

	return text.replace(struct, "//kv_pair defined in kmap.h")

if __name__ == "__main__":
	text = exec_gperf()

	pipeline = (add_include, add_diagnostic, remove_struct)

	for func in pipeline:
		text = func(text)

	print(text)
