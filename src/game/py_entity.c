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
#include "py_local.h"

PyObject *EntityForGentity( gentity_t *gentity );
PyObject *Vec3dforVec3_t( vec3_t *vect );

typedef struct {
  PyObject_HEAD
  int num; /* client number */
  vec3_t *vector;
} Vec3d;

// Garbage collection
static int Vec3d_traverse(Vec3d *self, visitproc visit, void *arg) {
  return 0;
}
// Deletion
static int Vec3d_clear(Vec3d *self) {
  return 0;
}
// Deallocation
static void Vec3d_dealloc(Vec3d* self) {
  Vec3d_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}
// 
static PyObject *Vec3d_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  Vec3d *self;

  self = (Vec3d *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->vector = NULL;
  }
  
  return (PyObject *)self;
}

static int Vec3d_init(Vec3d *self, vec3_t *vector)
{
  self->vector = vector;
  return 0;
}

typedef enum {
  X,
  Y,
  Z,
} Vec3d_getset_types;

static PyObject *Vec3d_get(Vec3d *self, void *closure)
{
  Vec3d_getset_types gettype = (Vec3d_getset_types)closure;
  vec3_t temp;
  VectorCopy(*self->vector, temp);
  switch (gettype){
    case X:
      return Py_BuildValue("f", temp[0]);
    case Y:
      return Py_BuildValue("f", temp[1]);
    case Z:
      return Py_BuildValue("f", temp[2]);
    default:
      PyErr_SetString(PyExc_TypeError, "Vec3d_get fallthrough");
      return NULL;
  }
  
}

static int Vec3d_set(Vec3d *self, PyObject *value, void *closure)
{
  int settype = (int)closure;
  if (value == NULL) {
    PyErr_SetString(PyExc_TypeError, "Invalid value");
    return -1;
  }
  vec3_t temp;
  float value_float;
  value_float = PyFloat_AsDouble( value );
  VectorCopy(*self->vector, temp);
  switch (settype) {
    case X:
      temp[0] = value_float;
      break;
    case Y:
      temp[1] = value_float;
      break;
    case Z:
      temp[2] = value_float;
      break;
    default:
      PyErr_SetString(PyExc_TypeError, "EntityState_set fallthrough");
      return -1;
  }
  VectorCopy(temp, *self->vector);
  return 0;
}

static PyMemberDef Vec3d_members[] = {
  {NULL}  /* Sentinel */
};

static PyGetSetDef Vec3d_getseters[] = {
    {"x",        (getter)Vec3d_get, (setter)Vec3d_set, "True if Entitiy's slot num is currently being used", (void*)X },
    {"y",        (getter)Vec3d_get, (setter)Vec3d_set, "True if Entitiy's slot num is currently being used", (void*)Y },
    {"z",        (getter)Vec3d_get, (setter)Vec3d_set, "True if Entitiy's slot num is currently being used", (void*)Z },
    {NULL}  /* Sentinel */
};

static PyMethodDef Vec3d_methods[] = {
    {NULL}  /* Sentinel */
};

PyTypeObject Vec3dType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "game.Vec3d",             /*tp_name*/
    sizeof(Vec3d),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Vec3d_dealloc, /*tp_dealloc*/
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
    "game.Vec3d()\n\nClass representing a vec3_t",           /* tp_doc */
    (traverseproc)Vec3d_traverse, /* tp_traverse */
//    0,                         /* tp_traverse */
    (inquiry)Vec3d_clear,     /* tp_clear */
//    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Vec3d_methods,            /* tp_methods */
    Vec3d_members,            /* tp_members */
    Vec3d_getseters,          /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,     /* tp_init */
    0,                         /* tp_alloc */
    Vec3d_new,                /* tp_new */
};

typedef struct {
  PyObject_HEAD
  int num; /* client number */
  gentity_t *e;
} EntityState;

// Garbage collection
static int EntityState_traverse(EntityState *self, visitproc visit, void *arg) {
  return 0;
}
// Deletion
static int EntityState_clear(EntityState *self) {
  return 0;
}
// Deallocation
static void EntityState_dealloc(EntityState* self) {
  EntityState_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}
// 
static PyObject *EntityState_new(PyTypeObject *type , PyObject *args, PyObject *kwds)
{
  EntityState *self;

  self = (EntityState *)type->tp_alloc(type, 0);
  if (self != NULL) {
    self->num = -1;
    self->e = NULL;
  }

  return (PyObject *)self;
}

static int EntityState_init(EntityState *self, PyObject *args, PyObject *kwds)
{
  if (!PyArg_ParseTuple(args, "i", &self->num) )
      return -1;
  if (self->num < 0  || self->num > MAX_GENTITIES)
  {
    PyErr_SetString(PyExc_IndexError, "EntityState number out of range.");
    return -1;
  }
  self->e = &g_entities[ self->num ];

  return 0;
}

typedef enum {
  ESNUM,
  ETYPE,
  ORIGIN,
  NUM_ENTITYSTATE_GETSET_TYPES
} EntityState_getset_types;

static PyObject *EntityState_get(EntityState *self, void *closure)
{
  EntityState_getset_types gettype = (EntityState_getset_types)closure;
  switch (gettype) {
    case ESNUM:
      return PyInt_FromLong( self->num );
    case ETYPE:
      return Py_BuildValue( "i", self->e->s.eType );
    case ORIGIN:
      return Vec3dforVec3_t( &self->e->s.origin );
      
    default:
      PyErr_SetString(PyExc_TypeError, "EntityState_get fallthrough");
      return NULL;
  }
}

static int EntityState_set(EntityState *self, PyObject *value, void *closure)
{
  int settype = (int)closure;
  if (value == NULL) {
    PyErr_SetString(PyExc_TypeError, "Invalid value");
    return -1;
  }
  switch (settype) {
    case ESNUM:
      PyErr_SetString(PyExc_TypeError, "Cannot change EntityState number");
      return -1;
    case ETYPE:
      self->e->s.eType = PyInt_AsLong( value );
      if ( self->e->s.eType == -1 && PyErr_Occurred() ) {
        PyErr_SetString(PyExc_ValueError, "Invalid value, value must be an integer");
        return -1;
      }
      return 1;
    default:
      PyErr_SetString(PyExc_TypeError, "EntityState_set fallthrough");
      return -1;
  }
}

static PyMemberDef EntityState_members[] = {
//  {"num", T_INT, offsetof(EntityState, num), 0, "EntityState number"},
  {NULL}  /* Sentinel */
};

static PyGetSetDef EntityState_getseters[] = {
    {"num", (getter)EntityState_get, (setter)EntityState_set, "EntityState number", (void*)ESNUM },
    {"eType",(getter)EntityState_get, (setter)EntityState_set, "Entity's current type", (void*)ETYPE },
//    {"origin",(getter)EntityState_get, (setter)EntityState_set, "Entity's current origin", (void*)ORIGIN },
    {NULL}  /* Sentinel */
};

static PyMethodDef EntityState_methods[] = {
    {NULL}  /* Sentinel */
};

PyTypeObject EntityStateType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "game.EntityState",             /*tp_name*/
    sizeof(EntityState),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)EntityState_dealloc, /*tp_dealloc*/
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
    "Class representing a entityState_t",           /* tp_doc */
    (traverseproc)EntityState_traverse, /* tp_traverse */
//    0,                         /* tp_traverse */
    (inquiry)EntityState_clear,     /* tp_clear */
//    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    EntityState_methods,            /* tp_methods */
    EntityState_members,            /* tp_members */
    EntityState_getseters,          /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)EntityState_init,     /* tp_init */
    0,                         /* tp_alloc */
    EntityState_new,                /* tp_new */
};

typedef struct {
  PyObject_HEAD
  int num; /* client number */
  gentity_t *e;
  PyObject *s; // entityState_t
} Entity;

// Garbage collection
static int Entity_traverse(Entity *self, visitproc visit, void *arg) {
  Py_VISIT(self->s);
  return 0;
}
// Deletion
static int Entity_clear(Entity *self) {
  Py_CLEAR(self->s);
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
    self->s = EntityState_new( &EntityStateType, NULL, NULL);
    if (self->s == NULL)
    {
      Py_DECREF(self);
      return NULL;
    }
  }
  
  return (PyObject *)self;
}

static int Entity_init(Entity *self, PyObject *args, PyObject *kwds)
{
  if (!PyArg_ParseTuple(args, "i", &self->num) )
      return -1;
  if (self->num < 0  || self->num > MAX_GENTITIES)
  {
    PyErr_SetString(PyExc_IndexError, "Entity number out of range.");
    return -1;
  }
  self->e = &g_entities[ self->num ];
  EntityState_init( (EntityState *)self->s, Py_BuildValue("(i)", self->num), NULL );
  return 0;
}

typedef enum {
  INUSE,
  CLASSNAME,
  SPAWNFLAGS,
  PARENT,
  PARENTNODE,
  OVERMINDNODE,
  BUILDER,
  TARGETED,
  POS1,
  POS2,
  HEALTH,
  LASTHEALTH,
  NUM_ENTITY_GETSET_TYPES
} Entity_getset_types;

static PyObject *Entity_get(Entity *self, void *closure)
{
  Entity_getset_types gettype = (Entity_getset_types)closure;
  switch (gettype){
    case INUSE:
      if(self->e->inuse) Py_RETURN_TRUE; 
      else Py_RETURN_FALSE;
    case CLASSNAME:
      return Py_BuildValue( "s", self->e->classname);
    case PARENT:
      if ( !self->e->parent ) return Py_BuildValue("");
      return EntityForGentity( self->e->parent );
    case PARENTNODE:
      if ( !self->e->parentNode ) return Py_BuildValue("");
      return EntityForGentity( self->e->parentNode );
    case BUILDER:
      if ( !self->e->builder ) return Py_BuildValue("");
      return EntityForGentity( self->e->builder );
    case OVERMINDNODE:
      if ( !self->e->overmindNode ) return Py_BuildValue("");
      return EntityForGentity( self->e->overmindNode );
    case TARGETED:
     if ( !self->e->targeted ) return Py_BuildValue("");
     return EntityForGentity( self->e->targeted );
    case HEALTH:
      return Py_BuildValue( "i", self->e->health );
    case LASTHEALTH:
      return Py_BuildValue( "i", self->e->lastHealth );
    default:
      PyErr_SetString(PyExc_TypeError, "Entity_get fallthrough");
      return NULL;
  }
  
}

static int Entity_set(Entity *self, PyObject *value, void *closure)
{
  int settype = (int)closure;
  if (value == NULL) {
    PyErr_SetString(PyExc_TypeError, "Invalid value");
    return -1;
  }
  switch (settype) {
    case HEALTH:
      self->e->health = PyInt_AsLong( value );
      break;
    case LASTHEALTH:
      self->e->lastHealth = PyInt_AsLong( value );
      break;
    default:
      PyErr_SetString(PyExc_SystemError, "Entity_set fallthrough");
      return -1;
  }
  return 0;
}

static PyMemberDef Entity_members[] = {
  {"num", T_INT,       offsetof(Entity, num),   0, "Entity number"},
  {"s",   T_OBJECT_EX, offsetof(Entity, s), 0, "game.EntityState of this Entity" },
  {NULL}  /* Sentinel */
};

static PyGetSetDef Entity_getseters[] = {
    {"inuse",        (getter)Entity_get, (setter)Entity_set, "True if Entitiy's slot num is currently being used", (void*)INUSE },
    {"classname",    (getter)Entity_get, (setter)Entity_set, "Entity's classname",                               (void*)CLASSNAME },
    {"parent",       (getter)Entity_get, (setter)Entity_set, "Entity's parent Entity",                           (void*)PARENT },
    {"parentNode",   (getter)Entity_get, (setter)Entity_set, "Entity's parentNode Entity",                       (void*)PARENTNODE },
    {"builder",      (getter)Entity_get, (setter)Entity_set, "Entity of the occupant of hovel if self is a Hovel",(void*)BUILDER },
    {"overmindNode", (getter)Entity_get, (setter)Entity_set, "Controlling overmind",                             (void*)OVERMINDNODE },
    {"targeted",     (getter)Entity_get, (setter)Entity_set, "Entity of turret currently targeting this entity", (void*)TARGETED },
    {"health",       (getter)Entity_get, (setter)Entity_set, "Current health amount",                            (void*)HEALTH },
    {"lastHealth",   (getter)Entity_get, (setter)Entity_set, "Health the entity had last frame",                 (void*)LASTHEALTH },
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
    "game.Entity( entity number)\n\nClass representing a gentity_t",           /* tp_doc */
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
  Entity_init( (Entity *)temp, ArgsTuple, NULL);
  Py_DECREF(ArgsTuple);
  return temp;
}

PyObject *Vec3dforVec3_t( vec3_t *vect )
{
//  vec3_t vector;
  PyObject *ArgsTuple, *temp;
  temp = Vec3d_new( &Vec3dType, NULL, NULL );
  Vec3d_init( temp, vect );
  if ( temp == NULL ) return Py_BuildValue("");
//  if ( vec3d == NULL) return Py_BuildValue(""); // Failed to load vec3d.py
//  VectorCopy(vect, vector);
//  
//  ArgsTuple = PyTuple_New(3);
//  PyTuple_SetItem(ArgsTuple, 0, Py_BuildValue("f", vector[0] ) );
//  PyTuple_SetItem(ArgsTuple, 1, Py_BuildValue("f", vector[1] ) );
//  PyTuple_SetItem(ArgsTuple, 2, Py_BuildValue("f", vector[2] ) );
//  temp = PyObject_CallObject( vec3d, ArgsTuple);
//  Py_DECREF(ArgsTuple);
  return temp;
}

#endif /* USE_PYTHON */
