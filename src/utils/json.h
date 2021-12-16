// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// JSON serialisation utilities

#pragma once

#include "str.h"

typedef struct json json;

json *JsonInit(void);

cstring *JsonToString(json *self);

JsonOpenObject(json *self);

JsonCloseObject(json *self);
