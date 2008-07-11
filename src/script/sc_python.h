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


typedef struct {
  PyObject_HEAD
  scDataType_t           type;
  union {
    scDataTypeFunction_t *function;
    scObjectType_t       *objecttype;
  } data;
} PyFunction;

typedef struct {
  PyObject_HEAD
  scObjectInstance_t *instance;
  scObjectType_t     *type;
} PyScObject;

PyTypeObject PyScObject_Type;

PyObject *PyScObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
int PyScObject_init(PyScObject *self, scObjectType_t *type, scDataTypeValue_t *args);

PyObject *PyFunction_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
int PyFunction_init(PyFunction *self, int type, void *closure);
PyTypeObject PyFunctionType;

extern PyObject *vec3d;
PyObject *convert_from_value( scDataTypeValue_t *value );
void convert_to_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type );

void                SC_Python_Init( void );
void                SC_Python_Shutdown( void );
qboolean            SC_Python_RunScript( const char *filename );
void                SC_Python_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret );
void                SC_Python_InitObectType( scObjectType_t * type );

#endif
