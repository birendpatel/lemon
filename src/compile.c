/**
 * @file compile.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Lemon-to-bytecode compiler implementation.
 */

#include "compile.h"
#include "parser.h"

xerror compile(options *opt, char *src)
{
	assert(opt);
	assert(src);

	parse(src);

	return XESUCCESS;
}
