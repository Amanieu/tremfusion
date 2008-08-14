/*
===========================================================================
Copyright (C) 2008 John Black

This file is part of Tremulous.

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

#include "ui_local.h"

int               current_draw_func_index=0;
scDataTypeArray_t draw_func_array;
scDataTypeArray_t draw_func_arg_array;

static void UI_Script_f(void);
static void SC_UIModuleInit( void );

/*
================
UI_ScriptInit

Initialize scripting system and load libraries
================
*/
void UI_ScriptInit( void )
{
  Com_Printf("------- UI Scripting System Initializing -------\n");
  BG_InitMemory(); // scripting system needs BG_Alloc to work
  SC_Init();
  SC_UIModuleInit();
  SC_LoadLangages();
  SC_AutoLoad();
#ifndef Q3_VM
  trap_AddCommand( "script", UI_Script_f );
#endif
  Com_Printf("-----------------------------------\n");
}

/*
================
UI_ScriptShutdown

Shutdown scripting system and unload libraries
================
*/
void UI_ScriptShutdown( void )
{
  Com_Printf("------- UI Scripting System Shutdown -------\n");
  SC_Shutdown( );
  Com_Printf("-----------------------------------\n");
}

/*  
=================
UI_Script_f
=================
*/  
static void UI_Script_f(void)
{
  char filename[128];
  trap_Argv( 1, filename, 128 );
  SC_RunScript( SC_LangageFromFilename(va("scripts/%s", filename) ), va("scripts/%s", filename) );
}

void SC_UIRefresh ( void )
{
  int i;
  for( i = 0; i < draw_func_array.size; i++ )
  {
    scDataTypeValue_t ret;
    scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
    if(draw_func_array.data[i].type != TYPE_FUNCTION) continue;
    args[0].data.function->argument[0] = TYPE_ANY;
    args[0].data.function->return_type = TYPE_ANY;
    SC_ArrayGet(&draw_func_arg_array, i, &args[0]);
    SC_RunFunction( draw_func_array.data[i].data.function, args, &ret );
    SC_ArraySet( &draw_func_arg_array, i, &ret);
  }
}

#define ADD_DRAW_FUNC_DESC "index = ui.AddDrawFunc(function) \n\n Adds function to an array of functions " \
                           "that will be called every UI_Refresh\n Returns index of function in " \
                           "array for ui.RemoveDrawFunc(index)"
static int add_draw_func( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure )
{
  SC_ArraySet( &draw_func_array, current_draw_func_index, &in[0]);
  SC_ArraySet( &draw_func_arg_array, current_draw_func_index, &in[1]);
  SC_BuildValue(out, "i", current_draw_func_index);
  current_draw_func_index++;
  out->type = TYPE_UNDEF;
  return 0;
}

static int draw_text( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure )
{
  int x, y;
  float scale;
  const char *text;
  x     = in[0].data.integer;
  y     = in[1].data.integer;
  scale = in[2].data.floating;
  text  = SC_StringToChar(in[3].data.string);
  UI_Text_Paint( x, y, scale, g_color_table[7], text, 0, 0, ITEM_TEXTSTYLE_NORMAL);
  out->type = TYPE_UNDEF;
  return 0;
}

static scLibFunction_t ui_lib[] = {
  { "AddDrawFunc", ADD_DRAW_FUNC_DESC, add_draw_func, { TYPE_FUNCTION, TYPE_ANY, TYPE_UNDEF }, TYPE_INTEGER, NULL },
  { "DrawText", "", draw_text, { TYPE_INTEGER, TYPE_INTEGER, TYPE_FLOAT, TYPE_STRING, TYPE_UNDEF }, TYPE_ANY, NULL },
  { "" }
};

static void SC_UIModuleInit( void )
{
  SC_AddLibrary( "ui", ui_lib );
}

