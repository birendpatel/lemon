/**
 * @file defs.h
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Miscellaneous definitions.
 */

#pragma once

//lemon compiler info
#define LEMON_VERSION "6.0.0.0 (alpha)"

//allocation helpers
#define KiB(x) ((size_t) x * (size_t) 1024)

//wrapper for gcc fallthrough extension
#define fallthrough __attribute__((fallthrough))

/*******************************************************************************
 * @def kmalloc
 *
 * @details For the Lemon compiler, there is no situation where the program
 * will enter a recovery path and successfully sidestep an allocation failure.
 * If we checked the alloc return code, in all cases it would get passed up the 
 * call stack to main where a fatal buffer flush in xerror would be triggered.
 *
 * In many areas of the compiler (i.e., the parser), allocs are used so often
 * that checking the return value absolutely destroys code readability. It
 * offers very, very little benefit in return.
 *
 * I learned the following trick from Jens Gustedt's blog. If an alloc fails,
 * memset attempts to write a 0 to the first byte at NULL. It fails fast and
 * it fails early. Since the return value of an alloc is not known at compile
 * time, it also avoids triggering a -Wnonull warning from the GCC compiler.
 *
 * Also, note that this header doesn't automatically include the strings.h and
 * stdlib.h headers.
 ******************************************************************************/
#define kmalloc(target, bytes) memset((target = malloc(bytes)), 0, 1)
