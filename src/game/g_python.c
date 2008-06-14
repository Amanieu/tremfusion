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
}

void G_ShutdownPython( void )
{
  
}

#endif /* USE_PYTHON */
