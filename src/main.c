/*
 * @file main.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Lemon compiler
 */

#include <stdio.h>
#include <stdlib.h>

#include "lemon.h"
#include "options.h"

options config_options(int argc, char **argv, int *argi, lemon_error *err);

int main(int argc, char **argv)
{
	lemon_error err = LEMON_EUNDEF;
	int argi = 0;

	options opt = config_options(argc, argv, &argi, &err);

	if (err) {
		fprintf(stderr, "CLemon error: %s\n", lemon_describe(err));
	}

	if (opt.diagnostic & DIAGNOSTIC_OPT) {
		options_display(&opt);
	}

	return EXIT_SUCCESS;
}

options config_options(int argc, char **argv, int *argi, lemon_error *err)
{
	options opt = options_init();

	*err = options_parse(&opt, argc, argv, argi);

	if (*err != LEMON_ESUCCESS) {
		return (options) {0};
	}

	return opt;
}
