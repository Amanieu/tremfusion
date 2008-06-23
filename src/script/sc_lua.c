
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

// TODO: move it to common scripting layer
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

// TODO: move it to common scripting layer
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

static void push_value( lua_State *L, scDataTypeValue_t *value );

static void push_array( lua_State *L, scDataTypeArray_t *array )
{
  int i;

  lua_newtable( L );
  for( i = 0; i < array->size; i++ )
  {
    push_value( L, & (&array->data)[i] );
    lua_rawseti( L, -1, i );
  }
}

static void push_hash( lua_State *L, scDataTypeHash_t *hash )
{
  int i;

  lua_newtable( L );
  for( i = 0; i < hash->size; i++ )
  {
    push_value( L, & (&hash->data)[i].value );
    lua_setfield( L, -2, & (&hash->data)[i].key->data );
  }
}

static void push_function( lua_State *L, scDataTypeFunction_t *function )
{

}

static void push_value( lua_State *L, scDataTypeValue_t *value )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      lua_pushnil( L );
      break;
    case TYPE_INTEGER:
      lua_pushinteger( L, value->data.integer );
      break;
    case TYPE_FLOAT:
      lua_pushnumber( L, value->data.floating );
      break;
    case TYPE_STRING:
      lua_pushstring( L, & value->data.string->data );
      break;
    case TYPE_ARRAY:
      push_array( L, value->data.array );
      break;
    case TYPE_HASH:
      push_hash( L, value->data.hash );
      break;
    case TYPE_NAMESPACE:
      push_hash( L, (scDataTypeHash_t*) value->data.namespace );
      break;
    case TYPE_FUNCTION:
      push_function( L, value->data.function );
      break;
  }
}

static void pop_value( lua_State *L, scDataTypeValue_t *value )
{
  // TODO: pop a value here
}

static void push_path( lua_State *L, const char *path )
{
  // TODO: push a path here
}

static int bridge( lua_State *L )
{
  // TODO: error cases
  int top = lua_gettop(L);
  int i;
  scDataTypeValue_t path, func, ret;
  scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
  
  for(i = 0; i < top; i++)
  {
    pop_value( L, & args[i] );
  }
  args[i].type = TYPE_UNDEF;

  pop_value(L, &path);

  SC_NamespaceGet(& path.data.string->data, &func);
  if(func.type != TYPE_FUNCTION)
  {
    // TODO: error case here
  }

  SC_RunFunction(func.data.function, args, &ret);
  if(ret.type != TYPE_UNDEF)
  {
    push_value(L, &ret);
    return 1;
  }

  return 0;
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
  char            buf[MAX_STRING_CHARS];

  G_Printf("------- Game Lua Initialization -------\n");

  g_luaState = lua_open();

  luaL_openlib(g_luaState, "stack", stacklib, 0);

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
  {
    G_Printf("SC_Lua_RunScript: cannot load lua file: %s\n", lua_tostring(g_luaState, -1));
    return qfalse;
  }

  if(lua_pcall(g_luaState, 0, 0, 0))
  {
    G_Printf("SC_Lua_RunScript: cannot pcall: %s\n", lua_tostring(g_luaState, -1));
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
    G_Printf("G_RunLuaFunction: error running function `%s': %s\n", func, lua_tostring(L, -1));

  pop_value( L, ret );
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

  G_Printf( va( "size: %d\n", top ) );
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
    G_Printf(" ");			// put a separator
  }
  G_Printf("\n");				// end the listing
}

void SC_Lua_Import( const char *path, scDataTypeValue_t *value )
{
  lua_State *L = g_luaState;

  push_value( L, value );
  push_path( L, path );
  
  lua_settable( L, -2 );
}

#endif

