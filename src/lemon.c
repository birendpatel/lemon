/**
 * @file lemon.c
 * @author Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
 * @brief Implementation file for lemon.h API.
 */

#include "lemon.h"

static const char *const lookup[LEMON_EUNDEF] = {
	[LEMON_ESUCCESS] = "function terminated successfully.",
	[LEMON_ENOMEM] = "dynamic allocation failed.",
	[LEMON_EOPTION] = "options parsing failed."
};

const char *lemon_describe(lemon_error err)
{
	static const char *erange = "(critical) invalid error code";

	if (err < LEMON_ESUCCESS || err >= LEMON_EUNDEF) {
		return erange;
	}

	return lookup[err];
}
