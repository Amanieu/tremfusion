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

#ifndef _SCRIPT_SC_PUBLIC_H_
#define _SCRIPT_SC_PUBLIC_H_

#include "../qcommon/q_shared.h"

#define MAX_TAG_LENGTH          16
#define MAX_FUNCTION_ARGUMENTS  16
#define MAX_OBJECT_MEMBERS      32
#define MAX_NAMESPACE_DEPTH     8
#define MAX_NAMESPACE_LENGTH    16
#define MAX_PATH_LENGTH         64
#define MAX_DESC_LENGTH         1024

#define offsetof(st, m)   ( (char *)&((st *)(0))->m)

/* SC_GC_DEBUG controls debug infomation about reference counting
 * 1 = Printouts of object creation and deletion
 * 2 = Printouts of actual reference increases and decreases
 */
#define SC_GC_DEBUG 0

#ifdef USE_LUA
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#ifdef USE_PYTHON
#define _UNISTD_H 1 // Prevent syscall from being defined in unisd.h 
#include <Python.h>
#include "structmember.h"
#endif

#ifdef GAME
#include "../game/g_local.h"
#endif
#ifdef UI
#include "../ui/ui_local.h"
#endif

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

typedef int (*scCRef_t)(scDataTypeValue_t*, scDataTypeValue_t*, void*);

#ifdef USE_LUA
typedef unsigned int scLuaFunc_t;
#endif

#ifdef USE_PYTHON
typedef PyObject *scPYFunc_t;
#endif

struct scDataTypeFunction_s
{
  scGC_t                gc;
  scLangage_t           langage;
  scDataType_t          argument[ MAX_FUNCTION_ARGUMENTS + 1 ];
  scDataType_t          return_type;
  void*                 closure;
  union
  {
    scCRef_t            ref;
#ifdef USE_LUA
    scLuaFunc_t         luafunc;
#endif
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
    void                   *any;
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
  char                  name[MAX_PATH_LENGTH+1];
  char                  desc[MAX_DESC_LENGTH+1];
  scDataType_t          type;
  scDataTypeMethod_t    set;
  scDataTypeMethod_t    get;
  void                  *closure;
} scObjectMember_t;

typedef struct
{
  char                  name[MAX_PATH_LENGTH+1];
  char                  desc[MAX_DESC_LENGTH+1];
  scDataTypeMethod_t    method;
} scObjectMethod_t;

typedef struct
{
  char         name[MAX_PATH_LENGTH+1];
  char         desc[MAX_DESC_LENGTH+1];
  scDataType_t type;
  int          ofs;
} scField_t;

struct scClass_s
{
  scGC_t                gc;
  char                  name[MAX_PATH_LENGTH+1];
  char                  desc[MAX_DESC_LENGTH+1];
  scDataTypeMethod_t    constructor;
  scDataTypeMethod_t    destructor;
  scObjectMember_t      *members;
  int                   memcount;
  scObjectMethod_t      *methods;
  int                   methcount;
  scField_t             *fields;
  int                   fieldcount;
#ifdef USE_PYTHON
  PyObject              *python_type;
#endif
};

struct scObject_s
{
  scGC_t                gc;
  scClass_t             *class;
  scDataTypeValue_t     data;
};

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

// sc_datatype.c

scDataTypeString_t *SC_StringNew(void);
scDataTypeString_t *SC_StringNewFromChar(const char* str);
scDataTypeString_t *SC_StringNewFromCharAndSize(const char* str, int len);
void SC_Strcat( scDataTypeString_t *string, const char *src );
void SC_StrcatWithLen( scDataTypeString_t *string, const char *src, int len );
void SC_Strcpy( scDataTypeString_t *string, const char *src );
qboolean SC_StringIsEmpty(scDataTypeString_t *string);
const char* SC_StringToChar(scDataTypeString_t *string);
void SC_StringClear(scDataTypeString_t *string);
void SC_StringGCInc(scDataTypeString_t *string);
void SC_StringGCDec(scDataTypeString_t *string);

void SC_ValueGCInc(scDataTypeValue_t *value);
void SC_ValueGCDec(scDataTypeValue_t *value);

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
const char *SC_HashFirst(scDataTypeHash_t *hash, scDataTypeValue_t *value);
const char *SC_HashNext(const scDataTypeHash_t *hash, const char *key, scDataTypeValue_t *value);
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
void SC_ObjectDestroy( scObject_t *object );

scClass_t *SC_ClassNew(const char *name);
scObjectMethod_t *SC_ClassGetMethod(scClass_t *class, const char *name);
scObjectMember_t *SC_ClassGetMember(scClass_t *class, const char *name);
scField_t *SC_ClassGetField(scClass_t *class, const char *name);
void SC_ClassGCInc(scClass_t *class);
void SC_ClassGCDec(scClass_t *class);

const char* SC_LangageToString(scLangage_t langage);
const char* SC_DataTypeToString(scDataType_t type);
void SC_DataDump( void );
void SC_HashDump(scDataTypeHash_t *hash);
void SC_ArrayDump(scDataTypeArray_t *array);
void SC_ValueDump(scDataTypeValue_t *value);

// sc_main.c

#define SC_EXEC_ERROR(STRING) { out->type = TYPE_STRING; \
                        out->data.string = SC_StringNewFromChar(STRING); \
                        return -1; }

void SC_Init( void );
void SC_LoadLangages( void );
void SC_Shutdown( void );
int  SC_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret);
int SC_RunScript( scLangage_t langage, const char *filename );
scLangage_t SC_LangageFromFilename(const char* filename);
void SC_InitClass( scClass_t *class );

// sc_c.c

typedef struct
{
  char                  name[MAX_PATH_LENGTH+1];
  char                  desc[MAX_DESC_LENGTH+1];
  scCRef_t              ref;
  scDataType_t          argument[MAX_FUNCTION_ARGUMENTS+1];
  scDataType_t          return_type;
  void                  *closure;
} scLibFunction_t;

typedef struct
{
  scCRef_t              ref;
  scDataType_t          argument[MAX_FUNCTION_ARGUMENTS+1];
  scDataType_t          return_type;
} scLibAnonymousFunction_t;

typedef struct
{
  char                  name[ MAX_PATH_LENGTH+1];
  char                  desc[ MAX_DESC_LENGTH+1];
  scCRef_t              ref;
  scDataType_t          argument[MAX_FUNCTION_ARGUMENTS+1];
  scDataType_t          return_type;
  void                  *closure;
} scLibVariable_t;

typedef struct
{
  char                  name[MAX_PATH_LENGTH+1];
  char                  desc[MAX_DESC_LENGTH+1];
  scDataType_t          type;
  scCRef_t              set;
  scCRef_t              get;
  void                  *closure;
} scLibObjectMember_t;

typedef struct
{
  char                  name[MAX_PATH_LENGTH+1];
  char                  desc[MAX_DESC_LENGTH+1];
  scCRef_t              method;
  scDataType_t          argument[ MAX_FUNCTION_ARGUMENTS + 1 ];
  scDataType_t          return_type;
  void                  *closure;
} scLibObjectMethod_t;

typedef struct
{
  char                      name[MAX_PATH_LENGTH+1];
  char                      desc[MAX_DESC_LENGTH+1];
  scCRef_t                  constructor;
  scDataType_t              constructor_arguments[MAX_FUNCTION_ARGUMENTS];
  scCRef_t                  destructor;
  scLibObjectMember_t       *members;
  scLibObjectMethod_t       *methods;
  scField_t                 *fields;
  void                      *closure;
} scLibObjectDef_t;

void SC_AddLibrary( const char *namespace, scLibFunction_t lib[] );

scClass_t *SC_AddClass( const char *namespace, scLibObjectDef_t *def );

int SC_Field_Set( scObject_t *object, scField_t *field, scDataTypeValue_t *value);
int SC_Field_Get( scObject_t *object, scField_t *field, scDataTypeValue_t *value);

int SC_BuildValue(scDataTypeValue_t *val, const char *format, ...);

// sc_common.c

extern scClass_t *vec3_class;
extern scClass_t *module_class;

void SC_Common_Constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure);

scObject_t *SC_Vec3FromVec3_t( float *vect );
scObject_t *SC_Vec4FromVec4_t( float *vect );
vec4_t     *SC_Vec4t_from_Vec4( scObject_t *vectobject );

// modules
int SC_Module_Init(scObject_t *self);
int SC_Module_Register(scObject_t *self, scObject_t *toregister);
int SC_Module_Load(scObject_t *self, scDataTypeValue_t *out);
int SC_Module_Unload(scObject_t *self, scDataTypeValue_t *out, int force);


// sc_event.c

#define MAX_TAG_SIZE 32
typedef struct scEventNode_s scEventNode_t;
typedef struct scHook_s scHook_t;
typedef struct scEvent_s scEvent_t;

typedef enum
{
  SC_EVENT_NODE_TYPE_UNDEF=0,
  SC_EVENT_NODE_TYPE_GROUP,
  SC_EVENT_NODE_TYPE_HOOK,
} scEventNodeType_t;

struct scEventNode_s
{
  char                  tag[MAX_TAG_SIZE+1];

  scEventNode_t         *parent; // Parent node in tree.

  scEventNode_t         *next; // Next node in linked list
  scEventNode_t         *previous; // Previous node in linked list

  scEventNodeType_t     type;

  scEventNode_t         *first; // Childs in "inside" branch
  scEventNode_t         *last; // Childs in "inside" branch

  scDataTypeFunction_t  *hook; // Hook

  qboolean              skip;
};

struct scEvent_s
{
  scEventNode_t         *root;
  scEventNode_t         *current_node;
  qboolean              skip;
};

struct scHook_s
{
  scDataTypeFunction_t function;
};

extern scClass_t *event_class;

scEvent_t *SC_Event_New(void);
int SC_Event_Call(scObject_t *event, scDataTypeHash_t *params, scDataTypeValue_t *out);
int SC_Event_AddNode(scEventNode_t *parent, scEventNode_t *previous, scEventNode_t *new, scDataTypeValue_t *out);
int SC_Event_DeleteNode(scEventNode_t *node, scDataTypeValue_t *out);
int SC_Event_Skip(scEvent_t *event, const char *tag, scDataTypeValue_t *out);
scEventNode_t *SC_Event_NewNode(const char *tag);
scEventNode_t *SC_Event_FindChild(scEventNode_t *node, const char *tag);
void SC_Event_Init(void);
void SC_Event_Dump(scEvent_t *event);
int SC_Event_AddMainGroups(scEvent_t *event, scDataTypeValue_t *out);

// sc_utils.c

#define MAX_CONSTANT_LENGTH 32

typedef struct
{
  char            name[MAX_CONSTANT_LENGTH+1];
  scDataType_t    type; // can't be composite types like array or hash
  void            *data;
} scConstant_t;

extern scConstant_t *sc_constants;

void SC_Constant_Init(void);
void SC_Constant_Add(scConstant_t *constants);
scConstant_t *SC_Constant_Get(const char *name);

// sc_lua.c
#ifdef USE_LUA
extern lua_State *g_luaState;
#endif


#endif

