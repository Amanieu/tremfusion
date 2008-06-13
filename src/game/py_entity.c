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
#include "g_local.h"
#include "g_python.h"

PyObject *EntityForGentity( gentity_t *gentity );

typedef struct {
  PyObject_HEAD
  int num; /* client number */
  gentity_t *e;
} Entity;

// Garbage collection
static int Entity_traverse(Entity *self, visitproc visit, void *arg) {
  return 0;
}
// Deletion
static int Entity_clear(Entity *self) {
  return 0;
}
// Deallocation
static void Entity_dealloc(Entity* self) {
  Entity_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}
// 
static PyObject *Entity_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Entity *self;

  self = (Entity *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->num = -1;
    self->e = NULL;
  }

  return (PyObject *)self;
}

static int Entity_init(Entity *self, PyObject *args, PyObject *kwds)
{
  PyObject *tmp;

  if (!PyArg_ParseTuple(args, "i", &self->num) )
      return -1;
  if (self->num < 0  || self->num > MAX_GENTITIES)
  {
    PyErr_SetString(PyExc_IndexError, "Entity number out of range.");
    return -1;
  }
  self->e = &g_entities[ self->num ];

  return 0;
}

typedef enum {
  INUSE,
  CLASSNAME,
  SPAWNFLAGS,
  PARENT,
  NUM_ENTITY_GETSET_TYPES
} Entity_getset_types;

static PyObject *Entity_get(Entity *self, void *closure)
{
  Entity_getset_types gettype = (Entity_getset_types)closure;
  
  
  char *buf;
  switch (gettype){
  case INUSE:
    return Py_BuildValue( "i", (self->e->inuse) ? 1 : 0 );
  case CLASSNAME:
    return Py_BuildValue( "s", self->e->classname);
  case PARENT:
    if ( !self->e->parent ) return Py_BuildValue("");
    return EntityForGentity( self->e->parent );
  }
}

static int Entity_set(Entity *self, PyObject *value, void *closure)
{
//  int settype = (int)closure;
//  if (value == NULL) {
    PyErr_SetString(PyExc_TypeError, "Cannot set any Entity attributes yet");
    return -1;
//  }
}

static PyMemberDef Entity_members[] = {
  {"num", T_INT, offsetof(Entity, num), 0, "Entity number"},
  {NULL}  /* Sentinel */
};

static PyGetSetDef Entity_getseters[] = {
    {"inuse",(getter)Entity_get, (setter)Entity_set, "True is entitiy slot num is currently being used", (void*)INUSE },
    {"classname",(getter)Entity_get, (setter)Entity_set, "Entity's classname", (void*)CLASSNAME },
    {"parent",(getter)Entity_get, (setter)Entity_set, "Entity's parent entity", (void*)PARENT },
//    {"muted",(getter)Entity_get, (setter)Entity_set, "Whether or not player is muted", (void*)MUTED },
    {NULL}  /* Sentinel */
};

static PyMethodDef Entity_methods[] = {
    {NULL}  /* Sentinel */
};

PyTypeObject EntityType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "game.Entity",             /*tp_name*/
    sizeof(Entity),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Entity_dealloc, /*tp_dealloc*/
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
//    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Class representing a gentity_t",           /* tp_doc */
    (traverseproc)Entity_traverse, /* tp_traverse */
//    0,                         /* tp_traverse */
    (inquiry)Entity_clear,     /* tp_clear */
//    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Entity_methods,            /* tp_methods */
    Entity_members,            /* tp_members */
    Entity_getseters,          /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Entity_init,     /* tp_init */
    0,                         /* tp_alloc */
    Entity_new,                /* tp_new */
};

PyObject *EntityForGentity( gentity_t *gentity )
{
  PyObject* ArgsTuple, *temp;
  temp = Entity_new( &EntityType, NULL, NULL );
  if (temp == NULL) return Py_BuildValue("");
  ArgsTuple = PyTuple_New(1);
  PyTuple_SetItem(ArgsTuple, 0, Py_BuildValue("i", gentity->s.number ) );
  Entity_init( temp, ArgsTuple, NULL);
  return temp;
}

#endif /* USE_PYTHON */
