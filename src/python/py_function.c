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
  SC_FunctionGCDec(self->function);
  return 0;
}
// Deallocation
static void PyFunction_dealloc(PyFunction* self) {
  PyFunction_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}
// 
PyObject *PyFunction_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyFunction *self;

  self = (PyFunction *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->function = NULL;
  }
  
  return (PyObject *)self;
}

int PyFunction_init(PyFunction *self, scDataTypeFunction_t *function)
{
  SC_FunctionGCInc( function );
  self->function = function;
  return 0;
}

static PyObject *Call( PyFunction* self, PyObject* pArgs, PyObject* kArgs )
{
  int i, argcount;
  scDataTypeValue_t ret;
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  argcount = 0;
  
  argcount = PyTuple_Size( pArgs );
  
  for( i=0; i < argcount; i++)
  {
    convert_to_value( PyTuple_GetItem( pArgs, i ), &args[i], self->function->argument[i] );
    if (args[i].type != self->function->argument[i] )
    {
      PyErr_SetNone(PyExc_TypeError);
      return NULL;
    }
  }
  args[argcount].type = TYPE_UNDEF;
  
  SC_RunFunction( self->function, args, &ret );
  
  
  
  Com_Printf("HII\n");
  return convert_from_value(&ret);
}

static PyMemberDef PyFunction_members[] = {
  {NULL}  /* Sentinel */
};

static PyMethodDef PyFunction_methods[] = {
//    {"__call__",    Call,          METH_VARARGS, "" },
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
