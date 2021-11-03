// Copyright (C) 2021 Biren Patel. GNU General Public License v3.0.

#include <assert.h>
#include <errno.h>

#include "file.h"
#include "defs.h"
#include "xerror.h"

static void FileCleanup(FILE **);
static cstring *cStringFromFile(FILE *);
static size_t GetFileSize(FILE *);

//on failure returns NULL, else returns a dynamically allocated cstring. Any
//errors are reported to the xerror log.
cstring *FileLoad(const cstring *filename)
{
	assert(filename);

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
