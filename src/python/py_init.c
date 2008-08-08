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

#include "python_local.h"

void PY_Init( void )
{
  PyObject *servermodule;
  
//  Com_Printf( "----- PY_Init -----\n" );
//  // Initialize python
//  Py_Initialize();
//  // Make python threads work at all
//  PyEval_InitThreads( );
//  
//  PyRun_SimpleString("import os, sys, time"); // Import some basic modules
//  PyRun_SimpleString("print \"Python version: \" + sys.version.replace(\"\\n\", \"\\nBuilt with: \")"); // Make sys.version prettier and print it
//  
//  // Create a module for server related stuff...
//  servermodule = Py_InitModule("server", server_methods);
//  // ...like whether or not its running
//  PyRun_SimpleString( serverModuleInitstring);
//  PyRun_SimpleString("server.running = True\n");
//  Python_Initialized = qtrue;
}

void PY_CvarInit( void ){
  if (Python_Initialized)
  {
    PyImport_AddModule("cvar"); //Add module
    Py_InitModule("cvar", cvar_methods);
    PyRun_SimpleString("import cvar\n");
  }
}

void PY_Shutdown() {
//  Python_Initialized = qfalse;
//  Com_Printf("Shutdown python interpreter\n");
//  Py_Finalize();
//  Com_Printf("Shutdown of python interpreter complete\n");
}
