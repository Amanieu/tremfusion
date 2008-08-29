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

/*
 * TODO:
 * - string has metatable by default ! get metatable is not enough to see if data is registered
 * - Test GC (and check GC with popped values)
 * - Rewrite some basic lua functions : assert, error, dofile, tonumber, unpack
 * - Remove some basic lua functions : getfenv, setfenv, gcinfo, collectgarbage
 * - `load'/`loadfile'/`loadstring' basic functions : what is it ?
 * - select, newproxy: what is it ?
 * - module and require integration in scripting module engine
 */

#include "sc_public.h"
#include "sc_local.h"

#ifdef USE_LUA

lua_State *g_luaState = NULL;

/*
========================
SC_Lua_sctype2luatype
========================
*/

int SC_Lua_sctype2luatype(scDataType_t sctype)
{
  switch(sctype)
  {
    case TYPE_BOOLEAN: return LUA_TBOOLEAN;
    case TYPE_INTEGER:
    case TYPE_FLOAT: return LUA_TNUMBER;
    case TYPE_STRING: return LUA_TSTRING;
    case TYPE_FUNCTION: return LUA_TFUNCTION;
    case TYPE_ARRAY:
    case TYPE_HASH:
    case TYPE_CLASS:
    case TYPE_OBJECT:
    case TYPE_NAMESPACE: return LUA_TTABLE;
    case TYPE_USERDATA: return LUA_TUSERDATA;
    case TYPE_UNDEF:
    default: return LUA_TNIL;
  }
}

/*
===========================
SC_Lua_is_registered_type
===========================
*/

int SC_Lua_is_registered(lua_State *L, int index, scDataType_t type)
{
  if(lua_getmetatable(L, index))
  {
    lua_getfield(L, -1, "_type");
    if(lua_isnumber(L, -1))
    {
      if(type != TYPE_ANY && lua_tointeger(L, -1) != type)
      {
        // luaL_typeerror
      }
      lua_pop(L, 2);
      return 1;
    }
    else
      lua_pop(L, 2);
  }

  if(type != TYPE_ANY && lua_type(L, index) != SC_Lua_sctype2luatype(type))
  {
    // luaL_typeerror
  }
  return 0;
}

/*
============
SC_Lua_Init
============
*/

static void loadconstants(lua_State *L)
{
  scConstant_t *cst = sc_constants;
  float val;
  while(cst->name[0] != '\0')
  {
    switch(cst->type)
    {
      case TYPE_BOOLEAN:
        lua_pushboolean(L, (int)cst->data);
        lua_setglobal(L, cst->name);
        break;
      case TYPE_INTEGER:
        lua_pushinteger(L, (int)cst->data);
        lua_setglobal(L, cst->name);
        break;
      case TYPE_FLOAT:
        memcpy(&val, &cst->name, sizeof(val));
        lua_pushnumber(L, val);
        lua_setglobal(L, cst->name);
        break;
      case TYPE_STRING:
        lua_pushstring(L, (char*)cst->data);
        lua_setglobal(L, cst->name);
        break;
      case TYPE_USERDATA:
        lua_pushlightuserdata(L, cst->data);
        lua_setglobal(L, cst->name);
        break;
      default:
        Com_Printf("Can't load %s: %d: invalid type\n", cst->name, cst->type);
        break;
    }

    cst++;
  }
}

void SC_Lua_Init( void )
{
  Com_Printf("Lua initializing... ");
  scDataTypeValue_t value;
  lua_State *L = lua_open();

  // Lua standard lib
  luaopen_base(L);
  luaopen_string(L);
  luaopen_table(L);
  lua_pop(L, 4);

  loadconstants(L);

  SC_Lua_loadlib(L);

  // Register global table as root namespace
  SC_NamespaceGet("", &value);

  // root metatable
  lua_newtable(L);
  lua_pushlightuserdata(L, value.data.namespace);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_string_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");
  lua_pushinteger(L, TYPE_NAMESPACE);
  lua_setfield(L, -2, "_type");

  lua_pushcfunction(L, SC_Lua_index_hash_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_newindex_hash_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_len_hash_metamethod);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, LUA_GLOBALSINDEX);

  g_luaState = L;
  Com_Printf("done\n");
}

/*
=================
SC_Lua_Shutdown
=================
*/
void SC_Lua_Shutdown( void )
{
  Com_Printf("Lua shutting down... ");

  if(g_luaState)
  {
    lua_close(g_luaState);
    g_luaState = NULL;
  }

  Com_Printf("done\n");
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

  Com_Printf("...loading '%s'\n", filename);

  len = trap_FS_FOpenFile(filename, &f, FS_READ);
  if(!f)
  {
    Com_Printf(va(S_COLOR_RED "file not found: %s\n", filename));
    return qfalse;
  }

  if(len >= MAX_LUAFILE)
  {
    Com_Printf(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i\n", filename, len, MAX_LUAFILE));
    trap_FS_FCloseFile(f);
    return qfalse;
  }

  trap_FS_Read(buf, len, f);
  buf[len] = 0;
  trap_FS_FCloseFile(f);

  if(luaL_loadbuffer(g_luaState, buf, strlen(buf), filename))
  {
    Com_Printf("cannot load lua file `%s': %s\n", filename, lua_tostring(g_luaState, -1));
    return qfalse;
  }

  if(lua_pcall(g_luaState, 0, 0, 0))
  {
    Com_Printf("cannot run lua file `%s': %s\n", filename, lua_tostring(g_luaState, -1));
    return qfalse;
  }

  return qtrue;
}

/*
=================
SC_Lua_RunFunction
=================
*/
int SC_Lua_RunFunction(const scDataTypeFunction_t *func, scDataTypeValue_t *in, scDataTypeValue_t *out)
{
  int narg = 0;
  lua_State *L = g_luaState;

  lua_getfield(L, LUA_REGISTRYINDEX, va("func_%d", func->data.luafunc));
  if(lua_isnil(L, -1))
    SC_EXEC_ERROR(va("internal error : attempt to call an unregistered function at %s (%d)", __FILE__, __LINE__));

  while( in[narg].type != TYPE_UNDEF )
  {
    SC_Lua_push_value( L, &in[narg] );

    narg++;
  }

  // do the call
  if(lua_pcall(L, narg, 1, 0) != 0)
    SC_EXEC_ERROR(va("error running lua function : %s", lua_tostring(L, -1)));

  SC_Lua_pop_value(L, out, func->return_type);

  return 0;
}

/*
=================
SC_Lua_StackDump
=================
*/
void SC_Lua_StackDump(lua_State *L)
{
  int             i;
  int             top = lua_gettop(L);

  Com_Printf("--------- SC_Lua_StackDump ----------\n");
  for(i = 1; i <= top; i++)
  {
    // repeat for each level
    int             t = lua_type(L, i);

    switch (t)
    {
      case LUA_TSTRING:
        // strings
        Com_Printf("`%s'", lua_tostring(L, i));
        break;

      case LUA_TBOOLEAN:
        // booleans
        Com_Printf(lua_toboolean(L, i) ? "true" : "false");
        break;

      case LUA_TNUMBER:
        // numbers
        Com_Printf("%g", lua_tonumber(L, i));
        break;

      default:
        // other values
        Com_Printf("%s", lua_typename(L, t));
        break;

    }
    Com_Printf(" ");			// put a separator
  }
  Com_Printf("\n");				// end the listing
  Com_Printf("-------------------------------------\n");
}

#endif

