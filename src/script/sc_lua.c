
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
// sc_lua.c

#ifdef USE_LUA

#include "sc_script.h"
#include "sc_lua.h"

#define MAX_LUAFILE 32768

lua_State *g_luaState = NULL;

/*
============
initLua_global

load multiple global precreated lua scripts
============
*/

static void initLua_global(void)
{
  int             numdirs;
  int             numFiles;
  char            filename[128];
  char            dirlist[1024];
  char           *dirptr;
  int             i;
  int             dirlen;

  numFiles = 0;

  numdirs = trap_FS_GetFileList("scripts/global/", ".lua", dirlist, 1024);
  dirptr = dirlist;
  for(i = 0; i < numdirs; i++, dirptr += dirlen + 1)
  {
    dirlen = strlen(dirptr);
    strcpy(filename, "scripts/global/");
    strcat(filename, dirptr);
    numFiles++;

    // load the file
    SC_Lua_RunScript( filename );

  }

  Com_Printf("%i global files parsed\n", numFiles);
}

/*
============
initLua_local

load multiple lua scripts from the maps directory
============
*/

static void initLua_local(char mapname[MAX_STRING_CHARS])
{
  int             numdirs;
  int             numFiles;
  char            filename[128];
  char            dirlist[1024];
  char           *dirptr;
  int             i;
  int             dirlen;

  numFiles = 0;

  numdirs = trap_FS_GetFileList(va("scripts/%s", mapname), ".lua", dirlist, 1024);
  dirptr = dirlist;
  for(i = 0; i < numdirs; i++, dirptr += dirlen + 1)
  {
    dirlen = strlen(dirptr);
    strcpy(filename, va("scripts/%s/", mapname));
    strcat(filename, dirptr);
    Com_Printf("***find file to parse***\n");
    numFiles++;

    // load the file
    SC_Lua_RunScript( filename );

  }

  Com_Printf("%i local files parsed\n", numFiles);
}

void pusharray( lua_State *L, scDataTypeArray_t *array )
{
  // TODO: push array here
}

void pushhash( lua_State *L, scDataTypeHash_t *hash )
{
  // TODO: push hash here
}

void popValue( lua_State *L, scDataTypeValue_t *value )
{
  // TODO: pop a value here
}

/*
============
SC_Lua_Init
============
*/
void SC_Lua_Init( void )
{
  char            buf[MAX_STRING_CHARS];

  G_Printf("------- Game Lua Initialization -------\n");

  g_luaState = lua_open();

  // Lua standard lib
  luaopen_base(g_luaState);
  luaopen_string(g_luaState);
  luaopen_table(g_luaState);

  // Quake lib
  /*luaopen_entity(g_luaState);
  luaopen_player(g_luaState);
  luaopen_buildable(g_luaState);
  luaopen_game(g_luaState);
  luaopen_qmath(g_luaState);
  luaopen_vector(g_luaState);*/

  // load global scripts
  G_Printf("global lua scripts:\n");
  initLua_global();

  // load map-specific lua scripts
  G_Printf("map specific lua scripts:\n");
  trap_Cvar_VariableStringBuffer("mapname", buf, sizeof(buf));
  initLua_local( buf );

  G_Printf("-----------------------------------\n");
}

/*
=================
SC_Lua_Shutdown
=================
*/
void SC_Lua_Shutdown( void )
{
  G_Printf("------- Game Lua Finalization -------\n");

  if(g_luaState)
  {
    lua_close(g_luaState);
    g_luaState = NULL;
  }

  G_Printf("-----------------------------------\n");
}

/*
=================
SC_Lua_RunScript
=================
*/
qboolean SC_Lua_RunScript( const char *filename )
{
  int             len;
  fileHandle_t    f;
  char            buf[MAX_LUAFILE];

  G_Printf("...loading '%s'\n", filename);

  len = trap_FS_FOpenFile(filename, &f, FS_READ);
  if(!f)
  {
    G_Printf(va(S_COLOR_RED "file not found: %s\n", filename));
    return qfalse;
  }

  if(len >= MAX_LUAFILE)
  {
    G_Printf(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i\n", filename, len, MAX_LUAFILE));
    trap_FS_FCloseFile(f);
    return qfalse;
  }

  trap_FS_Read(buf, len, f);
  buf[len] = 0;
  trap_FS_FCloseFile(f);

  if(luaL_loadbuffer(g_luaState, buf, strlen(buf), filename))
    return qfalse;

  if(lua_pcall(g_luaState, 0, 0, 0))
    return qfalse;

  return qtrue;
}

/*
=================
SC_Lua_RunFunction
=================
*/
void SC_Lua_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  int narg = 0;
  lua_State *L = g_luaState;
  scDataType_t *dt = (scDataType_t*) func->argument;
  scDataTypeValue_t *value = args;

  lua_getglobal( L, func->data.path );		// get function

  while( *dt != TYPE_UNDEF )
  {
    switch( *dt )
    {
      case TYPE_INTEGER:
        lua_pushinteger( L, args->data.integer );
        break;
      case TYPE_FLOAT:
        lua_pushnumber( L, args->data.floating );
        break;
      case TYPE_STRING:
        lua_pushstring( L, & args->data.string->data );
        break;
      case TYPE_ARRAY:
        pusharray( L, args->data.array );
        break;
      case TYPE_HASH:
        pushhash( L, args->data.hash );
        break;
      case TYPE_NAMESPACE:
        pushhash( L, (scDataTypeHash_t*) args->data.namespace );
        break;
      default:
        break;
    }

    dt++;
    value++;
    narg++;
  }

  // do the call
  if(lua_pcall(L, narg, 1, 0) != 0)	// do the call
    G_Printf("G_RunLuaFunction: error running function `%s': %s\n", func, lua_tostring(L, -1));

  popValue( L, ret );
}

/*
=================
SC_Lua_DumpStack
=================
*/
void SC_Lua_DumpStack( void )
{
  int             i;
  lua_State      *L = g_luaState;
  int             top = lua_gettop(L);

  for(i = 1; i <= top; i++)
  {
    // repeat for each level
    int             t = lua_type(L, i);

    switch (t)
    {
      case LUA_TSTRING:
        // strings
        G_Printf("`%s'", lua_tostring(L, i));
        break;

      case LUA_TBOOLEAN:
        // booleans
        G_Printf(lua_toboolean(L, i) ? "true" : "false");
        break;

      case LUA_TNUMBER:
        // numbers
        G_Printf("%g", lua_tonumber(L, i));
        break;

      default:
        // other values
        G_Printf("%s", lua_typename(L, t));
        break;

    }
    G_Printf("  ");			// put a separator
  }
  G_Printf("\n");				// end the listing
}

#endif

