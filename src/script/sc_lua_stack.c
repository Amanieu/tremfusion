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
// sc_lua_stack.c

#ifdef USE_LUA

#include "sc_public.h"
#include "sc_local.h"

static scLuaFunc_t luaFunc_id = 0;

void fregister(scDataTypeFunction_t *function)
{
  lua_setfield(g_luaState, LUA_REGISTRYINDEX, va("func_%d", luaFunc_id));
  function->data.luafunc = luaFunc_id;

  luaFunc_id++;
}

void SC_Lua_push_boolean(lua_State *L, char boolean)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_eq_boolean_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, SC_Lua_tostring_boolean_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_BOOLEAN);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushboolean(L, boolean);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_integer(lua_State *L, int integer)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_add_number_metamethod);
  lua_setfield(L, -2, "__add");
  lua_pushcfunction(L, SC_Lua_sub_number_metamethod);
  lua_setfield(L, -2, "__sub");
  lua_pushcfunction(L, SC_Lua_mul_number_metamethod);
  lua_setfield(L, -2, "__mul");
  lua_pushcfunction(L, SC_Lua_div_number_metamethod);
  lua_setfield(L, -2, "__div");
  lua_pushcfunction(L, SC_Lua_mod_number_metamethod);
  lua_setfield(L, -2, "__mod");
  lua_pushcfunction(L, SC_Lua_pow_number_metamethod);
  lua_setfield(L, -2, "__pow");
  lua_pushcfunction(L, SC_Lua_unm_number_metamethod);
  lua_setfield(L, -2, "__unm");
  lua_pushcfunction(L, SC_Lua_concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, SC_Lua_eq_number_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, SC_Lua_lt_number_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, SC_Lua_le_number_metamethod);
  lua_setfield(L, -2, "__le");
  lua_pushcfunction(L, SC_Lua_tostring_number_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_INTEGER);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushinteger(L, integer);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_float(lua_State *L, float floating)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_add_number_metamethod);
  lua_setfield(L, -2, "__add");
  lua_pushcfunction(L, SC_Lua_sub_number_metamethod);
  lua_setfield(L, -2, "__sub");
  lua_pushcfunction(L, SC_Lua_mul_number_metamethod);
  lua_setfield(L, -2, "__mul");
  lua_pushcfunction(L, SC_Lua_div_number_metamethod);
  lua_setfield(L, -2, "__div");
  lua_pushcfunction(L, SC_Lua_mod_number_metamethod);
  lua_setfield(L, -2, "__mod");
  lua_pushcfunction(L, SC_Lua_pow_number_metamethod);
  lua_setfield(L, -2, "__pow");
  lua_pushcfunction(L, SC_Lua_unm_number_metamethod);
  lua_setfield(L, -2, "__unm");
  lua_pushcfunction(L, SC_Lua_concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, SC_Lua_eq_number_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, SC_Lua_lt_number_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, SC_Lua_le_number_metamethod);
  lua_setfield(L, -2, "__le");
  lua_pushcfunction(L, SC_Lua_tostring_number_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_FLOAT);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushnumber(L, floating);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_userdata(lua_State *L, void* userdata)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_USERDATA);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, userdata);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_string(lua_State *L, scDataTypeString_t *string)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for string
  SC_StringGCInc(string);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, SC_Lua_len_string_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_eq_string_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, SC_Lua_lt_string_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, SC_Lua_le_string_metamethod);
  lua_setfield(L, -2, "__le");
  lua_pushcfunction(L, SC_Lua_tostring_string_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_STRING);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, string);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_string_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_array( lua_State *L, scDataTypeArray_t *array )
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for array
  SC_ArrayGCInc(array);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_index_array_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_newindex_array_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_len_array_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_ARRAY);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, array);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_array_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_hash( lua_State *L, scDataTypeHash_t *hash, scDataType_t type )
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for hash
  SC_HashGCInc(hash);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_index_hash_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_newindex_hash_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_len_hash_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, type);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, hash);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_hash_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_function( lua_State *L, scDataTypeFunction_t *function )
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for hash
  SC_FunctionGCInc(function);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_FUNCTION);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, function);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_function_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_class(lua_State *L, scClass_t *class)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for class
  //SC_ClassGCInc(class);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");
  
  lua_pushinteger(L, TYPE_CLASS);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, class);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_class_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_object(lua_State *L, scObject_t *object)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for object
  SC_ObjectGCInc(object);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, SC_Lua_object_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, SC_Lua_object_newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, SC_Lua_invalid_metatable_metamethod);
  lua_setfield(L, -2, "__metatable");

  lua_pushinteger(L, TYPE_OBJECT);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, object);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, SC_Lua_gc_object_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

void SC_Lua_push_value( lua_State *L, scDataTypeValue_t *value )
{
  switch(value->type)
  {
    case TYPE_UNDEF:
      lua_pushnil(L);
      break;
    case TYPE_BOOLEAN:
      SC_Lua_push_boolean(L, value->data.boolean);
      break;
    case TYPE_INTEGER:
      SC_Lua_push_integer(L, value->data.integer);
      break;
    case TYPE_FLOAT:
      SC_Lua_push_float(L, value->data.floating);
      break;
    case TYPE_USERDATA:
      SC_Lua_push_userdata(L, value->data.userdata);
      break;
    case TYPE_STRING:
	  SC_Lua_push_string(L, value->data.string);
      break;
    case TYPE_ARRAY:
      SC_Lua_push_array(L, value->data.array);
      break;
    case TYPE_HASH:
      SC_Lua_push_hash(L, value->data.hash, TYPE_HASH);
      break;
    case TYPE_NAMESPACE:
      SC_Lua_push_hash(L, (scDataTypeHash_t*) value->data.namespace, TYPE_NAMESPACE);
      break;
    case TYPE_FUNCTION:
      SC_Lua_push_function(L, value->data.function);
      break;
    case TYPE_OBJECT:
      SC_Lua_push_object(L, value->data.object);
      break;
    case TYPE_CLASS:
      SC_Lua_push_class(L, value->data.class);
      break;
    default:
      luaL_error(L, "internal error : attempt to push an unknow value (%d) in %s (%d)", value->type, __FILE__, __LINE__);
      break;
  }
}

scDataTypeString_t* SC_Lua_pop_lua_string(lua_State *L)
{
  const char *lstr;
  scDataTypeString_t *string;

  lstr = lua_tostring(L, -1);
  string = SC_StringNewFromChar(lstr);
  lua_pop(L, 1);

  return string;
}

scDataTypeArray_t* SC_Lua_pop_lua_array(lua_State *L)
{
  scDataTypeValue_t val;
  scDataTypeArray_t *array;

  array = SC_ArrayNew();

  lua_pushnil(L);
  while(lua_next(L, -2) != 0)
  {
    SC_Lua_pop_value(L, &val, TYPE_ANY);
    SC_ArraySet(array, lua_tointeger(L, -1) - 1, &val);
  }

  lua_pop(L, 1);

  return array;
}

scDataTypeHash_t* SC_Lua_pop_lua_hash(lua_State *L)
{
  scDataTypeValue_t val;
  scDataTypeString_t *key;
  const char *lstr;

  scDataTypeHash_t *hash = SC_HashNew();

  lua_pushnil(L);
  while(lua_next(L, -2) != 0)
  {
    lstr = lua_tostring(L, -2);
    key = SC_StringNewFromChar(lstr);
    SC_Lua_pop_value(L, &val, TYPE_ANY);
    SC_HashSet(hash, SC_StringToChar(key), &val);
  }

  lua_pop(L, 1);

  return hash;
}

scDataTypeFunction_t* SC_Lua_pop_lua_function(lua_State *L)
{
  scDataTypeFunction_t *function;

  function = SC_FunctionNew();
  function->langage = LANGAGE_LUA;
  // Impossible to know arguments type, set generic types
  function->argument[0] = TYPE_ANY;
  function->argument[1] = TYPE_UNDEF;
  function->return_type = TYPE_ANY;
  function->closure = NULL;
  fregister(function);

  return function;
}

void SC_Lua_pop_value(lua_State *L, scDataTypeValue_t *value, scDataType_t type)
{
  int ltype = lua_type(L, -1);

  switch(ltype)
  {
    case LUA_TNIL:
      value->type = TYPE_UNDEF;
      lua_pop(L, 1);
      break;

    case LUA_TNUMBER:
      if(type == TYPE_INTEGER)
      {
        value->type = TYPE_INTEGER;
        value->data.integer = lua_tointeger(L, -1);
      }
      else
      {
        value->type = TYPE_FLOAT;
        value->data.floating = lua_tonumber(L, -1);
      }
      lua_pop(L, 1);
      break;

    case LUA_TBOOLEAN:
      value->type = TYPE_BOOLEAN;
      value->data.boolean = lua_toboolean(L, -1);
      lua_pop(L, 1);
      break;

    case LUA_TUSERDATA:
      value->type = TYPE_USERDATA;
      value->data.userdata = lua_touserdata(L, -1);
      lua_pop(L, 1);
      break;

    case LUA_TSTRING:
      value->type = TYPE_STRING;
      value->data.string = SC_Lua_pop_lua_string(L);
      break;

    case LUA_TTABLE:
      if(lua_getmetatable(L, -1))
      {
        lua_getfield(L, -1, "_type");
        value->type = lua_tointeger(L, -1);

        lua_getfield(L, -2, "_ref");
        value->data.any = lua_touserdata(L, -1);

        lua_pop(L, 4);
      }
      else
      {
        if(type == TYPE_ARRAY)
        {
          value->type = TYPE_ARRAY;
          value->data.array = SC_Lua_pop_lua_array(L);
        }
        else if(type == TYPE_NAMESPACE)
        {
          value->type = TYPE_NAMESPACE;
          value->data.hash = SC_Lua_pop_lua_hash(L);
        }
        else if(type == TYPE_ANY)
        {
          // If there is only integers as key, table is an array
          int n = 0;

          type = TYPE_ARRAY;
          lua_pushnil(L);
          while(lua_next(L, -2) != 0)
          {
            if(lua_type(L, -2) != LUA_TNUMBER && lua_tointeger(L, -2) != n)
            {
              type = TYPE_HASH;
              break;
            }
            n++;
            lua_pop(L, 1);
          }

          if(type == TYPE_ARRAY)
          {
            value->type = TYPE_ARRAY;
            value->data.array = SC_Lua_pop_lua_array(L);
          }
          else
          {
            value->type = TYPE_HASH;
            value->data.hash = SC_Lua_pop_lua_hash(L);
          }
        }
        else
        {
          value->type = TYPE_HASH;
          value->data.hash = SC_Lua_pop_lua_hash(L);
        }
      }
      break;

    case LUA_TFUNCTION:
      value->type = TYPE_FUNCTION;
      value->data.function = SC_Lua_pop_lua_function(L);
      break;

    default:
      luaL_error(L, "internal error : attempt to pop an unknow value (%s) in %s (%d)", lua_typename(L, ltype), __FILE__, __LINE__);
      break;
  }
}

const char* SC_Lua_get_string(lua_State *L, int index)
{
  // TODO: check errors

  if(lua_istable(L, index))
  {
    scDataTypeString_t *string;

    lua_getmetatable(L, index);
    lua_getfield(L, -1, "_ref");
    string = lua_touserdata(L, -1);
    
    lua_pop(L, 2);
    return SC_StringToChar(string);
  }
  else
    return lua_tostring(L, index);
}

int SC_Lua_get_boolean(lua_State *L, int index)
{
  // TODO: check errors

  if(lua_istable(L, index))
  {
    union { void* v; int n; } ud;

    lua_getmetatable(L, index);
    lua_getfield(L, -1, "_ref");
    ud.v = lua_touserdata(L, -1);
    
    lua_pop(L, 2);
    return ud.n;
  }
  else
    return lua_toboolean(L, index);
}

int SC_Lua_get_integer(lua_State *L, int index)
{
  // TODO: check errors

  if(lua_istable(L, index))
  {
    union { void* v; int n; } ud;

    lua_getmetatable(L, index);
    lua_getfield(L, -1, "_ref");
    ud.v = lua_touserdata(L, -1);
    
    lua_pop(L, 2);
    return ud.n;
  }
  else
    return lua_tointeger(L, index);
}

lua_Number SC_Lua_get_number(lua_State *L, int index)
{
  // TODO: check errors
  int type;
  lua_Number number = 0.;

  if(lua_istable(L, index))
  {
    union { void* v; int n; float f; } ud;

    lua_getmetatable(L, index);
    lua_getfield(L, -1, "_type");
    type = lua_tointeger(L, -1);
    lua_getfield(L, -1, "_ref");
    
    switch(type)
    {
      case TYPE_INTEGER:
        ud.v = lua_touserdata(L, -1);
        number = ud.n;
        break;

      case TYPE_FLOAT:
        ud.v = lua_touserdata(L, -1);
        number = ud.f;
        break;

      default:
        // TODO: error
        break;
    }
    
    lua_pop(L, 3);
    return number;
  }
  else
    return lua_tonumber(L, index);
}

#endif

