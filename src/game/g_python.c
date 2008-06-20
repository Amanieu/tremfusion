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

#include "g_python.h"
#include "g_local.h"

static PyMethodDef game_methods[] = {
 {NULL, NULL, 0, NULL}
};

PyObject *vec3d_module;
PyObject *vec3d;

void G_InitPython( void )
{
//  PyRun_SimpleString()
  PyObject *gamemodule;
  PyImport_AddModule("game");
  gamemodule = Py_InitModule("game", game_methods);
  if (PyType_Ready(&EntityType) < 0)
    return;
  Py_INCREF(&EntityType);
  PyModule_AddObject(gamemodule, "Entity", (PyObject *)&EntityType);
  if (PyType_Ready(&EntityStateType) < 0)
    return;
  Py_INCREF(&EntityStateType);
  PyModule_AddObject(gamemodule, "EntityState", (PyObject *)&EntityStateType);
  if (PyType_Ready(&Vec3dType) < 0)
    return;
  Py_INCREF(&Vec3dType);
  PyModule_AddObject(gamemodule, "Vec3d", (PyObject *)&Vec3dType);
  PyRun_SimpleString("sys.path.append(\"/home/john/tremulous/server/test_base/stfu-trem/python\")");
  vec3d_module= PyImport_ImportModule("vec3d");
  if (!vec3d_module){
    Com_Printf("^1Cannot find vec3d.py\n");
    vec3d = NULL;
  } else {
    vec3d = PyObject_GetAttrString(vec3d_module, "vec3d" );
  }
//  PyObject *testobject = PyObject_CallObject( vec3d, Py_BuildValue("(iii)", 0, 1, 2));
//  PyObject_Print( testobject, stdout, 0);
  
}

void G_ShutdownPython( void )
{
  if (vec3d_module)
    Py_DECREF( vec3d_module);
  if (vec3d)
    Py_DECREF( vec3d );
}

#endif /* USE_PYTHON */
