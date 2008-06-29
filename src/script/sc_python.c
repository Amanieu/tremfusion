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

#define MAX_PYTHONFILE 32768

#include "sc_public.h"
#include "sc_python.h"
//#include "../python/python_local.h"

// TODO: move these to something like src/python/py_game.c
static PyMethodDef game_methods[] = {
 {NULL, NULL, 0, NULL}
};

PyObject *gamemodule;
PyObject *vec3d_module;
PyObject *vec3d;

/* 
 * We can't keep making new python module objects everytime we call 
 * update_context so we steal them from the main dict
 */
PyObject *mainModule;
PyObject *mainDict;

static void convert_to_sc_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type );

/* Convert a python object into a script data value */
void convert_to_sc_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type )
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

static PyObject *convert_from_array( scDataTypeArray_t *array )
{
  int i;
  PyObject *list;

  list = PyList_New( array->size ); // Creat a new python list
  for( i = 0; i < array->size; i++ )
  {
    PyList_SetItem( list, i, convert_from_sc_value( &array->data[i] ) );
  }
  return list;
}

static PyObject *convert_from_hash( scDataTypeHash_t *hash )
{
  int i;
  PyObject *dict, *temp;

  dict = PyDict_New();
  for( i = 0; i < hash->size; i++ )
  {
    temp = convert_from_sc_value( &hash->data[i].value );
    PyDict_SetItemString( dict, SC_StringToChar(&hash->data[i].key), temp);
  }
  return dict;
}

/* Convert a script data value to a python object */
PyObject *convert_from_sc_value( scDataTypeValue_t *value )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      return Py_BuildValue(""); // Python None object
    case TYPE_INTEGER:
      return Py_BuildValue("i", value->data.integer ); // Python int or long type
    case TYPE_FLOAT:
      return Py_BuildValue("f", value->data.floating ); // Python float type
    case TYPE_STRING:
      return Py_BuildValue("s", & value->data.string->data ); // Python str type
    case TYPE_ARRAY:
      return convert_from_array( value->data.array );
    case TYPE_HASH:
      return convert_from_hash( value->data.hash );
//    case TYPE_NAMESPACE:
//      push_hash( L, (scDataTypeHash_t*) value->data.namespace );
//      break;
//    case TYPE_FUNCTION:
//      push_function( L, value->data.function );
//      break;
    default:
#ifdef UNITTEST
      printf("convert_from_sc_value type fallthrough %d \n", value->type);
#endif
      return Py_BuildValue(""); // Python None object
      break;
  }
}
#ifndef UNITTEST

void SC_Python_Init( void )
{
  scDataTypeValue_t *value;
  
  G_Printf("------- Game Python Initialization -------\n");
  Py_Initialize();
  // Make python threads work at all
  PyEval_InitThreads( );
  
  mainModule = PyImport_AddModule("__main__"); // get __main__ ...
  mainDict = PyModule_GetDict( mainModule ); // ... so we can get its dict ...
//  
//  value = BG_Alloc( sizeof( scDataTypeValue_t ));
//  value->type = TYPE_INTEGER;
//  value->data.integer = 1337;
  
//  SC_NamespaceSet("game.test", value);
  PyImport_AddModule("game");
  gamemodule = Py_InitModule("game", game_methods);
  if (PyType_Ready(&EntityType) < 0)
    return;
  Py_INCREF(&EntityType);
  PyModule_AddObject(gamemodule, "Entity", (PyObject *)&EntityType);
  if (PyType_Ready(&EntityStateType) < 0)
    return;
  Py_INCREF(&EntityStateType);
  PyModule_AddObject(gamemodule, "EntityState", (PyObject *)&EntityStateType);
//  if (PyType_Ready(&Vec3dType) < 0)
//    return;
//  Py_INCREF(&Vec3dType);
//  PyModule_AddObject(gamemodule, "Vec3d", (PyObject *)&Vec3dType);
//  PyRun_SimpleString("sys.path.append(\"/home/john/tremulous/server/test_base/stfu-trem/python\")");
//  vec3d_module= PyImport_ImportModule("vec3d");
//  if (!vec3d_module){
//    Com_Printf("^1Cannot find vec3d.py\n");
//    vec3d = NULL;
//  } else {
//    vec3d = PyObject_GetAttrString(vec3d_module, "vec3d" );
//  }

  G_Printf("-----------------------------------\n");
}

/*
=================
SC_Python_Shutdown
=================
*/
void SC_Python_Shutdown( void )
{
  G_Printf("------- Game Python Finalization -------\n");
  Py_DECREF( mainModule );
  Py_DECREF( mainDict );

//  if (vec3d_module){ 
//    Py_DECREF( vec3d_module);
//    vec3d = NULL;
//  }
//  if (vec3d){
//    Py_DECREF( vec3d );
//    vec3d = NULL;
//  }
//  Py_DECREF( &EntityStateType );
//  Py_DECREF( &EntityType );
//  if (gamemodule){
//    Py_DECREF( gamemodule );
//    gamemodule = NULL;
//  }
  Py_Finalize();
  G_Printf("-----------------------------------\n");
}

static void update_module( scDataTypeString_t *module, scDataTypeValue_t *value )
{
  PyObject *pyModule;
  scDataTypeHash_t* hash =(scDataTypeHash_t*) value->data.namespace;
  
  pyModule = PyImport_AddModule( SC_StringToChar( module ) );
  if (!pyModule)
    return; //ERROR!
  int i;
  for( i = 0; i < hash->size; i++ )
  {
    PyModule_AddObject( pyModule, SC_StringToChar(&hash->data[i].key), convert_from_sc_value( &hash->data[i].value ));
  }
}

static void update_context( void )
{
  // TODO: make better updating system
  scDataTypeHash_t* hash = (scDataTypeHash_t*) namespace_root;
  int i;
  for( i = 0; i < hash->size; i++ )
  {
    if ( hash->data[i].value.type == TYPE_NAMESPACE )
    {
      update_module( &hash->data[i].key,  &hash->data[i].value );
    }
    else 
    {
      PyModule_AddObject( mainModule, SC_StringToChar(&hash->data[i].key), convert_from_sc_value( &hash->data[i].value ));
    }
  }
}

/*
=================
SC_Python_RunScript
=================
*/
qboolean SC_Python_RunScript( const char *filename )
{
  int             len;
  fileHandle_t    f;
  char            buf[MAX_PYTHONFILE];
  PyObject       *codeObject, *loc, *dum;

  Com_Printf("...loading '%s'\n", filename);

  len = trap_FS_FOpenFile(filename, &f, FS_READ);
  if(!f)
  {
    Com_Printf(va(S_COLOR_RED "file not found: %s\n", filename));
    return qfalse;
  }

  if(len >= MAX_PYTHONFILE)
  {
    Com_Printf(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i\n", filename, len, MAX_PYTHONFILE));
    trap_FS_FCloseFile(f);
    return qfalse;
  }

  trap_FS_Read(buf, len, f);
  buf[len] = 0;
  trap_FS_FCloseFile(f);

  update_context();
  loc = PyDict_New ();
  codeObject = Py_CompileString( buf, filename, Py_file_input );
  if (!codeObject){
    PyErr_Print( );
    return qfalse;
  }
  PyDict_SetItemString (mainDict, "__builtins__", PyEval_GetBuiltins ());
  dum = PyEval_EvalCode( (PyCodeObject*)codeObject, mainDict, loc );
  if (!dum){
    PyErr_Print( );
    return qfalse;
  }
  Py_XDECREF(loc);
  Py_XDECREF(dum);
  return qtrue;
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
  ReturnValue = PyObject_CallObject( func->data.pyfunc, ArgsTuple); // do the call
  Py_DECREF(ArgsTuple);
  convert_to_sc_value(ReturnValue, ret, func->return_type);
  Py_DECREF(ReturnValue);
}
#endif /*#ifndef UNITTEST*/
#endif
