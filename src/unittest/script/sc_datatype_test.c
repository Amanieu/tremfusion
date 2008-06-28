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

void test_strings()
{
  scDataTypeString_t *s1;
  scDataTypeString_t *s2;
  scDataTypeString_t *s3;
  scDataTypeString_t *s4;
  scDataTypeString_t *dest;

  printf("A\\ Test Strings\n");

  printf("\t1: New\n");
  s1 = SC_StringNewFromChar("Hello");
  s2 = SC_StringNewFromChar(", ");
  s3 = SC_StringNewFromChar("world !");
  s4 = SC_StringNewFromChar("\n" );
  dest = SC_StringNew();
  printf("\t  ... ok\n");

  printf("\t2: Copy/Cat\n");
  SC_Strcpy(dest, SC_StringToChar(s1));
  SC_Strcat(dest, SC_StringToChar(s2));
  SC_Strcat(dest, SC_StringToChar(s3));
  SC_Strcat(dest, SC_StringToChar(s4));
  printf("\t  ... ok\n");

  printf("\t3: Match value\n");
  assert(strcmp( SC_StringToChar(dest), "Hello, world !\n" ) == 0 );
  printf("\t  ... ok\n");

  printf("\t4: Overwriting\n");
  SC_Strcpy(dest, SC_StringToChar(s1));
  assert(strcmp(SC_StringToChar(dest), "Hello" ) == 0 );
  printf("\t  ... ok\n");

  printf("\t5: Delete\n");
  SC_StringFree(s1);
  SC_StringFree(s2);
  SC_StringFree(s3);
  SC_StringFree(s4);

  SC_StringFree(dest);
  printf("\t  ... ok\n");

  printf("  ... Test strings: ok\n");
}

void test_arrays( )
{
  scDataTypeArray_t *array;
  scDataTypeValue_t value1, value2;

  printf("B\\ Test arrays\n");

  printf("\t1: Create array\n");
  array = SC_ArrayNew();
  printf("\t  ... ok\n");

  printf("\t2: Get undef values\n");
  SC_ArrayGet(array, 0, &value1);
  assert(value1.type == TYPE_UNDEF);
  SC_ArrayGet(array, 4, &value1);
  assert(value1.type == TYPE_UNDEF);
  SC_ArrayGet(array, 1000, &value1);
  assert(value1.type == TYPE_UNDEF);
  printf("\t  ... ok\n");

  printf("\t3: Set/Get values\n");
  value1.type = TYPE_STRING;
  value1.data.string = SC_StringNewFromChar(HELLO_WORLD);
  SC_ArraySet(array, 4, &value1);
  assert(array->size == 5);
  assert(SC_ArrayGet(array, 4, &value2));
  assert(value2.type == TYPE_STRING);
  assert(strcmp(SC_StringToChar(value2.data.string), HELLO_WORLD ) == 0 );

  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  SC_ArraySet( array, 30, & value1 );
  assert( array->size == 31 );
  assert( SC_ArrayGet( array, 30, & value2 ) );
  assert( value2.type == TYPE_INTEGER );
  assert( value2.data.integer == 1337 );

  value1.type = TYPE_FLOAT;
  value1.data.floating = 43.7;
  SC_ArraySet( array, 0, & value1 );
  assert( array->size == 31 );
  assert( SC_ArrayGet( array, 0, & value2 ) );
  assert( value2.type == TYPE_FLOAT );
  assert( value2.data.floating > 43.6 && value2.data.floating < 43.8 );
  printf("\t  ... ok\n");

  printf("\t4: Delete values\n");
  SC_ArrayDelete( array, 4 );
  assert( array->size == 31 );
  assert( SC_ArrayGet( array, 4, & value1 ) );
  assert( value1.type == TYPE_UNDEF );

  SC_ArrayDelete( array, 30 );
  assert( array->size == 1 );
  SC_ArrayGet( array, 30, & value1 ); assert( value1.type == TYPE_UNDEF );

  SC_ArrayDelete( array, 0 );
  assert( array->size == 0 );
  SC_ArrayGet( array, 0, & value1 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("\t5: Free array\n");
  SC_ArrayFree(array);
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
  hash = SC_HashNew();
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
  value1.data.string = SC_StringNewFromChar(HELLO_WORLD);
  SC_HashSet( hash, "plop", & value1 );
  assert( hash->size == 1 );
  assert( SC_HashGet( hash, "plop", & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( SC_StringToChar(value2.data.string), HELLO_WORLD ) == 0 );

  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  assert(SC_HashSet( hash, "", & value1 ) == qfalse);
  assert(SC_HashSet( hash, "coin", & value1 ) == qtrue);
  assert( hash->size == 2 );
  assert( SC_HashGet( hash, "coin", & value2 ) );
  assert( value2.type == TYPE_INTEGER );
  assert( value2.data.integer == 1337 );

  value1.type = TYPE_FLOAT;
  value1.data.floating = 43.7;
  SC_HashSet( hash, "plop", & value1 );
  assert( hash->size == 2 );
  assert( SC_HashGet( hash, "plop", & value2 ) );
  assert( value2.type == TYPE_FLOAT );
  assert( value2.data.floating > 43.6 && value2.data.floating < 43.8 );
  printf("\t  ... ok\n");

  printf("\t4: Get keys\n");
  array = SC_HashGetKeys(hash);
  assert( array->size == 2 );

  assert( SC_ArrayGet( array, 0, & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( SC_StringToChar(value2.data.string), "plop" ) == 0 || strcmp( SC_StringToChar(value2.data.string), "coin" ) == 0 );

  assert( SC_ArrayGet( array, 1, & value2 ) );
  assert( value2.type == TYPE_STRING );
  assert( strcmp( SC_StringToChar(value2.data.string), "plop" ) == 0 || strcmp( SC_StringToChar(value2.data.string), "coin" ) == 0 );
  printf("\t  ... ok\n");

  printf("\t5: Delete values\n");
  SC_HashDelete( hash, "plop" );
  assert( SC_HashGet( hash, "plop", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );

  SC_HashDelete( hash, "coin" );
  assert( hash->size == 0 );
  assert( SC_HashGet( hash, "coin", & value1 ) == 0 );
  assert( value1.type == TYPE_UNDEF );
  printf("\t  ... ok\n");

  printf("\t6: Buffer resizing\n");
  for( i = 0; i < 35; i++ )
  {
    sprintf(cstr, "plop%d", i);
    value1.data.integer = 1337;
    SC_HashSet( hash, cstr, & value1 );
  }
  assert( hash->size == 35 );
  printf("\t  ... ok\n");

  printf("\t7: Clear hash\n");
  SC_HashClear(hash);
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

