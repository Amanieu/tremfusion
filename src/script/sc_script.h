/*
===========================================================================
Copyright (C) 2008 Maurice Doison

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

#include "qcommon/q_shared.h"

// TODO: remove me !
#define Q_strncpyz( DEST, SRC, LEN ) strncpy( DEST, SRC, LEN )

#define MAX_TAG_LENGTH          16
#define MAX_FUNCTION_ARGUMENTS  16
#define MAX_NAMESPACE_DEPTH     8

// Langages

typedef enum
{
  LANGAGE_INVALID,
  LANGAGE_C,
  LANGAGE_LUA,
  LANGAGE_PYTHON
} scLangage_t;

// Data Types

typedef enum
{
  TYPE_UNDEF,
  TYPE_INTEGER,
  TYPE_FLOAT,
  TYPE_STRING,
  TYPE_SCALAR,
  TYPE_ARRAY,
  TYPE_HASH
} scDataType_t;

typedef long  scDataTypeInteger_t;
typedef float scDataTypeFloat_t;
typedef struct
{
  int   length;
  int   buflen;
  char  data;
  // following with datas...
} scDataTypeString_t;

typedef struct scDataTypeValue_s scDataTypeValue_t;
typedef struct scDataTypeArray_s scDataTypeArray_t;
typedef struct scDataTypeHash_s scDataTypeHash_t;
typedef struct scDataTypeHashEntry_s scDataTypeHashEntry_t;
typedef struct scDataTypeFunction_s scDataTypeFunction_t;
typedef struct scDataTypeNamespace_s scDataTypeNamespace_t;

struct scDataTypeValue_s
{
  scDataType_t    type;
  union
  {
    scDataTypeInteger_t   integer;
    scDataTypeFloat_t     floating;
    scDataTypeString_t    *string;
    scDataTypeArray_t     *array;
    scDataTypeHash_t      *hash;
  } data;
};

struct scDataTypeArray_s
{
  int                   size;
  int                   buflen;
  scDataTypeValue_t     data;
  // following with datas...
};

struct scDataTypeHash_s
{
  int                   size;
  int                   buflen;
  // TODO: a hash table should be a tree
  struct scDataTypeHashEntry_s
  {
    scDataTypeString_t  *key;
    scDataTypeValue_t   value;
  } data;
  // following with datas...
};

struct scDataTypeNamespace_s
{
  scDataTypeString_t    name[MAX_NAMESPACE_DEPTH];
};

struct scFunction_s
{
  scLangage_t           langage;
  scDataTypeNamespace_t namespace;
  scDataTypeString_t    name;
  scDataType_t          argument[MAX_FUNCTION_ARGUMENTS];
  void                  *ref;
};

// Events data structure

typedef struct scnode_s scnode_t;
typedef struct schook_s schook_t;

struct scnode_s
{
  char                  name[MAX_TAG_LENGTH+1];
  int                   leaf;

  scnode_t              *before;
  scnode_t              *in;
  scnode_t              *after;

  scDataTypeFunction_t  *hook;

  scnode_t              *next;
  //scnode_t              *parent;
};

scDataTypeString_t *SC_NewString( );
scDataTypeString_t *SC_NewStringFromChar( const char* str );
scDataTypeString_t *SC_Strcat( scDataTypeString_t *string, const scDataTypeString_t *src );
scDataTypeString_t *SC_Strcpy( scDataTypeString_t *string, const scDataTypeString_t *src );
void SC_StringFree( scDataTypeString_t *string );

qboolean SC_ValueIsScalar( const scDataTypeValue_t *value );
void SC_ValueFree( scDataTypeValue_t *value );

scDataTypeArray_t *SC_NewArray( );
qboolean SC_ArrayGet( const scDataTypeArray_t *array, int index, scDataTypeValue_t *value );
scDataTypeArray_t *SC_ArraySet( scDataTypeArray_t *array, int index, scDataTypeValue_t *value );
scDataTypeArray_t *SC_ArrayDelete( scDataTypeArray_t *array, int index );
scDataTypeArray_t *SC_ArrayClear( scDataTypeArray_t *array );
void SC_ArrayFree( scDataTypeArray_t *array );

scDataTypeHash_t *SC_NewHash( );
qboolean SC_HashGet( const scDataTypeHash_t *hash, const scDataTypeString_t *key, scDataTypeValue_t *value );
scDataTypeHash_t *SC_HashSet( scDataTypeHash_t *hash, const scDataTypeString_t *key, scDataTypeValue_t *value );
scDataTypeArray_t *SC_HashGetKeys( scDataTypeHash_t *hash );
scDataTypeHash_t *SC_HashDelete( scDataTypeHash_t *hash, const scDataTypeString_t *key );
scDataTypeHash_t *SC_HashClear( scDataTypeHash_t *hash );
void SC_HashFree( scDataTypeHash_t *hash );
