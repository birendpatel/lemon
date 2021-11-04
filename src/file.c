// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "file.h"
#include "defs.h"
#include "xerror.h"

static void FileCleanup(FILE **);
static cstring *cStringFromFile(FILE *);
static size_t GetFileSize(FILE *);
static cstring *GetFileName(const cstring *);
static bool HasExtension(const cstring *);
static bool Match(const cstring *, const cstring *);

//on failure returns NULL, else returns a dynamically allocated cstring. Any
//errors are reported to the xerror log.
cstring *FileLoad(const cstring *name)
{
	assert(name);

	RAII(cStringFree) cstring *filename = GetFileName(name);

	RAII(FileCleanup) FILE *fp = fopen(filename, "r");

	if (!fp) {
		xerror_issue("%s: %s", filename, strerror(errno));
		return NULL;
	}

	cstring *src = cStringFromFile(fp);

	if (!src) {
		const cstring *msg = "%s: cannot copy file to memory";
		xerror_issue(msg, filename);
	}

	return src;
}

//determines the filename as it exists on disk. Both imports and the compiler
//arguments to main can accept names with or without the extension. The return
//string is dynamically allocated.
static cstring *GetFileName(const cstring *name)
{
	if (HasExtension(name)) {
		return cStringDuplicate(name);
	}

	vstring vstr = vStringInit(strlen(name));

	for (const char *ch = name; *ch; ch++) {
		vStringAppend(&vstr, *ch);
	}

	const cstring *extension = ".lem";

	for (const char *ch = extension; *ch; ch++) {
		vStringAppend(&vstr, *ch);
	}

	return cStringFromvString(&vstr);
}

//returns true if name ends with ".lem"
static bool HasExtension(const cstring *name)
{
	const size_t len = strlen(name);

	if (len >= 4) {
		const char *last_four_chars = strrchr(name, '\0') - 4;
		const char *extension = ".lem";

		if (Match(last_four_chars, extension)) {
			return true;
		}
	}

	return false;
}

//returns true if a is identical to b
static bool Match(const cstring *a, const cstring *b)
{
	assert(a);
	assert(b);

	return !strcmp(a, b);
}

//for use with gcc cleanup
static void FileCleanup(FILE **handle)
{
	if (*handle) {
		fclose(*handle);
	}
}

//on failure returns NULL, else returns a dynamically allocated cstring
static cstring *cStringFromFile(FILE *openfile)
{
	assert(openfile);

	size_t filesize = GetFileSize(openfile);

	if (!filesize) {
		xerror_issue("cannot calculate file size");
		return NULL;
	}

	const size_t buflen = sizeof(char) * filesize + 1;
	cstring *buffer = AbortMalloc(buflen);

	size_t total_read = fread(buffer, sizeof(char), filesize, openfile);

	if (total_read != filesize) {
		xerror_issue("%s", strerror(errno));
		return NULL;
	}

	buffer[filesize] = '\0';
	return buffer;
}

//on failure returns zero, the openfile position will be set to zero on success
static size_t GetFileSize(FILE *openfile)
{
	assert(openfile);

	rewind(openfile);

	long err = fseek(openfile, 0L, SEEK_END);

	if (err == -1) {
		xerror_issue("fseek: cannot move to EOF: %s", strerror(errno));
		return 0;
	}

	long bytecount = ftell(openfile);

	if (bytecount == -1) {
		xerror_issue("ftell: cannot fetch count: %s", strerror(errno));
		return 0;
	}

	rewind(openfile);

	return (size_t) bytecount;
}
