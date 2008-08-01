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

#ifndef _SCRIPT_SC_SCRIPT_H_
#define _SCRIPT_SC_SCRIPT_H_

#define MAX_TAG_LENGTH          16
#define MAX_FUNCTION_ARGUMENTS  16
#define MAX_OBJECT_MEMBERS      32
#define MAX_NAMESPACE_DEPTH     8
#define MAX_NAMESPACE_LENGTH    16
#define MAX_PATH_LENGTH         64
#define MAX_OBJECT_DESCRIPTION  1024
#define MAX_MEMBER_DESCRIPTION  1024

#ifdef USE_PYTHON
#define _UNISTD_H 1 // Prevent syscall from being defined in unisd.h 
#include <Python.h>
#include "structmember.h"
#endif

#include "../game/g_local.h"

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
  TYPE_ANY=-1,
  TYPE_UNDEF,
  TYPE_BOOLEAN,
  TYPE_INTEGER,
  TYPE_FLOAT,
  TYPE_STRING,
  TYPE_FUNCTION,
  TYPE_ARRAY,
  TYPE_HASH,
  TYPE_NAMESPACE,
  TYPE_OBJECT,
  TYPE_CLASS,
  TYPE_USERDATA,
} scDataType_t;

typedef struct
{
  int count;
} scGC_t;

typedef char  scDataTypeBoolean_t;
typedef long  scDataTypeInteger_t;
typedef float scDataTypeFloat_t;
typedef void* scDataTypeUserdata_t;
typedef struct
{
  scGC_t  gc;
  int     length;
  int     buflen;
  char    *data;
} scDataTypeString_t;

typedef struct scDataTypeValue_s scDataTypeValue_t;
typedef struct scDataTypeArray_s scDataTypeArray_t;
typedef struct scDataTypeHash_s scDataTypeHash_t;
typedef struct scDataTypeHashEntry_s scDataTypeHashEntry_t;
typedef struct scDataTypeFunction_s scDataTypeFunction_t;
typedef struct scObject_s scObject_t;
typedef struct scClass_s scClass_t;
typedef scDataTypeHash_t scNamespace_t;

typedef void (*scCRef_t)(scDataTypeValue_t*, scDataTypeValue_t*);
typedef void (*scCMethodRef_t)(scObject_t *self, void *closure, scDataTypeValue_t*, scDataTypeValue_t*);

#ifdef USE_PYTHON
typedef PyObject *scPYFunc_t;
#endif

struct scDataTypeFunction_s
{
  scGC_t                gc;
  scLangage_t           langage;
  scDataType_t          argument[ MAX_FUNCTION_ARGUMENTS + 1 ];
  scDataType_t          return_type;
  union
  {
    char                path[ MAX_PATH_LENGTH + 1 ];
    scCRef_t            ref;
#ifdef USE_PYTHON
    scPYFunc_t          pyfunc;
#endif
  } data;
};

struct scDataTypeValue_s
{
  scDataType_t    type;
  scGC_t          gc;
  union
  {
    scDataTypeBoolean_t    boolean;
    scDataTypeInteger_t    integer;
    scDataTypeFloat_t      floating;
    scDataTypeString_t     *string;
    scDataTypeFunction_t   *function;
    scDataTypeArray_t      *array;
    scDataTypeHash_t       *hash;
    scNamespace_t          *namespace;
    scObject_t             *object;
    scClass_t              *class;
    scDataTypeUserdata_t   userdata;
  } data;
};

typedef scDataTypeFunction_t scDataTypeMethod_t;

typedef struct
{
  scDataType_t          type;
  char                  *desc;
  scDataTypeMethod_t    set;
  scDataTypeMethod_t    get;
  void                  *closure;
} scObjectMember_t;

typedef struct
{
  scDataType_t          type;
  char                  *desc;
  scDataTypeMethod_t    method;
  void                  *closure;
} scObjectMethod_t;

struct scClass_s
{
  char                  *name;
  char                  *desc;
  scObjectMethod_t      *constructor;
  scObjectMethod_t      *destructor;
  scObjectMember_t      *members;
  scObjectMethod_t      *methods;
};

struct scObject_s
{
  scGC_t                gc;
  scClass_t             *class;
  scDataTypeValue_t     data;
};

/*struct scDataTypeObjectType_s
{
  scDataTypeString_t typename;
};

struct scDataTypeObject_s
{
  scDataTypeObjectType_t  *type;
//  scDataTypeHash_t       *members;
//  scDataTypeHash_t        *values;
};*/

struct scDataTypeArray_s
{
  scGC_t                gc;
  int                   size;
  int                   buflen;
  scDataTypeValue_t     *data;
};

struct scDataTypeHash_s
{
  scGC_t                gc;
  int                   size;
  int                   buflen;
  struct scDataTypeHashEntry_s
  {
    scDataTypeString_t  key;
    scDataTypeValue_t   value;
  } *data;
};

// Events data structure

typedef struct scnode_s scnode_t;
typedef struct schook_s schook_t;

struct scnode_s
{
  char                  name[ MAX_TAG_LENGTH + 1 ];
  int                   leaf;

  scnode_t              *before;
  scnode_t              *in;
  scnode_t              *after;

  scDataTypeFunction_t  *hook;

  scnode_t              *next;
  //scnode_t              *parent;
};

extern scNamespace_t *namespace_root;

scDataTypeString_t *SC_StringNew(void);
scDataTypeString_t *SC_StringNewFromChar(const char* str);
void SC_Strcat( scDataTypeString_t *string, const char *src );
void SC_Strcpy( scDataTypeString_t *string, const char *src );
qboolean SC_StringIsEmpty(scDataTypeString_t *string);
const char* SC_StringToChar(scDataTypeString_t *string);
void SC_StringClear(scDataTypeString_t *string);
void SC_StringGCInc(scDataTypeString_t *string);
void SC_StringGCDec(scDataTypeString_t *string);

scDataTypeValue_t *SC_ValueNew( void );
void SC_ValueGCInc(scDataTypeValue_t *value);
void SC_ValueGCDec(scDataTypeValue_t *value);
scDataTypeValue_t *SC_ValueStringFromChar( const char* str );

scDataTypeArray_t *SC_ArrayNew( void );
qboolean SC_ArrayGet(scDataTypeArray_t *array, int index, scDataTypeValue_t *value);
void SC_ArraySet(scDataTypeArray_t *array, int index, scDataTypeValue_t *value);
qboolean SC_ArrayDelete( scDataTypeArray_t *array, int index );
void SC_ArrayClear(scDataTypeArray_t *array);
void SC_ArrayGCInc(scDataTypeArray_t *array);
void SC_ArrayGCDec(scDataTypeArray_t *array);

scDataTypeHash_t* SC_HashNew( void );
qboolean SC_HashGet(scDataTypeHash_t *hash, const char *key, scDataTypeValue_t *value);
qboolean SC_HashSet( scDataTypeHash_t *hash, const char *key, scDataTypeValue_t *value );
scDataTypeArray_t *SC_HashGetKeys(const scDataTypeHash_t *hash);
scDataTypeArray_t *SC_HashToArray(scDataTypeHash_t *hash);
qboolean SC_HashDelete(scDataTypeHash_t *hash, const char *key);
void SC_HashClear(scDataTypeHash_t *hash);
void SC_HashGCInc(scDataTypeHash_t *hash);
void SC_HashGCDec(scDataTypeHash_t *hash);

void SC_NamespaceInit( void );
qboolean SC_NamespaceGet( const char *path, scDataTypeValue_t *value );
qboolean SC_NamespaceSet( const char *path, scDataTypeValue_t *value );
qboolean SC_NamespaceDelete( const char *path );

scDataTypeFunction_t* SC_FunctionNew(void);
void SC_FunctionGCInc(scDataTypeFunction_t *function);
void SC_FunctionGCDec(scDataTypeFunction_t *function);

scObject_t *SC_ObjectNew(scClass_t *class);
void SC_ObjectGCInc(scObject_t *object);
void SC_ObjectGCDec(scObject_t *object);

char* SC_LangageToString(scLangage_t langage);
void SC_PrintData( void );

// sc_main.c

void SC_Init( void );
void SC_AutoLoad( void );
void SC_Shutdown( void );
void SC_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret );
int SC_RunScript( scLangage_t langage, const char *filename );
int SC_CallHooks( const char *path, gentity_t *entity );
scLangage_t SC_LangageFromFilename(const char* filename);
void SC_InitObject( scObject_t * type);

// sc_c.c

typedef struct
{
  char                  name[ MAX_PATH_LENGTH + 1];
  scCRef_t              ref;
  scDataType_t          argument[ MAX_FUNCTION_ARGUMENTS + 1 ];
  scDataType_t          return_type;
} scLib_t;

typedef struct
{
  char                  *name;
  char                  *desc;
  scDataType_t          type;
  scCMethodRef_t        set;
  scCMethodRef_t        get;
  void                  *closure;
} scLibObjectMember_t;

typedef struct
{
  char                  *name;
  char                  *desc;
  scCMethodRef_t        method;
  scDataType_t          argument[ MAX_FUNCTION_ARGUMENTS + 1 ];
  scDataType_t          return_type;
  void                  *closure;
} scLibObjectMethod_t;

typedef struct
{
  scCMethodRef_t        method;
  scDataType_t          argument[ MAX_FUNCTION_ARGUMENTS + 1 ];
} scLibObjectClassMethod_t;

typedef struct
{
  const char                *name;
  scLibObjectClassMethod_t  constructor;
  scLibObjectClassMethod_t  destructor;
  scLibObjectMember_t       *members;
  scLibObjectMethod_t       *methods;
} scLibObjectDef_t;

void SC_AddLibrary( const char *namespace, scLib_t lib[] );

scClass_t *SC_AddObjectType( const char *namespace, scLibObjectDef_t *def );

// sc_common.c
scObject_t *SC_Vec3FromVec3_t( float *vect );
void SC_Common_Init( void );

#endif

