/**
 * @file compile.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Lemon-to-bytecode compiler implementation.
 */

#include <assert.h>

#include "compile.h"
#include "parser.h"

xerror compile(char *src, options *opt)
{
	assert(opt);
	assert(src);

	parse(src, opt);

	return XESUCCESS;
}
