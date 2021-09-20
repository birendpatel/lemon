/**
 * @file compile.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Lemon-to-bytecode compiler implementation.
 */

#include <assert.h>

#include "compile.h"
#include "nodes.h"
#include "parser.h"

//temporary
#include <stdio.h>

xerror compile(char *src, options *opt, char *fname)
{
	assert(opt);
	assert(src);

	file *ast = NULL;
	xerror err = parse(src, opt, fname, &ast);

	if (err) {
		return err;
	}

	return XESUCCESS;
}
