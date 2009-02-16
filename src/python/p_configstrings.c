/*
===========================================================================
Copyright (C) 2008 John Black

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "p_local.h"
#ifndef DEDICATED
#include "../client/client.h"
#endif

static PyObject *CS_Get(PyObject *self, PyObject *args)
{
  int number, ofs;
  if (!PyArg_ParseTuple(args, "i", &number) )
    return NULL;
#ifndef DEDICATED
  ofs = cl.gameState.stringOffsets[ number ];
  if ( !ofs ) {
    Py_RETURN_NONE;
  }
  return Py_BuildValue("s", cl.gameState.stringData + ofs);
#endif
}

static PyMethodDef P_configstrings_methods[] =
  {
    {"get", CS_Get, METH_VARARGS,  "get configstring string"},
    {NULL}      /* sentinel */
  };

void P_Configstring_Init(void)
{
  PyObject *mod;
  mod = Py_InitModule("configstring", P_configstrings_methods);
  ADD_MODULE_CONSTANT(mod, MAX_CONFIGSTRINGS);
  ADD_MODULE_CONSTANT(mod, CS_SERVERINFO);
  ADD_MODULE_CONSTANT(mod, CS_SYSTEMINFO);
  ADD_MODULE_CONSTANT(mod, RESERVED_CONFIGSTRINGS);
}
