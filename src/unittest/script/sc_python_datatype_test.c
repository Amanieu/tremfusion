/*
===========================================================================
Copyright (C) 2008 John Black

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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <Python.h>

#include "../../script/sc_script.h"
#include "../../script/sc_python.h"

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

void print_pyobject ( PyObject *object )
{
  PyObject *repr;
  repr = PyObject_Repr( object );
  if (!repr)
    printf("NULL\n");
  printf("%s\n", PyString_AsString( repr ));
  
  Py_DECREF( repr );
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

int value_equals( PyObject *pyvalue, scDataTypeValue_t *value)
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      return pyvalue == Py_None;
    case TYPE_INTEGER:
      if (!PyInt_Check( pyvalue ) )
        return 0;
      return value->data.integer ==  PyInt_AsLong( pyvalue );
    case TYPE_FLOAT:
      if (!PyFloat_Check( pyvalue ) )
        return 0;
      return value->data.floating ==  PyFloat_AsDouble( pyvalue );
    case TYPE_STRING:
      if (!PyString_Check( pyvalue ) )
        return 0;
     return strcmp( PyString_AsString( pyvalue ), & value->data.string->data ) == 0;
//
//    case TYPE_ARRAY:
//      print_array( value->data.array, tab );
//      break;
//
//    case TYPE_HASH:
//      print_hash( value->data.hash, tab );
//      break;
//
//    case TYPE_NAMESPACE:
//      print_namespace( value->data.namespace, tab );
//      break;

    default:
      return -1;
  }
}

void test_basic( void )
{
  scDataTypeString_t *s1;
  PyObject *temp, *repr;
  scDataTypeValue_t *value;

  printf("A\\ Test Basic Conversion\n");
  printf("\t1: String coversion\n");
  SC_StringNewFromChar( &s1, HELLO_WORLD );
  value = BG_Alloc( sizeof( scDataTypeValue_t ));
  value->type = TYPE_STRING;
  value->data.string = s1;
  
  temp = convert_from_sc_value( value );
//  print_pyobject( temp );
  assert( value_equals( temp, value ) );
  
  Py_DECREF( temp );
  printf("\t  ... ok\n");
  SC_ValueFree( value );
  printf("\t2: Interger coversion\n");
  value->type = TYPE_INTEGER;
  value->data.integer = 1337;
  temp = convert_from_sc_value( value );
  assert( value_equals( temp, value ) );
  printf("\t  ... ok\n");
  Py_DECREF( temp );
  SC_ValueFree( value );
  printf("\t2: Float coversion\n");
  value->type = TYPE_FLOAT;
  value->data.floating = 1337.0;
  temp = convert_from_sc_value( value );
  assert( value_equals( temp, value ) );
  printf("\t  ... ok\n");
  Py_DECREF( temp );
  printf("  ... Test Basic Conversion: ok\n");
}

void test_advanced( void )
{
  scDataTypeArray_t *array;
  scDataTypeHash_t *hash;
  PyObject *temp, *repr;
  scDataTypeValue_t value1, hashvalue, arrayvalue;
  int i;

  printf("B\\ Test Advanced Conversion\n");
  printf("\t1: Array conversion\n");
  SC_ArrayNew( &array );
  
  value1.type = TYPE_STRING;
  SC_StringNewFromChar( & value1.data.string, HELLO_WORLD );
  SC_ArraySet( &array, 4, & value1 );

  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  SC_ArraySet( &array, 30, & value1 );

  value1.type = TYPE_FLOAT;
  value1.data.floating = 43.7;
  SC_ArraySet( &array, 0, & value1 );
  
  arrayvalue.type = TYPE_ARRAY;
  arrayvalue.data.array = array;
  
  temp = convert_from_sc_value( &arrayvalue );
  
  assert(PyList_Check(temp));
  //TODO: Proper testing
//  assert( value_equals( PyList_GetItem(temp, 0 ), &value1 ) );
//  SC_ArrayGet( array, 0, & value2 )
//  assert(  );
  
//  print_pyobject( temp ); 
  printf("\t  ... ok\n");
  
  printf("\t2: Hash conversion\n");
  SC_HashNew( &hash );

  SC_HashGet( hash, "plop", & value1 );

  SC_HashGet( hash, "", & value1 );

  value1.type = TYPE_STRING;
  SC_StringNewFromChar( & value1.data.string, HELLO_WORLD );
  SC_HashSet( &hash, "plop", & value1 );

  value1.type = TYPE_INTEGER;
  value1.data.integer = 1337;
  SC_HashSet( &hash, "dfad", & value1 );

  value1.type = TYPE_FLOAT;
  value1.data.floating = 43.7;
  SC_HashSet( &hash, "plop", & value1 );
  
  hashvalue.type = TYPE_HASH;
  hashvalue.data.hash = hash;
  
  temp = convert_from_sc_value( &hashvalue );
  
  assert(PyDict_Check(temp));
  
  print_pyobject( temp ); 
  
  printf("\t  ... ok\n");
  
  
  printf("  ... Test Advanced Conversion: ok\n");
}

int main ()
{
  Py_Initialize();
  PyRun_SimpleString("import os, sys, time"); // Import some basic modules
  PyRun_SimpleString("print \"Python version: \" + sys.version.replace(\"\\n\", \"\\nBuilt with: \")"); // Make sys.version prettier and print it
  BG_InitMemory( );
  test_basic();
  test_advanced();
  Py_Finalize();
}
