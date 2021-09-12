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

	file ast = {0};

	parse(src, opt, fname, &ast);

	//temporary
	printf("file name: %s\n", ast.name);

	for (size_t i = 0; i < ast.fiats.len; i++) {
		printf("tag: %d\n", ast.fiats.data[i].tag);
	}
	//end temporary

	return XESUCCESS;
}
