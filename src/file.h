// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// File IO utilities

#pragma once

#include <stdio.h>

#include "lib/str.h"

//load the file named fname into memory as a null-terminated dynamically
//allocated C string. On failure returns NULL and errors are reported to
//the xerror log.
cstring *FileLoad(const cstring *name);
