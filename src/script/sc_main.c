/*
===========================================================================
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
// sc_main.c

#include "../game/g_local.h"
#include "../qcommon/q_shared.h"

#include "sc_public.h"
#include "sc_lua.h"

void SC_Init( void )
{
  SC_NamespaceInit( );

#ifdef USE_LUA
  SC_Lua_Init( );
#endif
}

void SC_Shutdown( void )
{
#ifdef USE_LUA
  SC_Lua_Shutdown( );
#endif
}

void SC_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  switch( func->langage )
  {
    case LANGAGE_C:
      func->data.ref(args, ret);
      break;
#ifdef USE_LUA
    case LANGAGE_LUA:
      return SC_Lua_RunFunction( func, args, ret );
#endif
    default:
	  break;
  }
}

int SC_RunScript( scLangage_t langage, const char *filename )
{
  switch( langage )
  {
    case LANGAGE_C:
      // invalid call, C can't have scripts
      // TODO: display error
#ifdef USE_LUA
    case LANGAGE_LUA:
      return SC_Lua_RunScript( filename );
#endif
    default:
	  break;
  }

  return -1;
}

int SC_CallHooks( const char *path, gentity_t *entity )
{
  // TODO: implement function
  return 1;
}

