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

// sc_datatype.c
// contain scripting datatypes

#include "../game/g_local.h"
#include "../qcommon/q_shared.h"

#include "sc_public.h"

scNamespace_t *namespace_root;

static void stringFree(scDataTypeString_t *string);
static void arrayFree(scDataTypeArray_t *array);
static void hashFree( scDataTypeHash_t *hash );
static void functionFree(scDataTypeFunction_t *function);

static void print_string( scDataTypeString_t *string );
static void print_value( scDataTypeValue_t *value, int tab );
static void print_hash( scDataTypeHash_t *hash, int tab );
static void print_array( scDataTypeArray_t *array, int tab );
static void print_namespace( scNamespace_t *hash, int tab );

// String

static void strRealloc( scDataTypeString_t *string, int maxLen )
{
  char* old = string->data;
  // TODO: implement a real realloc
  // FIXME: must unlarge the string with a bigger buffer
  string->buflen = maxLen;
  string->data = (char*) BG_Alloc(maxLen);

  if(old)
  {
    memcpy( string->data, old, string->length );
    BG_Free( old );
  }
}

scDataTypeString_t *SC_StringNew(void)
{
  // FIXME: need an ideal buffer size
  scDataTypeString_t *string = (scDataTypeString_t*) BG_Alloc(sizeof(scDataTypeString_t));
  string->length = 0;
  string->buflen = 0;
  string->data = NULL;
  strRealloc(string, 1);
  string->data[0] = '\0';
  string->gc.count = 0;
  return string;
}

scDataTypeString_t *SC_StringNewFromChar(const char* str)
{
  scDataTypeString_t *string = SC_StringNew();
  SC_Strcat(string, str);
  return string;
}

const char* SC_StringToChar(scDataTypeString_t *string)
{
  return string->data;
}

static void stringFree(scDataTypeString_t *string)
{
  if(string->data)
    BG_Free(string->data);
  BG_Free(string);
}

void SC_Strcat( scDataTypeString_t *string, const char *src )
{
  int len = strlen(src);
  if(len == 0)
    return;

  if(string->buflen < string->length + len + 1)
    strRealloc(string, string->length + len + 1);

  Q_strncpyz(string->data + string->length, src, len + 1);
  string->length += len;
}

void SC_Strcpy( scDataTypeString_t *string, const char *src )
{
  string->length = 0;
  SC_Strcat(string, src);
}

qboolean SC_StringIsEmpty(scDataTypeString_t *string)
{
  return string->length == 0;
}

void SC_StringClear(scDataTypeString_t *string)
{
  string->length = 0;
  string->data[0] = '\0';
}

void SC_StringGCInc(scDataTypeString_t *string)
{
  string->gc.count++;
}

void SC_StringGCDec(scDataTypeString_t *string)
{
  string->gc.count--;
  if(string->gc.count == 0)
    stringFree(string);
}

// Value

qboolean SC_ValueIsScalar( const scDataTypeValue_t *value )
{
  return value->type == TYPE_INTEGER ||
         value->type == TYPE_FLOAT ||
         value->type == TYPE_STRING;
}

void SC_ValueGCInc(scDataTypeValue_t *value)
{
  switch(value->type)
  {
    case TYPE_STRING:
      SC_StringGCInc(value->data.string);
      break;

    case TYPE_ARRAY:
      SC_ArrayGCInc(value->data.array);
      break;

    case TYPE_HASH:
      SC_HashGCInc(value->data.hash);
      break;

    case TYPE_FUNCTION:
      SC_FunctionGCInc(value->data.function);
      break;

    case TYPE_NAMESPACE:
      SC_HashGCInc((scNamespace_t*) value->data.namespace);
      break;

    default:
      break;
  }
}

void SC_ValueGCDec(scDataTypeValue_t *value)
{
  switch(value->type)
  {
    case TYPE_STRING:
      SC_StringGCDec(value->data.string);
      break;

    case TYPE_ARRAY:
      SC_ArrayGCDec(value->data.array);
      break;

    case TYPE_HASH:
      SC_HashGCDec(value->data.hash);
      break;

    case TYPE_FUNCTION:
      SC_FunctionGCDec(value->data.function);
      break;

    case TYPE_NAMESPACE:
      SC_HashGCDec((scNamespace_t*) value->data.namespace);
      break;

    default:
      break;
  }
}

// Array
static void arrayRealloc(scDataTypeArray_t *array, int buflen)
{
  scDataTypeValue_t *old = array->data;

  // FIXME: must unlarge the array with a bigger buffer
  // FIXME: create new tag

  array->buflen = buflen;
  array->data = (scDataTypeValue_t*) BG_Alloc( sizeof(scDataTypeValue_t) * buflen);

  if(old)
  {
    memcpy(array->data, old, sizeof( scDataTypeArray_t ) * array->size);
    BG_Free(old);
  }
}

scDataTypeArray_t *SC_ArrayNew(void)
{
  scDataTypeArray_t *array = (scDataTypeArray_t*) BG_Alloc(sizeof(scDataTypeArray_t));
  array->buflen = 0;
  array->size = 0;
  array->data = NULL;
  array->gc.count = 0;
  return array;
}

qboolean SC_ArrayGet(scDataTypeArray_t *array, int index, scDataTypeValue_t *value)
{
  if( index >= array->size )
  {
    value->type = TYPE_UNDEF;
    return qfalse;
  }

  memcpy(value, &array->data[index], sizeof(scDataTypeValue_t));
  return qtrue;
}

void SC_ArraySet(scDataTypeArray_t *array, int index, scDataTypeValue_t *value)
{
  if(index >= array->buflen)
    arrayRealloc(array, index + 1);

  SC_ValueGCInc(value);
  if(index >= array->size)
  {
    memset(array->data + array->size, 0x00, (index - array->size) * sizeof(*array->data));
    array->size = index + 1;
  }
  else 
    SC_ValueGCDec(&array->data[index]);

  memcpy(&array->data[index], value, sizeof(scDataTypeValue_t));
}

qboolean SC_ArrayDelete(scDataTypeArray_t *array, int index)
{
  if(index >= array->size)
    return qfalse;

  SC_ValueGCDec(&array->data[index]);
  array->data[index].type = TYPE_UNDEF;


  if( index == array->size - 1 )
  {
    while( array->data[array->size-1].type == TYPE_UNDEF && array->size >  0 )
      array->size--;
  }

  return qtrue;
}

void SC_ArrayClear(scDataTypeArray_t *array)
{
  int i;

  array->size = 0;
  for(i = 0; i < array->size; i++)
    SC_ValueGCDec(&array->data[i]);
}

static void arrayFree(scDataTypeArray_t *array)
{
  SC_ArrayClear(array);
  BG_Free(array->data);
  BG_Free(array);
}

void SC_ArrayGCInc(scDataTypeArray_t *array)
{
  array->gc.count++;
}

void SC_ArrayGCDec(scDataTypeArray_t *array)
{
  array->gc.count--;
  if(array->gc.count == 0)
    arrayFree(array);
}

// Hash

// TODO: Make a better HashTable implementation

static void hashRealloc(scDataTypeHash_t *hash, int buflen)
{
  scDataTypeHashEntry_t *old = hash->data;

  // FIXME: can't use realloc
  // FIXME: must unlarge the hash with a bigger buffer
  hash->buflen = buflen;
  hash->data = (scDataTypeHashEntry_t*) BG_Alloc(sizeof(scDataTypeHashEntry_t) * hash->buflen);

  if(old)
  {
    memcpy( hash->data, old, sizeof(scDataTypeHashEntry_t) * hash->size);
    BG_Free(old);
  }
}

scDataTypeHash_t* SC_HashNew(void)
{
  scDataTypeHash_t *hash = (scDataTypeHash_t*) BG_Alloc(sizeof(scDataTypeHash_t));
  hash->size = 0;
  hash->buflen = 0;
  hash->data = NULL;
  hash->gc.count = 0;
  return hash;
}

qboolean SC_HashGet(scDataTypeHash_t *hash, const char *key, scDataTypeValue_t *value)
{
  int i;
  scDataTypeString_t *iKey;

  if(strlen(key) == 0)
  {
    value->type = TYPE_UNDEF;
    return qfalse;
  }

  for( i = 0; i < hash->size; i++ )
  {
    iKey = &hash->data[i].key;
    if( iKey && strcmp( SC_StringToChar(iKey), key ) == 0 )
    {
      memcpy(value, &hash->data[i].value, sizeof(scDataTypeValue_t));
      return qtrue;
    }
  }

  value->type = TYPE_UNDEF;
  return qfalse;
}

qboolean SC_HashSet( scDataTypeHash_t *hash, const char *key, scDataTypeValue_t *value )
{
  int i, freeIdx = -1;
  scDataTypeString_t *iKey;

  if(strlen(key) == 0)
    return qfalse;

  for( i = 0; i < hash->size; i++ )
  {
    iKey = &hash->data[i].key;
    if( ! iKey )
    {
      if(freeIdx == -1)
        freeIdx = i;
    }
    if( strcmp( SC_StringToChar(iKey), key ) == 0 )
    {
      SC_ValueGCInc(value);
      SC_ValueGCDec(&hash->data[i].value);
      memcpy(&hash->data[i].value, value, sizeof(scDataTypeValue_t));
      return qtrue;
    }
  }

  if( freeIdx == -1 )
  {
    freeIdx = hash->size;

    if(hash->size == hash->buflen)
      hashRealloc( hash, hash->size + 1 );

    hash->size++;
  }
  
  SC_ValueGCInc(value);
  SC_Strcpy(&hash->data[freeIdx].key, key);
  memcpy(&hash->data[freeIdx].value, value, sizeof(scDataTypeValue_t));

  return qtrue;
}

scDataTypeArray_t *SC_HashGetKeys(const scDataTypeHash_t *hash)
{
  scDataTypeArray_t *array;
  int iHash, iArray;
  scDataTypeValue_t value;
  
  array = SC_ArrayNew();

  iArray = 0;
  value.type = TYPE_STRING;
  for( iHash = 0; iHash < hash->size; iHash++ )
  {
    if(!SC_StringIsEmpty(&hash->data[iHash].key))
    {
      value.data.string = SC_StringNewFromChar(SC_StringToChar(&hash->data[iHash].key));
      SC_ArraySet( array, iArray, &value );
      SC_StringGCDec(value.data.string);
      iArray++;
    }
  }

  return array;
}

qboolean SC_HashDelete(scDataTypeHash_t *hash, const char *key)
{
  scDataTypeString_t *iKey;
  int i;

  for(i = 0; i < hash->size; i++)
  {
    iKey = &hash->data[i].key;
    if(iKey && strcmp (SC_StringToChar(iKey), key) == 0)
    {
      SC_StringClear(&hash->data[i].key);

      SC_ValueGCDec(&hash->data[i].value);

      if( i == hash->size - 1 )
      {
        do
        {
          hash->size--;
        } while(SC_StringIsEmpty(&hash->data[hash->size - 1].key) && hash->size > 0);
      }

      return qtrue;
    }
  }

  return qfalse;
}

void SC_HashClear( scDataTypeHash_t *hash )
{
  int i;
  for( i = 0; i < hash->size; i++ )
  {
    SC_StringClear(&hash->data[i].key);
    SC_ValueGCDec(&hash->data[i].value);
  }

  hash->size = 0;
}

static void hashFree( scDataTypeHash_t *hash )
{
  SC_HashClear(hash);
  BG_Free(hash->data);
  BG_Free(hash);
}

void SC_HashGCInc(scDataTypeHash_t *hash)
{
  hash->gc.count++;
}

void SC_HashGCDec(scDataTypeHash_t *hash)
{
  hash->gc.count--;
  if(hash->gc.count == 0)
    hashFree(hash);
}

// namespace
qboolean SC_NamespaceGet( const char *path, scDataTypeValue_t *value )
{
  char *idx;
  const char *base = path;
  char tmp[ MAX_NAMESPACE_LENGTH ];
  scNamespace_t *namespace = namespace_root;

  if( strcmp( path, "" ) == 0 )
  {
    value->type = TYPE_NAMESPACE;
    value->data.namespace = namespace_root;
    return qtrue;
  }
  
  idx = Q_strrchr( base, '.' );
  while( idx != NULL )
  {
    Q_strncpyz( tmp, base, idx - base + 1 );
	tmp[idx-base] = '\0';

    if( SC_HashGet( (scDataTypeHash_t*) namespace, tmp, value ) )
	{
      if( value->type != TYPE_NAMESPACE )
        return qfalse;
    }
    else
    {
      value->type = TYPE_UNDEF;
      return qfalse;
    }

    namespace = value->data.namespace;
    base = idx + 1;
    idx = Q_strrchr( base, '.' );
  }

  return SC_HashGet( (scDataTypeHash_t*) namespace, base, value );
}

qboolean SC_NamespaceDelete( const char *path )
{
  const char *idx;
  const char *base = path;
  char tmp[ MAX_NAMESPACE_LENGTH ];
  scNamespace_t *namespace = namespace_root;
  scDataTypeValue_t value;

  if( strcmp( path, "" ) == 0 )
    return qfalse;
  
  while( ( idx = Q_strrchr( base, '.' ) ) != NULL )
  {
    Q_strncpyz( tmp, base, idx - base + 1 );
	tmp[idx-base] = '\0';

    if( SC_HashGet( (scDataTypeHash_t*) namespace, tmp, & value ) )
	{
      if( value.type != TYPE_NAMESPACE )
        return qfalse;
    }
    else
      return qfalse;

    namespace = value.data.namespace;
    base = idx + 1;
  }

  return SC_HashDelete( (scDataTypeHash_t*) namespace, base );
  // TODO : reaffect namespace variable
}

qboolean SC_NamespaceSet( const char *path, scDataTypeValue_t *value )
{
  const char *idx;
  const char *base = path;
  char tmp[ MAX_NAMESPACE_LENGTH ];
  scNamespace_t *namespace = namespace_root;
  scDataTypeValue_t tmpval;
  
  if( strcmp( path, "" ) == 0 )
    return qfalse;
  
  while( ( idx = Q_strrchr( base, '.' ) ) != NULL )
  {
    Q_strncpyz( tmp, base, idx - base + 1 );
    tmp[idx-base] = '\0';
    if( SC_HashGet( (scDataTypeHash_t*) namespace, tmp, &tmpval ) )
    {
      if( tmpval.type != TYPE_NAMESPACE )
        return qfalse;
    }
    else
    {
      tmpval.type = TYPE_NAMESPACE;
      tmpval.data.namespace = SC_HashNew();
      SC_HashSet( (scDataTypeHash_t*) namespace, tmp, & tmpval );
    }

    namespace = tmpval.data.namespace;
    base = idx + 1;
  }

  return SC_HashSet((scDataTypeHash_t*) namespace, base, value );
  // TODO : reaffect namespace variable
}

void SC_NamespaceInit( void )
{
  namespace_root = (scNamespace_t*) SC_HashNew();
  SC_HashGCInc((scNamespace_t*) namespace_root);
}

// Function

scDataTypeFunction_t* SC_FunctionNew()
{
  scDataTypeFunction_t *function = ( scDataTypeFunction_t* ) BG_Alloc( sizeof( scDataTypeFunction_t ) );
  function->gc.count = 0;
  return function;
}

static void functionFree(scDataTypeFunction_t *function)
{
  BG_Free(function);
}

void SC_FunctionGCInc(scDataTypeFunction_t *function)
{
  function->gc.count++;
}

void SC_FunctionGCDec(scDataTypeFunction_t *function)
{
  function->gc.count--;
  if(function->gc.count == 0)
    functionFree(function);
}

// Display data tree

static void print_tabs( int tab )
{
  while( tab-- )
    Com_Printf("\t");
}

static void print_string( scDataTypeString_t *string )
{
  Com_Printf(string->data);
}

static void print_value( scDataTypeValue_t *value, int tab )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      print_tabs( tab );
      Com_Printf("<UNDEF>");
      break;

    case TYPE_INTEGER:
      print_tabs( tab );
      Com_Printf(va("%ld\n", value->data.integer));
      break;

    case TYPE_FLOAT:
      print_tabs( tab );
      Com_Printf(va("%f\n", value->data.floating));
      break;

    case TYPE_STRING:
      print_tabs( tab );
      print_string( value->data.string );
      Com_Printf("\n");
      break;

    case TYPE_ARRAY:
      print_array( value->data.array, tab );
      break;

    case TYPE_HASH:
      print_hash( value->data.hash, tab );
      break;

    case TYPE_NAMESPACE:
      print_namespace( value->data.namespace, tab );
      break;

    case TYPE_FUNCTION:
      print_tabs(tab);
      Com_Printf(va("function in %s\n", SC_LangageToString(value->data.function->langage)));
	  break;

    default:
      print_tabs( tab );
      Com_Printf("unknow type\n");
      break;
  }
}

static void print_array( scDataTypeArray_t *array, int tab )
{
  int i;

  print_tabs(tab);
  Com_Printf("Array [\n");
  for( i = 0; i < array->size; i++ )
    print_value( &array->data[i], tab + 1 );

  print_tabs(tab);
  Com_Printf("]\n");
}

static void print_hash( scDataTypeHash_t *hash, int tab )
{
  int i;

  print_tabs(tab);
  Com_Printf("Hash [\n");
  for( i = 0; i < hash->size; i++)
  {
    print_tabs( tab );
    print_string( &hash->data[i].key );
    Com_Printf(" =>\n");
    print_value( &hash->data[i].value, tab + 1 );
  }

  print_tabs(tab);
  Com_Printf("]\n");
}

static void print_namespace( scNamespace_t *hash, int tab )
{
  int i;

  print_tabs(tab);
  Com_Printf("Namespace [\n");
  for( i = 0; i < ( ( scDataTypeHash_t* ) hash )->size; i++ )
  {
    print_tabs( tab );
    print_string( &((scDataTypeHash_t*)hash)->data[i].key);
    Com_Printf(" =>\n");
    print_value( &((scDataTypeHash_t*)hash)->data[i].value, tab + 1 );
  }

  print_tabs(tab);
  Com_Printf("]\n");
}

void SC_PrintData( void )
{
  scDataTypeValue_t value;

  SC_NamespaceGet( "", & value );

  Com_Printf("----- Scripting data tree -----\n");
  print_namespace((scDataTypeHash_t*)value.data.namespace, 0);
  Com_Printf("-------------------------------\n");
}

char* SC_LangageToString(scLangage_t langage)
{
  switch(langage)
  {
    case LANGAGE_C: return "C";
    case LANGAGE_LUA: return "lua";
    case LANGAGE_PYTHON: return "python";
    case LANGAGE_INVALID: return "invalid";
  }

  return "unknow";
}
 
