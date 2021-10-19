# -*- MakeFile -*-
# Copyright (C) 2021 Biren Patel
# Lemon is licensed under the GNU General Public License v3.0

CC = gcc

CFLAGS = -std=gnu17 -Wall -Wextra -Werror
CFLAGS += -Wdouble-promotion -Wconversion -Wcast-qual -Wnull-dereference
CFLAGS += -DCOLOURS

# disable unused function warnings so that they don't interfere with C-style
# templating for vectors, channels, and maps.
CFLAGS += -Wno-unused-function

#-------------------------------------------------------------------------------
# setup
#-------------------------------------------------------------------------------

vpath %.h ./src
vpath %.h ./src/lib
vpath %.h ./src/assets
vpath %.h ./extern/unity
vpath %.h ./extern/cexception

vpath %.c ./src
vpath %.c ./src/lib
vpath %.c ./src/assets
vpath %.c ./extern/unity
vpath %.c ./extern/cexception

objects_raw := main.o xerror.o options.o parser.o scanner.o kmap.o cexception.o

DEBUG_DIR = ./debug/
objects_debug := $(addprefix $(DEBUG_DIR), $(objects_raw))

RELEASE_DIR = ./release/
objects_release := $(addprefix $(RELEASE_DIR), $(objects_raw))

#-------------------------------------------------------------------------------
# source dependencies
#-------------------------------------------------------------------------------

main_deps := defs.h options.h parser.h xerror.h str.h

xerror_deps := CException.h str.h xerror.h

options_deps := xerror.h defs.h options.h

scanner_deps := options.h xerror.h channel.h str.h scanner.h defs.h kmap.h

parser_deps := options.h scanner.h xerror.h str.h vector.h parser.h defs.h \
	channel.h

kmap_deps := scanner.h kmap.h

cexception_deps := CException.c

#-------------------------------------------------------------------------------
# debug build
#-------------------------------------------------------------------------------

.PHONY: all debug debug_deps

all: debug

debug: CFLAGS += -ggdb -O0 -DDEBUG
debug: debug_deps $(DEBUG_DIR)lemon
	@echo "\nBuild finished successfully."
	@echo "Lemon was compiled in debug mode."
	@echo "Issue 'make release' to turn on optimisations."

debug_deps:
	@mkdir -p $(DEBUG_DIR)

$(DEBUG_DIR)lemon: $(objects_debug)
	$(CC) -o $@ $^ -lpthread

$(DEBUG_DIR)main.o: main.c $(main_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEBUG_DIR)xerror.o: xerror.c $(xerror_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEBUG_DIR)options.o : options.c $(options_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEBUG_DIR)parser.o : parser.c $(parser_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEBUG_DIR)scanner.o : scanner.c $(scanner_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEBUG_DIR)kmap.o : kmap.c $(kmap_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(DEBUG_DIR)cexception.o : CException.c $(cexception_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

#-------------------------------------------------------------------------------
# release build
#-------------------------------------------------------------------------------

.PHONY: release release_deps

release: CFLAGS += -O3 -march=native
release: release_deps $(RELEASE_DIR)lemon
	@echo "\nBuild finished successfully."
	@echo "Lemon was compiled in release mode."
	@echo "Issue 'make' or 'make debug' to turn on debugging."

release_deps:
	@mkdir -p ./release

$(RELEASE_DIR)lemon: $(objects_release)
	$(CC) -o $@ $^ -lpthread

$(RELEASE_DIR)main.o: main.c $(main_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(RELEASE_DIR)xerror.o: xerror.c $(xerror_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(RELEASE_DIR)options.o : options.c $(options_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(RELEASE_DIR)parser.o : parser.c $(parser_deps) 
	$(CC) $(CFLAGS) -c -o $@ $<

$(RELEASE_DIR)scanner.o : scanner.c $(scanner_deps) 
	$(CC) $(CFLAGS) -c -o $@ $<

$(RELEASE_DIR)kmap.o : kmap.c $(kmap_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

$(RELEASE_DIR)cexception.o : CException.c $(cexception_deps)
	$(CC) $(CFLAGS) -c -o $@ $<

#-------------------------------------------------------------------------------
# install
#-------------------------------------------------------------------------------

.PHONY: install uninstall

INSTALL_PATH = /usr/local/bin

install: release
	cp ./release/lemon $(INSTALL_PATH)
	@echo "\nLemon installed successfully."

uninstall:
	rm -f $(INSTALL_PATH)/lemon
	@echo "\nLemon uninstalled successfully."

#-------------------------------------------------------------------------------
# memory checks
#-------------------------------------------------------------------------------

.PHONY: memcheck

memcheck: debug
	valgrind --leak-check=yes ./debug/lemon ./examples/expressions.lem

#-------------------------------------------------------------------------------
# tests
# Unity framework is external, compiled w/out flags
#-------------------------------------------------------------------------------

.PHONY: test test_scanner

TESTNAMES = test_scanner

test: $(TESTNAMES)

unity.o: unity.c unity.h unity_internals.h
	$(CC) -c $< -o $@

test_scanner: debug
	cd ./test; python3 ./test_scanner.py

#-------------------------------------------------------------------------------
# clean
#-------------------------------------------------------------------------------

.PHONY: clean

clean:
	@rm -rf ./lemon ./release ./debug ./docs
	@echo "directory cleaned."
