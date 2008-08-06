/*
===========================================================================
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>
Copyright (C) 2008 Maurice Doison

This file is part of Tremulous source code.

Tremulous source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// sc_lua.h

#ifndef _SCRIPT_SC_LUA_H
#define _SCRIPT_SC_LUA_H

#ifdef USE_LUA

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "sc_public.h"

extern lua_State *g_luaState;

void                SC_Lua_Init( void );
void                SC_Lua_Shutdown( void );
qboolean            SC_Lua_RunScript( const char *filename );
void                SC_Lua_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret );
void                SC_Lua_DumpStack( void );

void                SC_Lua_Fregister(scDataTypeFunction_t *function);

#endif

#endif

