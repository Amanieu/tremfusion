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
// sc_lua_metamethod.c

#include "sc_public.h"
#include "sc_local.h"

#ifdef USE_LUA

static const char *to_comparable_string(lua_State *L, int index)
{
  scDataTypeString_t *string;
  int ltype, sctype;
  const char *str = NULL;

  ltype = lua_type(L, index);
  if(ltype == LUA_TSTRING)
    str = lua_tostring(L, index);
  else if(ltype == LUA_TTABLE)
  {
    lua_getmetatable(L, index);
    lua_getfield(L, -1, "_type");

    sctype = lua_tointeger(L, -1);
    if(sctype != TYPE_STRING)
      return NULL;

	lua_getfield(L, -2, "_ref");
    string = *((scDataTypeString_t**) lua_touserdata(L, -1));
    str = SC_StringToChar(string);

    lua_pop(L, 3);
  }
  else
    return NULL;

  return str;
}

/*
========================
invalid metamethods
========================
*/

int SC_Lua_invalid_index_metamethod(lua_State *L)
{
  int type;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);

  luaL_error(L, "attempt to index a %s value", lua_typename(L, SC_Lua_sctype2luatype(type)));

  return 0;
}

int SC_Lua_invalid_length_metamethod(lua_State *L)
{
  int type;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);

  luaL_error(L, "attempt to get length of a %s value", lua_typename(L, SC_Lua_sctype2luatype(type)));

  return 0;
}

/*
========================
generic metamethods
========================
*/

int SC_Lua_concat_metamethod(lua_State *L)
{
  lua_getglobal(L, "tostring");
  if(!lua_isstring(L, 1))
  {
    scDataTypeValue_t value;
    lua_pushvalue(L, -1);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    SC_Lua_pop_value(L, &value, TYPE_STRING);
    lua_pushstring(L, SC_StringToChar(value.data.string));
  }
  else
    lua_pushvalue(L, 1);

  if(!lua_isstring(L, 2))
  {
    scDataTypeValue_t value;
    lua_pushvalue(L, -2);
    lua_pushvalue(L, 2);
    lua_call(L, 1, 1);
    SC_Lua_pop_value(L, &value, TYPE_STRING);
    lua_pushstring(L, SC_StringToChar(value.data.string));
  }
  else
    lua_pushvalue(L, 2);

  lua_concat(L, 2);

  return 1;
}

int SC_Lua_call_metamethod( lua_State *L )
{
  scDataTypeFunction_t *function = NULL;
  int top;
  int i;
  int type;
  int mod = 0;
  int any;
  scClass_t *class;
  scDataTypeValue_t out;
  scDataTypeValue_t in[MAX_FUNCTION_ARGUMENTS+1];
  lua_Debug ar;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");

  if(type == TYPE_FUNCTION)
    function = *((scDataTypeFunction_t**) lua_touserdata(L, -1));
  else if(type == TYPE_CLASS)
  {
    class = *((scClass_t**) lua_touserdata(L, -1));
    function = &class->constructor;
    in[0].type = TYPE_CLASS;
    in[0].data.class = class;
    SC_ValueGCInc(&in[0]);
    mod = 1;
  }
  else
    luaL_error(L, "internal error: can't call a %s (%s (%d))", SC_DataTypeToString(type), __FILE__, __LINE__);

  lua_pop(L, 3);

  top = lua_gettop(L);
  if(function->argument[mod] == TYPE_ANY)
    any = 1;
  else
    any = 0;

  for(i = 0; i < top-1; i++)
  {
    int index = i + mod;
    if(any == 0)
    {
      if(function->argument[index] == TYPE_ANY)
        any = 1;
      else if(function->argument[index] == TYPE_UNDEF)
      {
        if(lua_getstack(L, 0, &ar))
        {
          lua_getinfo(L, "n", &ar);
          luaL_error(L, LUA_QS " takes %d arguments (%d given)", ar.name, i, top-1);
        }
        else
          luaL_error(L, "function takes %d arguments (%d given)", i, top-1);
      }
    }
    if(any == 1)
      SC_Lua_get_value(L, i-top+1, &in[index], TYPE_ANY);
    else
      SC_Lua_get_value(L, i-top+1, &in[index], function->argument[index]);

    SC_ValueGCInc(&in[index]);
  }
  
  if(any == 0 && function->argument[i+mod] != TYPE_UNDEF)
  {
    while(function->argument[i+mod] != TYPE_UNDEF && function->argument[i+mod] != TYPE_ANY)
      i++;

    if(lua_getstack(L, 0, &ar))
    {
      lua_getinfo(L, "n", &ar);
      luaL_error(L, LUA_QS " takes %d arguments (%d given)", ar.name, i, top-1);
    }
    else
      luaL_error(L, "function takes %d arguments (%d given)", i, top-1);
  }
  lua_pop(L, i);

  in[i+mod].type = TYPE_UNDEF;

  if(SC_RunFunction(function, in, &out) != 0)
    luaL_error(L, SC_StringToChar(out.data.string));

  i--;
  while(i >= 0)
  {
    SC_ValueGCDec(&in[i]);
    i--;
  }

  if(out.type != TYPE_UNDEF)
  {
    SC_Lua_push_value(L, &out);
    return 1;
  }

  return 0;
}

int SC_Lua_tostring_metamethod( lua_State *L )
{
  int type;
  void *ref;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");
  ref = *((void**) lua_touserdata(L, -1));

  lua_pushstring(L, va("%s: %p(C)",
      SC_DataTypeToString(type),
      ref));
  return 1;
}

/*
========================
boolean metamethods
========================
*/

int SC_Lua_eq_boolean_metamethod(lua_State *L)
{
  int b1 = SC_Lua_get_arg_boolean(L, 1);
  int b2 = SC_Lua_get_arg_boolean(L, 2);
  lua_pushboolean(L, b1 == b2);
  return 1;
}

int SC_Lua_tostring_boolean_metamethod(lua_State *L)
{
  int b1 = SC_Lua_get_arg_boolean(L, 1);
  if(b1)
    lua_pushstring(L, "true");
  else
    lua_pushstring(L, "false");
  return 1;
}


/*
========================
number metamethods
========================
*/

int SC_Lua_add_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushnumber(L, n1 + n2);
  return 1;
}

int SC_Lua_sub_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushnumber(L, n1 - n2);
  return 1;
}

int SC_Lua_mul_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushnumber(L, n1 * n2);
  return 1;
}

int SC_Lua_div_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  if(n2 == 0)
    lua_pushnumber(L, INFINITY);
  else
    lua_pushnumber(L, n1 / n2);
  return 1;
}

int SC_Lua_mod_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  if(n2 == 0)
    lua_pushnumber(L, NAN);
  else
    lua_pushnumber(L, n1 - floor(n1/n2) * n2);
  return 1;
}

int SC_Lua_pow_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushnumber(L, pow(n1, n2));
  return 1;
}

int SC_Lua_unm_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_pushnumber(L, -n1);
  return 1;
}

int SC_Lua_eq_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushboolean(L, n1 == n2);
  return 1;
}

int SC_Lua_lt_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushboolean(L, n1 < n2);
  return 1;
}

int SC_Lua_le_number_metamethod(lua_State *L)
{
  lua_Number n1 = SC_Lua_get_arg_number(L, 1);
  lua_Number n2 = SC_Lua_get_arg_number(L, 2);
  lua_pushboolean(L, n1 <= n2);
  return 1;
}

int SC_Lua_tostring_number_metamethod(lua_State *L)
{
  lua_Number b1 = SC_Lua_get_arg_number(L, 1);
  lua_pushstring(L, va("%.14g", b1));
  return 1;
}

/*
========================
string metamethods
========================
*/

int SC_Lua_le_string_metamethod(lua_State *L)
{
  const char *str1, *str2;

  str1 = to_comparable_string(L, 1);
  str2 = to_comparable_string(L, 2);

  if(!str1 || !str2)
    lua_pushboolean(L, 0);
  else
    lua_pushboolean(L, strcmp(str1, str2) <= 0);
  return 1;
}

int SC_Lua_lt_string_metamethod(lua_State *L)
{
  const char *str1, *str2;

  str1 = to_comparable_string(L, 1);
  str2 = to_comparable_string(L, 2);

  if(!str1 || !str2)
    lua_pushboolean(L, 0);
  else
    lua_pushboolean(L, strcmp(str1, str2) < 0);
  return 1;
}

int SC_Lua_eq_string_metamethod(lua_State *L)
{
  const char *str1, *str2;

  str1 = to_comparable_string(L, 1);
  str2 = to_comparable_string(L, 2);

  if(!str1 || !str2)
    lua_pushboolean(L, 0);
  else
    lua_pushboolean(L, strcmp(str1, str2) == 0);
  return 1;
}

int SC_Lua_tostring_string_metamethod(lua_State *L)
{
  scDataTypeString_t *string;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  string = *((scDataTypeString_t**) lua_touserdata(L, -1));

  lua_pushstring(L, SC_StringToChar(string));
  return 1;
}

int SC_Lua_len_string_metamethod(lua_State *L)
{
  scDataTypeString_t *string;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  string = *((scDataTypeString_t**) lua_touserdata(L, -1));

  lua_pushinteger(L, string->length);
  return 1;
}

int SC_Lua_gc_string_metamethod(lua_State *L)
{
  scDataTypeString_t *string;

   string = *((scDataTypeString_t**) lua_touserdata(L, -1));
  SC_StringGCDec(string);

  return 0;
}

/*
========================
array metamethods
========================
*/

int SC_Lua_index_array_metamethod(lua_State *L)
{
  scDataTypeArray_t *array;
  scDataTypeValue_t value;
  int nidx;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  array = *((scDataTypeArray_t**) lua_touserdata(L, -1));

  nidx = SC_Lua_get_arg_integer(L, 2);

  SC_ArrayGet(array, nidx-1, &value);
  SC_Lua_push_value(L, &value);

  return 1;
}

int SC_Lua_newindex_array_metamethod(lua_State *L)
{
  scDataTypeArray_t *array;
  scDataTypeValue_t value;
  int nidx;

  SC_Lua_pop_value(L, &value, TYPE_ANY);

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  array = *((scDataTypeArray_t**) lua_touserdata(L, -1));

  nidx = SC_Lua_get_arg_integer(L, 2);
  SC_ArraySet(array, nidx-1, &value);

  return 0;
}

int SC_Lua_len_array_metamethod(lua_State *L)
{
  scDataTypeArray_t *array;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  array = *((scDataTypeArray_t**) lua_touserdata(L, -1));

  lua_pushinteger(L, array->size);
  return 1;
}

int SC_Lua_gc_array_metamethod(lua_State *L)
{
  scDataTypeArray_t *array;

  array = *((scDataTypeArray_t**) lua_touserdata(L, -1));
  SC_ArrayGCDec(array);

  return 0;
}

/*
========================
hash metamethods
========================
*/

int SC_Lua_len_hash_metamethod(lua_State *L)
{
  scDataTypeHash_t *hash;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  hash = *((scDataTypeHash_t**) lua_touserdata(L, -1));

  lua_pushinteger(L, hash->size);
  return 1;
}

int SC_Lua_index_hash_metamethod(lua_State *L)
{
  scDataTypeHash_t *hash;
  scDataTypeValue_t value;
  const char *sidx;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  hash = *((scDataTypeHash_t**) lua_touserdata(L, -1));

  value.type = TYPE_HASH;
  value.data.hash = hash;

  sidx = SC_Lua_get_arg_string(L, 2);
  SC_HashGet(hash, sidx, &value);
  SC_Lua_push_value(L, &value);

  return 1;
}

int SC_Lua_newindex_hash_metamethod(lua_State *L)
{
  scDataTypeHash_t *hash;
  scDataTypeValue_t value;
  const char *sidx;

  SC_Lua_pop_value(L, &value, TYPE_ANY);

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  hash = *((scDataTypeHash_t**) lua_touserdata(L, -1));

  sidx = SC_Lua_get_arg_string(L, 2);
  SC_HashSet(hash, sidx, &value);

  return 0;
}

int SC_Lua_gc_hash_metamethod(lua_State *L)
{
  scDataTypeHash_t *hash;

  hash = *((scDataTypeHash_t**) lua_touserdata(L, -1));
  SC_HashGCDec(hash);

  return 0;
}

/*
========================
function metamethods
========================
*/

int SC_Lua_gc_function_metamethod(lua_State *L)
{
  scDataTypeFunction_t *function;

  function = *((scDataTypeFunction_t**) lua_touserdata(L, -1));
  SC_FunctionGCDec(function);

  return 0;
}

/*
========================
class metamethods
========================
*/

int SC_Lua_gc_class_metamethod(lua_State *L)
{
  scClass_t *class;

  class = *((scClass_t**) lua_touserdata(L, -1));
  //SC_ClassGCDec(class);

  return 0;
}

/*
========================
object metamethods
========================
*/

int SC_Lua_gc_object_metamethod(lua_State *L)
{
  scObject_t *object;

  object = *((scObject_t**) lua_touserdata(L, -1));
  SC_ObjectGCDec(object);

  return 0;
}

int SC_Lua_object_index_metamethod(lua_State *L)
{
  scObject_t *object;
  scObjectMethod_t *method;
  scObjectMember_t *member;
  scField_t        *field;
  scDataTypeValue_t in[MAX_FUNCTION_ARGUMENTS+1];
  scDataTypeValue_t out;
  const char *name;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  object = *((scObject_t**) lua_touserdata(L, -1));
  name = luaL_checkstring(L, 2);

  method = SC_ClassGetMethod(object->class, name);
  if(method)
  {
    SC_Lua_push_function(L, &method->method);
    return 1;
  }

  field = SC_ClassGetField(object->class, name);
  if(field)
  {
    // Call SC_Field_Get, push results
    SC_Field_Get(object, field, &out);
    
    SC_Lua_push_value(L, &out);
    return 1;
  }

  member = SC_ClassGetMember(object->class, name);
  if(member)
  {
    // Call 'get' method, push result
    in[0].type = TYPE_OBJECT;
    in[0].data.object = object;
	if(strcmp(member->name, "_") == 0)
	{
      in[1].type = TYPE_STRING;
      in[1].data.string = SC_StringNewFromChar(name);
      in[2].type = TYPE_UNDEF;
    }
    else
      in[1].type = TYPE_UNDEF;

    if(SC_RunFunction(&member->get, in, &out) != 0)
      luaL_error(L, SC_StringToChar(out.data.string));

    SC_Lua_push_value(L, &out);
    return 1;
  }

  luaL_error(L, va("instance has no attribute `%s'", name));
  return 0;
}

int SC_Lua_object_newindex_metamethod(lua_State *L)
{
  scObject_t *object;
  scObjectMember_t *member;
  scField_t        *field;
  scDataTypeValue_t in[MAX_FUNCTION_ARGUMENTS+1];
  scDataTypeValue_t out;
  const char *name;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  object = *((scObject_t**) lua_touserdata(L, -1));
  lua_pop(L, 2);

  name = luaL_checkstring(L, 2);

  field  = SC_ClassGetField(object->class, name);
  if(field)
  {
    // Call SC_Field_Set with popped value
    SC_Lua_pop_value(L, &in[1], field->type);
    
    SC_Field_Set( object, field, &in[1] );

    return 0;
  }

  member = SC_ClassGetMember(object->class, name);
  if(member)
  {
    in[0].type = TYPE_OBJECT;
    in[0].data.object = object;
    // Call 'set' method with popped value
    if(strcmp(member->name, "_") == 0)
    {
      in[1].type = TYPE_STRING;
      in[1].data.string = SC_StringNewFromChar(name);

      SC_Lua_pop_value(L, &in[2], member->set.argument[1]);
      in[3].type = TYPE_UNDEF;
    }
    else
    {
      SC_Lua_pop_value(L, &in[1], member->set.argument[1]);
      in[2].type = TYPE_UNDEF;
    }

    if(SC_RunFunction(&member->set, in, &out) != 0)
      luaL_error(L, SC_StringToChar(out.data.string));

    return 0;
  }

  luaL_error(L, va("instance has no attribute `%s'", name));
  return 0;
}

#endif

