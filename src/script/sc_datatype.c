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

static void print_string( scDataTypeString_t *string );
static void print_value( scDataTypeValue_t *value, int tab );
static void print_hash( scDataTypeHash_t *hash, int tab );
static void print_array( scDataTypeArray_t *array, int tab );
static void print_namespace( scNamespace_t *hash, int tab );

// String

static void strRealloc( scDataTypeString_t **string, int maxLen )
{
  scDataTypeString_t *old = *string;
  // TODO: implement a real realloc
  // FIXME: must unlarge the string with a bigger buffer
  *string = ( scDataTypeString_t* ) BG_Alloc( sizeof( scDataTypeString_t ) - 1 + maxLen );
  memcpy( *string, old, sizeof( scDataTypeString_t ) - 1 + old->length );
  (*string)->buflen = maxLen;
  BG_Free( old );
}

void SC_StringNew( scDataTypeString_t **string )
{
  // FIXME: need an ideal buffer size
  *string = ( scDataTypeString_t* ) BG_Alloc( ( sizeof( scDataTypeString_t ) - 1 ) + 64 );
  (*string)->length = 0;
  (*string)->buflen = 0;
  (*string)->data = '\0';
}

void SC_StringNewFromChar( scDataTypeString_t **string, const char* str )
{
  int len = strlen( str );
  SC_StringNew( string );

  Q_strncpyz( & (*string)->data, str, len + 1 );
}

const char* SC_StringToChar(scDataTypeString_t *string)
{
  return &string->data;
}

void SC_StringFree( scDataTypeString_t *string )
{
  BG_Free( string );
}

void SC_Strcat( scDataTypeString_t **string, const scDataTypeString_t *src )
{
  int len = strlen( & src->data );
  if( (*string)->buflen < (*string)->length + len )
    strRealloc( string, (*string)->length + len );

  Q_strncpyz( & (*string)->data + (*string)->length, & src->data, len + 1 );
  (*string)->length += len;
}

void SC_Strcpy( scDataTypeString_t **string, const scDataTypeString_t *src )
{
  (*string)->length = 0;
  SC_Strcat( string, src );
}

// Value

qboolean SC_ValueIsScalar( const scDataTypeValue_t *value )
{
  return value->type == TYPE_INTEGER ||
         value->type == TYPE_FLOAT ||
         value->type == TYPE_STRING;
}

void SC_ValueFree( scDataTypeValue_t *value )
{
  switch( value->type )
  {
    case TYPE_STRING:
      SC_StringFree( value->data.string );
      break;

    case TYPE_ARRAY:
      SC_ArrayFree( value->data.array );
      break;

    case TYPE_HASH:
      SC_HashFree( value->data.hash );
      break;

    default:
      break;
  }
}

// Array
static void arrayRealloc( scDataTypeArray_t **array, int buflen )
{
  scDataTypeArray_t *old = *array;

  // FIXME: can't use realloc
  // FIXME: must unlarge the array with a bigger buffer
  // FIXME: create new tag
  *array = ( scDataTypeArray_t* ) BG_Alloc( sizeof( scDataTypeArray_t ) + ( - 1 + buflen ) * sizeof( scDataTypeValue_t ) );
  memcpy( *array, old, sizeof( scDataTypeArray_t ) + ( - 1 + old->size ) * sizeof( scDataTypeValue_t ) );
  (*array)->buflen = buflen;
  BG_Free( old );
}

void SC_ArrayNew( scDataTypeArray_t **array )
{
  int buflen = 8;
  int size = ( sizeof( scDataTypeArray_t ) -
               sizeof( scDataTypeValue_t ) ) +
             sizeof( scDataTypeValue_t ) * buflen;

  // FIXME: create new tag
  *array = (scDataTypeArray_t*) BG_Alloc( size );

  (*array)->buflen = buflen;
  (*array)->size = 0;
}

qboolean SC_ArrayGet( const scDataTypeArray_t *array, int index, scDataTypeValue_t *value )
{
  if( index >= array->size )
  {
    value->type = TYPE_UNDEF;
    return qfalse;
  }

  memcpy( value, & array->data + index, sizeof( scDataTypeValue_t ) );
  return qtrue;
}

void SC_ArraySet( scDataTypeArray_t **array, int index, scDataTypeValue_t *value )
{
  if( index >= (*array)->buflen )
    arrayRealloc( array, index + 1 );

  // TODO: memset new data
  if( index >= (*array)->size )
    (*array)->size = index + 1;

  memcpy( & (*array)->data + index, value, sizeof( scDataTypeValue_t ) );
}

qboolean SC_ArrayDelete( scDataTypeArray_t **array, int index )
{
  if( index >= (*array)->size )
    return qfalse;

  ( & (*array)->data )[ index ].type = TYPE_UNDEF;

  // TODO: run GC

  if( index == (*array)->size - 1 )
  {
    while( ( & (*array)->data )[ (*array)->size - 1 ].type == TYPE_UNDEF  && (*array)->size >  0 )
      (*array)->size--;
  }

  return qtrue;
}

void SC_ArrayClear( scDataTypeArray_t **array )
{
  (*array)->size = 0;

  // TODO: run GC
}

void SC_ArrayFree( scDataTypeArray_t *array )
{
  SC_ArrayClear( &array );
  BG_Free( array );
}

// Hash

// TODO: Make a better HashTable implementation

static void hashRealloc( scDataTypeHash_t **hash, int buflen )
{
  scDataTypeHash_t *old = *hash;

  // FIXME: can't use realloc
  // FIXME: must unlarge the hash with a bigger buffer
  *hash = ( scDataTypeHash_t* ) BG_Alloc( sizeof( scDataTypeHash_t ) + ( - 1 + buflen ) * sizeof( scDataTypeHashEntry_t ) );
  memcpy( *hash, old, sizeof( scDataTypeHash_t ) + ( - 1 + old->size ) * sizeof( scDataTypeHashEntry_t ) );
  (*hash)->buflen = buflen;
  BG_Free( old );
}

void SC_HashNew( scDataTypeHash_t **hash )
{
  int buflen = 8;

  *hash = (scDataTypeHash_t*) BG_Alloc(
      ( sizeof( scDataTypeHash_t ) -
        sizeof( scDataTypeHashEntry_t ) ) +
      sizeof( scDataTypeHashEntry_t ) * buflen );

  (*hash)->buflen = buflen;
  (*hash)->size = 0;
}

qboolean SC_HashGet( const scDataTypeHash_t *hash, const char *key, scDataTypeValue_t *value )
{
  int i;
  scDataTypeString_t *iKey;
  for( i = 0; i < hash->size; i++ )
  {
    iKey = ( & hash->data )[ i ].key;
    if( iKey && strcmp( & iKey->data, key ) == 0 )
    {
      memcpy( value, & ( & hash->data)[ i ].value, sizeof( scDataTypeValue_t ) );
      return qtrue;
    }
  }

  value->type = TYPE_UNDEF;
  return qfalse;
}

qboolean SC_HashSet( scDataTypeHash_t **hash, const char *key, scDataTypeValue_t *value )
{
  int i, freeIdx = -1;
  scDataTypeString_t *iKey;

  for( i = 0; i < (*hash)->size; i++ )
  {
    iKey = ( & (*hash)->data )[ i ].key;
    if( ! iKey )
    {
      if( freeIdx == -1 )
        freeIdx = i;
    }
    if( strcmp( & iKey->data, key ) == 0 )
    {
      memcpy( & ( & (*hash)->data )[ i ].value, value, sizeof( scDataTypeValue_t ) );
      return qtrue;
    }
  }

  if( freeIdx == -1 )
  {
    freeIdx = (*hash)->size;

    if( (*hash)->size == (*hash)->buflen )
      hashRealloc( hash, (*hash)->size + 2 );

    (*hash)->size++;
  }
  
  SC_StringNewFromChar( & ( & (*hash)->data )[ freeIdx ].key, key );
  memcpy( & ( & (*hash)->data )[ freeIdx ].value, value, sizeof( scDataTypeValue_t ) );

  return qtrue;
}

void SC_HashGetKeys( const scDataTypeHash_t *hash, scDataTypeArray_t **array )
{
  int iHash, iArray;
  scDataTypeValue_t value;
  
  SC_ArrayNew( array );

  iArray = 0;
  value.type = TYPE_STRING;
  for( iHash = 0; iHash < hash->size; iHash++ )
  {
    if( ( & hash->data)[ iHash ].key != NULL )
    {
      SC_StringNew( & value.data.string );
      SC_Strcpy( & value.data.string, ( & hash->data)[ iHash ].key );
      SC_ArraySet( array, iArray, & value );
      iArray++;
    }
  }
}

qboolean SC_HashDelete( scDataTypeHash_t **hash, const char *key )
{
  scDataTypeString_t *iKey;
  int i;

  for( i = 0; i < (*hash)->size; i++ )
  {
    iKey = ( & (*hash)->data)[ i ].key;
    if( iKey && strcmp ( & iKey->data, key ) == 0 )
    {
      SC_StringFree( ( & (*hash)->data )[ i ].key );
      ( & (*hash)->data )[ i ].key = NULL;

      // TODO: run GC

      if( i == (*hash)->size - 1 )
      {
        do
        {
          (*hash)->size--;
        } while( ( & (*hash)->data )[ (*hash)->size - 1 ].key == NULL );
      }

      return qtrue;
    }
  }

  return qfalse;
}

void SC_HashClear( scDataTypeHash_t **hash )
{
  int i;
  for( i = 0; i < (*hash)->size; i++ )
  {
    SC_StringFree( ( & (*hash)->data )[ i ].key );
    ( & (*hash)->data )[ i ].key = NULL;

    // TODO: run GC
  }

  (*hash)->size = 0;
}

void SC_HashFree( scDataTypeHash_t *hash )
{
  SC_HashClear( & hash );
  BG_Free( hash );
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

  return SC_HashDelete( (scDataTypeHash_t**) & namespace, base );
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
      SC_HashNew( (scDataTypeHash_t**) & tmpval.data.namespace );
      SC_HashSet( (scDataTypeHash_t**) & namespace, tmp, & tmpval );
    }

    namespace = tmpval.data.namespace;
    base = idx + 1;
  }

  return SC_HashSet( (scDataTypeHash_t**) & namespace, base, value );
  // TODO : reaffect namespace variable
}

void SC_NamespaceInit( void )
{
  SC_HashNew( (scDataTypeHash_t**) & namespace_root );
}

// Function

void SC_FunctionNew( scDataTypeFunction_t **func )
{
  *func = ( scDataTypeFunction_t* ) BG_Alloc( sizeof( *func ) );
}

// Display data tree

static void print_tabs( int tab )
{
  while( tab-- )
    Com_Printf("\t");
}

static void print_string( scDataTypeString_t *string )
{
  Com_Printf( & string->data );
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
    print_value( & (& array->data)[ i ], tab + 1 );

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
    print_string( (& hash->data)[i].key );
    Com_Printf(" =>\n");
    print_value( & (& hash->data)[i].value, tab + 1 );
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
    print_string( ( & ( ( scDataTypeHash_t* ) hash )->data )[ i ].key );
    Com_Printf(" =>\n");
    print_value( & ( & ( ( scDataTypeHash_t* ) hash )->data )[ i ].value, tab + 1 );
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
 
