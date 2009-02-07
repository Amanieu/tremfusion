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


#include <Python.h>

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

qboolean p_initilized;

//
// p_cvar.c
//
void P_Cvar_Init(void);
void P_Event_Init(void);
void P_Configstring_Init(void);
void P_Init_PlayerState(PyObject*);

#define ADD_MODULE_CONSTANT(m, x) PyModule_AddIntConstant(m, #x, x)
