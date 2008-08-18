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
  PyObject *arg = NULL;
  scDataTypeValue_t val;
  int index;
  
  static char *kwlist[] = {"sequence", 0};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:list", kwlist, &arg))
    return -1;
  
  self->array = SC_ArrayNew();
  SC_ArrayGCInc(self->array);
  
  PyObject *iterator = PyObject_GetIter(args);
  PyObject *item;

  if (iterator == NULL) {
    return -1;
  }

  index = 0;
  while ( (item = PyIter_Next(iterator) ) ) {
    convert_to_value( item, &val, TYPE_ANY );
    SC_ArraySet(self->array, index, &val);
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

  return 0;
}

static void PyScArray_dealloc(PyScArray* self)
{
  SC_ArrayGCDec(self->array);
}

static int PyScArray_print(PyScArray *self, FILE *fp, int flags)
{
  int rc;
  Py_ssize_t i;

  rc = Py_ReprEnter((PyObject*)self);
  if (rc != 0) {
    if (rc < 0)
      return rc;
    fprintf(fp, "[...]");
    return 0;
  }
  fprintf(fp, "[");
  for (i = 0; i < self->array->size; i++) {
    scDataTypeValue_t value;
    PyObject *pyvalue;
    if (i > 0)
      fprintf(fp, ", ");
    if(!SC_ArrayGet(self->array, i, &value))
      goto Error;
    pyvalue = convert_from_value(&value);
    if (pyvalue == NULL)
      goto Error;
    if (PyObject_Print(pyvalue, fp, 0) != 0)
      goto Error;
    Py_DECREF(pyvalue);
  }
  fprintf(fp, "]");
  Py_ReprLeave((PyObject *)self);
  return 0;
  Error:
  Py_ReprLeave((PyObject *)self);
  return -1;
}

static PyObject *PyScArray_repr(PyScArray *self)
{
  // Code adapted from pythons list object's list_repr (listobject.c)
  Py_ssize_t i;
  PyObject *s, *temp;
  PyObject *pieces = NULL, *result = NULL;

  i = Py_ReprEnter((PyObject*)self);
  if (i != 0) {
    return i > 0 ? PyString_FromString("[...]") : NULL;
  }

  if (self->array->size == 0) {
    result = PyString_FromString("[]");
    goto Done;
  }

  pieces = PyList_New(0);
  if (pieces == NULL)
    goto Done;

  /* Do repr() on each element.  Note that this may mutate the list,
     so must refetch the list size on each iteration. */
  for (i = 0; i < self->array->size; ++i) {
    int status;
    scDataTypeValue_t value;
    PyObject *pyvalue;
    if(!SC_ArrayGet(self->array, i, &value))
      goto Done;
    pyvalue = convert_from_value(&value);
    if (pyvalue == NULL)
      goto Done;
    s = PyObject_Repr(pyvalue);
    Py_DECREF(pyvalue);
    if (s == NULL)
      goto Done;
    status = PyList_Append(pieces, s);
    Py_DECREF(s);  /* append created a new ref */
    if (status < 0)
      goto Done;
  }

  /* Add "[]" decorations to the first and last items. */
  assert(PyList_GET_SIZE(pieces) > 0);
  s = PyString_FromString("[");
  if (s == NULL)
    goto Done;
  temp = PyList_GET_ITEM(pieces, 0);
  PyString_ConcatAndDel(&s, temp);
  PyList_SET_ITEM(pieces, 0, s);
  if (s == NULL)
    goto Done;

  s = PyString_FromString("]");
  if (s == NULL)
    goto Done;
  temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
  PyString_ConcatAndDel(&temp, s);
  PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
  if (temp == NULL)
    goto Done;

  /* Paste them all together with ", " between. */
  s = PyString_FromString(", ");
  if (s == NULL)
    goto Done;
  result = _PyString_Join(s, pieces);
  Py_DECREF(s);

Done:
  Py_XDECREF(pieces);
  Py_ReprLeave((PyObject *)self);
  return result;
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
    return -1;
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
  (printfunc)PyScArray_print,    /* tp_print */
  0,                             /* tp_getattr */
  0,                             /* tp_setattr */
  0,                             /* tp_compare */
  (reprfunc)PyScArray_repr,      /* tp_repr */
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
