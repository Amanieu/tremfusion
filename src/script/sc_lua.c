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

static int type_method(lua_State *L)
{
  int type;

  // If table has a metatable, run type on _data instead
  if(lua_istable(L, -1) && lua_getmetatable(L, -1))
  {
    lua_pop(L, 1);

    // original pairs method is saved in registry
    lua_getfield(L, -1, "_type");
    type = lua_tonumber(L, -1);
    lua_pushstring(L, luatype2string(sctype2luatype(type)));
    lua_remove(L, -2);
    lua_remove(L, -2);
  }
  else
  {
    lua_getfield(L, LUA_REGISTRYINDEX, "type");
    lua_insert(L, -2);

    lua_call(L, 1, 1);
  }

  return 1;
}

static int pairs_method(lua_State *L)
{
  // TODO: OUT TO DATE, have to rewrite it !

  // If table has a metatable, get defined type
  /*if(lua_istable(L, -1) && lua_getmetatable(L, -1))
  {
    lua_pop(L, 1);

    lua_getfield(L, -1, "_data");
    lua_remove(L, -2);
  }

  lua_getfield(L, LUA_REGISTRYINDEX, "pairs");
  lua_insert(L, -2);

  lua_call(L, 1, 2);
  lua_remove(L, -3);

  return 2;*/
  return 0;
}

static int null_metamethod(lua_State *L)
{
  // TODO: error message
  Com_Printf("*** Warning: null_metamethod called\n");
 
  return 0;
}

static int le_metamethod(lua_State *L)
{
  // TODO: OUT TO DATE, have to rewrite it !
  /*int result;
  lua_getfield(L, -2, "_data");
  lua_getfield(L, -2, "_data");
  result = lua_lessthan(L, -1, -2) || lua_equal(L, -1, -2);
  lua_pop(L, 4);

  lua_pushboolean(L, result);*/

  return 1;
}

static int lt_metamethod(lua_State *L)
{
  // TODO: OUT TO DATE, have to rewrite it !
  /*int result;
  lua_getfield(L, -2, "_data");
  lua_getfield(L, -2, "_data");
  result = lua_lessthan(L, -1, -2);
  lua_pop(L, 4);

  lua_pushboolean(L, result);*/

  lua_pushboolean(L, qfalse);
  return 1;
}

static int eq_metamethod(lua_State *L)
{
  // TODO: OUT TO DATE, have to rewrite it !
  /*int result;
  lua_getfield(L, -2, "_data");
  lua_getfield(L, -2, "_data");
  result = lua_equal(L, -1, -2);
  lua_pop(L, 4);

  lua_pushboolean(L, result);*/

  lua_pushboolean(L, qfalse);
  return 1;
}

static int len_metamethod(lua_State *L)
{
  // TODO: OUT TO DATE, have to rewrite it !
  /*lua_getfield(L, -1, "_data");
  lua_pushinteger(L, lua_objlen(L, -1));
  lua_remove(L, -2);
  lua_remove(L, -2);*/

  lua_pushinteger(L, 0);
  return 1;
}

static int concat_metamethod(lua_State *L)
{
  // TODO: OUT TO DATE, have to rewrite it !
  /*if(lua_istable(L, -2))
    lua_getfield(L, -2, "_data");
  else
    lua_pushvalue(L, -2);

  if(lua_istable(L, -2))
    lua_getfield(L, -2, "_data");
  else
    lua_pushvalue(L, -2);

  lua_concat(L, 2);
  lua_remove(L, -2);
  lua_remove(L, -2);*/

  return 1;
}

static int index_metamethod(lua_State *L)
{
  int type;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;

  lua_getfield(L, -2, "_type");
  type = lua_tointeger(L, -1);
  lua_pop(L, 1);

  switch(type)
  {
    case TYPE_HASH:
    case TYPE_NAMESPACE:
      lua_getfield(L, -2, "_ref");
      hash = lua_touserdata(L, -1);
      lua_pop(L, 1);

      value.type = TYPE_HASH;
      value.data.hash = hash;

      SC_HashGet(hash, lua_tostring(L, -1), &value);
      break;

    case TYPE_ARRAY:
      lua_getfield(L, -2, "_ref");
      array = lua_touserdata(L, -1);
      lua_pop(L, 1);

      SC_ArrayGet(array, lua_tointeger(L, -1), &value);
      break;
    default:
      // ERROR
      break;
  }

  lua_pop(L, 2);

  push_value(L, &value);
  return 1;
}

static int newindex_metamethod(lua_State *L)
{
  int type;
  scDataTypeHash_t *hash;
  scDataTypeArray_t *array;
  scDataTypeValue_t value;

  lua_getfield(L, -3, "_type");
  type = lua_tointeger(L, -1);
  lua_pop(L, 1);

  pop_value(L, &value, TYPE_OBJECT);

  switch(type)
  {
    case TYPE_HASH:
    case TYPE_NAMESPACE:
      lua_getfield(L, -2, "_ref");
      hash = lua_touserdata(L, -1);
      lua_pop(L, 1);

      SC_HashSet(hash, lua_tostring(L, -1), &value);
      break;

    case TYPE_ARRAY:
      lua_getfield(L, -2, "_ref");
      array = lua_touserdata(L, -1);
      lua_pop(L, 1);

      SC_ArraySet(array, lua_tointeger(L, -1), &value);
      break;
    default:
      // ERROR
      break;
  }

  lua_pop(L, 2);

  return 0;
}

static int call_metamethod( lua_State *L )
{
  scDataTypeFunction_t *function = NULL;
  int top = lua_gettop(L);
  int i;
  int type;
  int mod = 0;
  scClass_t *class;
  scDataTypeValue_t ret;
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  
  luaL_checktype(L, -top, LUA_TTABLE);

  lua_getfield(L, -top, "_type");
  type = lua_tointeger(L, -1);
  lua_pop(L, 1);
  lua_getfield(L, -top, "_ref");
  // FIXME: can userdata be modified under lua ? Maybe security issue...
  if(type == TYPE_FUNCTION)
  {
    function = lua_touserdata(L, -1);
  }
  else if(type == TYPE_CLASS)
  {
    class = lua_touserdata(L, -1);
    function = &class->constructor;
    args[0].type = TYPE_CLASS;
    args[0].data.class = class;
    mod = 1;
  }
  else
  {
    // TODO: raise error
  }

  lua_pop(L, 1);
  lua_remove(L, -top);

  top = lua_gettop(L);

  for(i = top-1; i >= 0; i--)
  {
    // FIXME: have to consider metatables to check type
    //luaL_checktype(L, -1, sctype2luatype(function->argument[i+mod]));
    pop_value(L, &args[i+mod], function->argument[i+mod]);
    SC_ValueGCInc(&args[i+mod]);
  }

  args[top+mod].type = TYPE_UNDEF;

  SC_RunFunction(function, args, &ret);

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

  lua_getfield(L, -2, "_ref");
  object = lua_touserdata(L, -1);
  lua_pop(L, 1);

  method = SC_ClassGetMethod(object->class, lua_tostring(L, -1));
  if(method)
  {
    push_function(L, &method->method);
    return 1;
  }

  member = SC_ClassGetMember(object->class, lua_tostring(L, -1));
  if(member)
  {
    // Call 'get' method, push result
    in[0].type = TYPE_OBJECT;
    in[0].data.object = object;
    in[1].type = TYPE_UNDEF;
    SC_RunFunction(&member->get, in, &out);

    push_value(L, &out);
    return 1;
  }
  field = SC_ClassGetField(object->class, lua_tostring(L, -1));
  if(field)
  {
    // Call SC_Field_Get, push results
    SC_Field_Get(object, field, &out);
    
    push_value(L, &out);
    return 1;
  }
  // TODO: Error: unknow value
  return 0;
}

static int object_newindex_metamethod(lua_State *L)
{
  scObject_t *object;
  scObjectMember_t *member;
  scField_t        *field;
  scDataTypeValue_t in[MAX_FUNCTION_ARGUMENTS+1];
  scDataTypeValue_t out;

  lua_getfield(L, -3, "_ref");
  object = lua_touserdata(L, -1);
  lua_pop(L, 1);

  member = SC_ClassGetMember(object->class, lua_tostring(L, -2));
  field  = SC_ClassGetField(object->class, lua_tostring(L, -2));
  if(member)
  {
    // Call 'set' method with popped value
    pop_value(L, &in[1], member->set.argument[2]);

    in[0].type = TYPE_OBJECT;
    in[0].data.object = object;
    in[2].type = TYPE_UNDEF;
    SC_RunFunction(&member->set, in, &out);
  }
  else if(field)
  {
    // Call SC_Field_Set with popped value
    pop_value(L, &in[1], field->type);
    
    SC_Field_Set( object, field, &in[1] );
  }
  else
  {
    // TODO: Error: unknow value
  }

  return 0;
}

static void map_luamethod(lua_State *L, const char *name, lua_CFunction func)
{
  char newname[MAX_PATH_LENGTH+1];
  newname[0] = '_';
  strcpy(newname+1, name);

  lua_getglobal(L, name);
  lua_setglobal(L, newname);
  lua_pushcfunction(L, func);
  lua_setglobal(L, name);
}

static scDataTypeString_t* pop_string(lua_State *L)
{
  const char *lstr;
  scDataTypeString_t *string;

  if(lua_isstring(L, -1))
    lstr = lua_tostring(L, -1);
  else
  {
    luaL_checktype(L, -1, LUA_TTABLE);

    // String is an object with a metatable
    if(lua_getmetatable(L, -1))
    {
      // TODO: Error
    }

    luaL_checktype(L, -1, LUA_TTABLE);
    lua_pop(L, 1);
    lua_getfield(L, -1, "_data");
    lua_remove(L, -2);
    lua_remove(L, -2);

    luaL_checktype(L, -1, LUA_TSTRING);
    lstr = lua_tostring(L, -1);

  }
  string = SC_StringNewFromChar(lstr);
  lua_pop(L, 1);

  return string;
}

static scObject_t* pop_object(lua_State *L)
{
  // TODO: error: objects always have a _ref entry

  /*luaL_checktype(L, -1, LUA_TTABLE);
  lua_pop(L, 1);
  lua_getfield(L, -1, "_data");
  lua_remove(L, -2);
  luaL_checktype(L, -1, LUA_TTABLE);

  scDataTypeArray_t *array = SC_ArrayNew();

  lua_pushnil(L);
  while(lua_next(L, -1) != 0)
  {
    pop_value(L, &val);
    SC_ArraySet(array, lua_tonumber(L, -1), &val);
  }
*/
  lua_pop(L, 1);

  return NULL;
}

static scClass_t* pop_class(lua_State *L)
{
  // TODO: is it really usefull ?
  lua_pop(L, 1);

  return NULL;
}

static scDataTypeArray_t* pop_array(lua_State *L)
{
  scDataTypeValue_t val;

  luaL_checktype(L, -1, LUA_TTABLE);

  // Array is an object with a metatable
  if(lua_getmetatable(L, -1))
  {
    // TODO: Error
  }

  luaL_checktype(L, -1, LUA_TTABLE);
  lua_pop(L, 1);
  lua_getfield(L, -1, "_data");
  lua_remove(L, -2);
  luaL_checktype(L, -1, LUA_TTABLE);

  scDataTypeArray_t *array = SC_ArrayNew();

  lua_pushnil(L);
  while(lua_next(L, -1) != 0)
  {
    pop_value(L, &val, TYPE_ANY);
    SC_ArraySet(array, lua_tonumber(L, -1), &val);
  }

  lua_pop(L, 2);

  return array;
}

static scDataTypeHash_t* pop_hash(lua_State *L)
{
  scDataTypeValue_t val;
  scDataTypeString_t *key;
  const char *lstr;

  luaL_checktype(L, -1, LUA_TTABLE);
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

static scDataTypeFunction_t* pop_function(lua_State *L)
{
  scDataTypeFunction_t *function;

  if(lua_istable(L, -1))
  {
    lua_getmetatable(L, -1);
    luaL_checktype(L, -1, LUA_TTABLE);
    lua_pop(L, 1);
    lua_getfield(L, -1, "_ref");
    luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
    function = lua_touserdata(L, -1);
    lua_pop(L, 3);
  }
  else
  {
    function = SC_FunctionNew();
    function->langage = LANGAGE_LUA;
    // TODO: What are arguments and return type ?
    function->argument[0] = TYPE_ANY;
    function->argument[1] = TYPE_UNDEF;
    function->return_type = TYPE_ANY;
    function->closure = NULL;
    SC_Lua_Fregister(function);
  }

  return function;
}

static void pop_value(lua_State *L, scDataTypeValue_t *value, scDataType_t type)
{
  int ltype = lua_type(L, -1);
  scDataType_t gtype;
      
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
      value->data.string = pop_string(L);
      break;

    case LUA_TTABLE:
      if(lua_getmetatable(L, -1))
      {
        // Metatable, could be any type
        lua_pop(L, 1);
        lua_getfield(L, -1, "_type");
        gtype = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "_ref");
        if(!lua_isnil(L, -1))
        {
          value->type = gtype;
          value->data.any = lua_touserdata(L, -1);
        }
        else
        {
          switch(gtype)
          {
            case TYPE_UNDEF:
              value->type = TYPE_UNDEF;
              lua_pop(L, 1);
              break;
            case TYPE_BOOLEAN:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_BOOLEAN;
              value->data.boolean = lua_toboolean(L, -1);
              lua_pop(L, 1);
              break;
            case TYPE_INTEGER:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_INTEGER;
              value->data.integer = lua_tointeger(L, -1);
              lua_pop(L, 1);
              break;
            case TYPE_FLOAT:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_INTEGER;
              value->data.floating = lua_tonumber(L, -1);
              lua_pop(L, 1);
              break;
            case TYPE_STRING:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_STRING;
              value->data.string = pop_string(L);
              break;
            case TYPE_FUNCTION:
              lua_getfield(L, -1, "_ref");
              lua_remove(L, -2);
              value->type = TYPE_FUNCTION;
              value->data.function = pop_function(L);
              break;
            case TYPE_ARRAY:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_ARRAY;
              value->data.array = pop_array(L);
              break;
            case TYPE_HASH:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_HASH;
              value->data.hash = pop_hash(L);
              break;
            case TYPE_NAMESPACE:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_NAMESPACE;
              value->data.namespace = pop_hash(L);
              break;
            case TYPE_OBJECT:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_OBJECT;
              value->data.object = pop_object(L);
              break;
            case TYPE_CLASS:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_CLASS;
              value->data.class = pop_class(L);
              break;
            case TYPE_USERDATA:
              lua_getfield(L, -1, "_data");
              lua_remove(L, -2);
              value->type = TYPE_USERDATA;
              value->data.userdata = lua_touserdata(L, -1);
              lua_pop(L, 1);
              break;
            default:
              // TODO: error case
              break;
          }
        }
        lua_pop(L, 2);
      }
      else
      {
        if(type == TYPE_ARRAY)
        {
          value->type = TYPE_ARRAY;
          value->data.array = pop_array(L);
        }
        else if(type == TYPE_NAMESPACE)
        {
          value->type = TYPE_NAMESPACE;
          value->data.hash = pop_hash(L);
        }
        else
        {
          // TODO: Should check : if type = TYPE_ANY and 
          // there is only integers, it's a TYPE_ARRAY
          value->type = TYPE_HASH;
          value->data.hash = pop_hash(L);
        }
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

static void push_string(lua_State *L, scDataTypeString_t *string)
{
  // global table
  lua_newtable(L);

  // data table
  lua_pushlightuserdata(L, string);
  lua_setfield(L, -2, "_ref");
  
  lua_pushstring(L, SC_StringToChar(string));
  lua_setfield(L, -2, "_data");

  lua_pushinteger(L, TYPE_STRING);
  lua_setfield(L, -2, "_type");

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, concat_metamethod);
  lua_setfield(L, -2, "__concat");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
  lua_pushcfunction(L, eq_metamethod);
  lua_setfield(L, -2, "__eq");
  lua_pushcfunction(L, lt_metamethod);
  lua_setfield(L, -2, "__lt");
  lua_pushcfunction(L, le_metamethod);
  lua_setfield(L, -2, "__le");

  lua_setmetatable(L, -2);
}

static void push_array( lua_State *L, scDataTypeArray_t *array )
{
  // global table
  lua_newtable(L);

  lua_pushlightuserdata(L, array);
  lua_setfield(L, -2, "_ref");

  lua_pushinteger(L, TYPE_ARRAY);
  lua_setfield(L, -2, "_type");

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
}

static void push_hash( lua_State *L, scDataTypeHash_t *hash, scDataType_t type )
{
  // global table
  lua_newtable( L );

  lua_pushlightuserdata(L, hash);
  lua_setfield(L, -2, "_ref");

  lua_pushinteger(L, TYPE_HASH);
  lua_setfield(L, -2, "_type");

  // metatable
  lua_newtable(L);
  lua_pushcfunction(L, index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, len_metamethod);
  lua_setfield(L, -2, "__len");
  lua_setmetatable(L, -2);
}

static void push_function( lua_State *L, scDataTypeFunction_t *function )
{
  // function global table
  lua_newtable(L);
  lua_pushlightuserdata(L, function);
  lua_setfield(L, -2, "_ref");

  lua_pushinteger(L, TYPE_FUNCTION);
  lua_setfield(L, -2, "_type");

  // function metatable
  lua_newtable(L);
  lua_pushcfunction(L, call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__len");

  lua_setmetatable(L, -2);
}

static void push_class(lua_State *L, scClass_t *class)
{
  // main table
  lua_newtable(L);

  lua_pushlightuserdata(L, class);
  lua_setfield(L, -2, "_ref");

  lua_pushinteger(L, TYPE_CLASS);
  lua_setfield(L, -2, "_type");

  // object metatable
  lua_newtable(L);
  lua_pushcfunction(L, call_metamethod);
  lua_setfield(L, -2, "__call");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, null_metamethod);
  lua_setfield(L, -2, "__len");

  lua_setmetatable(L, -2);
}

static void push_object(lua_State *L, scObject_t *object)
{
  // main table
  lua_newtable(L);

  lua_pushlightuserdata(L, object);
  lua_setfield(L, -2, "_ref");

  lua_pushinteger(L, TYPE_OBJECT);
  lua_setfield(L, -2, "_type");

  // object metatable
  lua_newtable(L);
  lua_pushcfunction(L, object_index_metamethod);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, object_newindex_metamethod);
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
    case TYPE_USERDATA:
      lua_pushlightuserdata(L, value->data.userdata);
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
        // Error: type invalid
        break;
    }

    cst++;
  }
}

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

  loadconstants(g_luaState);

  map_luamethod(g_luaState, "pairs", pairs_method);
  map_luamethod(g_luaState, "type", type_method);
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
  scDataType_t *dt;
  scDataTypeValue_t *value;

  value = args;
  dt = (scDataType_t*) func->argument;

  update_context(L);

  lua_getfield(L, LUA_REGISTRYINDEX, va("func_%d", func->data.luafunc));

  while( *dt != TYPE_UNDEF )
  {
    push_value( L, args );

    dt++;
    value++;
    narg++;
  }

  // do the call
  if(lua_pcall(L, narg, 1, 0) != 0)	// do the call
    Com_Printf("G_RunLuaFunction: error running function : %s\n", lua_tostring(L, -1));

  pop_value(L, ret, func->return_type);
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

