# -*- MakeFile -*-
# Copyright (C) 2021 Biren Patel
# Lemon is licensed under the GNU General Public License v3.0

CC = gcc

CFLAGS = -Wall -Wextra -Werror -Wpedantic -Wnull-dereference
CFLAGS += -Wdouble-promotion -Wconversion -Wcast-qual 
CFLAGS += -march=native

################################################################################
# setup
################################################################################

.PHONY: all debug release docs clean

vpath %.h ./src
vpath %.h ./src/lib
vpath %.h ./extern/unity

vpath %.c ./src
vpath %.c ./extern/unity

objects := main.o xerror.o options.o compile.o parser.o scanner.o

###############################################################################
# build
###############################################################################

all: debug

debug: CFLAGS += -ggdb -O0 -DDEBUG
debug: lemon
	mkdir -p ./debug
	mv ./lemon ./debug
	@echo "\nBuild finished successfully."
	@echo "Lemon was compiled in debug mode."
	@echo "Issue 'make release' to turn on optimisations."

release: CFLAGS += -O2
release: lemon
	mkdir -p ./release
	mv ./lemon ./release
	@echo "\nBuild finished successfully."
	@echo "Lemon was compiled in release mode."
	@echo "Issue 'make' or 'make debug' to turn on debugging."

lemon: $(objects)
	$(CC) -o $@ $^ -lpthread

main.o: xerror.h compile.h options.h vector.h
xerror.o: xerror.h
options.o : xerror.h options.h
compile.o : xerror.h compile.h parser.h
parser.o : xerror.h parser.h channel.h scanner.h
scanner.o : xerror.h scanner.h channel.h

###############################################################################
# docs
###############################################################################

docs:
	rm -rf ./docs
	doxygen
	mkdir -p ./docs
	mv ./html/* ./docs
	rm -rf ./html ./latex
	@echo "\ndocumentation generated, see ./docs/index.html."

###############################################################################
# tests
# Unity framework is external, compiled w/out flags
###############################################################################

unity.o: unity.c unity.h unity_internals.h
	$(CC) -c $< -o $@

###############################################################################
# clean
###############################################################################

clean:
	@rm -f *.o
	@rm -rf ./lemon ./release ./debug
	@rm -rf ./docs ./html ./latex
	@echo "directory cleaned."
