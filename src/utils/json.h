// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// This API provides JSON serialisation utility functions. Application code may
// use the API to construct JSON parse trees, which can be serialised to C null
// terminated strings.

#pragma once

#include <stdint.h>

#include "str.h"
#include "vector.h"

//------------------------------------------------------------------------------
// JSON parse trees must be rooted at either a json_object or json_array node

typedef struct json_object json_object;
typedef struct json_value json_value;
typedef struct json_array json_array;

typedef enum json_value_tag {
	JSON_VALUE_OBJECT,
	JSON_VALUE_ARRAY,
	JSON_VALUE_STRING,
	JSON_VALUE_NUMBER,
	JSON_VALUE_TRUE,
	JSON_VALUE_FALSE,
	JSON_VALUE_NULL,
} json_value_tag;

//true, false, and null tags do not have an associated payload; the object,
//array, and string pointers must not be NULL
struct json_value {
	json_value_tag tag;
	union {
		json_object *object;
		json_array *array;
		cstring *string;
		int64_t number;
	}
};

make_map(json_value, JsonValue, static)
make_vector(json_value, JsonValue, static)

struct json_object {
	map(JsonValue) values;
};

struct json_array {
	vector(JsonValue) values;
};

//------------------------------------------------------------------------------
// convert a JSON parse tree to a dynamically allocated C string or return NULL
// if the input parse tree is ill-formed

cstring *JsonSerializeObject(json_object object);
cstring *JsonSerializeArray(json_array array);
