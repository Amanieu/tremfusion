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
//
// sc_python.h

#ifdef USE_PYTHON

PyTypeObject EntityType;
PyTypeObject EntityStateType;
PyTypeObject Vec3dType;

typedef struct _scpytypeobject {
  PyObject_VAR_HEAD
  const char *tp_name; /* For printing, in format "<module>.<name>" */
  Py_ssize_t tp_basicsize, tp_itemsize; /* For allocation */
  /* Methods to implement standard operations */
  destructor tp_dealloc;
  printfunc tp_print;
  getattrfunc tp_getattr;
  setattrfunc tp_setattr;
  cmpfunc tp_compare;
  reprfunc tp_repr;
  /* Method suites for standard classes */
  PyNumberMethods *tp_as_number;
  PySequenceMethods *tp_as_sequence;
  PyMappingMethods *tp_as_mapping;
  /* More standard operations (here for binary compatibility) */
  hashfunc tp_hash;
  ternaryfunc tp_call;
  reprfunc tp_str;
  getattrofunc tp_getattro;
  setattrofunc tp_setattro;
  /* Functions to access object as input/output buffer */
  PyBufferProcs *tp_as_buffer;
  /* Flags to define presence of optional/expanded features */
  long tp_flags;
  const char *tp_doc; /* Documentation string */
  /* Assigned meaning in release 2.0 */
  /* call function for all accessible objects */
  traverseproc tp_traverse;
  /* delete references to contained objects */
  inquiry tp_clear;
  /* Assigned meaning in release 2.1 */
  /* rich comparisons */
  richcmpfunc tp_richcompare;
  /* weak reference enabler */
  Py_ssize_t tp_weaklistoffset;
  /* Added in release 2.2 */
  /* Iterators */
  getiterfunc tp_iter;
  iternextfunc tp_iternext;
  /* Attribute descriptor and subclassing stuff */
  struct PyMethodDef *tp_methods;
  struct PyMemberDef *tp_members;
  struct PyGetSetDef *tp_getset;
  struct _typeobject *tp_base;
  PyObject *tp_dict;
  descrgetfunc tp_descr_get;
  descrsetfunc tp_descr_set;
  Py_ssize_t tp_dictoffset;
  initproc tp_init;
  allocfunc tp_alloc;
  newfunc tp_new;
  freefunc tp_free; /* Low-level free-memory routine */
  inquiry tp_is_gc; /* For PyObject_IS_GC */
  PyObject *tp_bases;
  PyObject *tp_mro; /* method resolution order */
  PyObject *tp_cache;
  PyObject *tp_subclasses;
  PyObject *tp_weaklist;
  destructor tp_del;
#ifdef COUNT_ALLOCS
  /* these must be last and never explicitly initialized */
  Py_ssize_t tp_allocs;
  Py_ssize_t tp_frees;
  Py_ssize_t tp_maxalloc;
  struct _typeobject *tp_prev;
  struct _typeobject *tp_next;
#endif
  scObjectType_t *tp_scobjtype;
} ScPyTypeObject;

typedef struct {
  PyObject_HEAD
  scDataType_t           type;
  scDataTypeFunction_t *function;
} PyFunction;
PyTypeObject PyFunctionType;
PyObject *PyFunction_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
int PyFunction_init(PyFunction *self, scDataTypeFunction_t *function);


typedef struct {
  PyObject_HEAD
  scObjectInstance_t *instance;
  scObjectType_t     *type;
} PyScObject; 
ScPyTypeObject PyScObject_Type;
PyObject *PyScObject_new(ScPyTypeObject *type, PyObject *args, PyObject *kwds);
int PyScObject_init(PyScObject *self, PyObject* pArgs, PyObject* kArgs);

typedef struct {
  PyObject_HEAD
  scObjectInstance_t *instance;
  scObjectMethod_t   *method;
  PyObject           *parent;
} PyScMethod;

PyTypeObject PyScMethod_Type;
PyObject *PyScMethod_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
int PyScMethod_init(PyScMethod *self, PyObject *parent, scObjectInstance_t* instance, scObjectMethod_t* method);

extern PyObject *vec3d;
PyObject *convert_from_value( scDataTypeValue_t *value );
void convert_to_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type );

void                SC_Python_Init( void );
void                SC_Python_Shutdown( void );
qboolean            SC_Python_RunScript( const char *filename );
void                SC_Python_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret );
void                SC_Python_InitObectType( scObjectType_t * type );

#endif
