/**
 * @file parser.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.
 * @brief Top-down operator precedence parser (aka the Pratt Parser).
 */

#include <assert.h>

#include "scanner.h"

//this function is not yet implemented,
//just a quick test loop  for now.
xerror parse(char *src)
{
	assert(src);

	xerror err = XESUCCESS;

	scanner *scn;

	err = scanner_init(&scn, src);

	xerror_trace("scanner init with return: %d", err);
	xerror_flush();

	token t;

	err = scanner_recv(scn, &t);
	xerror_trace("scanner recv with return: %d", err);
	xerror_flush();

	token_print(t);

	return err;
}
