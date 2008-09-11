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

static PyObject *PyScHash_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyScHash *self;

  self = (PyScHash *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->hash = NULL;
  }

  return (PyObject *)self;
}


static int PyScHash_init(PyScHash *self, PyObject *args, PyObject *kwds)
{
  PyObject *arg = NULL;
  
  static char *kwlist[] = {"dict", 0};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:dict", kwlist, &arg))
    return -1;
  
  self->hash = SC_HashNew();
  SC_HashGCInc(self->hash);
  
  if(arg)
  {
    PyObject *key, *value;
    scDataTypeValue_t val;
    char *keystring;
    Py_ssize_t pos = 0;

    if (!PyDict_Check(arg)) {
      return -1;
    }
    
    while (PyDict_Next(arg, &pos, &key, &value)) {
      if(!PyString_Check(key))
      {
        PyErr_SetString(PyExc_TypeError, "sc_hash's keys must be strings");
        return -1;
      }
      keystring = PyString_AsString(key);
      convert_to_value( value, &val, TYPE_ANY );
      SC_HashSet(self->hash, keystring, &val);
    }
  }

  return 0;
}

static void PyScHash_dealloc(PyScHash* self)
{
  SC_HashGCDec(self->hash);
}

//static int PyScHash_print(PyScHash *self, FILE *fp, int flags)
//{
//}
//
//static PyObject *PyScHash_repr(PyScHash *self)
//{
//}

/* Set a key error with the specified argument, wrapping it in a
 * tuple automatically so that tuple keys are not unpacked as the
 * exception arguments. */ //From pythons dictobject.c
static void
set_key_error(PyObject *arg)
{
  PyObject *tup;
  tup = PyTuple_Pack(1, arg);
  if (!tup)
    return; /* caller will expect error to be set anyway */
  PyErr_SetObject(PyExc_KeyError, tup);
  Py_DECREF(tup);
}

static PyObject *PyScHash_subscript(PyScHash *self, PyObject *key)
{
  scDataTypeValue_t value;
  char              *keystring;
  
  if(!PyString_Check(key)) 
  {
    PyErr_SetString(PyExc_TypeError, "sc_hash's keys must be strings");
    return NULL;
  }
  
  keystring = PyString_AsString( key );
  if(!keystring)
  {
    return NULL;
  }
  
  if(!SC_HashGet(self->hash, keystring, &value))
  {
    set_key_error(key);
    return NULL;
  }
  return convert_from_value(&value);
}

static int PyScHash_DelItem(PyScHash *self, PyObject *key)
{
  char              *keystring;
  
  if(!PyString_Check(key)) 
  {
    PyErr_SetString(PyExc_TypeError, "sc_hash's keys must be strings");
    return -1;
  }
  
  keystring = PyString_AsString( key );
  if(!keystring)
  {
    return -1;
  }
  if(!SC_HashDelete(self->hash, keystring))
  {
    set_key_error(key);
    return -1;
  }
  return 0;
}

static int PyScHash_SetItem(PyScHash *self, PyObject *key, PyObject *value)
{
  scDataTypeValue_t scValue;
  char              *keystring;
  
  if(!PyString_Check(key)) 
  {
    PyErr_SetString(PyExc_TypeError, "sc_hash's keys must be strings");
    return -1;
  }
  
  keystring = PyString_AsString( key );
  if(!keystring)
  {
    return -1;
  }
  if(!convert_to_value(value, &scValue, TYPE_ANY)) 
  {
    PyErr_SetString(PyExc_TypeError, "cannot convert to script type");
    return -1;
  }
  SC_HashSet( self->hash, keystring, &scValue);
  return 0;
}

static int PyScHash_ass_sub (PyScHash *self, PyObject *key, PyObject *value)
{
  if (value == NULL)
    return PyScHash_DelItem(self, key);
  else
    return PyScHash_SetItem(self, key, value);
}

static Py_ssize_t PyScHash_length(PyScHash *self)
{
  return self->hash->size;
}

static PySequenceMethods PyScHash_as_sequence = {
  0,                  /* sq_length */
  0,                  /* sq_concat */
  0,                  /* sq_repeat */
  0,                  /* sq_item */
  0,                  /* sq_slice */
  0,                  /* sq_ass_item */
  0,                  /* sq_ass_slice */
//  PyScHash_Contains,  /* sq_contains */
  0,                  /* sq_contains */
  0,                  /* sq_inplace_concat */
  0,                  /* sq_inplace_repeat */
};

static PyMappingMethods PyScHash_as_mapping = {
  (lenfunc)PyScHash_length,        /*mp_length*/
  (binaryfunc)PyScHash_subscript,  /*mp_subscript*/
  (objobjargproc)PyScHash_ass_sub, /*mp_ass_subscript*/
};

PyTypeObject PyScHash_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                             /* ob_size */
  "python.ScHash",               /* tp_name */
  sizeof(PyScHash),              /* tp_basicsize */
  0,                             /* tp_itemsize */
  (destructor)PyScHash_dealloc,  /* tp_dealloc */
//  (printfunc)PyScHash_print,    /* tp_print */
  0,                             /* tp_print */
  0,                             /* tp_getattr */
  0,                             /* tp_setattr */
  0,                             /* tp_compare */
//  (reprfunc)PyScHash_repr,      /* tp_repr */
  0,                             /* tp_repr */
  0,                             /* tp_as_number */
  &PyScHash_as_sequence,         /* tp_as_sequence */
  &PyScHash_as_mapping,          /* tp_as_mapping */
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
  (initproc)PyScHash_init,       /* tp_init */
  0,                             /* tp_alloc */
  PyScHash_new,                  /* tp_new */
};

PyScHash *PyScHashFrom_ScHash( scDataTypeHash_t *hash)
{
  PyScHash *pyScHash;
  pyScHash = (PyScHash*)PyType_GenericNew( &PyScHash_Type, NULL, NULL);
  
  SC_HashGCInc(hash);
  pyScHash->hash = hash;
  
  return pyScHash;
}
#endif
