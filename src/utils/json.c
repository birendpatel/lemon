// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include "arena.h"
#include "json.h"

typedef struct json json;

static cstring *Serialize(const void *, const json_value_tag);
static void SerializeObject(json *, const json_object *);
static void Dispatch(json *, const json_value);

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

cstring *JsonSerializeObject(const json_object *object)
{
	return Serialize(object, JSON_VALUE_OBJECT);
}

cstring *JsonSerializeArray(const json_array *array)
{
	return Serialize(array, JSON_VALUE_ARRAY);
}

struct json {
	vstring vstr;
	size_t indent;
};

static cstring *Serialize(const void *data, const json_value_tag tag)
{
	assert(data);
	assert(tag == JSON_VALUE_OBJECT || tag == JSON_VALUE_ARRAY);

	json js = {
		.vstr = vStringInit(KiB(1)),
		.indent = 0
	};

	SerializeObject(&js, data);

	return cStringFromvString(&js.vstr);
}

static void SerializeObject(json *self, const json_object *object)
{
	assert(self);

	vStringAppend(&self->vstr, '{');
	self->indent++;

	map(JsonValue) map = object->values;

	for (size_t i = 0; i < map.cap; i++) {
		if (map.buffer[i].status != SLOT_CLOSED) {
			continue;
		}

		vStringAppendcString(&self->vstr, map.buffer[i].key);
		Dispatch(self, map.buffer[i].value);
	}

	vStringAppend(&self->vstr, '}');
	self->indent--;
}

static void Dispatch(json *self, const json_value value)
{
	assert(self);

	switch (value.tag) {
	case JSON_VALUE_OBJECT:
		SerializeObject(self, value.object);
		break;

	default:
		break;
	}
}
