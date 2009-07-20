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

#ifndef __MEM_H
#define __MEM_H

/* Memory management
 * We use malloc & co by default, but when we're debugging we want to know if
 * we abuse our buffers in any way. */

#define DEBUG_MALLOC

#ifndef DEBUG_MALLOC

static inline __restrict void *Mem_Alloc(size_t size)
{
	void *ptr = malloc(size);

	if (!ptr)
		Error("Out of memory (Tried to alloc %zd bytes)", size);

	return ptr;
}

static inline __restrict void *Mem_Calloc(size_t size)
{
	void *ptr = calloc(1, size);

	if (!ptr)
		Error("Out of memory (Tried to alloc %zd bytes)", size);

	return ptr;
}

static inline void *Mem_Realloc(void *ptr, size_t size)
{
	void *newptr = realloc(ptr, size);

	if (!newptr && size)
		Error("Out of memory (Tried to alloc %zd bytes)", size);

	return newptr;
}

static inline void Mem_Free(void *ptr)
{
	free(ptr);
}

#else

#define Mem_Alloc(s) Mem_Alloc_Debug(__FILE__, __LINE__, __func__, s)
void *Mem_Alloc_Debug(const char *file, int line, const char *function, size_t size) __restrict;

#define Mem_Calloc(s) Mem_Calloc_Debug(__FILE__, __LINE__, __func__, s)
void *Mem_Calloc_Debug(const char *file, int line, const char *function, size_t size) __restrict;

#define Mem_Realloc(p, s) Mem_Realloc_Debug(__FILE__, __LINE__, __func__, p, s)
void *Mem_Realloc_Debug(const char *file, int line, const char *function, void *ptr, size_t size);

#define Mem_Free(p) Mem_Free_Debug(__FILE__, __LINE__, __func__, p)
void Mem_Free_Debug(const char *file, int line, const char *function, void *ptr);

void Mem_Check(void);

#endif

/* Special handler to allocate space for a copy of a string.
 * The returned memory should not be written to, since it may return a statically
 * allocated buffer for some common strings. */
#ifndef DEBUG_MALLOC
const char *Mem_CopyString(const char *string);
void Mem_FreeString(const char *string);
#else
#define Mem_CopyString(s) Mem_CopyString_Debug(__FILE__, __LINE__, __func__, s)
const char *Mem_CopyString_Debug(const char *file, int line, const char *function, const char *string);
#define Mem_FreeString(s) Mem_FreeString_Debug(__FILE__, __LINE__, __func__, s)
void Mem_FreeString_Debug(const char *file, int line, const char *function, const char *string);
#endif

/* Stack-based memory allocation, automatically freed on function return
 * This method has a lot less overhead than standard memory allocation, but
 * is unable to allocate large amounts of memory. */
#ifdef _WIN32
#define Mem_Stack_Alloc(s) _alloca(s)
#else
#define Mem_Stack_Alloc(s) alloca(s)
#endif

#endif
