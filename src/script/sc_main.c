/*
===========================================================================
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
// sc_main.c

#include "../game/g_local.h"
#include "../qcommon/q_shared.h"

#include "sc_public.h"
#include "sc_lua.h"
#include "sc_python.h"

static void SC_script_module_init( void );

void SC_Init( void )
{
  SC_NamespaceInit( );

#ifdef USE_LUA
  if (sc_lua.integer)
    SC_Lua_Init( );
#endif
#ifdef USE_PYTHON
  if (sc_python.integer)
    SC_Python_Init( );
#endif 
  SC_script_module_init();
  SC_Common_Init();
}

static void autoload_global(void)
{
  int             numdirs;
  int             numFiles;
  char            filename[128];
  char            dirlist[1024];
  char           *dirptr;
  int             i;
  int             dirlen;

  numFiles = 0;

  numdirs = trap_FS_GetFileList("scripts/global/", "", dirlist, 1024);
  dirptr = dirlist;
  for(i = 0; i < numdirs; i++, dirptr += dirlen + 1)
  {
    dirlen = strlen(dirptr);
    strcpy(filename, "scripts/global/");
    strcat(filename, dirptr);

    // load the file
    if (SC_RunScript(SC_LangageFromFilename(filename), filename) != -1 )
      numFiles++;
  }

  Com_Printf("%i global files parsed\n", numFiles);
}

static void autoload_local(char mapname[MAX_STRING_CHARS])
{
  int             numdirs;
  int             numFiles;
  char            filename[128];
  char            dirlist[1024];
  char           *dirptr;
  int             i;
  int             dirlen;

  numFiles = 0;

  numdirs = trap_FS_GetFileList(va("scripts/%s", mapname), "", dirlist, 1024);
  dirptr = dirlist;
  for(i = 0; i < numdirs; i++, dirptr += dirlen + 1)
  {
    dirlen = strlen(dirptr);
    strcpy(filename, va("scripts/%s/", mapname));
    strcat(filename, dirptr);
    Com_Printf("***find file to parse***\n");

    // load the file
    if (SC_RunScript(SC_LangageFromFilename(filename), filename) != -1 )
      numFiles++;
  }

  Com_Printf("%i local files parsed\n", numFiles);
}

void SC_AutoLoad( void )
{
  char            buf[MAX_STRING_CHARS];

  Com_Printf("load global scripts:\n");
  autoload_global();

  Com_Printf("load map specific scripts:\n");
  trap_Cvar_VariableStringBuffer("mapname", buf, sizeof(buf));
  autoload_local(buf);
}

void SC_Shutdown( void )
{
#ifdef USE_LUA
  if (sc_lua.integer)
    SC_Lua_Shutdown( );
#endif
#ifdef USE_PYTHON
  if (sc_python.integer)
    SC_Python_Shutdown( );
#endif
}

void SC_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret)
{
  switch( func->langage )
  {
    case LANGAGE_C:
      func->data.ref(args, ret, func->closure);
      break;
#ifdef USE_LUA
    case LANGAGE_LUA:
      return SC_Lua_RunFunction( func, args, ret );
#endif
#ifdef USE_PYTHON
    case LANGAGE_PYTHON:
      return SC_Python_RunFunction( func, args, ret );
#endif
    default:
	  break;
  }
}

int SC_RunScript( scLangage_t langage, const char *filename )
{
  switch( langage )
  {
    case LANGAGE_C:
      Com_Printf(va("Can't load %s: C can't be used as script\n", filename));
      break;
#ifdef USE_LUA
    case LANGAGE_LUA:
      return SC_Lua_RunScript( filename );
#endif
#ifdef USE_PYTHON
    case LANGAGE_PYTHON:
      return SC_Python_RunScript( filename );
#endif     
    default:
      Com_Printf(va("Can't load %s: unknow langage\n", filename));
	  break;
  }

  return -1;
}

int SC_CallHooks( const char *path, gentity_t *entity )
{
  // TODO: implement function
  return 1;
}

scLangage_t SC_LangageFromFilename(const char* filename)
{
  const char *ext = NULL;
  const char *i = filename;
  while(*i != '\0')
  {
    if(*i == '.')
      ext = i;
    i++;
  }

  if(ext == NULL)
    return LANGAGE_INVALID;
  
  ext++;
  if(strcmp(ext, "lua") == 0)
    return LANGAGE_LUA;
  else if(strcmp(ext, "py") == 0)
    return LANGAGE_PYTHON;

  return LANGAGE_INVALID;
}

void SC_InitObject( scObject_t *object)
{
#ifdef USE_LUA
//  SC_Lua_InitObject( object );
#endif
#ifdef USE_PYTHON
  SC_Python_InitObject( object );
#endif
}

static void script_NamespaceAdd( scDataTypeValue_t *args, scDataTypeValue_t *ret, void *closure )
{
  SC_NamespaceSet( SC_StringToChar(args[0].data.string), &args[1] );
  ret->type = TYPE_UNDEF;
}

static scLibFunction_t script_lib[] = {
  { "NamespaceAdd", "", script_NamespaceAdd, { TYPE_STRING , TYPE_ANY, TYPE_UNDEF }, TYPE_UNDEF, NULL },
//  { "Command", game_Command, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF },
  { "" }
};

static void SC_script_module_init( void )
{
  SC_AddLibrary( "script", script_lib );
}

