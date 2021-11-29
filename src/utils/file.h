// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// Disk IO utilities for transferring lemon source code, standard libraries, and
// precompiled/cached IR to RAM.

#pragma once

#include <stdio.h>

#include "str.h"

//load the file named fname into memory as a null-terminated dynamically
//allocated C string. On failure returns NULL and errors are reported to
//the xerror log.
cstring *FileLoad(const cstring *name);

//adds the ".lem" extension to the input name and returns a dynamically
//allocated cstring. If the extension already exists, a duplicate copy
//of the input is returned.
cstring *FileGetDiskName(const cstring *name);
