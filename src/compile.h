// Copyright (C) 2021 Biren Patel. GNU General Public License v.3.0.

#pragma once

#include "options.h"
#include "xerror.h"

xerror compile(char *src, options *opt, char *fname);
