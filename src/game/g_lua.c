/*
===========================================================================
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>

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
// g_lua.c

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

lua_State *g_luaState = NULL;

#endif

#include "g_local.h"

#define MAX_LUAFILE 32768

/*
============
G_InitLua_Global

load multiple global precreated lua scripts
============
*/

void G_InitLua_Global(void)
{
#ifdef USE_LUA

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
		G_LoadLuaScript(NULL, filename);

	}

	Com_Printf("%i global files parsed\n", numFiles);

#endif
}

/*
============
G_InitLua_Local

load multiple lua scripts from the maps directory
============
*/

void G_InitLua_Local(char mapname[MAX_STRING_CHARS])
{
#ifdef USE_LUA

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
		G_LoadLuaScript(NULL, filename);

	}

	Com_Printf("%i local files parsed\n", numFiles);

#endif
}


/*
============
G_InitLua
============
*/
void G_InitLua()
{
#ifdef USE_LUA
	char            buf[MAX_STRING_CHARS];

	G_Printf("------- Game Lua Initialization -------\n");

	g_luaState = lua_open();

	// Lua standard lib
	luaopen_base(g_luaState);
	luaopen_string(g_luaState);
	luaopen_table(g_luaState);

	// Quake lib
	luaopen_entity(g_luaState);
	luaopen_player(g_luaState);
	luaopen_buildable(g_luaState);
	luaopen_game(g_luaState);
	luaopen_qmath(g_luaState);
	luaopen_vector(g_luaState);

	// load global scripts
	G_Printf("global lua scripts:\n");
	G_InitLua_Global();

	// load map-specific lua scripts
	G_Printf("map specific lua scripts:\n");
	trap_Cvar_VariableStringBuffer("mapname", buf, sizeof(buf));
	G_InitLua_Local(buf);

	G_Printf("-----------------------------------\n");


#endif
}



/*
=================
G_ShutdownLua
=================
*/
void G_ShutdownLua()
{
#ifdef USE_LUA
	G_Printf("------- Game Lua Finalization -------\n");

	if(g_luaState)
	{
		lua_close(g_luaState);
		g_luaState = NULL;
	}

	G_Printf("-----------------------------------\n");
#endif
}


/*
=================
G_LoadLuaScript
=================
*/
void G_LoadLuaScript(gentity_t * ent, const char *filename)
{
#ifdef USE_LUA
	int             len;
	fileHandle_t    f;
	char            buf[MAX_LUAFILE];

	G_Printf("...loading '%s'\n", filename);

	len = trap_FS_FOpenFile(filename, &f, FS_READ);
	if(!f)
	{
		G_Printf(va(S_COLOR_RED "file not found: %s\n", filename));
		return;
	}

	if(len >= MAX_LUAFILE)
	{
		G_Printf(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i\n", filename, len, MAX_LUAFILE));
		trap_FS_FCloseFile(f);
		return;
	}

	trap_FS_Read(buf, len, f);
	buf[len] = 0;
	trap_FS_FCloseFile(f);

	if(luaL_loadbuffer(g_luaState, buf, strlen(buf), filename))
		G_Printf("G_RunLuaScript: cannot load lua file: %s\n", lua_tostring(g_luaState, -1));

	if(lua_pcall(g_luaState, 0, 0, 0))
		G_Printf("G_RunLuaScript: cannot pcall: %s\n", lua_tostring(g_luaState, -1));
#endif
}

/*
=================
G_RunLuaFunction
=================
*/
void G_RunLuaFunction(const char *func, const char *sig, ...)
{
#ifdef USE_LUA
	va_list         vl;
	int             narg, nres;	// number of arguments and results
	lua_State      *L = g_luaState;

	if(!func || !func[0])
		return;

	va_start(vl, sig);
	lua_getglobal(L, func);		// get function

	// push arguments
	narg = 0;
	while(*sig)
	{
		switch (*sig++)
		{
			case 'f':
				// float argument
				lua_pushnumber(L, va_arg(vl, double));

				break;

			case 'i':
				// int argument
				lua_pushnumber(L, va_arg(vl, int));

				break;

			case 's':
				// string argument
				lua_pushstring(L, va_arg(vl, char *));

				break;

			case 'e':
				// entity argument
				lua_pushentity(L, va_arg(vl, gentity_t *));
				break;

			case 'v':
				// vector argument
				lua_pushvector(L, va_arg(vl, vec_t *));
				break;

			case '>':
				goto endwhile;

			default:
				G_Printf("G_RunLuaFunction: invalid option (%c)\n", *(sig - 1));
		}
		narg++;
		luaL_checkstack(L, 1, "too many arguments");
	}
  endwhile:

	// do the call
	nres = strlen(sig);			// number of expected results
	if(lua_pcall(L, narg, nres, 0) != 0)	// do the call
		G_Printf("G_RunLuaFunction: error running function `%s': %s\n", func, lua_tostring(L, -1));

	// retrieve results
	nres = -nres;				// stack index of first result
	while(*sig)
	{							// get results
		switch (*sig++)
		{

			case 'f':
				// float result
				if(!lua_isnumber(L, nres))
					G_Printf("G_RunLuaFunction: wrong result type\n");
				*va_arg(vl, float *) = lua_tonumber(L, nres);

				break;

			case 'i':
				// int result
				if(!lua_isnumber(L, nres))
					G_Printf("G_RunLuaFunction: wrong result type\n");
				*va_arg(vl, int *) = (int)lua_tonumber(L, nres);

				break;

			case 's':
				// string result
				if(!lua_isstring(L, nres))
					G_Printf("G_RunLuaFunction: wrong result type\n");
				*va_arg(vl, const char **) = lua_tostring(L, nres);

				break;

			default:
				G_Printf("G_RunLuaFunction: invalid option (%c)\n", *(sig - 1));
		}
		nres++;
	}
	va_end(vl);
#endif
}


/*
=================
G_DumpLuaStack
=================
*/
void G_DumpLuaStack()
{
#ifdef USE_LUA
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
#endif
}

/*
=================
G_CallHooks
=================
*/
/* Lua need in stack value:
 * -1: triggered object
 * -2: table to store hooks
 */
int G_CallHooks(const char *event)
{
#ifdef USE_LUA
  lua_State *L = g_luaState;
  int ret = 1;

  lua_getfield(L, -2, "hooks");
  lua_getfield(L, -1, event);
  /* TODO: is a valid event ? */
  lua_remove(L, -2);

  lua_pushnil(L);
  while(lua_next(L, -2))
  {
    lua_pushvalue(L, -4);
    if(lua_pcall(L, 1, 1, 0) != 0)
      G_Error("G_CallHooks: can't call hook : %s\n", lua_tostring(L, -1));

    ret = ret & lua_toboolean(L, -1);
    lua_pop(L, 1);
  }

  lua_pop(L, 1);

  return ret;
#else
  return 1;
#endif
}

/*
=================
G_AddHook
=================
*/

#ifdef USE_LUA
/* To use this function, Lua must have in stack:
 * -1: table to store hooks
 * -2: act to do
 */
int G_AddHook(lua_State *L, const char *event)
{
  lua_getglobal(L, "table");
  lua_getfield(L, -1, "insert");
  lua_remove(L, -2);

  lua_getfield(L, -2, "hooks");
  lua_getfield(L, -1, event);
  lua_remove(L, -2);

  lua_pushvalue(L, -4);

  if(lua_pcall(L, 2, 0, 0) != 0)
    G_Printf("G_AddHook: error in adding hook : %s\n", lua_tostring(L, -1));

  return 0;
}

/*
=================
G_CreateHooksTable
=================
*/

/* To use this function, Lua must have in stack:
 * -1: event name
 */
void G_CreateHooksTable(lua_State *L, const char *event)
{
  lua_newtable(L);
  lua_setfield(L, -2, event);
}
#endif

