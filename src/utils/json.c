// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <stdlib.h>

#include "arena.h"
#include "json.h"

static void AddIndentation(json *);

//@vstr: contains the serialized output
//@indent: tracks nesting depth 
struct json {
	vstring vstr;
	size_t depth;
};

//------------------------------------------------------------------------------

json *JsonInit(void)
{
	json *new = allocate(sizeof(json));
	new->vstr = vStringInit(KiB(1));
	new->depth = 0;

	return new;
}

cstring *JsonToString(json *self)
{
	assert(self);

	cstring *buffer = cStringFromvString(&self->vstr);

	return cStringDuplicate(buffer);
}

static void AddIndentation(json *self)
{
	assert(self);

	for (size_t i = 0; i < self->depth; i++) {
		vStringAppend(&self->vstr, '\t');
	}
}

//------------------------------------------------------------------------------

void JsonOpenObject(json *self)
{
	assert(self);

	const cstring *open = "{\n";

	vStringAppend(&self->vstr, '\n');
	AddIndentation(self);
	vStringAppendcString(&self->vstr, open);

	self->depth++;
}

void JsonCloseObject(json *self)
{
	assert(self);

	const cstring *close = "}\n";

	self->depth--;

	vStringAppend(&self->vstr, '\n');
	AddIndentation(self);
	vStringAppendcString(&self->vstr, close);
}
