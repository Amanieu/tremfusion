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
//
// sc_datatype_test.c
// unit test for scripting datatypes

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "sc_public.h"

#define HELLO_WORLD "Hello, world !"

void print_string( scDataTypeString_t *string );
void print_value( scDataTypeValue_t *value, int tab );
void print_hash( scDataTypeHash_t *hash, int tab );
void print_array( scDataTypeArray_t *array, int tab );
void print_namespace( scNamespace_t *hash, int tab );
void print_root_namespace( );

static void print_tabs( int tab )
{
  while( tab-- )
    printf("\t");
}

void print_string( scDataTypeString_t *string )
{
  printf( & string->data );
}

void print_value( scDataTypeValue_t *value, int tab )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      print_tabs( tab );
      printf("<UNDEF>");
      break;

    case TYPE_INTEGER:
      print_tabs( tab );
      printf( "%ld\n", value->data.integer );
      break;

    case TYPE_FLOAT:
      print_tabs( tab );
      printf( "%f\n", value->data.floating );
      break;

    case TYPE_STRING:
      print_tabs( tab );
      print_string( value->data.string );
      printf("\n");
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

    default:
      print_tabs( tab );
      printf("unknow type\n");
      break;
  }
}

void print_array( scDataTypeArray_t *array, int tab )
{
  int i;

  print_tabs(tab);
  printf("Array [\n");
  for( i = 0; i < array->size; i++ )
    print_value( & (& array->data)[ i ], tab + 1 );

  print_tabs(tab);
  printf("]\n");
}

void print_hash( scDataTypeHash_t *hash, int tab )
{
  int i;

  print_tabs(tab);
  printf("Hash [\n");
  for( i = 0; i < hash->size; i++)
  {
    print_tabs( tab );
    print_string( (& hash->data)[i].key );
    printf(" =>\n");
    print_value( & (& hash->data)[i].value, tab + 1 );
  }

  print_tabs(tab);
  printf("]\n");
}

void print_namespace( scNamespace_t *hash, int tab )
{
  int i;

  print_tabs(tab);
  printf("Namespace [\n");
  for( i = 0; i < ( ( scDataTypeHash_t* ) hash )->size; i++ )
  {
    print_tabs( tab );
    print_string( ( & ( ( scDataTypeHash_t* ) hash )->data )[ i ].key );
    printf(" =>\n");
    print_value( & ( & ( ( scDataTypeHash_t* ) hash )->data )[ i ].value, tab + 1 );
  }

  print_tabs(tab);
  printf("]\n");
}

void print_root_namespace( )
{
  scDataTypeValue_t value;

  SC_NamespaceGet( "", & value );

  print_namespace( value.data.namespace, 0 );
}

void test_strings()
{
  scDataTypeString_t *s1;
  scDataTypeString_t *s2;
  scDataTypeString_t *s3;
  scDataTypeString_t *s4;
  scDataTypeString_t *dest;

  printf("A\\ Test Strings\n");

  printf("\t1: New\n");
  SC_StringNewFromChar( &s1, "Hello" );
  SC_StringNewFromChar( &s2, ", " );
  SC_StringNewFromChar( &s3, "world !" );
  SC_StringNewFromChar( &s4, "\n" );
  SC_StringNew( &dest );
  printf("\t  ... ok\n");

  printf("\t2: Copy/Cat\n");
  SC_Strcpy( &dest, s1 );
  SC_Strcat( &dest, s2 );
  SC_Strcat( &dest, s3 );
  SC_Strcat( &dest, s4 );
  printf("\t  ... ok\n");

  printf("\t3: Match value\n");
  assert( strcmp( & dest->data, "Hello, world !\n" ) == 0 );
  printf("\t  ... ok\n");

  printf("\t4: Overwriting\n");
  SC_Strcpy( &dest, s1 );
  assert( strcmp( & dest->data, "Hello" ) == 0 );
  printf("\t  ... ok\n");

  printf("\t5: Delete\n");
  SC_StringFree( s1 );
  SC_StringFree( s2 );
  SC_StringFree( s3 );
  SC_StringFree( s4 );

  SC_StringFree( dest );
  printf("\t  ... ok\n");

  printf("  ... Test strings: ok\n");
}

void test_arrays( )
{
  scDataTypeArray_t *array;
  scDataTypeValue_t value1, value2;

  printf("B\\ Test arrays\n");

  printf("\t1: Create array\n");
  SC_ArrayNew( &array );
  printf("\t  ... ok\n");

  printf("\t2: Get undef values\n");
  SC_ArrayGet( array, 0, & value1 );
  assert( value1.type == TYPE_UNDEF );
  SC_ArrayGet( array, 4, & value1 );
  assert( value1.type == TYPE_UNDEF );
  SC_ArrayGet( array, 1000, & value1 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("\t3: Set/Get values\n");
  value1.type = TYPE_STRING;
  SC_StringNewFromChar( & value1.data.string, HELLO_WORLD );
  SC_ArraySet( &array, 4, & value1 );
  assert( array->size == 5 );
  assert( SC_ArrayGet( array, 4, & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( & value2.data.string->data, HELLO_WORLD ) == 0 );

  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  SC_ArraySet( &array, 30, & value1 );
  assert( array->size == 31 );
  assert( SC_ArrayGet( array, 30, & value2 ) );
  assert( value2.type == TYPE_INTEGER );
  assert( value2.data.integer == 1337 );

  value1.type = TYPE_FLOAT;
  value1.data.floating = 43.7;
  SC_ArraySet( &array, 0, & value1 );
  assert( array->size == 31 );
  assert( SC_ArrayGet( array, 0, & value2 ) );
  assert( value2.type == TYPE_FLOAT );
  assert( value2.data.floating > 43.6 && value2.data.floating < 43.8 );
  printf("\t  ... ok\n");

  printf("\t4: Delete values\n");
  SC_ArrayDelete( &array, 4 );
  assert( array->size == 31 );
  assert( SC_ArrayGet( array, 4, & value1 ) );
  assert( value1.type == TYPE_UNDEF );

  SC_ArrayDelete( &array, 30 );
  assert( array->size == 1 );
  SC_ArrayGet( array, 30, & value1 ); assert( value1.type == TYPE_UNDEF );

  SC_ArrayDelete( &array, 0 );
  assert( array->size == 0 );
  SC_ArrayGet( array, 0, & value1 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("  ... Test arrays: ok\n");
}

void test_hashs()
{
  scDataTypeHash_t *hash;
  scDataTypeValue_t value1, value2;
  scDataTypeArray_t *array;
  char cstr[16];
  int i;

  printf("C\\ Test hashmaps\n");

  printf("\t1: Create hash table\n");
  SC_HashNew( &hash );
  assert( hash->size == 0 );
  printf("\t  ... ok\n");

  printf("\t2: Get undef values\n");
  SC_HashGet( hash, "plop", & value1 );
  assert( value1.type == TYPE_UNDEF );

  SC_HashGet( hash, "", & value1 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("\t3: Set/Get values\n");
  value1.type = TYPE_STRING;
  SC_StringNewFromChar( & value1.data.string, HELLO_WORLD );
  SC_HashSet( &hash, "plop", & value1 );
  assert( hash->size == 1 );
  assert( SC_HashGet( hash, "plop", & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( & value2.data.string->data, HELLO_WORLD ) == 0 );

  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  SC_HashSet( &hash, "", & value1 );
  assert( hash->size == 2 );
  assert( SC_HashGet( hash, "", & value2 ) );
  assert( value2.type == TYPE_INTEGER );
  assert( value2.data.integer == 1337 );

  value1.type = TYPE_FLOAT;
  value1.data.floating = 43.7;
  SC_HashSet( &hash, "plop", & value1 );
  assert( hash->size == 2 );
  assert( SC_HashGet( hash, "plop", & value2 ) );
  assert( value2.type == TYPE_FLOAT );
  assert( value2.data.floating > 43.6 && value2.data.floating < 43.8 );
  printf("\t  ... ok\n");

  printf("\t4: Get keys\n");
  SC_HashGetKeys( hash, &array );
  assert( array->size == 2 );

  assert( SC_ArrayGet( array, 0, & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( & value2.data.string->data, "plop" ) == 0 || strcmp( & value2.data.string->data, "" ) == 0 );

  assert( SC_ArrayGet( array, 1, & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( & value2.data.string->data, "plop" ) == 0 || strcmp( & value2.data.string->data, "" ) == 0 );
  printf("\t  ... ok\n");

  printf("\t5: Delete values\n");
  SC_HashDelete( &hash, "plop" );
  assert( SC_HashGet( hash, "plop", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );

  SC_HashDelete( &hash, "" );
  assert( hash->size == 0 );
  assert( SC_HashGet( hash, "", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("\t6: Buffer resizing\n");
  for( i = 0; i < 35; i++ )
  {
    sprintf(cstr, "plop%d", i);
    value1.data.integer = 1337;
    SC_HashSet( &hash, cstr, & value1 );
  }
  assert( hash->size == 35 );
  printf("\t  ... ok\n");

  printf("\t7: Clear hash\n");
  SC_HashClear( &hash );
  assert( hash->size == 0 );

  assert( SC_HashGet( hash, "plop1", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );
  assert( SC_HashGet( hash, "plop2", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );
  assert( SC_HashGet( hash, "plop3", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("\t8: Delete hash\n");
  SC_HashFree( hash );
  printf("\t  ... ok\n");

  printf("  ... Test hashmap: ok\n");
}

void test_namespaces( )
{
  scDataTypeValue_t root, value1, value2;

  printf("D\\ Test Namespaces\n");

  printf("\t1: Init namespaces\n");
  SC_NamespaceInit( );
  printf("\t  ... ok\n");

  printf("\t2: Looking for root\n");
  assert( SC_NamespaceGet( "", & root ) == qtrue );
  assert( root.type == TYPE_NAMESPACE );
  printf("\t ... ok\n");

  printf("\t3: Looking for void data\n");
  assert( SC_NamespaceGet( "meuh", & value1 ) == qfalse );
  assert( SC_NamespaceGet( "coin.bouh", & value1 ) == qfalse );
  printf("\t  ... ok\n");

  printf("\t4: Set data\n");
  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  assert( SC_NamespaceSet( "plop", & value1 ) == qtrue );
  assert( SC_NamespaceGet( "plop", & value2 ) == qtrue );
  assert( value2.data.integer == 1337 );
  assert( value2.type == TYPE_INTEGER );

  assert( SC_NamespaceSet( "meuh.bouh", & value1 ) == qtrue );
  assert( SC_NamespaceGet( "meuh", & value2 ) == qtrue );
  assert( value2.type == TYPE_NAMESPACE );
  assert( SC_NamespaceGet( "meuh.bouh", & value2 ) == qtrue );
  assert( value2.data.integer == 1337 );
  assert( value2.type == TYPE_INTEGER );

  assert( SC_NamespaceSet( "", & value1 ) == qfalse );
  printf("\t  ... ok\n");

  printf("\t5: Delete data\n");
  assert( SC_NamespaceDelete( "plop" ) == qtrue );
  assert( SC_NamespaceGet( "plop", & value2 ) == qfalse );
  assert( value2.type == TYPE_UNDEF );

  assert( SC_NamespaceDelete( "meuh" ) == qtrue );
  assert( SC_NamespaceGet( "meuh", & value2 ) == qfalse );
  assert( value2.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("  ... Test namespaces: ok\n");
}

int main()
{
  BG_InitMemory( );
  test_strings( );
  test_arrays( );
  test_hashs( );
  test_namespaces( );

  printf("unittest: ok\n");

  return 0;
}

