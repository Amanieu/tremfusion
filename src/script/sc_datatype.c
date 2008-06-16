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

#include "sc_script.h"

// String

static scDataTypeString_t *strRealloc( scDataTypeString_t *string, int maxLen )
{
  // FIXME: can't use realloc
  // FIXME: must unlarge the string with a bigger buffer
  string->buflen = maxLen;
  string = ( scDataTypeString_t* ) realloc( string, ( sizeof( scDataTypeString_t ) - 1 ) + maxLen );

  return string;
}

scDataTypeString_t *SC_NewString( )
{
  // FIXME: can't use malloc
  // FIXME: need an ideal buffer size
  scDataTypeString_t *string = ( scDataTypeString_t* ) malloc( ( sizeof( scDataTypeString_t ) - 1 ) + 64 );
  string->length = 0;
  string->buflen = 0;
  string->data = '\0';

  return string;
}

scDataTypeString_t *SC_NewStringFromChar( const char* str )
{
  int len = strlen( str );
  scDataTypeString_t *string = SC_NewString( );

  Q_strncpyz( & string->data, str, len );

  return string;
}

void SC_StringFree( scDataTypeString_t *string )
{
  free( string );
}

scDataTypeString_t *SC_Strcat( scDataTypeString_t *string, const scDataTypeString_t *src )
{
  int len = strlen( & src->data );
  if( string->buflen < string->length + len )
    string = strRealloc( string, string->length + len );

  Q_strncpyz( & string->data + string->length, & src->data, len );
  string->length += len;

  return string;
}

scDataTypeString_t *SC_Strcpy( scDataTypeString_t *string, const scDataTypeString_t *src )
{
  string->length = 0;
  return SC_Strcat( string, src );
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
static scDataTypeArray_t* arrayRealloc( scDataTypeArray_t *array, int buflen )
{
  int oldsize = ( sizeof( scDataTypeArray_t ) -
                  sizeof( scDataTypeValue_t ) ) +
                sizeof( scDataTypeValue_t ) * array->buflen;
  int newsize = oldsize + sizeof( scDataTypeValue_t ) * ( buflen - array->buflen );

  // FIXME: can't use realloc
  // FIXME: must unlarge the array with a bigger buffer
  array->buflen = buflen;
  array = ( scDataTypeArray_t* ) realloc( array, newsize );
  memset( (char*) array + oldsize, 0x00, newsize - oldsize );

  return array;
}

scDataTypeArray_t *SC_NewArray( )
{
  int buflen = 8;
  int size = ( sizeof( scDataTypeArray_t ) -
               sizeof( scDataTypeValue_t ) ) +
             sizeof( scDataTypeValue_t ) * buflen;

  scDataTypeArray_t *array = malloc( size );
  memset( (char*) array, 0x00, size );

  array->buflen = buflen;
  array->size = 0;

  return array;
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

scDataTypeArray_t *SC_ArraySet( scDataTypeArray_t *array, int index, scDataTypeValue_t *value )
{
  if( index >= array->buflen )
    array = arrayRealloc( array, index + 1 );

  if( index >= array->size )
    array->size = index + 1;

  memcpy( & array->data + index, value, sizeof( scDataTypeValue_t ) );

  return array;
}

scDataTypeArray_t *SC_ArrayDelete( scDataTypeArray_t *array, int index )
{
  if( index >= array->size )
    return array;

  ( & array->data )[ index ].type = TYPE_UNDEF;

  // TODO: run GC

  if( index == array->size - 1 )
  {
    while( ( & array->data )[ array->size - 1 ].type == TYPE_UNDEF  && array->size >  0 )
      array->size--;
  }

  return array;
}

scDataTypeArray_t *SC_ArrayClear( scDataTypeArray_t *array )
{
  array->size = 0;

  // TODO: run GC

  return array;
}

void SC_ArrayFree( scDataTypeArray_t *array )
{
  // TODO: run GC

  free(array );
}

// Hash

// TODO: Make a better HashTable implementation

static scDataTypeHash_t* hashRealloc( scDataTypeHash_t *hash, int buflen )
{
  // FIXME: can't use realloc
  // FIXME: must unlarge the hash with a bigger buffer
  hash->buflen = buflen;
  hash = ( scDataTypeHash_t* ) realloc( hash,
      ( sizeof( scDataTypeHash_t ) -
        sizeof( scDataTypeHashEntry_t ) ) +
      sizeof( scDataTypeHashEntry_t ) * buflen );

  return hash;
}

scDataTypeHash_t *SC_NewHash( )
{
  int buflen = 8;

  scDataTypeHash_t *hash = malloc(
      ( sizeof( scDataTypeHash_t ) -
        sizeof( scDataTypeHashEntry_t ) ) +
      sizeof( scDataTypeHashEntry_t ) * buflen );

  hash->buflen = buflen;
  hash->size = 0;

  return hash;
}

qboolean SC_HashGet( const scDataTypeHash_t *hash, const scDataTypeString_t *key, scDataTypeValue_t *value )
{
  int i;
  scDataTypeString_t *iKey;
  for( i = 0; i < hash->size; i++ )
  {
    iKey = ( & hash->data )[ i ].key;
    if( iKey && strcmp( & iKey->data, & key->data ) == 0 )
    {
      memcpy( value, & ( & hash->data)[ i ].value, sizeof( scDataTypeValue_t ) );
      return qtrue;
    }
  }

  value->type = TYPE_UNDEF;
  return qfalse;
}

scDataTypeHash_t *SC_HashSet( scDataTypeHash_t *hash, const scDataTypeString_t *key, scDataTypeValue_t *value )
{
  int i, freeIdx = -1;
  scDataTypeString_t *iKey;

  for( i = 0; i < hash->size; i++ )
  {
    iKey = ( & hash->data )[ i ].key;
    if( ! iKey )
    {
      if( freeIdx == -1 )
        freeIdx = i;
    }
    if( strcmp( & iKey->data, & key->data ) == 0 )
    {
      memcpy( & ( & hash->data )[ i ].value, value, sizeof( scDataTypeValue_t ) );
      return hash;
    }
  }

  if( freeIdx == -1 )
  {
    freeIdx = hash->size;

    if( hash->size == hash->buflen )
      hash = hashRealloc( hash, hash->size + 2 );

    hash->size++;
  }
  
  ( & hash->data )[ freeIdx ].key = SC_NewString( );
  SC_Strcpy( ( & hash->data )[ freeIdx ].key, key );
  memcpy( & ( & hash->data )[ freeIdx ].value, value, sizeof( scDataTypeValue_t ) );

  return hash;
}

scDataTypeArray_t *SC_HashGetKeys( scDataTypeHash_t *hash )
{
  int iHash, iArray;
  scDataTypeValue_t value;
  scDataTypeArray_t *array = SC_NewArray( );

  iArray = 0;
  value.type = TYPE_STRING;
  for( iHash = 0; iHash < hash->size; iHash++ )
  {
    if( ( & hash->data)[ iHash ].key != NULL )
    {
      value.data.string = SC_NewString( );
      SC_Strcpy( value.data.string, ( & hash->data)[ iHash ].key );
      SC_ArraySet( array, iArray, & value );
      iArray++;
    }
  }

  return array;
}

scDataTypeHash_t *SC_HashDelete( scDataTypeHash_t *hash, const scDataTypeString_t *key )
{
  scDataTypeString_t *iKey;
  int i;

  for( i = 0; i < hash->size; i++ )
  {
    iKey = ( & hash->data)[ i ].key;
    if( iKey && strcmp ( & iKey->data, & key->data ) == 0 )
    {
      SC_StringFree( ( & hash->data )[ i ].key );
      ( & hash->data )[ i ].key = NULL;

      // TODO: run GC

      if( i == hash->size - 1 )
      {
        do
        {
          hash->size--;
        } while( ( & hash->data )[ hash->size - 1 ].key == NULL );
      }
    }
  }

  return hash;
}

scDataTypeHash_t *SC_HashClear( scDataTypeHash_t *hash )
{
  int i;
  for( i = 0; i < hash->size; i++ )
  {
    SC_StringFree( ( & hash->data )[ i ].key );
    ( & hash->data )[ i ].key = NULL;

    // TODO: run GC
  }

  hash->size = 0;

  return hash;
}

void SC_HashFree( scDataTypeHash_t *hash )
{
  hash = SC_HashClear( hash );
  free( hash );
}

