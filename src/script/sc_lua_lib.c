/*
===========================================================================
Copyright (C) 2008 Maurice Doison

This file is part of Tremfusion source code.

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
// sc_lua_lib.c

#include "sc_public.h"
#include "sc_local.h"

#ifdef USE_LUA

static int method_disabled(lua_State *L)
{
  luaL_error(L, "function disabled");

  return 0;
}

static int print_method(lua_State *L)
{
  int top, i;

  top = lua_gettop(L);
  lua_getglobal(L, "tostring");
  for(i = 1; i <= top; i++)
  {
    const char *s;
    lua_pushvalue(L, -1);
    lua_pushvalue(L, i);
    lua_call(L, 1, 1);
    s = lua_tostring(L, -1);
    if(s == NULL)
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
          LUA_QL("print"));

    if(i > 1)
      Com_Printf("\t");

    Com_Printf("%s", s);
    lua_pop(L, 1);
  }
  Com_Printf("\n");

  return 0;
}

static int type_method(lua_State *L)
{
  int type;

  // If data is userdata, find type in data
  if(lua_istable(L, 1) && lua_getmetatable(L, 1))
  {
    lua_getfield(L, -1, "_type");
    type = lua_tointeger(L, -1);
    lua_pushstring(L, lua_typename(L, SC_Lua_sctype2luatype(type)));
  }
  else
  {
    // get original `type' method
    lua_getfield(L, LUA_REGISTRYINDEX, "_type");
    lua_insert(L, -2);

    lua_call(L, 1, 1);
  }

  return 1;
}

static int next_method(lua_State *L)
{
  int type, keystate;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;
  const char *key;
  const char *string;

  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  luaL_checktype(L, 1, LUA_TTABLE);

  if(lua_getmetatable(L, 1))
  {
    lua_getfield(L, -1, "_type");
    type = lua_tointeger(L, -1);
    lua_getfield(L, -2, "_ref");

    switch(type)
    {
      case TYPE_ARRAY:
        array = *((scDataTypeArray_t**) lua_touserdata(L, -1));

        if(lua_isnil(L, 2))
          keystate = 0;
        else
          SC_Lua_get_integer(L, 2, &keystate);

        while(keystate < array->buflen)
        {
          SC_ArrayGet(array, keystate, &value);
          if(value.type == TYPE_UNDEF)
            keystate++;
          else
          {
            // push index as return value
            lua_pushinteger(L, keystate+1);

            // push array value as return value
            SC_Lua_push_value(L, &value);

            return 2;
          }
        }

        // no more entries, push nil
        lua_pushnil(L);
        return 1;

      case TYPE_HASH:
      case TYPE_NAMESPACE:
        // get hash data
        hash = *((scDataTypeHash_t**) lua_touserdata(L, -1));

        if(lua_isnil(L, 2))
          key = SC_HashFirst(hash, &value);
        else
        {
          SC_Lua_get_string(L, 2, &string);
          key = SC_HashNext(hash, string, &value);
        }

        if(key)
        {
          // push key
          lua_pushstring(L, key);

          // push value
          SC_Lua_push_value(L, &value);

          return 2;
        }
        else
        {
          // no more values, push nil
          lua_pushnil(L);
          return 1;
        }

      default:
        luaL_error(L, "lua internal error in %s (%d) : unknow datatype", __FILE__, __LINE__);
    }
  }
  else
  {
    lua_getfield(L, LUA_REGISTRYINDEX, "_next");
    lua_insert(L, 1);

    lua_call(L, 2, 2);

    return 2;
  }

  return 0;
}

static int ipairs_closure(lua_State *L)
{
  int keystate;
  int type;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;

  keystate = lua_tointeger(L, lua_upvalueindex(1));

  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");

  switch(type)
  {
    case TYPE_ARRAY:
      array = *((scDataTypeArray_t**) lua_touserdata(L, -1));

      if(keystate < array->buflen)
      {
        SC_ArrayGet(array, keystate, &value);
        if(value.type != TYPE_UNDEF)
        {
          // push index as return value
          lua_pushinteger(L, keystate+1);

          // update upvalue 
          keystate++;
          lua_pushinteger(L, keystate);
          lua_replace(L, lua_upvalueindex(1));

          // push array value as return value
          SC_Lua_push_value(L, &value);

          return 2;
        }
      }

      // no more entries, push nil
      lua_pushnil(L);
      return 1;

    case TYPE_HASH:
    case TYPE_NAMESPACE:
      // get hash data
      hash = *((scDataTypeHash_t**) lua_touserdata(L, -1));

      // lookup in hashtable from current index to max hashtable size
      if(keystate < hash->buflen)
      {
        // Find next used entry in hashtable
        if(!SC_StringIsEmpty(&hash->data[keystate].key))
        {
          // push key
          SC_Lua_push_string(L, &hash->data[keystate].key);

          // push value
          SC_Lua_push_value(L, &hash->data[keystate].value);

          // update upvalue
          keystate++;
          lua_pushinteger(L, keystate);
          lua_replace(L, lua_upvalueindex(1));

          return 2;
        }
      }

      // no more values, push nil
      lua_pushnil(L);
      return 1;
  }

  luaL_error(L, "lua internal error in %s (%d) : unknow datatype", __FILE__, __LINE__);

  return 0;
}

static int pairs_method(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TTABLE);

  if(lua_getmetatable(L, 1))
  {
    // Push integer as closure upvalue
    lua_pushcclosure(L, next_method, 0);

    // Push table as first argument
    lua_pushvalue(L, 1);

    lua_pushnil(L);

    return 3;
  }
  else
  {
    lua_getfield(L, LUA_REGISTRYINDEX, "_pairs");
    lua_replace(L, -3);

    lua_call(L, 1, 3);

    return 3;
  }
}

static int ipairs_method(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TTABLE);

  if(lua_getmetatable(L, 1))
  {
    // Push integer as closure upvalue
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, ipairs_closure, 1);

    // Push table as first argument
    lua_pushvalue(L, 1);

    lua_pushnil(L);

    return 3;
  }
  else
  {
    lua_getfield(L, LUA_REGISTRYINDEX, "_ipairs");
    lua_replace(L, -3);

    lua_call(L, 1, 3);

    return 3;
  }
}

static int stack_dump( lua_State *L )
{
  SC_Lua_StackDump(L);
  return 0;
}

static int register_boolean(lua_State *L)
{
  char boolean;

  if(!SC_Lua_is_registered(L, 1, TYPE_BOOLEAN))
  {
    boolean = lua_toboolean(L, 1);
    SC_Lua_push_boolean(L, boolean);
  }
  return 1;
}

static int register_integer(lua_State *L)
{
  int integer;

  if(!SC_Lua_is_registered(L, 1, TYPE_INTEGER))
  {
    integer = (int)lua_tonumber(L, 1);
    SC_Lua_push_integer(L, integer);
  }
  return 1;
}

static int register_float(lua_State *L)
{
  float floating;

  if(!SC_Lua_is_registered(L, 1, TYPE_FLOAT))
  {
    floating = lua_tonumber(L, 1);
    SC_Lua_push_float(L, floating);
  }
  return 1;
}

static int register_string(lua_State *L)
{
  scDataTypeString_t *string;

  if(!SC_Lua_is_registered(L, 1, TYPE_STRING))
  {
    string = SC_Lua_pop_lua_string(L);
    SC_Lua_push_string(L, string);
  }
  return 1;
}

static int register_function(lua_State *L)
{
  scDataTypeFunction_t *function;

  if(!SC_Lua_is_registered(L, 1, TYPE_FUNCTION))
  {
    function = SC_Lua_pop_lua_function(L);
    SC_Lua_push_function(L, function);
  }
  return 1;
}

static int register_array(lua_State *L)
{
  scDataTypeArray_t *array;

  if(!SC_Lua_is_registered(L, 1, TYPE_ARRAY))
  {
    array = SC_Lua_pop_lua_array(L);
    SC_Lua_push_array(L, array);
  }
  return 1;
}

static int register_hash(lua_State *L)
{
  scDataTypeHash_t *hash;

  if(!SC_Lua_is_registered(L, 1, TYPE_HASH))
  {
    hash = SC_Lua_pop_lua_hash(L);
    SC_Lua_push_hash(L, hash, TYPE_HASH);
  }
  return 1;
}

static int register_namespace(lua_State *L)
{
  scDataTypeHash_t *namespace;

  if(!SC_Lua_is_registered(L, 1, TYPE_NAMESPACE))
  {
    namespace = SC_Lua_pop_lua_hash(L);
    SC_Lua_push_hash(L, namespace, TYPE_NAMESPACE);
  }
  return 1;
}

static int registered(lua_State *L)
{
  lua_pushboolean(L, SC_Lua_is_registered(L, 1, TYPE_ANY));

  return 1;
}

static int sctype(lua_State *L)
{
  const char *str;

  if(lua_istable(L, 1) && lua_getmetatable(L, 1))
  {
    lua_getfield(L, -1, "_type");
    if(!lua_isnumber(L, -1))
    {
      lua_pushstring(L, SC_DataTypeToString(TYPE_UNDEF));
      lua_pop(L, 2);
    }
    else
    {
      str = SC_DataTypeToString(lua_tointeger(L, -1));
      lua_pop(L, 2);
      lua_pushstring(L, str);
    }
  }
  else
    lua_pushstring(L, SC_DataTypeToString(TYPE_UNDEF));

  return 1;
}

static const struct luaL_reg lualib[] = {
  { "dump", stack_dump },
  // scripting data registeration
  { "boolean", register_boolean },
  { "integer", register_integer },
  { "float", register_float },
  { "string", register_string },
  { "func", register_function },
  { "array", register_array },
  { "hash", register_hash },
  { "namespace", register_namespace },
  { "registered", registered },
  { "sctype", sctype },
  {NULL, NULL}
};

static void map_luamethod(lua_State *L, const char *name, lua_CFunction func)
{
  lua_getglobal(L, name);
  lua_setfield(L, LUA_REGISTRYINDEX, va("_%s", name));
  lua_pushcfunction(L, func);
  lua_setglobal(L, name);
}

void SC_Lua_loadlib(lua_State *L)
{
  map_luamethod(L, "ipairs", ipairs_method);
  map_luamethod(L, "pairs", pairs_method);
  map_luamethod(L, "next", next_method);
  map_luamethod(L, "type", type_method);
  map_luamethod(L, "print", print_method);

  map_luamethod(L, "getfenv", method_disabled);
  map_luamethod(L, "setfenv", method_disabled);
  map_luamethod(L, "gcinfo", method_disabled);
  map_luamethod(L, "collectgarbage", method_disabled);
  map_luamethod(L, "newproxy", method_disabled);

  // disable functions because they don't work with current implementation
  // (and for security reason)
  map_luamethod(L, "load", method_disabled);
  map_luamethod(L, "loadfile", method_disabled);
  map_luamethod(L, "loadstring", method_disabled);
  map_luamethod(L, "dofile", method_disabled);
  map_luamethod(L, "assert", method_disabled);
  map_luamethod(L, "error", method_disabled);
  map_luamethod(L, "tonumber", method_disabled);
  map_luamethod(L, "unpack", method_disabled);
  map_luamethod(L, "select", method_disabled);
  //map_luamethod(L, "module", method_disabled);
  map_luamethod(L, "require", method_disabled);

  // disable coroutines (not tested, security issues...)
  lua_pushnil(L);
  lua_setglobal(L, "coroutine");

  luaL_register(L, "lua", lualib);
  lua_pop(L, 1);
}

#endif

