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
static int PyScMethod_clear(PyScMethod *self) {
  return 0;
}
// Deallocation
static void PyScMethod_dealloc(PyScMethod* self) {
  PyScMethod_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}
// 
PyObject *PyScMethod_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyScMethod *self;

  self = (PyScMethod *)type->tp_alloc(type, 0);
  
  return (PyObject *)self;
}

int PyScMethod_init(PyScMethod *self, PyObject *parent, scObjectInstance_t* instance, scObjectMethod_t* method)
{
  self->parent   = parent;
  self->instance = instance;
  self->method   = method;
  return 0;
}

static PyObject *PyScMethod_call( PyScMethod *self, PyObject* pArgs, PyObject* kArgs )
{
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  scDataType_t      arguments[ MAX_FUNCTION_ARGUMENTS + 1 ];
  scDataTypeValue_t *ret;
  PyObject          *pyret;
  int               i, argcount;
  
  memcpy( arguments, self->method->argument, sizeof( arguments ) );
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
  
  ret = self->method->method( self->instance, args, self->method->closure);
  
  pyret = convert_from_value( ret );
  BG_Free( ret );
  return pyret;
}

static PyObject *PyScMethod_repr( PyScMethod *self )
{
  return PyString_FromFormat("<built-in method %s of %s object at %p>",
             self->method->name,
             self->parent->ob_type->tp_name,
             self->parent);
}

PyTypeObject PyScMethod_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "script.PyScMethod", /*tp_name*/
    sizeof(PyScMethod),  /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyScMethod_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc)PyScMethod_repr, /*tp_repr*/ //built-in method link of Entity object at 0xacead060
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    (ternaryfunc)PyScMethod_call,  /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "",                        /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PyScMethod_init, /* tp_init */
    0,                         /* tp_alloc */
    (newfunc)PyScMethod_new,   /* tp_new */
    0,                         /*tp_scobjtype*/
};

static PyObject *New_PyMethod( PyObject *parent, scObjectInstance_t* instance, scObjectMethod_t* method )
{
  PyObject *pymethod;
  pymethod = PyScMethod_new( &PyScMethod_Type, NULL, NULL );
  PyScMethod_init( (PyScMethod*)pymethod, parent, instance, method );
  if ( pymethod == NULL )
  {
    Com_Printf("Error creating method \"%s\"\n", method->name);
    return Py_BuildValue("");
  }
  return pymethod;
}

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
PyObject *PyScObject_new(ScPyTypeObject *type, PyObject *args, PyObject *kwds)
{
  PyScObject *self;

  self = (PyScObject *)type->tp_alloc((PyTypeObject*)type, 0);
  
  self->type = type->tp_scobjtype;
  
  return (PyObject *)self;
}

int PyScObject_init(PyScObject *self, PyObject* pArgs, PyObject* kArgs)
{
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  scDataType_t      arguments[ MAX_FUNCTION_ARGUMENTS + 1 ];
  int               i, argcount;
  
  memcpy( arguments, self->type->initArguments, sizeof( arguments ) );
  argcount = PyTuple_Size( pArgs );
  
  for( i=0; i < argcount; i++)
  {
    convert_to_value( PyTuple_GetItem( pArgs, i ), &args[i], arguments[i] );
    if (arguments[i] !=  TYPE_ANY && args[i].type != arguments[i])
    {
      PyErr_SetNone(PyExc_TypeError);
      return -1;
    }
  }
  args[argcount].type = TYPE_UNDEF;
  
  self->instance = self->type->init( self->type, args);
  
  return 0;
}

static PyMemberDef PyScObject_members[] = {
  {NULL}  /* Sentinel */
};

static PyMethodDef PyScObject_methods[] = {
    {NULL}  /* Sentinel */
};

ScPyTypeObject PyScObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "script.PyScObject",             /*tp_name*/
    sizeof(PyScObject),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)PyScObject_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
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
    (newfunc)PyScObject_new,   /* tp_new */
    0,                         /*tp_scobjtype*/
};

static PyObject *get_wrapper(PyScObject *self, void *closure)
{
  scObjectMember_t *member;
  scDataTypeValue_t *value;
  
  member = (scObjectMember_t*)closure;
  
  value = member->get( self->instance, member->closure );
  return convert_from_value( value );
}

static int set_wrapper(PyScObject *self, PyObject *pyvalue, void *closure)
{
  scObjectMember_t *member;
  scDataTypeValue_t value;
  
  member = (scObjectMember_t*)closure;
  
  convert_to_value( pyvalue, &value, member->type );
  
  if( member->type !=  TYPE_ANY && member->type != value.type )
  {
    PyErr_SetNone(PyExc_TypeError);
    return -1;
  }
  
  member->set( self->instance, &value, member->closure );
  return 0;
}

static PyObject *get_method(PyScObject *self, void *closure)
{
  scObjectMethod_t *method;
  PyObject         *pymethod;
  
  method = (scObjectMethod_t*)closure;
  
  Com_Printf("Creating PyMethod \"%s\" for object type \"%s\"\n", method->name, self->type->name);
  pymethod = New_PyMethod( (PyObject*)self, self->instance, method);
//  Py_INCREF( pymethod );
  return pymethod;
}

static int set_method(PyScObject *self, PyObject *pyvalue, void *closure)
{
  scObjectMethod_t *method;
  
  method = (scObjectMethod_t*)closure;
  
  PyErr_Format(PyExc_AttributeError, "'%s' object attribute '%s' is read-only", self->type->name, method->name);
  return -1;
}

/* Create a copy of PyScObjectType and make it work somehow */
void SC_Python_InitObectType( scObjectType_t * type )
{
  ScPyTypeObject   *newtype;
  scObjectMember_t *member;
  scObjectMethod_t *method;
  PyGetSetDef      *getsetdef;
  int              i;
  
  newtype   = BG_Alloc( sizeof( ScPyTypeObject ) );
  getsetdef = BG_Alloc( sizeof( PyGetSetDef ) * (type->membercount + type->methodcount + 1) );
  
  member    = type->members;
  for (i=0; member->name != NULL; member++, i++) {
    getsetdef[i].name    = member->name;
    getsetdef[i].doc     = member->desc;
    getsetdef[i].get     = (getter)get_wrapper;
    getsetdef[i].set     = (setter)set_wrapper;
    getsetdef[i].closure = (void*)member;
  }
  
  method = type->methods;
  for (; method->name != NULL; method++, i++) {
    getsetdef[i].name    = method->name;
    getsetdef[i].doc     = method->desc;
    getsetdef[i].get     = (getter)get_method;
    getsetdef[i].set     = (setter)set_method;
    getsetdef[i].closure = (void*)method;
  }
  getsetdef[i+1].name = NULL;
  
  memcpy( newtype, &PyScObject_Type, sizeof( PyScObject_Type ) );
  
  newtype->tp_name      = type->name;
  newtype->tp_getset    = getsetdef;
  newtype->tp_scobjtype = type;
  
  if (PyType_Ready((PyTypeObject*)newtype) < 0)
    return;
  Py_INCREF(newtype);
  type->pythontype = (void*)newtype;
  
}

#endif
