// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.
//
// This API provides JSON serialisation utility functions. Application code may
// use the API to construct JSON parse trees, which can be serialised to C null
// terminated strings.

#pragma once

#include <stdint.h>

#include "map.h"
#include "str.h"
#include "vector.h"

typedef struct json_object json_object;
typedef struct json_value json_value;
typedef struct json_array json_array;

//------------------------------------------------------------------------------
//true, false, and null tags do not have an associated payload; the object,
//array, and string pointers must not be NULL

typedef enum json_value_tag {
	JSON_VALUE_OBJECT,
	JSON_VALUE_ARRAY,
	JSON_VALUE_STRING,
	JSON_VALUE_NUMBER,
	JSON_VALUE_TRUE,
	JSON_VALUE_FALSE,
	JSON_VALUE_NULL,
} json_value_tag;

struct json_value {
	json_value_tag tag;
	union {
		json_object *object;
		json_array *array;
		cstring *string;
		int64_t number;
	};
};

make_map(json_value, JsonValue, static)

make_vector(json_value, JsonValue, static)

//------------------------------------------------------------------------------
//JSON parse trees must be rooted at either a json_object or json_array node

struct json_object {
	map(JsonValue) values;
};

//returned pointer is always valid
json_object *JsonObjectInit(void);

//return false if the key already exists
bool JsonObjectAdd(json_object *object, cstring *key, json_value value);

struct json_array {
	vector(JsonValue) values;
};

//returned pointer is always valid
json_array *JsonArrayInit(void);

void JsonArrayAdd(json_array *array, json_value value);

//------------------------------------------------------------------------------
//convert a JSON parse tree to a dynamically allocated C string or return NULL
//if the input parse tree is ill-formed

cstring *JsonSerializeObject(const json_object *object);
cstring *JsonSerializeArray(const json_array *array);
