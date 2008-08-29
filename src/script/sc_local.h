/*
===========================================================================
Copyright (C) 2008 Maurice Doison

This file is part of Tremfusion.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef _SCRIPT_SC_LOCAL_H_
#define _SCRIPT_SC_LOCAL_H_

#include "sc_public.h"

// TODO: move here some stuff from sc_python.h and sc_public.h

#ifdef USE_LUA

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// sc_lua.c
#define MAX_LUAFILE 32768

extern lua_State *g_luaState;

void        SC_Lua_Init( void );
void        SC_Lua_Shutdown( void );
qboolean    SC_Lua_RunScript( const char *filename );
int         SC_Lua_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret );
void        SC_Lua_StackDump(lua_State *L);

int         SC_Lua_sctype2luatype(scDataType_t sctype);
const char  *SC_Lua_to_comparable_string(lua_State *L, int index, int left);
int         SC_Lua_is_registered(lua_State *L, int index, scDataType_t type);

// sc_lua_stack.c
void SC_Lua_push_boolean(lua_State *L, int boolean);
void SC_Lua_push_integer(lua_State *L, int integer);
void SC_Lua_push_float(lua_State *L, float floating);
void SC_Lua_push_userdata(lua_State *L, void* userdata);
void SC_Lua_push_string(lua_State *L, scDataTypeString_t *string);
void SC_Lua_push_array(lua_State *L, scDataTypeArray_t *array);
void SC_Lua_push_hash(lua_State *L, scDataTypeHash_t *hash, scDataType_t type);
void SC_Lua_push_function(lua_State *L, scDataTypeFunction_t *function);
void SC_Lua_push_class(lua_State *L, scClass_t *class);
void SC_Lua_push_object(lua_State *L, scObject_t *object);
void SC_Lua_push_value(lua_State *L, scDataTypeValue_t *value);

scDataTypeString_t* SC_Lua_get_lua_string(lua_State *L, int index);
scDataTypeArray_t* SC_Lua_get_lua_array(lua_State *L, int index);
scDataTypeHash_t* SC_Lua_get_lua_hash(lua_State *L, int index);
scDataTypeFunction_t* SC_Lua_get_lua_function(lua_State *L, int index);
void SC_Lua_get_value(lua_State *L, int index, scDataTypeValue_t *value, scDataType_t type);

scDataTypeString_t* SC_Lua_pop_lua_string(lua_State *L);
scDataTypeArray_t* SC_Lua_pop_lua_array(lua_State *L);
scDataTypeHash_t* SC_Lua_pop_lua_hash(lua_State *L);
scDataTypeFunction_t* SC_Lua_pop_lua_function(lua_State *L);
void SC_Lua_pop_value(lua_State *L, scDataTypeValue_t *value, scDataType_t type);

const char* SC_Lua_get_arg_string(lua_State *L, int index);
int SC_Lua_get_arg_boolean(lua_State *L, int index);
int SC_Lua_get_arg_integer(lua_State *L, int index);
lua_Number SC_Lua_get_arg_number(lua_State *L, int index);

// sc_lua_metamethod.c
int SC_Lua_invalid_metatable_metamethod(lua_State *L);
int SC_Lua_invalid_index_metamethod(lua_State *L);
int SC_Lua_invalid_length_metamethod(lua_State *L);

int SC_Lua_concat_metamethod(lua_State *L);
int SC_Lua_call_metamethod( lua_State *L );
int SC_Lua_tostring_metamethod( lua_State *L );

int SC_Lua_eq_boolean_metamethod(lua_State *L);
int SC_Lua_tostring_boolean_metamethod(lua_State *L);

int SC_Lua_add_number_metamethod(lua_State *L);
int SC_Lua_sub_number_metamethod(lua_State *L);
int SC_Lua_mul_number_metamethod(lua_State *L);
int SC_Lua_div_number_metamethod(lua_State *L);
int SC_Lua_mod_number_metamethod(lua_State *L);
int SC_Lua_pow_number_metamethod(lua_State *L);
int SC_Lua_unm_number_metamethod(lua_State *L);
int SC_Lua_eq_number_metamethod(lua_State *L);
int SC_Lua_lt_number_metamethod(lua_State *L);
int SC_Lua_le_number_metamethod(lua_State *L);
int SC_Lua_tostring_number_metamethod(lua_State *L);

int SC_Lua_le_string_metamethod(lua_State *L);
int SC_Lua_lt_string_metamethod(lua_State *L);
int SC_Lua_eq_string_metamethod(lua_State *L);
int SC_Lua_tostring_string_metamethod(lua_State *L);
int SC_Lua_len_string_metamethod(lua_State *L);
int SC_Lua_gc_string_metamethod(lua_State *L);

int SC_Lua_index_array_metamethod(lua_State *L);
int SC_Lua_newindex_array_metamethod(lua_State *L);
int SC_Lua_len_array_metamethod(lua_State *L);
int SC_Lua_gc_array_metamethod(lua_State *L);

int SC_Lua_len_hash_metamethod(lua_State *L);
int SC_Lua_index_hash_metamethod(lua_State *L);
int SC_Lua_newindex_hash_metamethod(lua_State *L);
int SC_Lua_gc_hash_metamethod(lua_State *L);

int SC_Lua_gc_function_metamethod(lua_State *L);

int SC_Lua_gc_class_metamethod(lua_State *L);

int SC_Lua_gc_object_metamethod(lua_State *L);
int SC_Lua_object_index_metamethod(lua_State *L);
int SC_Lua_object_newindex_metamethod(lua_State *L);

// sc_lua_lib.c
void SC_Lua_loadlib(lua_State *L);

#endif
#endif

