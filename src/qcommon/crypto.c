/*
===========================================================================
Copyright (C) 2007-2008 Amanieu d'Antras (amanieu@gmail.com)

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// Public-key identification

#include "q_shared.h"
#include "qcommon.h"
#include "crypto.h"

void *gmp_alloc(size_t size)
{
	return Z_TagMalloc(size, TAG_CRYPTO);
}

void *gmp_realloc(void *ptr, size_t old_size, size_t new_size)
{
	if (old_size >= new_size)
		return ptr;
	void *new_ptr = Z_TagMalloc(new_size, TAG_CRYPTO);
	if (ptr)
	{
		Com_Memcpy(new_ptr, ptr, old_size);
		Z_Free(ptr);
	}
	return new_ptr;
}

void gmp_free(void *ptr, size_t size)
{
	Z_Free(ptr);
}

void *nettle_realloc(void *ctx, void *ptr, unsigned size)
{
	void *new_ptr = gmp_realloc(ptr, *(int *)ctx, size);
	// new size will always be > old size
	*(int *)ctx = size;
	return new_ptr;
}

void qnettle_random(void *ctx, unsigned length, uint8_t *dst)
{
	Com_RandomBytes(dst, length);
}

void qnettle_buffer_init(struct nettle_buffer *buffer, int *size)
{
	qnettle_buffer_init_realloc(buffer, size, nettle_realloc);
}

#ifdef USE_NETTLE_DLOPEN
#include "../sys/sys_loadlib.h"

void (*qrsa_public_key_init)(struct rsa_public_key *);
void (*qrsa_public_key_clear)(struct rsa_public_key *);
int (*qrsa_public_key_prepare)(struct rsa_public_key *);
void (*qrsa_private_key_init)(struct rsa_private_key *);
int (*qrsa_encrypt)(const struct rsa_public_key *, void *, nettle_random_func, unsigned, const uint8_t *, mpz_t);
int (*qrsa_decrypt)(const struct rsa_private_key *, unsigned *, uint8_t *, const mpz_t);
int (*qrsa_generate_keypair)(struct rsa_public_key *, struct rsa_private_key *, void *, nettle_random_func, void *, nettle_progress_func, unsigned, unsigned);
int (*qrsa_keypair_from_sexp)(struct rsa_public_key *,struct rsa_private_key *, unsigned, unsigned, const uint8_t *);
int (*qrsa_keypair_to_sexp)(struct nettle_buffer *, const char *, struct rsa_public_key *,struct rsa_private_key *);
void (*qnettle_buffer_init_realloc)(struct nettle_buffer *, void *, nettle_realloc_func);
void (*qnettle_buffer_clear)(struct nettle_buffer *);
void (*qnettle_mpz_set_str_256_u)(mpz_t, unsigned, const uint8_t *);
void (*qnettle_mpz_init_set_str_256_u)(mpz_t, unsigned, const uint8_t *);

static void *nettlelib;
static qboolean init_failed;

#ifdef USE_GMP_DLOPEN
void (*qmp_set_memory_functions)(void *(*)(size_t), void *(*)(void *, size_t, size_t), void (*)(void *, size_t));
void (*qmpz_init)(mpz_t);
int (*qmpz_init_set_str)(mpz_t, char *, int);
char *(*qmpz_set_str)(mpz_t, char *, int);
void (*qmpz_set_ui)(mpz_t, unsigned long int);
void (*qmpz_clear)(mpz_t);
char *(*qmpz_get_str)(char *, int, mpz_t);

static void *gmplib;
#endif /* USE_GMP_DLOPEN */

/*
=================
GPA
=================
*/
static void *GPA(char *str, void *lib)
{
	void *rv;

	rv = Sys_LoadFunction(lib, str);
	if(!rv)
	{
		Com_Printf("Can't load symbol %s\n", str);
		init_failed = qtrue;
		return NULL;
	}
	else
	{
		Com_DPrintf("Loaded symbol %s (0x%p)\n", str, rv);
		return rv;
	}
}

static qboolean loadLib(void **lib, const char *libname)
{
	if (*lib)
		return qtrue;

	Com_Printf("Loading \"%s\"...\n", libname);
	if( (*lib = Sys_LoadLibrary(libname)) == 0 )
	{
#ifdef _WIN32
		return qfalse;
#else
		char fn[1024];
		Q_strncpyz( fn, Sys_Cwd( ), sizeof( fn ) );
		strncat(fn, "/", sizeof(fn)-strlen(fn)-1);
		strncat(fn, libname, sizeof(fn)-strlen(fn)-1);

		if( (*lib = Sys_LoadLibrary(fn)) == 0 )
			return qfalse;
#endif /* _WIN32 */
	}

	return qtrue;
}

qboolean CRYPTO_Init(void)
{
#ifdef USE_GMP_DLOPEN
	if (!loadLib( &gmplib, com_gmpLibName->string))
		return qfalse;
#endif /* USE_GMP_DLOPEN */

	if (!loadLib( &nettlelib, com_nettleLibName->string))
		return qfalse;

	init_failed = qfalse;

	qrsa_public_key_init = GPA("nettle_rsa_public_key_init", nettlelib);
	qrsa_public_key_clear = GPA("nettle_rsa_public_key_clear", nettlelib);
	qrsa_public_key_prepare = GPA("nettle_rsa_public_key_prepare", nettlelib);
	qrsa_private_key_init = GPA("nettle_rsa_private_key_init", nettlelib);
	qrsa_encrypt = GPA("nettle_rsa_encrypt", nettlelib);
	qrsa_decrypt = GPA("nettle_rsa_decrypt", nettlelib);
	qrsa_generate_keypair = GPA("nettle_rsa_generate_keypair", nettlelib);
	qrsa_keypair_from_sexp = GPA("nettle_rsa_keypair_from_sexp", nettlelib);
	qrsa_keypair_to_sexp = GPA("nettle_rsa_keypair_to_sexp", nettlelib);
	qnettle_buffer_init_realloc = GPA("nettle_buffer_init_realloc", nettlelib);
	qnettle_buffer_clear = GPA("nettle_buffer_clear", nettlelib);
	qnettle_mpz_set_str_256_u = GPA("qnettle_mpz_set_str_256_u", nettlelib);
	qnettle_mpz_init_set_str_256_u = GPA("qnettle_mpz_init_set_str_256_u", nettlelib);

#ifdef USE_GMP_DLOPEN
	qmp_set_memory_functions = GPA("__gmp_set_memory_functions", gmplib);
	qmpz_init = GPA("__gmpz_init", gmplib);
	qmpz_init_set_str = GPA("__gmpz_init_set_str", gmplib);
	qmpz_set_str = GPA("__gmpz_set_str", gmplib);
	qmpz_set_ui = GPA("__gmpz_set_ui", gmplib);
	qmpz_clear = GPA("__gmpz_clear", gmplib);
	qmpz_get_str = GPA("__gmpz_get_str", gmplib);
#endif /* USE_GMP_DLOPEN */

	if(init_failed)
	{
		CRYPTO_Shutdown();
		Com_Printf("FAIL One or more symbols not found\n");
		return qfalse;
	}
	qmp_set_memory_functions(gmp_alloc, gmp_realloc, gmp_free);
	Com_Printf("OK\n");

	return qtrue;
}

void CRYPTO_Shutdown(void)
{
	if (nettlelib)
	{
		Sys_UnloadLibrary(nettlelib);
		nettlelib = NULL;
	}

	qrsa_public_key_init = NULL;
	qrsa_public_key_clear = NULL;
	qrsa_public_key_prepare = NULL;
	qrsa_private_key_init = NULL;
	qrsa_encrypt = NULL;
	qrsa_decrypt = NULL;
	qrsa_generate_keypair = NULL;
	qrsa_keypair_from_sexp = NULL;
	qrsa_keypair_to_sexp = NULL;
	qnettle_buffer_init_realloc = NULL;
	qnettle_buffer_clear = NULL;
	qnettle_mpz_set_str_256_u = NULL;
	qnettle_mpz_init_set_str_256_u = NULL;

#ifdef USE_GMP_DLOPEN
	if (gmplib)
	{
		Sys_UnloadLibrary(gmplib);
		gmplib = NULL;
	}

	qmp_set_memory_functions = NULL;
	qmpz_init = NULL;
	qmpz_init_set_str = NULL;
	qmpz_set_str = NULL;
	qmpz_set_ui = NULL;
	qmpz_clear = NULL;
	qmpz_get_str = NULL;
#endif /* USE_GMP_DLOPEN */

	Z_FreeTags(TAG_CRYPTO);
}

#else /* USE_NETTLE_DLOPEN */

qboolean CRYPTO_Init(void)
{
	qmp_set_memory_functions(gmp_alloc, gmp_realloc, gmp_free);
	return qtrue;
}

void CRYPTO_Shutdown(void)
{
	Z_FreeTags(TAG_CRYPTO);
}

#endif /* USE_NETTLE_DLOPEN */
