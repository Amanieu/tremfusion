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
#include "../script/sc_public.h"
#include "../script/sc_python.h"

static int PyScArray_init(PyScArray *self, PyObject *args, PyObject *kwds)
{
  // TODO: Allow passing a sequence of items to args to initialize the array with
  self->array = SC_ArrayNew();
  SC_ArrayGCInc(self->array);
  return 0;
}

static void PyScArray_dealloc(PyScArray* self)
{
  SC_ArrayGCDec(self->array);
}

static PyObject *PyScArray_get_item(PyScArray *self, Py_ssize_t index)
{
  scDataTypeValue_t value;
  
  if(!SC_ArrayGet(self->array, index, &value))
  {
    PyErr_SetString(PyExc_IndexError, "scArray index out of range");
    return NULL;
  }
  return convert_from_value(&value);
}

static int PyScArray_ass_item (PyScArray *self, Py_ssize_t index, PyObject *value)
{
  scDataTypeValue_t scValue;
  if(!convert_to_value(value, &scValue, TYPE_ANY)) 
  {
    PyErr_SetString(PyExc_TypeError, "cannot convert to script type");
  }
  SC_ArraySet( self->array, index, &scValue);
  return 0;
}

static Py_ssize_t PyScArray_length(PyScArray *self)
{
  return self->array->size;
}

static PySequenceMethods PyScArray_as_sequence = {
  (lenfunc)PyScArray_length,                 /* sq_length */
//  (binaryfunc)concat,                        /* sq_concat */
  0,                                         /* sq_concat */
//  (ssizeargfunc)repeat,                      /* sq_repeat */
  0,                                         /* sq_repeat */
  (ssizeargfunc)PyScArray_get_item,          /* sq_item */
//  (ssizessizeargfunc)get_slice,              /* sq_slice */
  0,                                         /* sq_slice */
  (ssizeobjargproc)PyScArray_ass_item,       /* sq_ass_item */
//  (ssizessizeobjargproc)PyScArray_ass_slice, /* sq_ass_slice */
  0,                                         /* sq_ass_slice */
//  (objobjproc)contains,                      /* sq_contains */
  0,                                         /* sq_contains */
//  (binaryfunc)inplace_concat,                /* sq_inplace_concat */
  0,                                         /* sq_inplace_concat */
//  (ssizeargfunc)inplace_repeat,              /* sq_inplace_repeat */
  0,                                         /* sq_inplace_repeat */
};

PyTypeObject PyScArray_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                             /* ob_size */
  "python.ScArray",              /* tp_name */
  sizeof(PyScArray),             /* tp_basicsize */
  0,                             /* tp_itemsize */
  (destructor)PyScArray_dealloc, /* tp_dealloc */
  0,                             /* tp_print */
  0,                             /* tp_getattr */
  0,                             /* tp_setattr */
  0,                             /* tp_compare */
  0,                             /* tp_repr */
  0,                             /* tp_as_number */
  &PyScArray_as_sequence,        /* tp_as_sequence */
  0,                             /* tp_as_mapping */
  0,                             /* tp_hash */
  0,                             /* tp_call */
  0,                             /* tp_str */
  0,                             /* tp_getattro */
  0,                             /* tp_setattro */
  0,                             /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT |
    Py_TPFLAGS_BASETYPE,         /* tp_flags */
  0,                             /* tp_doc */
  0,                             /* tp_traverse */
  0,                             /* tp_clear */
  0,                             /* tp_richcompare */
  0,                             /* tp_weaklistoffset */
  0,                             /* tp_iter */
  0,                             /* tp_iternext */
  0,                             /* tp_methods */
  0,                             /* tp_members */
  0,                             /* tp_getset */
  0,                             /* tp_base */
  0,                             /* tp_dict */
  0,                             /* tp_descr_get */
  0,                             /* tp_descr_set */
  0,                             /* tp_dictoffset */
  (initproc)PyScArray_init,      /* tp_init */
  0,                             /* tp_alloc */
  0,                             /* tp_new */
};

PyScArray *PyScArrayFrom_ScArray( scDataTypeArray_t *array)
{
  PyScArray *pyScArray;
  pyScArray = (PyScArray*)PyType_GenericNew( &PyScArray_Type, NULL, NULL);
  
  SC_ArrayGCInc(array);
  pyScArray->array = array;
  
  return pyScArray;
}
#endif
