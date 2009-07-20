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

#ifndef __HASH_H
#define __HASH_H

// Simple string hash function by Dan Bernstein
static inline __pure unsigned int Hash_String(const char *str)
{
	unsigned int hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) ^ (unsigned int)c;

	return hash;
}

// MD5 secure hash algorithm (not that secure, but hey, this is just a game)
// The salt is optional, but heavily recommended for extra security
// out should point to a 33 byte char array to contain a hex string
void Hash_MD5(const void *salt, size_t salt_len, const void *buffer, size_t len, char *out);

#endif
