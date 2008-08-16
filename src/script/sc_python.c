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

PyObject *mainModule;
PyObject *mainDict;

static scDataTypeHash_t* convert_to_hash( PyObject *hash_obj )
{
  scDataTypeValue_t val;
  scDataTypeHash_t *hash = SC_HashNew();
  PyObject *key, *value, *keystr;
  Py_ssize_t pos = 0;

  while (PyDict_Next(hash_obj, &pos, &key, &value)) {
      convert_to_value( value, &val, TYPE_ANY );
      keystr = PyObject_Str( key );
      SC_HashSet(hash, PyString_AsString( keystr ), &val);
      Py_DECREF(keystr);
  }
  return hash;
}

static scDataTypeArray_t* convert_to_array( PyObject *array_obj )
{
  scDataTypeValue_t val;
  int index;
  scDataTypeArray_t *array = SC_ArrayNew();
  PyObject *iterator = PyObject_GetIter(array_obj);
  PyObject *item;

  if (iterator == NULL) {
    return array;
  }

  index = 0;
  while ( (item = PyIter_Next(iterator) ) ) {
    convert_to_value( item, &val, TYPE_ANY );
    SC_ArraySet(array, index, &val);
    Py_DECREF(item);
    index++;
  }

  Py_DECREF(iterator);

  if (PyErr_Occurred()) {
      /* propagate error */
  }
  else {
      /* continue doing useful work */
  }
  return array;
}
static scDataTypeString_t* convert_to_string( PyObject *str_obj  )
{
  const char *pystr = PyString_AsString( str_obj );
  scDataTypeString_t *string = SC_StringNewFromChar(pystr);
  return string;
}

static scDataTypeFunction_t* convert_to_function( PyObject *function_obj  )
{
  scDataTypeFunction_t *function = SC_FunctionNew();
  function->langage = LANGAGE_PYTHON;
  Py_INCREF(function_obj);
  function->data.pyfunc= function_obj;
  return function;
}

/* Convert a python object into a script data value */
void convert_to_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type )
{
  value->gc.count = 0;
  if (pyvalue == Py_None){
    value->type = TYPE_UNDEF;
  }
  else if ( PyInt_Check(pyvalue) )
  {
    value->type = TYPE_INTEGER;
    value->data.integer = PyInt_AsLong( pyvalue );
  }
  else if ( PyFloat_Check(pyvalue) )
  {
    value->type = TYPE_FLOAT;
    value->data.floating = PyFloat_AsDouble( pyvalue );
  }
  else if (  PyBool_Check(pyvalue) )
  {
    value->type = TYPE_BOOLEAN;
    value->data.boolean = ( pyvalue == Py_True) ? qtrue : qfalse;
  }
  else if (  PyString_Check(pyvalue) )
  {
    value->type = TYPE_STRING;
    value->data.string = convert_to_string( pyvalue );
  }
  else if ( PyDict_Check(pyvalue) )
  {
    value->type = TYPE_HASH;
    value->data.hash = convert_to_hash( pyvalue );
  }
  else if ( PyList_Check(pyvalue) || PyTuple_Check( pyvalue ) )
  {
    value->type = TYPE_ARRAY;
    value->data.array = convert_to_array( pyvalue );
  }
  else if ( PyCallable_Check(pyvalue) )
  {
    value->type = TYPE_FUNCTION;
    value->data.function = convert_to_function( pyvalue );
  }
  else if ( PyType_IsSubtype(pyvalue->ob_type, &PyScBaseObject_Type) )
  {
    PyScBaseObject *py_scobject;
    py_scobject = (PyScBaseObject*)pyvalue;
    value->type = TYPE_OBJECT;
    value->data.object = py_scobject->sc_object;
  }
  else
  {
#ifdef UNITTEST
    printf("convert_to_value type fallthrough\n");
    print_pyobject( pyvalue->ob_type );
#endif
    value->type = TYPE_UNDEF;
  }
}

static PyObject *convert_from_array( scDataTypeArray_t *array )
{
  int i;
  PyObject *list;
  scDataTypeValue_t value;

  list = PyList_New( array->size ); // Creat a new python list
  for( i = 0; i < array->size; i++ )
  {
    SC_ArrayGet(array, i, &value);
    PyList_SetItem( list, i, convert_from_value( &value ) );
  }
  return list;
}

static PyObject *convert_from_hash( scDataTypeHash_t *hash )
{
  int i;
  PyObject *dict, *temp;

  dict = PyDict_New();
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    temp = convert_from_value( &hash->data[i].value );
    PyDict_SetItemString( dict, SC_StringToChar(&hash->data[i].key), temp);
  }
  return dict;
}

static PyObject *convert_from_function( scDataTypeFunction_t *function )
{
#ifndef UNITTEST
  PyObject *temp;
  temp = PyFunction_new( &PyFunctionType, NULL, NULL );
  PyFunction_init( (PyFunction*)temp, function );
  if ( temp == NULL ) return Py_BuildValue("");
  return temp;
#else
  return Py_BuildValue("");
#endif
}

static PyObject *convert_from_object( scObject_t *sc_object )
{
  PyScBaseObject *py_object;
  py_object = (PyScBaseObject*)PyScObject_new( (ScPyTypeObject*)sc_object->class->python_type, NULL, NULL);
  
  py_object->sc_object = sc_object;
  
  return (PyObject*)py_object;
}

static PyObject *convert_from_class( scClass_t *class )
{
  return class->python_type;
}

/* Convert a script data value to a python object */
PyObject *convert_from_value( scDataTypeValue_t *value )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      return Py_BuildValue(""); // Python None object
    case TYPE_BOOLEAN:
      if (value->data.boolean)
        Py_RETURN_TRUE;
      else  // Py_RETURN_TRUE and Py_RETURN_FALSE are 
        Py_RETURN_FALSE;//  Macros for returning Py_True or Py_False, respectively
    case TYPE_INTEGER:
      return Py_BuildValue("i", value->data.integer ); // Python int or long type
    case TYPE_FLOAT:
      return Py_BuildValue("f", value->data.floating ); // Python float type
    case TYPE_STRING:
      return Py_BuildValue("s", value->data.string->data ); // Python str type
    case TYPE_ARRAY:
      return convert_from_array( value->data.array );
    case TYPE_HASH:
      return convert_from_hash( value->data.hash );
//    case TYPE_NAMESPACE:
//      push_hash( L, (scDataTypeHash_t*) value->data.namespace );
//      break;
    case TYPE_FUNCTION:
      return convert_from_function( value->data.function );
    case TYPE_OBJECT:
      return convert_from_object( value->data.object );
    case TYPE_CLASS:
      return convert_from_class( value->data.class );
    default:
#ifdef UNITTEST
      printf("convert_from_value type fallthrough %d \n", value->type);
#endif
      return Py_BuildValue(""); // Python None object
      break;
  }
}
#ifndef UNITTEST

static void loadconstants(void)
{
  scConstant_t *cst = sc_constants;
  float val;
  PyObject* module;
  module = PyImport_AddModule("constants");
  while(cst->name[0] != '\0')
  {
    switch(cst->type)
    {
//      case TYPE_BOOLEAN:
//        
//        lua_pushboolean(L, (int)cst->data);
//        PyModule_AddObject( mainModule, cst->name, (cst->data) ? );
//        lua_setglobal(L, );
//        break;
      case TYPE_INTEGER:
        PyModule_AddIntConstant(module, cst->name, (long)cst->data);
        break;
//      case TYPE_FLOAT: TODO: Floats are broken
//        memcpy(&val, &cst->data, sizeof(val));
//        PyModule_AddObject(module, cst->name, PyFloat_FromDouble(val));
//        break;
      case TYPE_STRING:
        PyModule_AddStringConstant(module, cst->name, (char*)cst->data);
        break;
      case TYPE_USERDATA:
        PyModule_AddObject(module, cst->name, PyCObject_FromVoidPtr(cst->data, NULL));
        break;
      default:
        // Error: type invalid
        break;
    }

    cst++;
  }
}

void SC_Python_Init( void )
{
//  scDataTypeValue_t *value;
  
  Com_Printf("Python initializing... ");
  Py_Initialize();
  // Make python threads work at all
//  PyEval_InitThreads( );
  
  trap_Cvar_Set( "py_initialized", "1" );
  trap_Cvar_Update( &py_initialized ); 
  
  mainModule = PyImport_AddModule("__main__"); // get __main__ ...
  mainDict = PyModule_GetDict( mainModule ); // ... so we can get its dict ..
  
  if (PyType_Ready(&PyFunctionType) < 0)
    return;
  if (PyType_Ready(&PyScMethod_Type) < 0)
    return;
  Py_INCREF(&PyScMethod_Type);
  
  loadconstants();
//  if (PyType_Ready(&PyScObject_Type) < 0)
//    return;
//  Py_INCREF(&EntityType);
  Com_Printf("done\n");
}

/*
=================
SC_Python_Shutdown
=================
*/
void SC_Python_Shutdown( void )
{
  Com_Printf("Python shutting down... ");
  Py_DECREF( mainModule );
  Py_DECREF( mainDict );

  trap_Cvar_Set( "py_initialized", "0" );
  Py_Finalize();
  Com_Printf("done\n");
}

static void update_module( scDataTypeString_t *module, scDataTypeValue_t *value )
{
  PyObject *pyModule;
  scDataTypeHash_t* hash =(scDataTypeHash_t*) value->data.namespace;
  char *keystring;
  
  pyModule = PyImport_AddModule( SC_StringToChar( module ) );
  if (!pyModule)
    return; //ERROR!
  int i;
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    keystring = (char*)SC_StringToChar(&hash->data[i].key);
    if( PyObject_HasAttrString(pyModule, keystring) )
      continue;
    PyModule_AddObject( pyModule, keystring, convert_from_value( &hash->data[i].value ));
  }
}

static void update_context( void )
{
  // TODO: make better updating system
  scDataTypeHash_t* hash = (scDataTypeHash_t*) namespace_root;
  char *keystring;
  int i;
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    if ( hash->data[i].value.type == TYPE_NAMESPACE )
    {
      update_module( &hash->data[i].key,  &hash->data[i].value );
    }
    else 
    {
      keystring = (char*)SC_StringToChar(&hash->data[i].key);
      PyModule_AddObject( mainModule, keystring, convert_from_value( &hash->data[i].value ));
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
    Com_Printf(va(S_COLOR_RED "Cannot load %s: file not found\n", filename));
    return qfalse;
  }

  if(len >= MAX_PYTHONFILE)
  {
    Com_Printf(S_COLOR_RED "Cannot load %s: file too large, file is %i, max allowed is %i\n", filename, len, MAX_PYTHONFILE);
    trap_FS_FCloseFile(f);
    return qfalse;
  }
#ifndef UNITTEST
  if (!sc_python.integer){
    Com_Printf(S_COLOR_RED "Cannot load %s: python disabled\n", filename);
    return qfalse;
  }
  
  if (!py_initialized.integer){
    Com_Printf(S_COLOR_RED "Cannot load %s: python not initilized\n", filename);
    return qfalse;
  }
#endif
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

#endif /*#ifndef UNITTEST*/

int SC_Python_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  PyObject *ArgsTuple, *ReturnValue;
  
#ifndef UNITTEST
  if (!sc_python.integer){
    Com_Printf(S_COLOR_RED "Cannot run function: python disabled\n");
    return -1;
  }
  
  if (!py_initialized.integer){
    Com_Printf(S_COLOR_RED "Cannot run function: python not initilized\n");
    return -1;
  }
#endif
  
  if (args)
  {
    scDataTypeValue_t *value;
    int index, narg;
    value = args;
    
    narg = 0;
    
    while ( value->type != TYPE_UNDEF && value->type <= TYPE_NAMESPACE )
    {
      narg++;
      value++;
    }
#ifdef UNITTEST
    printf("%d args\n", narg);
#endif
    ArgsTuple = PyTuple_New( narg );
    
    value = args;
    index = 0;
    while ( value->type != TYPE_UNDEF && value->type <= TYPE_NAMESPACE)
    {
      PyTuple_SetItem( ArgsTuple, index++, convert_from_value( value ) );
      narg++;
      value++;
    }
  }
  else
    ArgsTuple = PyTuple_New( 0 );
  ReturnValue = PyObject_CallObject( func->data.pyfunc, ArgsTuple); // do the call
  Py_DECREF(ArgsTuple);
  if (!ReturnValue)
  {
    PyErr_Print();
    return -1;
  }
  convert_to_value(ReturnValue, ret, TYPE_ANY);
  Py_DECREF(ReturnValue);

  return 0;
}


#endif
