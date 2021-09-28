// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.

#pragma once

#include "options.h"
#include "xerror.h"

//create an abstract syntax tree. The input src must be null terminated.
xerror parse(options *opt, char *src, char *fname, file **ast);
