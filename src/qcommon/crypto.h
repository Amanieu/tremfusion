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

#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include "q_shared.h"
#include "qcommon.h"

// dlopen has only been tested on Linux, don't use it on other platforms
#ifdef WIN32
#define DEFAULT_GMP_LIB "libgmp-3.dll"
#define DEFAULT_NETTLE_LIB "libnettle-2.dll"
#elif defined(MACOS_X)
#define DEFAULT_GMP_LIB "libgmp.dylib"
#define DEFAULT_NETTLE_LIB "libnettle.dylib"
#else
#define DEFAULT_GMP_LIB "libgmp.so.3"
#define DEFAULT_NETTLE_LIB "libnettle.so.2"
#endif

#define PKEY_FILE "pkey"

/* This should not be changed because this value is
 * expected to be the same on the client and on the server */
#define RSA_PUBLIC_EXPONENT 65537

// FIXME: add support for USE_LOCAL_HEADERS
#include <gmp.h>
#include <nettle/bignum.h>
#include <nettle/rsa.h>
#include <nettle/buffer.h>

#ifdef USE_GMP_DLOPEN
extern void (*qmp_set_memory_functions)(void *(*)(size_t), void *(*)(void *, size_t, size_t), void (*)(void *, size_t));
extern void (*qmpz_init)(mpz_t);
extern int (*qmpz_init_set_str)(mpz_t, char *, int);
extern char *(*qmpz_set_str)(mpz_t, char *, int);
extern void (*qmpz_set_ui)(mpz_t, unsigned long int);
extern void (*qmpz_clear)(mpz_t);
extern char *(*qmpz_get_str)(char *, int, mpz_t);
#else /* USE_GMP_DLOPEN */
#define qmp_set_memory_functions mp_set_memory_functions
#define qmpz_init mpz_init
#define qmpz_init_set_str mpz_init_set_str
#define qmpz_set_str mpz_set_str
#define qmpz_set_ui mpz_set_ui
#define qmpz_clear mpz_clear
#define qmpz_get_str mpz_get_str
#endif /* USE_GMP_DLOPEN */

#ifdef USE_NETTLE_DLOPEN
extern void (*qrsa_public_key_init)(struct rsa_public_key *);
extern void (*qrsa_public_key_clear)(struct rsa_public_key *);
extern int (*qrsa_public_key_prepare)(struct rsa_public_key *);
extern void (*qrsa_private_key_init)(struct rsa_private_key *);
extern int (*qrsa_encrypt)(const struct rsa_public_key *, void *, nettle_random_func, unsigned, const uint8_t *, mpz_t);
extern int (*qrsa_decrypt)(const struct rsa_private_key *, unsigned *, uint8_t *, const mpz_t);
extern int (*qrsa_generate_keypair)(struct rsa_public_key *, struct rsa_private_key *, void *, nettle_random_func, void *, nettle_progress_func, unsigned, unsigned);
extern int (*qrsa_keypair_from_sexp)(struct rsa_public_key *,struct rsa_private_key *, unsigned, unsigned, const uint8_t *);
extern int (*qrsa_keypair_to_sexp)(struct nettle_buffer *, const char *, struct rsa_public_key *,struct rsa_private_key *);
extern void (*qnettle_buffer_init_realloc)(struct nettle_buffer *, void *, nettle_realloc_func);
extern void (*qnettle_buffer_clear)(struct nettle_buffer *);
extern void (*qnettle_mpz_set_str_256_u)(mpz_t, unsigned, const uint8_t *);
extern void (*qnettle_mpz_init_set_str_256_u)(mpz_t, unsigned, const uint8_t *);
#else /* USE_NETTLE_DLOPEN */
#define qrsa_public_key_init nettle_rsa_public_key_init
#define qrsa_public_key_clear nettle_rsa_public_key_clear
#define qrsa_public_key_prepare nettle_rsa_public_key_prepare
#define qrsa_private_key_init nettle_rsa_private_key_init
#define qrsa_encrypt nettle_rsa_encrypt
#define qrsa_decrypt nettle_rsa_decrypt
#define qrsa_generate_keypair nettle_rsa_generate_keypair
#define qrsa_keypair_from_sexp nettle_rsa_keypair_from_sexp
#define qrsa_keypair_to_sexp nettle_rsa_keypair_to_sexp
#define qnettle_buffer_init_realloc nettle_buffer_init_realloc
#define qnettle_buffer_clear nettle_buffer_clear
#define qnettle_mpz_set_str_256_u nettle_mpz_set_str_256_u
#define qnettle_mpz_init_set_str_256_u nettle_mpz_init_set_str_256_u
#endif /* USE_NETTLE_DLOPEN */

qboolean CRYPTO_Init(void);
void CRYPTO_Shutdown(void);
// The size is stored in the second arg, and will change as the buffer grows
void qnettle_buffer_init(struct nettle_buffer *buffer, int *size);
// Random function used for key generation and encryption
void qnettle_random(void *ctx, unsigned length, uint8_t *dst);

#endif /* __CRYPTO_H__ */
