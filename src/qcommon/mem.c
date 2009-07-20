/*
===========================================================================
Copyright (C) 2009 Amanieu d'Antras

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef DEBUG_MALLOC

/* This structure is prepended to every allocated block. There is also a
 * no-mans-land chunk, filled with a specific byte, at the end of every
 * allocated block as well. */
#define NO_MANS_LAND_BYTE 0x5a
#define NO_MANS_LAND_SIZE 64
typedef struct memTag_s {
        struct memTag_s *next;
        const char *alloc_file, *alloc_func, *free_file, *free_func;
        void *data;
        size_t size;
        int alloc_line, free_line;
		qboolean freed;
        char no_mans_land[NO_MANS_LAND_SIZE];
} memTag_t;

/* Root of the list of memory tags */
static memTag_t *mem_root;

/* Some statistics */
static size_t mem_bytes, mem_bytes_max;
static int mem_calls;

static memTag_t *FindTag(const void *ptr, memTag_t **prev_tag)
{
	memTag_t *tag, *prev;

	prev = NULL;
	tag = mem_root;
	while (tag && tag->data != ptr) {
		prev = tag;
		tag = tag->next;
	}

	if (prev_tag)
		*prev_tag = prev;

	return tag;
}

void *Mem_Alloc_Debug(const char *file, int line, const char *function,
                      size_t size)
{
	memTag_t *tag;
	size_t real_size;

	real_size = size + sizeof(memTag_t) + NO_MANS_LAND_SIZE;
	tag = malloc(real_size);
	if (!tag)
		Error("Out of memory (%s() at %s:%d tried to alloc %zd bytes)", function, file, line, size);
	tag->data = (char *)tag + sizeof(memTag_t);
	tag->size = size;
	tag->alloc_file = file;
	tag->alloc_line = line;
	tag->alloc_func = function;
	tag->freed = qtrue;
	memset(tag->no_mans_land, NO_MANS_LAND_BYTE, NO_MANS_LAND_SIZE);
	memset((char *)tag->data + size, NO_MANS_LAND_BYTE, NO_MANS_LAND_SIZE);
	tag->next = NULL;
	if (mem_root)
		tag->next = mem_root;
	mem_root = tag;
	mem_bytes += size;
	mem_calls++;
	if (mem_bytes > mem_bytes_max)
		mem_bytes_max = mem_bytes;
	return tag->data;
}

void *Mem_Calloc_Debug(const char *file, int line, const char *function,
                       size_t size)
{
	void *ptr = Mem_Alloc_Debug(file, line, function, size);

	memset(ptr, 0, size);

	return ptr;
}

void *Mem_Realloc_Debug(const char *file, int line, const char *function,
                        void *ptr, size_t size)
{
	memTag_t *tag, *prev_tag, *old_tag;
	size_t real_size;

	if (!ptr)
		return Mem_Alloc_Debug(file, line, function, size);

	if (!size) {
		Mem_Free_Debug(file, line, function, ptr);
		return NULL;
	}

	if (!(tag = FindTag(ptr, &prev_tag)))
		Error("%s() at %s:%d tried to reallocate unallocated address (%p)", function, file, line, ptr);

	old_tag = tag;
	real_size = size + sizeof(memTag_t) + NO_MANS_LAND_SIZE;
	tag = realloc((char *)ptr - sizeof(memTag_t), real_size);
	if (!tag)
		Error("Out of memory (%s() at %s:%d tried to alloc %zd bytes)", function, file, line, size);

	if (prev_tag)
		prev_tag->next = tag;
	if (old_tag == mem_root)
		mem_root = tag;
	mem_bytes += size - tag->size;
	if (size > tag->size) {
		mem_calls++;
		if (mem_bytes > mem_bytes_max)
			mem_bytes_max = mem_bytes;
	}
	tag->size = size;
	tag->alloc_file = file;
	tag->alloc_line = line;
	tag->alloc_func = function;
	tag->data = (char *)tag + sizeof(memTag_t);
	memset((char *)tag->data + size, NO_MANS_LAND_BYTE, NO_MANS_LAND_SIZE);

	return tag->data;
}

/* This function will check for 3 things:
 * If the memory was never allocated
 * If the memory was already freed
 * If the no-man-lang has been corrupted
 */
void Mem_Free_Debug(const char *file, int line, const char *function, void *ptr)
{
	memTag_t *tag, *prev_tag, *old_tag;
	int i;

	if (!ptr)
		return;

	if (!(tag = FindTag(ptr, &prev_tag)))
		Error("%s() at %s:%d tried to free unallocated address (%p)", function, file, line, ptr);

	if (tag->freed)
		Error("%s() at %s:%d tried to free address %p (%zd bytes allocated by %s() at %s:%d), which was already freed by %s() at %s:%d",
			  function, file, line, ptr, tag->size, tag->alloc_func, tag->alloc_file, tag->alloc_line, tag->free_func, tag->free_file, tag->free_line);

	for (i = 0; i < NO_MANS_LAND_SIZE; i++)
		if (tag->no_mans_land[i] != NO_MANS_LAND_BYTE)
			Error("Address (%p), %zd bytes allocated by %s() at %s:%d, freed by %s() at %s:%d, overran lower boundary",
			      ptr, tag->size, tag->alloc_func, tag->alloc_file, tag->alloc_line, function, file, line);

	for (i = 0; i < NO_MANS_LAND_SIZE; i++)
		if (((char *)ptr + tag->size)[i] != NO_MANS_LAND_BYTE)
			Error("Address (%p), %zd bytes allocated by %s() at %s:%d, freed by %s() at %s:%d, overran upper boundary",
			      ptr, tag->size, tag->alloc_func, tag->alloc_file, tag->alloc_line, function, file, line);

	tag->freed = qtrue;
	tag->free_file = file;
	tag->free_line = line;
	tag->free_func = function;
	old_tag = tag;

	/* Clear the memory to catch accesses to freed data */
	memset(tag->data, NO_MANS_LAND_BYTE, tag->size);
	tag = realloc(tag, sizeof(memTag_t));
	if (prev_tag)
		prev_tag->next = tag;
	if (old_tag == mem_root)
		mem_root = tag;
	mem_bytes -= tag->size;
}

void Mem_Check(void)
{
	memTag_t *tag;
	int tags = 0;

	for (tag = mem_root; tag; tag = tag->next) {
		unsigned int i;

		tags++;
		if (tag->freed)
			continue;

		Warning("%s() leaked %zd bytes in %s:%d", tag->alloc_func, tag->size, tag->alloc_file, tag->alloc_line);

		/* If we leaked a string, we can print it */
		if (tag->size < 1)
			continue;
		for (i = 0; isprint(((char *)tag->data)[i]); i++) {
			char buf[128];

			if (i >= tag->size - 1 || i >= sizeof(buf) - 1 ||
			    !((char *)tag->data)[i + 1]) {
				strcpy(buf, tag->data);
				Warning("Looks like a string: '%s'", buf);
				break;
			}
		}
	}

	Msg("%d allocation calls, high mark %.1fmb, %d tags", mem_calls, mem_bytes_max / 1048576.f, tags);
}

/* We store 2 copies of the string, to check if it was modified */
const char *Mem_CopyString_Debug(const char *file, int line, const char *function, const char *string)
{
	int len;
	char *buffer;

	len = strlen(string) + 1;
	buffer = Mem_Alloc_Debug(file, line, function, len * 2);
	memcpy(buffer, string, len);
	memcpy(buffer + len, string, len);

	return buffer;
}

/* Check if the string was modified */
void Mem_FreeString_Debug(const char *file, int line, const char *function, const char *string)
{
	memTag_t *tag;

	if (!(tag = FindTag(string, NULL)))
		Error("%s() at %s:%d tried to free unallocated address (%p)", function, file, line, string);

	if (tag->freed)
		Error("%s() at %s:%d tried to free address %p (%zd bytes allocated by %s() at %s:%d), which was already freed by %s() at %s:%d",
			  function, file, line, string, tag->size, tag->alloc_func, tag->alloc_file, tag->alloc_line, tag->free_func, tag->free_file, tag->free_line);

	if (memcmp(tag->data, tag->data + tag->size / 2, tag->size / 2)) {
		((char *)tag->data)[tag->size - 1] = '\0';
		((char *)tag->data)[tag->size / 2 - 1] = '\0';
		Error("String at address (%p), %zd bytes allocated by %s() at %s:%d, freed by %s() at %s:%d, was modified. Original string: \"%s\" New string: \"%s\"",
			      string, tag->size / 2, tag->alloc_func, tag->alloc_file, tag->alloc_line, function, file, line, (char *)tag->data + tag->size / 2, (char *)tag->data);
	}

	Mem_Free_Debug(file, line, function, (char *)string);
}

#else

static char nullstring[] = "";
static char numberstring[][2] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

const char *Mem_CopyString(const char *string)
{
	int len;
	char *buffer;

	if (!string[0])
		return nullstring;

	if (!string[1] && isdigit(string[0]))
		return numberstring[string[0] - '0'];

	len = strlen(string) + 1;
	buffer = Mem_Alloc(len);
	memcpy(buffer, string, len);
	return buffer;
}

void Mem_FreeString(const char *string)
{
	if (!string[0])
		return;

	if (!string[1] && isdigit(string[0]))
		return;

	Mem_Free((char *)string);
}

#endif
