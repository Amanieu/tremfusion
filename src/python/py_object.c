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

// Deletion
static int PyScObject_clear(PyScObject *self) {
  return 0;
}
// Deallocation
static void PyScObject_dealloc(PyScObject* self) {
  PyScObject_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}
// 
PyObject *PyScObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyScObject *self;

  self = (PyScObject *)type->tp_alloc(type, 0);
  
  return (PyObject *)self;
}

int PyScObject_init(PyScObject *self, scObjectType_t *type, scDataTypeValue_t *args)
{
  self->type = type;
  
  self->instance = type->init( type, args);
  
  return 0;
}

//static PyObject *Call( PyScObject* self, PyObject* pArgs, PyObject* kArgs )
//{
//  int i, argcount;
//  scDataTypeValue_t ret;
//  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
//  argcount = 0;
//  
//  argcount = PyTuple_Size( pArgs );
//  
//  for( i=0; i < argcount; i++)
//  {
//    convert_to_value( PyTuple_GetItem( pArgs, i ), &args[i], self->function->argument[i] );
//    if (self->function->argument[i] !=  TYPE_ANY && args[i].type != self->function->argument[i])
//    {
//      PyErr_SetNone(PyExc_TypeError);
//      return NULL;
//    }
//  }
//  args[argcount].type = TYPE_UNDEF;PyErr_SetNone
//  
//  return convert_from_value(&ret);
//}

static PyMemberDef PyScObject_members[] = {
  {NULL}  /* Sentinel */
};

static PyObject *getattr(PyScObject *self, char *name)
{
  scObjectMember_t *member;
  scDataTypeValue_t *value;
  
  member = self->type->members;
  
  for (; member->name != NULL; member++) {
    if (strcmp(member->name, name) == 0){
      value = member->get( self->instance, member->closure );
      return convert_from_value( value );
    }
  }
  PyErr_SetNone( PyExc_AttributeError );
  return NULL;
}

static int setattr(PyScObject *self, char *name, PyObject *pyvalue)
{
  scObjectMember_t *member;
  scDataTypeValue_t *value;
  
  member = self->type->members;
  
  for (; member->name != NULL; member++) {
    if (strcmp(member->name, name) == 0){
      convert_to_value( pyvalue, value, member->type );
      member->set( self->instance, value, member->closure );
      return 0;
    }
  }
  PyErr_SetNone( PyExc_AttributeError );
  return -1;
}

static PyMethodDef PyScObject_methods[] = {
    {NULL}  /* Sentinel */
};

PyTypeObject PyScObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "script.PyScObject",             /*tp_name*/
    sizeof(PyScObject),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyScObject_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    (getattrfunc)getattr,      /*tp_getattr*/
    (setattrfunc)setattr,      /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    PyScObject_methods,        /* tp_methods */
    PyScObject_members,        /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyScObject_init, /* tp_init */
    0,                         /* tp_alloc */
    PyScObject_new,                /* tp_new */
    0,                         /*tp_scobjtype*/
};

//static PyObject *get_wrapper(PyObject *self, void *closure)
//{
//  return Py_BuildValue("");
//}
//
//static int set_wrapper(PyObject *self, PyObject *value, void *closure)
//{
//  return 0;
//}

/* Create a copy of PyScObjectType and make it work somehow */
void SC_Python_InitObectType( scObjectType_t * type )
{
}

#endif
