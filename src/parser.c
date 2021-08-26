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

	scanner_init(&scn, src);

	token t;
	scanner_recv(scn, &t);

	while (t.type != _EOF) {
		token_print(t);
		scanner_recv(scn, &t);
	}

	scanner_free(scn);

	return err;
}
