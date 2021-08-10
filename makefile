# -*- MakeFile -*-
# Copyright (C) 2021 Biren Patel
# Lemon is licensed under the GNU General Public License v3.0

CC = gcc

CFLAGS = -Wall -Wextra -Werror -Wpedantic -Wnull-dereference
CFLAGS += -Wdouble-promotion -Wconversion -Wcast-qual -Wpacked -Wpadded
CFLAGS += -march=native

################################################################################
# setup
################################################################################

.PHONY: all debug release docs clean

vpath %.h ./src
vpath %.c ./src

objects := main.o
common := lemon.h

###############################################################################
# build
###############################################################################

all: debug

debug: CFLAGS += -ggdb -O0 -DDEBUG
debug: lemon
	@echo "Lemon was compiled in debug mode."
	@echo "Issue 'make release' to turn on optimisations."

release: CFLAGS += -O2
release: lemon
	@echo "Lemon was compiled in release mode."
	@echo "Issue 'make' or 'make debug' to turn on debugging."

lemon: $(objects)
	$(CC) $(objects) -o $@
	@echo "\nbuild finished successfully."

main.o : main.c $(common)
	$(CC) $(CFLAGS) -c $< -o $@

###############################################################################
# docs
###############################################################################

docs:
	doxygen
	cp ./html/index.html ./
	mv ./index.html ./lemon_docs.html
	@echo "documentation generated, see ./lemon_docs.html."

###############################################################################
# clean 
###############################################################################

clean:
	@rm -f *.o *.html
	@rm -rf ./html ./latex
	@rm -rf ./lemon
	@echo "directory cleaned."
