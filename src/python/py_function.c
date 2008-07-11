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
#include "../game/g_local.h"
#include "../script/sc_public.h"
#include "../script/sc_python.h"

// Garbage collection
static int PyFunction_traverse(PyFunction *self, visitproc visit, void *arg) {
  return 0;
}
// Deletion
static int PyFunction_clear(PyFunction *self) {
  SC_FunctionGCDec(self->data.function);
  return 0;
}
// Deallocation
static void PyFunction_dealloc(PyFunction* self) {
  if (self->type == TYPE_FUNCTION) {
    PyFunction_clear(self);
    self->ob_type->tp_free((PyObject*)self);
  }
}
// 
PyObject *PyFunction_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyFunction *self;

  self = (PyFunction *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->type = TYPE_UNDEF;
    self->data.function = NULL;
//    self->function = NULL;
  }
  
  return (PyObject *)self;
}

int PyFunction_init(PyFunction *self, int type, void *closure)
{
  self->type = type;
  switch (type){
    case TYPE_FUNCTION:
      SC_FunctionGCInc( (scDataTypeFunction_t*)closure );
      self->data.function = (scDataTypeFunction_t*)closure;
      break;
    case TYPE_OBJECTTYPE:
      self->data.objecttype = (scObjectType_t*)closure;
      break;
    default:
      Com_Printf("invalid PyFunction_init type: %d", type);
      return -1;
  }
  
  return 0;
}

static PyObject *Call( PyFunction* self, PyObject* pArgs, PyObject* kArgs )
{
  int i, argcount;
  scDataTypeValue_t ret;
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  scDataType_t      arguments[ MAX_FUNCTION_ARGUMENTS + 1 ];
  argcount = 0;
  PyObject *temp;
  
  switch (self->type){
    case TYPE_FUNCTION:
      memcpy( arguments, self->data.function->argument, sizeof( arguments ) );
      break;
    case TYPE_OBJECTTYPE:
      memcpy( arguments, self->data.objecttype->initArguments, sizeof( arguments ) );
      break;
    default:
      PyErr_SetNone(PyExc_SystemError);
      return NULL;
  }
  
  argcount = PyTuple_Size( pArgs );
  
  for( i=0; i < argcount; i++)
  {
    convert_to_value( PyTuple_GetItem( pArgs, i ), &args[i], arguments[i] );
    if (arguments[i] !=  TYPE_ANY && args[i].type != arguments[i])
    {
      PyErr_SetNone(PyExc_TypeError);
      return NULL;
    }
  }
  args[argcount].type = TYPE_UNDEF;
  
  switch (self->type){
    case TYPE_FUNCTION:
      SC_RunFunction( self->data.function, args, &ret );
      return convert_from_value(&ret);
    case TYPE_OBJECTTYPE:
      temp = PyScObject_new( &PyScObject_Type, NULL, NULL );
      PyScObject_init( (PyScObject*)temp, self->data.objecttype, args);
      if ( temp == NULL ) return Py_BuildValue("");
      return temp;
    default:
      PyErr_SetNone(PyExc_SystemError);
      return NULL;
  }
  
}

static PyMemberDef PyFunction_members[] = {
  {NULL}  /* Sentinel */
};

static PyMethodDef PyFunction_methods[] = {
    {NULL}  /* Sentinel */
};

PyTypeObject PyFunctionType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "game.PyFunction",             /*tp_name*/
    sizeof(PyFunction),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyFunction_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    (ternaryfunc)Call,         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
//    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "",           /* tp_doc */
    (traverseproc)PyFunction_traverse, /* tp_traverse */
//    0,                         /* tp_traverse */
    (inquiry)PyFunction_clear,     /* tp_clear */
//    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    PyFunction_methods,        /* tp_methods */
    PyFunction_members,        /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    PyFunction_new,                /* tp_new */
};
#endif
