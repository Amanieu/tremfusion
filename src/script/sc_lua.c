
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

#include "sc_public.h"
#include "sc_lua.h"

#define MAX_LUAFILE 32768

lua_State *g_luaState = NULL;

static void push_value(lua_State *L, scDataTypeValue_t *value);
static void pop_value(lua_State *L, scDataTypeValue_t *value, scDataType_t type);
static scDataTypeString_t* pop_string(lua_State *L);

static int null_metamethod(lua_State *L)
{
  // TODO: error message
  return 0;
}

static int index_metamethod(lua_State *L)
{
  lua_getfield(L, -2, "_data");
  lua_replace(L, -3);
  lua_rawget(L, -2);
  lua_remove(L, -2);

  return 1;
}

static int newindex_metamethod(lua_State *L)
{
  lua_getfield(L, -3, "_data");
  lua_replace(L, -4);
  lua_rawset(L, -3);

  return 0;
}

static int call_metamethod( lua_State *L )
{
  scDataTypeFunction_t *function;
  int top = lua_gettop(L);
  int i;
  scDataTypeValue_t ret;
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  
  // TODO: error cases

  lua_getfield(L, -top, "_ref");
  function = lua_touserdata(L, -1);
  lua_pop(L, 1);

  for(i = top-2; i >= 0; i--)
  {
    pop_value(L, &args[i], function->argument[i]);
  }
  args[top].type = TYPE_UNDEF;

  SC_RunFunction(function, args, &ret);
  if(ret.type != TYPE_UNDEF)
  {
    push_value(L, &ret);
    return 1;
  }

  return 0;
}

static scDataTypeString_t* pop_string(lua_State *L)
{
  const char *lstr = lua_tostring(L, -1);
  scDataTypeString_t *string = SC_StringNewFromChar(lstr);
  lua_pop(L, 1);

  return string;
}

static scDataTypeArray_t* pop_array(lua_State *L)
{
  scDataTypeValue_t val;

  scDataTypeArray_t *array = SC_ArrayNew();

  lua_pushnil(L);
  while(lua_next(L, -1) != 0)
  {
    pop_value(L, &val, TYPE_ANY);
    SC_ArraySet(array, lua_tonumber(L, -2), &val);
    lua_pop(L, 1);
  }

  return array;
}

static scDataTypeHash_t* pop_hash(lua_State *L)
{
  scDataTypeValue_t val;
  scDataTypeString_t *key;
  const char *lstr;
  scDataTypeHash_t *hash = SC_HashNew();

  lua_pushnil(L);
  while(lua_next(L, -1) != 0)
  {
    lstr = lua_tostring(L, -1);
    key = SC_StringNewFromChar(lstr);
    pop_value(L, &val, TYPE_ANY);
    SC_HashSet(hash, SC_StringToChar(key), &val);

    lua_pop(L, 1);
  }

  return hash;
}

static scDataTypeFunction_t* pop_function(lua_State *L)
{
  scDataTypeFunction_t *function = SC_FunctionNew();
  function->langage = LANGAGE_LUA;
  // TODO: pop a function from metatable
  return function;
}

static void pop_value( lua_State *L, scDataTypeValue_t *value, scDataType_t type )
{
  int ltype = lua_type(L, -1);
  switch(ltype)
  {
    case LUA_TNIL:
      value->type = TYPE_UNDEF;
      lua_pop(L, 1);
      break;

    case LUA_TNUMBER:
      if(type == TYPE_FLOAT)
      {
        value->type = TYPE_FLOAT;
        value->data.floating = lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      else
      {
        value->type = TYPE_INTEGER;
        value->data.integer = lua_tointeger(L, -1);
        lua_pop(L, 1);
      }
      break;

    case LUA_TBOOLEAN:
      value->type = TYPE_BOOLEAN;
      value->data.boolean = lua_toboolean(L, -1);
      lua_pop(L, 1);
      break;

    case LUA_TSTRING:
      value->type = TYPE_STRING;
      value->data.string = pop_string(L);
      break;

    case LUA_TTABLE:
      if(type == TYPE_ANY)
      {
        value->data.hash = pop_hash(L);
        // TODO: implement SC_HashToArray function
        /*if(isArray)
        {
          scDataTypeHash_t *hash = value->data.hash;
          value->type = TYPE_ARRAY;
          SC_HashToArray(hash, &value->data.array);
        }
        else*/
          value->type = TYPE_HASH;
      }
      if(type == TYPE_ARRAY)
      {
        value->type = TYPE_ARRAY;
        value->data.array = pop_array(L);
      }
      else if(type == TYPE_HASH)
      {
        value->type = TYPE_HASH;
        value->data.hash = pop_hash(L);
      }
      else
      {
        //TODO: Error
      }
      break;

    case LUA_TFUNCTION:
      value->type = TYPE_FUNCTION;
      value->data.function = pop_function(L);
      break;

    default:
      // TODO: Error
      break;
  }
}

static void push_array( lua_State *L, scDataTypeArray_t *array )
{
  int i;

  // global table
  lua_newtable(L);

  lua_pushlightuserdata(L, array);
  lua_setfield(L, -2, "_ref");

  // data table
  lua_newtable( L );
  for( i = 0; i < array->size; i++ )
  {
    push_value( L, &array->data[i] );
    lua_rawseti( L, -1, i );
  }
  lua_setfield(L, -2, "_data");

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
}

static void push_hash( lua_State *L, scDataTypeHash_t *hash )
{
  int i;

  // global table
  lua_newtable( L );

  lua_pushlightuserdata(L, hash);
  lua_setfield(L, -2, "_ref");

  // data table
  lua_newtable(L);
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    push_value( L, &hash->data[i].value);
    lua_setfield( L, -2, SC_StringToChar(&hash->data[i].key));
  }
  lua_setfield(L, -2, "_data");

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
}

static void push_function( lua_State *L, scDataTypeFunction_t *function )
{
  // function global table
  lua_newtable(L);
  lua_pushlightuserdata(L, function);
  lua_setfield(L, -2, "_ref");

  // function metatable
  lua_newtable(L);
  lua_pushcfunction(L, call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__newindex");

  lua_setmetatable(L, -2);
}

static void push_value( lua_State *L, scDataTypeValue_t *value )
{
  switch(value->type)
  {
    case TYPE_UNDEF:
      lua_pushnil(L);
      break;
    case TYPE_BOOLEAN:
      lua_pushboolean(L, value->data.boolean);
      break;
    case TYPE_INTEGER:
      lua_pushinteger(L, value->data.integer);
      break;
    case TYPE_FLOAT:
      lua_pushnumber(L, value->data.floating);
      break;
    case TYPE_STRING:
      lua_pushstring(L, SC_StringToChar(value->data.string));
      break;
    case TYPE_ARRAY:
      push_array(L, value->data.array);
      break;
    case TYPE_HASH:
      push_hash(L, value->data.hash);
      break;
    case TYPE_NAMESPACE:
      push_hash(L, (scDataTypeHash_t*) value->data.namespace);
      break;
    case TYPE_FUNCTION:
      push_function(L, value->data.function);
      break;
    default:
      // TODO: Error here
      break;
  }
}

static void push_path_rec( lua_State *L, const char *path )
{
  char *idx;
  char ns[MAX_NAMESPACE_LENGTH+1];

  idx = index(path, '.');
  if(idx == NULL)
    Q_strncpyz(ns, path, path-idx);
  else
    Q_strncpyz(ns, path, strlen(ns));

  lua_pushstring(L, ns);
  lua_rawget(L, -2);
  if(lua_isnil(L, -1))
  {
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushstring(L, ns);
    lua_rawset(L, -2);
  }

  if(idx != NULL)
    push_path_rec(L, ns);
}

static void push_path(lua_State *L, const char *path)
{
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  push_path_rec(L, path);
}

/*
============
SC_Lua_Init
============
*/
static int stack_dump( lua_State *L )
{
  SC_Lua_DumpStack();
  return 0;
}

static const struct luaL_reg stacklib [] = {
  {"dump", stack_dump},
  {NULL, NULL}
};

void SC_Lua_Init( void )
{
  g_luaState = lua_open();

  luaL_register(g_luaState, "stack", stacklib);
  lua_pop(g_luaState, 1);

  // Lua standard lib
  luaopen_base(g_luaState);
  luaopen_string(g_luaState);
  luaopen_table(g_luaState);
  lua_pop(g_luaState, 4);
}

/*
=================
SC_Lua_Shutdown
=================
*/
void SC_Lua_Shutdown( void )
{
  Com_Printf("------- Game Lua Finalization -------\n");

  if(g_luaState)
  {
    lua_close(g_luaState);
    g_luaState = NULL;
  }

  Com_Printf("-----------------------------------\n");
}

static void update_context(lua_State *L)
{
  // TODO: make better updating system
  scDataTypeHash_t* hash = (scDataTypeHash_t*) namespace_root;
  int i;
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    push_value( L, &hash->data[i].value);
    lua_setglobal( L, SC_StringToChar(&hash->data[i].key));
  }
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

  update_context(g_luaState);

  if(luaL_loadbuffer(g_luaState, buf, strlen(buf), filename))
  {
    Com_Printf("SC_Lua_RunScript: cannot load lua file: %s\n", lua_tostring(g_luaState, -1));
    return qfalse;
  }

  if(lua_pcall(g_luaState, 0, 0, 0))
  {
    Com_Printf("SC_Lua_RunScript: cannot pcall: %s\n", lua_tostring(g_luaState, -1));
    return qfalse;
  }

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

  update_context(L);

  lua_getglobal( L, func->data.path );		// get function

  while( *dt != TYPE_UNDEF )
  {
    push_value( L, args );

    dt++;
    value++;
    narg++;
  }

  // do the call
  if(lua_pcall(L, narg, 1, 0) != 0)	// do the call
    Com_Printf("G_RunLuaFunction: error running function `%s': %s\n", func->data.path, lua_tostring(L, -1));

  pop_value(L, ret, func->return_type);
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

  Com_Printf("--------- SC_Lua_DumpStack ----------\n");
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

void SC_Lua_ImportValue( const char *path, scDataTypeValue_t *value )
{
  lua_State *L = g_luaState;

  push_value( L, value );
  push_path( L, path );
  
  lua_settable( L, -2 );
}

#endif

