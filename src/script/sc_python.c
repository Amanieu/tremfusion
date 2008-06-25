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

#ifdef USE_PYTHON

#include "sc_script.h"
#include "../python/python_local.h"

static scDataTypeValue_t *convert_to_sc_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type )
{
//  int ltype = lua_type(L, -1);
//  switch(ltype)
//  {
//    case LUA_TNIL:
//      value->type = TYPE_UNDEF;
//      lua_pop(L, 1);
//      break;
//
//    case LUA_TNUMBER:
//      if(type == TYPE_FLOAT)
//      {
//        value->type = TYPE_FLOAT;
//        value->data.floating = lua_tonumber(L, -1);
//        lua_pop(L, 1);
//      }
//      else
//      {
//        value->type = TYPE_INTEGER;
//        value->data.integer = lua_tointeger(L, -1);
//        lua_pop(L, 1);
//      }
//      break;
//
//    // TODO: add boolean datatype
//    /*case LUA_TBOOLEAN:
//      value->type = TYPE_BOOLEAN;
//      value->data.boolean = lua_toboolean(L, -1);
//      lua_pop(L, 1);
//      break;*/
//
//    case LUA_TSTRING:
//      value->type = TYPE_STRING;
//      pop_string(L, &value->data.string);
//      break;
//
//    case LUA_TTABLE:
//      if(type == TYPE_ANY)
//      {
//        int isArray = pop_hash(L, &value->data.hash);
//        // TODO: implement SC_HashToArray function
//        /*if(isArray)
//        {
//          scDataTypeHash_t *hash = value->data.hash;
//          value->type = TYPE_ARRAY;
//          SC_HashToArray(hash, &value->data.array);
//        }
//        else*/
//          value->type = TYPE_HASH;
//      }
//      if(type == TYPE_ARRAY)
//      {
//        value->type = TYPE_ARRAY;
//        pop_array(L, &value->data.array);
//      }
//      else if(type == TYPE_HASH)
//      {
//        value->type = TYPE_HASH;
//        pop_hash(L, &value->data.hash);
//      }
//      else
//      {
//        //TODO: Error
//      }
//      break;
//
//    case LUA_TFUNCTION:
//      value->type = TYPE_FUNCTION;
//      pop_function(L, value->data.function);
//      break;
//
//    default:
//      // TODO: Error
//      break;
//  }
  value->type = TYPE_UNDEF;
}

static PyObject *convert_from_sc_value( scDataTypeValue_t *value )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      return Py_BuildValue(""); // Python None object
      break;
    case TYPE_INTEGER:
      return Py_BuildValue("i", value->data.integer ); // Python int or long type
      break;
    case TYPE_FLOAT:
      return Py_BuildValue("f", value->data.floating ); // Python float type
      break;
    case TYPE_STRING:
      return Py_BuildValue("s", & value->data.string->data ); // Python str type
      break;
//    case TYPE_ARRAY:
//      push_array( L, value->data.array );
//      break;
//    case TYPE_HASH:
//      push_hash( L, value->data.hash );
//      break;
//    case TYPE_NAMESPACE:
//      push_hash( L, (scDataTypeHash_t*) value->data.namespace );
//      break;
//    case TYPE_FUNCTION:
//      push_function( L, value->data.function );
//      break;
    default:
      return Py_BuildValue(""); // Python None object
      break;
  }
}

void SC_Python_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  int narg;
  scDataType_t *dt;
  scDataTypeValue_t *value;
//  PyObject *pyvalue;
  PyObject *ArgsTuple, *ReturnValue;

  narg = 0;
  dt = (scDataType_t*) func->argument;
  value = args;
  
  while( *dt != TYPE_UNDEF )
  {
    narg++;
  }
  
  ArgsTuple = PyTuple_New( narg );
  narg = 0;
  while( *dt != TYPE_UNDEF )
  {
    PyTuple_SetItem(ArgsTuple, narg, convert_from_sc_value( args ) );

    dt++;
    value++;
    narg++;
  }
  // do the call
  ReturnValue =PyObject_CallObject( (PyObject *)func->data.pyfunc, ArgsTuple); // do the call
  Py_DECREF(ArgsTuple);
  convert_to_sc_value(ReturnValue, ret, func->return_type);
}

#endif