// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "arena.h"
#include "json.h"

typedef struct json json;

static void StartNextLine(json *);
static void PutJsonString(json *, const cstring *);
static void PutCharNextLine(json *, const char);
static cstring *Serialize(const void *, const json_value_tag);
static void SerializeObject(json *, const json_object *);
static void SerializeArray(json *, const json_array *);
static void Dispatch(json *, const json_value);
static void SerializeString(json *, const cstring *);
static void SerializeNumber(json *, const int64_t);
static void SerializeBoolean(json *, bool);
static void SerializeNull(json *);

//------------------------------------------------------------------------------

json_object *JsonObjectInit(void)
{
	const size_t bytes = sizeof(json_object);
	json_object *new = allocate(bytes);
	new->values = JsonValueMapInit(MAP_DEFAULT_CAPACITY);

	return new;
}

bool JsonObjectAdd(json_object *object, cstring *key, json_value value)
{
	assert(object);
	assert(key);

	return JsonValueMapInsert(&object->values, key, value);
}

json_array *JsonArrayInit(void)
{
	const size_t bytes = sizeof(json_array);
	json_array *new = allocate(bytes);
	new->values = JsonValueVectorInit(0, VECTOR_DEFAULT_CAPACITY);

	return new;
}

void JsonArrayAdd(json_array *array, json_value value)
{
	assert(array);

	JsonValueVectorPush(&array->values, value);
}

//------------------------------------------------------------------------------
// pre-order traversal

struct json {
	vstring vstr;
	size_t indent;
};

cstring *JsonSerializeObject(const json_object *object)
{
	return Serialize(object, JSON_VALUE_OBJECT);
}

cstring *JsonSerializeArray(const json_array *array)
{
	return Serialize(array, JSON_VALUE_ARRAY);
}

static cstring *Serialize(const void *data, const json_value_tag tag)
{
	assert(data);
	assert(tag == JSON_VALUE_OBJECT || tag == JSON_VALUE_ARRAY);

	json js = {
		.vstr = vStringInit(KiB(1)),
		.indent = 0
	};

	if (tag == JSON_VALUE_OBJECT) {
		SerializeObject(&js, data);
	} else {
		SerializeArray(&js, data);
	}

	return cStringFromvString(&js.vstr);
}

//------------------------------------------------------------------------------
//formatting and whitespace utilities

static void StartNextLine(json *self)
{
	assert(self);

	static const cstring *tab = "    ";

	if (self->vstr.buffer[0] == '\0') {
		return;
	}

	vStringAppend(&self->vstr, '\n');

	for (size_t i = 0; i < self->indent; i++) {
		vStringAppendcString(&self->vstr, tab);
	}
}

static void PutJsonString(json *self, const cstring *cstr)
{
	assert(self);
	assert(cstr);

	vStringAppend(&self->vstr, '"');
	vStringAppendcString(&self->vstr, cstr);
	vStringAppend(&self->vstr, '"');
}

static void PutCharNextLine(json *self, const char ch)
{
	assert(self);

	StartNextLine(self);

	vStringAppend(&self->vstr, ch);
}

//------------------------------------------------------------------------------

static void Dispatch(json *self, const json_value value)
{
	assert(self);

	switch (value.tag) {
	case JSON_VALUE_OBJECT:
		SerializeObject(self, value.object);
		break;

	case JSON_VALUE_ARRAY:
		SerializeArray(self, value.array);
		break;

	case JSON_VALUE_STRING:
		SerializeString(self, value.string);
		break;

	case JSON_VALUE_NUMBER:
		SerializeNumber(self, value.number);
		break;

	case JSON_VALUE_TRUE:
		SerializeBoolean(self, true);
		break;

	case JSON_VALUE_FALSE:
		SerializeBoolean(self, false);
		break;

	case JSON_VALUE_NULL:
		SerializeNull(self);
		break;

	default:
		assert(0 != 0 && "invalid json value tag");
		__builtin_unreachable();
		break;
	}
}

static void SerializeObject(json *self, const json_object *object)
{
	assert(self);
	assert(object);

	vStringAppend(&self->vstr, '{');
	self->indent++;

	map(JsonValue) map = object->values;
	size_t items_placed = 0;

	for (size_t i = 0; i < map.cap; i++) {
		if (map.buffer[i].status != SLOT_CLOSED) {
			continue;
		}

		if (items_placed != 0) {
			vStringAppend(&self->vstr, ',');
		}

		StartNextLine(self);

		PutJsonString(self, map.buffer[i].key);
		vStringAppendcString(&self->vstr, ": ");
		Dispatch(self, map.buffer[i].value);

		items_placed++;
	}
	
	self->indent--;
	PutCharNextLine(self, '}');
}

static void SerializeArray(json *self, const json_array *array)
{
	assert(self);
	assert(array);

	vStringAppend(&self->vstr, '[');
	self->indent++;

	vector(JsonValue) vec = array->values;

	for (size_t i = 0; i < vec.len; i++) {
		if (i != 0) {
			vStringAppend(&self->vstr, ',');
		}

		Dispatch(self, vec.buffer[i]);
	}

	self->indent--;
	PutCharNextLine(self, ']');
}

static void SerializeString(json *self, const cstring *cstr)
{
	assert(self);
	assert(cstr);

	PutJsonString(self, cstr);
}

static void SerializeNumber(json *self, const int64_t number)
{
	assert(self);

	size_t num_digits = (size_t) (ceil(log10((double) number)) + 1);

	cstring *cstr = allocate(num_digits * sizeof(char));

	sprintf(cstr, "%" PRIx64 "", number);

	vStringAppendcString(&self->vstr, cstr);
}

static void SerializeBoolean(json *self, bool flag)
{
	assert(self);

	cstring *cstr = "false";

	if (flag) {
		cstr = "true";
	}

	vStringAppendcString(&self->vstr, cstr);
}

static void SerializeNull(json *self)
{
	assert(self);

	vStringAppendcString(&self->vstr, "null");
}
