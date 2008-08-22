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
 * - Add methods to make scripting registered data under lua
 * - `lua.registered' lua function
 * - Disable metatable access from lua
 * - Make lua type for TYPE_FLOAT, TYPE_INTEGER, TYPE_BOOLEAN (and metamethod)
 * - Bad GC when pop a value ?
 * - Error messages should always show script line
 * - Check number of arguments for (non metamethods) lua functions
 * - replace `luastring2string' by `lua_typename' / `luaL_typename'
 * - review errors with functions `luaL_argerror' / 'luaL_opterror'
 * - explode lua stuff in many files
 */

#ifdef USE_LUA

#include "sc_public.h"
#include "sc_lua.h"

#define MAX_LUAFILE 32768

lua_State *g_luaState = NULL;

static void push_value(lua_State *L, scDataTypeValue_t *value);
static void pop_value(lua_State *L, scDataTypeValue_t *value, scDataType_t type);
static void push_string(lua_State *L, scDataTypeString_t *string);
static scDataTypeString_t* pop_lua_string(lua_State *L);

static void push_function( lua_State *L, scDataTypeFunction_t *function );

/*static scDataType_t luatype2sctype(int luatype)
{
  switch(luatype)
  {
    case LUA_TNUMBER: return TYPE_FLOAT;
    case LUA_TBOOLEAN: return TYPE_BOOLEAN;
    case LUA_TSTRING: return TYPE_STRING;
    case LUA_TTABLE: return TYPE_HASH;
    case LUA_TFUNCTION: return TYPE_FUNCTION;
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA: return TYPE_USERDATA;
    case LUA_TTHREAD:
    case LUA_TNIL:
    default: return TYPE_UNDEF;
  }
}*/

static int sctype2luatype(scDataType_t sctype)
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

static const char* luatype2string(int luatype)
{
  switch(luatype)
  {
    case LUA_TNIL: return "nil";
    case LUA_TNUMBER: return "number";
    case LUA_TBOOLEAN: return "boolean";
    case LUA_TSTRING: return "string";
    case LUA_TTABLE: return "table";
    case LUA_TFUNCTION: return "function";
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA: return "userdata";
    case LUA_TTHREAD: return "thread";
  }

  return NULL;
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

    Com_Printf(s);
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
    lua_pushstring(L, luatype2string(sctype2luatype(type)));
  }
  else
  {
    // get original `type' method
    lua_getglobal(L, "_type");
    lua_insert(L, -2);

    lua_call(L, 1, 1);
  }

  return 1;
}

static int pairs_closure(lua_State *L)
{
  // TODO: test me
  int keystate;
  int type;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;

  keystate = lua_tointeger(L, lua_upvalueindex(1));

  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checktype(L, 2, LUA_TSTRING);

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -1, "_ref");

  switch(type)
  {
    case TYPE_ARRAY:
      array = lua_touserdata(L, -1);

      if(keystate >= array->size)
      {
        // no more entries, push nil
        lua_pushnil(L);
        return 1;
      }
      else
      {
        SC_ArrayGet(array, keystate, &value);

        // push index as return value
        lua_pushinteger(L, keystate);

        // update upvalue 
        keystate++;
        lua_pushinteger(L, keystate);
        lua_replace(L, lua_upvalueindex(1));

        // push array value as return value
        push_value(L, &value);

        return 2;
      }

    case TYPE_HASH:
    case TYPE_NAMESPACE:
      // get hash data
      hash = lua_touserdata(L, -1);

      // lookup in hashtable from current index to max hashtable size
      while(keystate < hash->buflen)
      {
        // Find next used entry in hashtable
        if(SC_StringIsEmpty(&hash->data[keystate].key))
          keystate++;
        else
        {
          // update upvalue
          lua_pushinteger(L, keystate);
          lua_replace(L, lua_upvalueindex(1));

          // push key
          push_string(L, &hash->data[keystate].key);

          // push value
          push_value(L, &hash->data[keystate].value);

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
  // TODO: test me
  luaL_checktype(L, 1, LUA_TTABLE);

  if(lua_getmetatable(L, 1))
  {
    // Push integer as closure upvalue
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, pairs_closure, 1);

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

static int invalid_index_metamethod(lua_State *L)
{
  int type;
  int top;

  top = lua_gettop(L);

  lua_getfield(L, -top, "_type");
  type = lua_tointeger(L, -1);

  luaL_error(L, "attempt to index a %s value", luatype2string(sctype2luatype(type)));

  return 0;
}

static int invalid_length_metamethod(lua_State *L)
{
  int type;
  int top;

  top = lua_gettop(L);

  lua_getfield(L, -top, "_type");
  type = lua_tointeger(L, -1);

  luaL_error(L, "attempt to get length of a %s value", luatype2string(sctype2luatype(type)));

  return 0;
}

static int tostring_metamethod(lua_State *L)
{
  // TODO: test me
  int type;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);

  if(type == TYPE_STRING)
  {
    scDataTypeString_t *string;

    lua_getfield(L, -2, "_ref");
    string = lua_touserdata(L, -1);

    lua_pushstring(L, SC_StringToChar(string));
  }
  else
  {
    // TODO: tostring for other values
    lua_pop(L, 1);
    lua_pushstring(L, "");
  }

  return 1;
}

static const char *to_comparable_string(lua_State *L, int index, int left)
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
    {
      if(left)
        luaL_error(L, "attempt to compare %s with string", luatype2string(sctype2luatype(sctype)));
      else
        luaL_error(L, "attempt to compare string with %s", luatype2string(sctype2luatype(sctype)));
    }

    string = lua_touserdata(L, -1);
    str = SC_StringToChar(string);

    lua_pop(L, 3);
  }
  else
  {
    if(left)
      luaL_error(L, "attempt to compare %s with string", luatype2string(ltype));
    else
      luaL_error(L, "attempt to compare string with %s", luatype2string(ltype));
  }

  return str;
}

static int le_string_metamethod(lua_State *L)
{
  // TODO: test me
  const char *str1, *str2;

  str1 = to_comparable_string(L, 1, 1);
  str2 = to_comparable_string(L, 2, 0);

  return strcmp(str1, str2) <= 0;
}

static int lt_string_metamethod(lua_State *L)
{
  // TODO: test me
  const char *str1, *str2;

  str1 = to_comparable_string(L, 1, 1);
  str2 = to_comparable_string(L, 2, 0);

  return strcmp(str1, str2) < 0;
}

static int eq_string_metamethod(lua_State *L)
{
  // TODO: test me
  const char *str1, *str2;

  str1 = to_comparable_string(L, 1, 1);
  str2 = to_comparable_string(L, 2, 0);

  return strcmp(str1, str2) == 0;
}

static int len_metamethod(lua_State *L)
{
  // TODO: test me
  int type;
  scDataTypeString_t *string;
  scDataTypeArray_t *array;
  scDataTypeHash_t *hash;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");

  switch(type)
  {
    case TYPE_STRING:
      string = lua_touserdata(L, -1);

      return string->length;

    case TYPE_ARRAY:
      array = lua_touserdata(L, -1);

      return array->size;

    case TYPE_HASH:
    case TYPE_NAMESPACE:
      hash = lua_touserdata(L, -1);

      return hash->size;

    default:
      luaL_error(L, "attempt to get length of a %s value", luatype2string(sctype2luatype(type)));
  }

  return 0;
}

static int concat_metamethod(lua_State *L)
{
  // TODO: test me
  lua_getglobal(L, "tostring");
  if(!lua_isstring(L, 1))
  {
    lua_pushvalue(L, -1);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
  }
  else
    lua_pushvalue(L, 1);

  if(!lua_isstring(L, 2))
  {
    lua_pushvalue(L, -2);
    lua_pushvalue(L, 2);
    lua_call(L, 1, 1);
  }
  else
    lua_pushvalue(L, 2);

  lua_concat(L, 2);

  return 1;
}

static const char* get_string(lua_State *L, int index)
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

static int get_integer(lua_State *L, int index)
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

static int index_metamethod(lua_State *L)
{
  // TODO: test me
  int type;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;
  const char *sidx;
  int nidx;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");

  switch(type)
  {
    case TYPE_HASH:
    case TYPE_NAMESPACE:
      hash = lua_touserdata(L, -1);

      value.type = TYPE_HASH;
      value.data.hash = hash;

      sidx = get_string(L, 2);
      SC_HashGet(hash, sidx, &value);
      break;

    case TYPE_ARRAY:
      array = lua_touserdata(L, -1);

      nidx = get_integer(L, 2);
      SC_ArrayGet(array, nidx-1, &value);
      break;
    default:
      luaL_error(L, va("internal error: unexpected datatype in `index_metamethod' at %s (%d)", __FILE__, __LINE__));
      break;
  }

  push_value(L, &value);

  return 1;
}

static int newindex_metamethod(lua_State *L)
{
  // TODO: test me
  int type;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;
  const char *sidx;
  int nidx;

  pop_value(L, &value, TYPE_ANY);

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");

  switch(type)
  {
    case TYPE_HASH:
    case TYPE_NAMESPACE:
      hash = lua_touserdata(L, -1);

      sidx = get_string(L, 2);
      SC_HashSet(hash, sidx, &value);
      break;

    case TYPE_ARRAY:
      array = lua_touserdata(L, -1);

      nidx = get_integer(L, 2);
      SC_ArraySet(array, nidx-1, &value);
      break;
    default:
      luaL_error(L, va("internal error: unexpected datatype (%d) in `newindex_metamethod' at %s (%d)", type, __FILE__, __LINE__));
      break;
  }

  return 0;
}

static int call_metamethod( lua_State *L )
{
  scDataTypeFunction_t *function = NULL;
  int top;
  int i;
  int type;
  int mod = 0;
  scClass_t *class;
  scDataTypeValue_t ret;
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  
  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_type");
  type = lua_tointeger(L, -1);
  lua_getfield(L, -2, "_ref");

  if(type == TYPE_FUNCTION)
    function = lua_touserdata(L, -1);
  else if(type == TYPE_CLASS)
  {
    class = lua_touserdata(L, -1);
    function = &class->constructor;
    args[0].type = TYPE_CLASS;
    args[0].data.class = class;
    mod = 1;
  }
  else
    luaL_error(L, va("internal error: %d datatype can't be called at %s (%d)", type, __FILE__, __LINE__));

  lua_pop(L, 3);

  top = lua_gettop(L);
  for(i = top-2; i >= 0; i--)
  {
    int idx = i + mod;
    pop_value(L, &args[idx], function->argument[idx]);
    SC_ValueGCInc(&args[idx]);
  }

  args[top-1+mod].type = TYPE_UNDEF;

  if(SC_RunFunction(function, args, &ret) != 0)
    luaL_error(L, SC_StringToChar(ret.data.string));

  i--;
  while(i >= 0)
  {
    SC_ValueGCDec(&args[i]);
    i--;
  }

  // TODO: garbage collection with return value

  if(ret.type != TYPE_UNDEF)
  {
    push_value(L, &ret);
    return 1;
  }

  return 0;
}

static int object_index_metamethod(lua_State *L)
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
  object = lua_touserdata(L, -1);
  name = luaL_checkstring(L, 2);

  method = SC_ClassGetMethod(object->class, name);
  if(method)
  {
    push_function(L, &method->method);
    return 1;
  }

  field = SC_ClassGetField(object->class, name);
  if(field)
  {
    // Call SC_Field_Get, push results
    SC_Field_Get(object, field, &out);
    
    push_value(L, &out);
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

    SC_RunFunction(&member->get, in, &out);

    push_value(L, &out);
    return 1;
  }

  luaL_error(L, va("can't index `%s' field : unknow member", name));
  return 0;
}

static int object_newindex_metamethod(lua_State *L)
{
  scObject_t *object;
  scObjectMember_t *member;
  scField_t        *field;
  scDataTypeValue_t in[MAX_FUNCTION_ARGUMENTS+1];
  scDataTypeValue_t out;
  const char *name;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  object = lua_touserdata(L, -1);
  lua_pop(L, 2);

  name = luaL_checkstring(L, 2);

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

      pop_value(L, &in[2], member->set.argument[1]);
      in[3].type = TYPE_UNDEF;
    }
    else
    {
      pop_value(L, &in[1], member->set.argument[1]);
      in[2].type = TYPE_UNDEF;
    }

    SC_RunFunction(&member->set, in, &out);

    return 0;
  }

  field  = SC_ClassGetField(object->class, name);
  if(field)
  {
    // Call SC_Field_Set with popped value
    pop_value(L, &in[1], field->type);
    
    SC_Field_Set( object, field, &in[1] );

    return 0;
  }

  luaL_error(L, va("can't index `%s' field : unknow member", name));
  return 0;
}

static int gc_string_metamethod(lua_State *L)
{
  scDataTypeString_t *string;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  string = lua_touserdata(L, -1);
  SC_StringGCDec(string);

  return 0;
}

static int gc_array_metamethod(lua_State *L)
{
  scDataTypeArray_t *array;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  array = lua_touserdata(L, -1);
  SC_ArrayGCDec(array);

  return 0;
}

static int gc_hash_metamethod(lua_State *L)
{
  scDataTypeHash_t *hash;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  hash = lua_touserdata(L, -1);
  SC_HashGCDec(hash);

  return 0;
}

static int gc_function_metamethod(lua_State *L)
{
  scDataTypeFunction_t *function;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  function = lua_touserdata(L, -1);
  SC_FunctionGCDec(function);

  return 0;
}

static int gc_class_metamethod(lua_State *L)
{
  scClass_t *class;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  class = lua_touserdata(L, -1);
  //SC_ClassGCDec(class);

  return 0;
}

static int gc_object_metamethod(lua_State *L)
{
  scObject_t *object;

  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "_ref");
  object = lua_touserdata(L, -1);
  SC_ObjectGCDec(object);

  return 0;
}

static void map_luamethod(lua_State *L, const char *name, lua_CFunction func)
{
  lua_getglobal(L, name);
  lua_setglobal(L, va("_%s", name));
  lua_pushcfunction(L, func);
  lua_setglobal(L, name);
}

static scDataTypeString_t* pop_lua_string(lua_State *L)
{
  const char *lstr;
  scDataTypeString_t *string;

  lstr = lua_tostring(L, -1);
  string = SC_StringNewFromChar(lstr);
  lua_pop(L, 1);

  return string;
}

static scDataTypeArray_t* pop_lua_array(lua_State *L)
{
  scDataTypeValue_t val;
  scDataTypeArray_t *array;

  array = SC_ArrayNew();

  lua_pushnil(L);
  while(lua_next(L, -2) != 0)
  {
    pop_value(L, &val, TYPE_ANY);
    SC_ArraySet(array, lua_tointeger(L, -1) - 1, &val);
  }

  lua_pop(L, 1);

  return array;
}

static scDataTypeHash_t* pop_lua_hash(lua_State *L)
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
    pop_value(L, &val, TYPE_ANY);
    SC_HashSet(hash, SC_StringToChar(key), &val);
  }

  lua_pop(L, 1);

  return hash;
}

static scDataTypeFunction_t* pop_lua_function(lua_State *L)
{
  scDataTypeFunction_t *function;

  function = SC_FunctionNew();
  function->langage = LANGAGE_LUA;
  // Impossible to know arguments type, set generic types
  function->argument[0] = TYPE_ANY;
  function->argument[1] = TYPE_UNDEF;
  function->return_type = TYPE_ANY;
  function->closure = NULL;
  SC_Lua_Fregister(function);

  return function;
}

static void pop_value(lua_State *L, scDataTypeValue_t *value, scDataType_t type)
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
      value->data.string = pop_lua_string(L);
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
          value->data.array = pop_lua_array(L);
        }
        else if(type == TYPE_NAMESPACE)
        {
          value->type = TYPE_NAMESPACE;
          value->data.hash = pop_lua_hash(L);
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
            value->data.array = pop_lua_array(L);
          }
          else
          {
            value->type = TYPE_HASH;
            value->data.hash = pop_lua_hash(L);
          }
        }
        else
        {
          value->type = TYPE_HASH;
          value->data.hash = pop_lua_hash(L);
        }
      }
      break;

    case LUA_TFUNCTION:
      value->type = TYPE_FUNCTION;
      value->data.function = pop_lua_function(L);
      break;

    default:
      luaL_error(L, "internal error : attempt to pop an unknow value (%s) in %s (%d)", lua_typename(L, ltype), __FILE__, __LINE__);
      break;
  }
}

static void push_boolean(lua_State *L, char boolean)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // TODO: metatable
  // metatable
  lua_newtable(L);
  /*lua_pushcfunction(L, concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, eq_string_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, lt_string_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, le_string_metamethod);
  lua_setfield(L, -2, "__le");*/
  lua_pushcfunction(L, tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_BOOLEAN);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushboolean(L, boolean);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_integer(lua_State *L, int integer)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // TODO: metatable
  // metatable
  lua_newtable(L);
  /*lua_pushcfunction(L, concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, eq_string_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, lt_string_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, le_string_metamethod);
  lua_setfield(L, -2, "__le");*/
  lua_pushcfunction(L, tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_INTEGER);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushinteger(L, integer);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_float(lua_State *L, float floating)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // TODO: metatable
  // metatable
  lua_newtable(L);
  /*lua_pushcfunction(L, concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, eq_string_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, lt_string_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, le_string_metamethod);
  lua_setfield(L, -2, "__le");*/
  lua_pushcfunction(L, tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_FLOAT);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushnumber(L, floating);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_userdata(lua_State *L, void* userdata)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_USERDATA);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, userdata);
  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_string(lua_State *L, scDataTypeString_t *string)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for string
  SC_StringGCInc(string);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, eq_string_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, lt_string_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, le_string_metamethod);
  lua_setfield(L, -2, "__le");
  lua_pushcfunction(L, tostring_metamethod);
  lua_setfield(L, -2, "__tostring");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_STRING);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, string);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_string_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_array( lua_State *L, scDataTypeArray_t *array )
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for array
  SC_ArrayGCInc(array);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_ARRAY);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, array);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_array_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_hash( lua_State *L, scDataTypeHash_t *hash, scDataType_t type )
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for hash
  SC_HashGCInc(hash);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, type);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, hash);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_hash_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_function( lua_State *L, scDataTypeFunction_t *function )
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for hash
  SC_FunctionGCInc(function);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");

  lua_pushinteger(L, TYPE_FUNCTION);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, function);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_function_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_class(lua_State *L, scClass_t *class)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for class
  //SC_ClassGCInc(class);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, invalid_index_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, invalid_length_metamethod);
  lua_setfield(L, -2, "__len");
  
  lua_pushinteger(L, TYPE_CLASS);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, class);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_class_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

  lua_setmetatable(L, -2);
}

static void push_object(lua_State *L, scObject_t *object)
{
  // maintable (empty, has only a metatable)
  lua_newtable(L);

  // add a new reference for object
  SC_ObjectGCInc(object);

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, object_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, object_newindex_metamethod);
  lua_setfield(L, -2, "__newindex");

  lua_pushinteger(L, TYPE_OBJECT);
  lua_setfield(L, -2, "_type");

  // push object
  lua_pushlightuserdata(L, object);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_object_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");

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
      push_boolean(L, value->data.boolean);
      break;
    case TYPE_INTEGER:
      push_integer(L, value->data.integer);
      break;
    case TYPE_FLOAT:
      push_float(L, value->data.floating);
      break;
    case TYPE_USERDATA:
      push_userdata(L, value->data.userdata);
      break;
    case TYPE_STRING:
	  push_string(L, value->data.string);
      break;
    case TYPE_ARRAY:
      push_array(L, value->data.array);
      break;
    case TYPE_HASH:
      push_hash(L, value->data.hash, TYPE_HASH);
      break;
    case TYPE_NAMESPACE:
      push_hash(L, (scDataTypeHash_t*) value->data.namespace, TYPE_NAMESPACE);
      break;
    case TYPE_FUNCTION:
      push_function(L, value->data.function);
      break;
    case TYPE_OBJECT:
      push_object(L, value->data.object);
      break;
    case TYPE_CLASS:
      push_class(L, value->data.class);
      break;
    default:
      luaL_error(L, "internal error : attempt to push an unknow value (%d) in %s (%d)", value->type, __FILE__, __LINE__);
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

static int is_registered_type(lua_State *L, int index, scDataType_t type)
{
  if(lua_getmetatable(L, index))
  {
    lua_getfield(L, -1, "_type");
    if(lua_tointeger(L, -1) != type)
    {
      // luaL_typeerror
    }
    lua_pop(L, 2);
    return 1;
  }
  else
  {
    if(lua_type(L, index) != sctype2luatype(type))
    {
      // luaL_typeerror
    }
    return 0;
  }
}

static int stack_dump( lua_State *L )
{
  SC_Lua_DumpStack();
  return 0;
}

static int register_boolean(lua_State *L)
{
  char boolean;

  if(!is_registered_type(L, 1, TYPE_BOOLEAN))
  {
    boolean = lua_toboolean(L, 1);
    push_boolean(L, boolean);
  }
  return 1;
}

static int register_integer(lua_State *L)
{
  int integer;

  if(!is_registered_type(L, 1, TYPE_INTEGER))
  {
    integer = lua_tointeger(L, 1);
    push_integer(L, integer);
  }
  return 1;
}

static int register_float(lua_State *L)
{
  float floating;

  if(!is_registered_type(L, 1, TYPE_FLOAT))
  {
    floating = lua_tonumber(L, 1);
    push_float(L, floating);
  }
  return 1;
}

static int register_string(lua_State *L)
{
  scDataTypeString_t *string;

  if(!is_registered_type(L, 1, TYPE_STRING))
  {
    string = pop_lua_string(L);
    push_string(L, string);
  }
  return 1;
}

static int register_function(lua_State *L)
{
  scDataTypeFunction_t *function;

  if(!is_registered_type(L, 1, TYPE_FUNCTION))
  {
    function = pop_lua_function(L);
    push_function(L, function);
  }
  return 1;
}

static int register_array(lua_State *L)
{
  scDataTypeArray_t *array;

  if(!is_registered_type(L, 1, TYPE_ARRAY))
  {
    array = pop_lua_array(L);
    push_array(L, array);
  }
  return 1;
}

static int register_hash(lua_State *L)
{
  scDataTypeHash_t *hash;

  if(!is_registered_type(L, 1, TYPE_HASH))
  {
    hash = pop_lua_hash(L);
    push_hash(L, hash, TYPE_HASH);
  }
  return 1;
}

static int register_namespace(lua_State *L)
{
  scDataTypeHash_t *namespace;

  if(!is_registered_type(L, 1, TYPE_NAMESPACE))
  {
    namespace = pop_lua_hash(L);
    push_hash(L, namespace, TYPE_NAMESPACE);
  }
  return 1;
}

static const struct luaL_reg stacklib [] = {
  { "dump", stack_dump },
  // scripting data registeration
  { "boolean", register_boolean },
  { "integer", register_integer },
  { "floating", register_float },
  { "string", register_string },
  { "function", register_function },
  { "array", register_array },
  { "hash", register_hash },
  { "namespace", register_namespace },
  {NULL, NULL}
};

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

  luaL_register(L, "stack", stacklib);
  lua_pop(L, 1);

  // Lua standard lib
  luaopen_base(L);
  luaopen_string(L);
  luaopen_table(L);
  lua_pop(L, 4);

  loadconstants(L);

  // TODO: write ipairs metamethod
  //map_luamethod(L, "ipairs", ipairs_method);
  map_luamethod(L, "pairs", pairs_method);
  map_luamethod(L, "type", type_method);
  map_luamethod(L, "print", print_method);

  // Register global table as root namespace
  SC_NamespaceGet("", &value);

  // root metatable
  lua_newtable(L);
  lua_pushlightuserdata(L, value.data.namespace);

  // userdata metatable
  lua_newtable(L);
  lua_pushcfunction(L, gc_string_metamethod);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);

  lua_setfield(L, -2, "_ref");
  lua_pushinteger(L, TYPE_NAMESPACE);
  lua_setfield(L, -2, "_type");

  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, len_metamethod);
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
    push_value( L, &in[narg] );

    narg++;
  }

  // do the call
  if(lua_pcall(L, narg, 1, 0) != 0)
    SC_EXEC_ERROR(va("error running lua function : %s", lua_tostring(L, -1)));

  pop_value(L, out, func->return_type);

  return 0;
}

/*
=================
SC_Lua_Fregister
=================
*/
static scLuaFunc_t luaFunc_id = 0;

void SC_Lua_Fregister(scDataTypeFunction_t *function)
{
  lua_setfield(g_luaState, LUA_REGISTRYINDEX, va("func_%d", luaFunc_id));
  function->data.luafunc = luaFunc_id;

  luaFunc_id++;
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

