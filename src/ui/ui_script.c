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

extern displayContextDef_t *DC;

int               current_draw_func_index=0;
int               current_move_func_index=0;
scDataTypeArray_t *draw_func_array;
scDataTypeArray_t *draw_func_arg_array;
scDataTypeArray_t *move_func_array;
scDataTypeArray_t *move_func_arg_array;

extern menuDef_t Menus[MAX_MENUS];
extern int menuCount;

static void UI_Script_f(void);
static void SC_UIModuleInit( void );
static scObject_t *WindowObjFromWindowDef_t( windowDef_t *windowptr);

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
  //SC_AutoLoad();
#ifndef Q3_VM
  trap_AddCommand( "script", UI_Script_f );
#endif
  draw_func_array     = SC_ArrayNew();
  draw_func_arg_array = SC_ArrayNew();
  move_func_array     = SC_ArrayNew();
  move_func_arg_array = SC_ArrayNew();
  SC_ArrayGCInc( draw_func_array );
  SC_ArrayGCInc( draw_func_arg_array );
  SC_ArrayGCInc( move_func_array );
  SC_ArrayGCInc( move_func_arg_array ); 
  
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
  SC_ArrayGCDec( draw_func_array );
  SC_ArrayGCDec( draw_func_arg_array );
  SC_ArrayGCDec( move_func_array );
  SC_ArrayGCDec( move_func_arg_array );
  SC_Shutdown( );
  Com_Printf("-----------------------------------\n");
}
/*
================
Script_ScRun

Run a sc_script for a ui_script
================
*/
void Script_ScRun( itemDef_t *item, char **args )
{
  const char *filename;

  if( String_Parse( args, &filename ) )
  {
    SC_RunScript( SC_LangageFromFilename(filename), va(UI_SCRIPT_DIRECTORY "%s", filename) );
  }
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
  SC_RunScript( SC_LangageFromFilename(filename), va(UI_SCRIPT_DIRECTORY "%s", filename) );
}

void SC_UIMove( void )
{
  int i;
  for( i = 0; i < move_func_array->size; i++ )
  {
    scDataTypeValue_t ret;
    scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
    scDataTypeValue_t function;
    SC_ArrayGet(move_func_array, i, &function);
    SC_ArrayGet(move_func_arg_array, i, &args[0]);
    args[1].type = TYPE_UNDEF;
    if(function.type != TYPE_FUNCTION) continue;
    function.data.function->argument[0] = TYPE_ANY;
    function.data.function->return_type = TYPE_ANY;
    if(SC_RunFunction( function.data.function, args, &ret ) )
    {
      // Error running function, remove draw func to prevent repeat errors
      SC_ArrayDelete(move_func_array, i);
      SC_ArrayDelete(move_func_arg_array, i);
      continue;
    }
    if(args[0].type != TYPE_UNDEF && ret.type != TYPE_UNDEF)
      SC_ArraySet( move_func_arg_array, i, &ret);
  }
}

void SC_UIDraw( void )
{
  int i;
  for( i = 0; i < draw_func_array->size; i++ )
  {
    scDataTypeValue_t ret;
    scDataTypeValue_t args[MAX_FUNCTION_ARGUMENTS+1];
    scDataTypeValue_t function;
    SC_ArrayGet(draw_func_array, i, &function);
    SC_ArrayGet(draw_func_arg_array, i, &args[0]);
    args[1].type = TYPE_UNDEF;
    if(function.type != TYPE_FUNCTION) continue;
    function.data.function->argument[0] = TYPE_ANY;
    function.data.function->return_type = TYPE_ANY;
    if(SC_RunFunction( function.data.function, args, &ret ) )
    {
      // Error running function, remove draw func to prevent repeat errors
      SC_ArrayDelete(draw_func_array, i);
      SC_ArrayDelete(draw_func_arg_array, i);
      continue;
    }
    if(args[0].type != TYPE_UNDEF && ret.type != TYPE_UNDEF)
      SC_ArraySet( draw_func_arg_array, i, &ret);
  }
}

#define ADD_DRAW_FUNC_DESC "index = ui.AddDrawFunc(function) \n\n Adds function to an array of functions " \
                           "that will be called every UI_Refresh after the Menus have been drawn\n Returns index of function in " \
                           "array for ui.RemoveDrawFunc(index)"
static int add_draw_func( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure )
{
  SC_ArraySet( draw_func_array, current_draw_func_index, &in[0]);
  SC_ArraySet( draw_func_arg_array, current_draw_func_index, &in[1]);
  SC_BuildValue(out, "i", current_draw_func_index);
  current_draw_func_index++;
  return 0;
}
#define REMOVE_DRAW_FUNC_DESC "ui.RemoveDrawFunc(index) \n\n Deletes the function that was added " \
                           "that will be called every UI_Refresh\n Returns: None"
static int remove_draw_func( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure )
{
  int func_index;
  func_index = in[0].data.integer;
  SC_ArrayDelete( draw_func_array, func_index);
  SC_ArrayDelete( draw_func_arg_array, func_index);
  out->type = TYPE_UNDEF;
  return 0;
}

#define ADD_MOVE_FUNC_DESC "index = ui.AddDrawFunc(function) \n\n Adds function to an array of functions " \
                           "that will be called every UI_Refresh before the Menus have been drawn\n Returns index of function in " \
                           "array for ui.RemoveMoveFunc(index)"
static int add_move_func( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure )
{
  SC_ArraySet( move_func_array, current_move_func_index, &in[0]);
  SC_ArraySet( move_func_arg_array, current_move_func_index, &in[1]);
  SC_BuildValue(out, "i", current_move_func_index);
  current_move_func_index++;
  return 0;
}
#define REMOVE_MOVE_FUNC_DESC "ui.RemoveMoveFunc(index) \n\n Deletes the function that was added " \
                           "that will be called every UI_Refresh\n Returns: None"
static int remove_move_func( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure )
{
  int func_index;
  func_index = in[0].data.integer;
  SC_ArrayDelete( move_func_array, func_index);
  SC_ArrayDelete( move_func_arg_array, func_index);
  out->type = TYPE_UNDEF;
  return 0;
}

static int draw_text( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure )
{
  float x, y, scale;
  vec4_t *colour;
  const char *text;
  x      = in[0].data.floating;
  y      = in[1].data.floating;
  scale  = in[2].data.floating;
  colour = SC_Vec4FromScript(in[3].data.object);
  text  = SC_StringToChar(in[4].data.string);
  UI_Text_Paint( x, y, scale, *colour, text, 0, 0, ITEM_TEXTSTYLE_NORMAL);
  out->type = TYPE_UNDEF;
  return 0;
}

static int draw_rect( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure )
{
  float x, y, w, h, size;
  vec4_t *colour;
  x      = in[0].data.floating;
  y      = in[1].data.floating;
  w      = in[2].data.floating;
  h      = in[3].data.floating;
  size   = in[4].data.floating;
  colour = SC_Vec4FromScript(in[5].data.object);
  DC->drawRect(x, y, w, h, size, *colour);
  return 0;
}

/*
======================================================================

Rectangle

======================================================================
*/

scClass_t *rect_class;

typedef struct
{
  qboolean  sc_created; // qtrue if created from python or lua, false if created by SC_Vec3FromVec3_t
                       // Prevents call of BG_Free on a vec3_t 
  rectDef_t *rect;
} sc_rect_t;

typedef enum 
{
  RECT_X,
  RECT_Y,
  RECT_W,
  RECT_H,
} sc_rect_closures;

static int rect_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  // TODO: error management
  scObject_t *self;
  sc_rect_t *data;
  rectDef_t *rect;
  
  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;
  self->data.type = TYPE_USERDATA;
  data = BG_Alloc(sizeof(sc_rect_t));
  rect = BG_Alloc(sizeof(rectDef_t));
  memset(rect, 0x00, sizeof(rectDef_t));
  
  data->sc_created = qtrue;
  data->rect = rect;
  self->data.data.userdata = data;

  return 0;
}

static int rect_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  // TODO: error management
  scObject_t *self;
  sc_rect_t *data;
  rectDef_t *rect;
  
  self = in[0].data.object;
  data = self->data.data.userdata;
  rect = data->rect;
  
  if(data->sc_created)
    BG_Free(rect);
  
  BG_Free(data);

  return 0;
}

static int rect_set ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  // TODO: error management
  int settype = closure.n;
  scObject_t *self;
  sc_rect_t *data;
  rectDef_t *rect;
  
  self = in[0].data.object;
  data = self->data.data.userdata;
  rect = data->rect;
  
  switch (settype)
  {
    case RECT_X:
      rect->x = in[1].data.floating;
      break;
    case RECT_Y:
      rect->y = in[1].data.floating;
      break;
    case RECT_W:
      rect->w = in[1].data.floating;
      break;
    case RECT_H:
      rect->h = in[1].data.floating;
      break;
    default:
      return -1;
  }

  return 0;
}

static int rect_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  // TODO: error management
  int gettype = closure.n;
  scObject_t *self;
  sc_rect_t *data;
  rectDef_t *rect;
  
  self = in[0].data.object;
  data = self->data.data.userdata;
  rect = data->rect;
  out[0].type = TYPE_FLOAT;
  
  switch (gettype)
  {
    case RECT_X:
      out[0].data.floating = rect->x;
      break;
    case RECT_Y:
      out[0].data.floating = rect->y;
      break;
    case RECT_W:
      out[0].data.floating = rect->w;
      break;
    case RECT_H:
      out[0].data.floating = rect->h;
      break;
    default:
      out[0].type = TYPE_UNDEF;
      return -1;
      // Error
  }

  return 0;
}

static int rect_containspoint(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  float x, y;
  scObject_t *self;
  sc_rect_t *data;
  rectDef_t *rect;
  
  self = in[0].data.object;
  data = self->data.data.userdata;
  rect = data->rect;
 
  x      = in[1].data.floating;
  y      = in[2].data.floating;
  
  out->type = TYPE_BOOLEAN;
  out->data.boolean = Rect_ContainsPoint(rect, x, y);
  return 0;
}

static scLibObjectMember_t rect_members[] = {
    { "x", "", TYPE_FLOAT, rect_set, rect_get, { .n = RECT_X } },
    { "y", "", TYPE_FLOAT, rect_set, rect_get, { .n = RECT_Y } },
    { "w", "", TYPE_FLOAT, rect_set, rect_get, { .n = RECT_W } },
    { "h", "", TYPE_FLOAT, rect_set, rect_get, { .n = RECT_H } },
    { "" },
};

static scLibObjectMethod_t rect_methods[] = {
  { "ContainsPoint", "", rect_containspoint, {TYPE_FLOAT, TYPE_FLOAT, TYPE_UNDEF}, TYPE_BOOLEAN, { .v = NULL } },
  { "" },
};
static scObject_t *RectObjFromRectDef_t(rectDef_t *rectptr)
{
  scObject_t *rectobj;
  sc_rect_t  *data;
  
  rectobj = SC_ObjectNew( rect_class );
  data    = BG_Alloc(sizeof(sc_rect_t));
  
  data->sc_created = qfalse;
  data->rect       = rectptr;
  
  rectobj->data.type = TYPE_USERDATA;
  rectobj->data.data.userdata = data;
  
  return rectobj;
}
// menuDef_t class

scClass_t *menu_class;

static int menu_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  // TODO: error management
  scObject_t *self;
  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;
   
  self->data.type = TYPE_USERDATA;
  if(in[1].type == TYPE_INTEGER)
    self->data.data.userdata = (void*)&Menus[ in[1].data.integer ];
  else if (in[1].type == TYPE_FLOAT) // damm you lua!!
    self->data.data.userdata = (void*)&Menus[ atoi( va("%.0f",in[1].data.floating) ) ];
  else
    return 1;
  
  return 0;
}

static int menu_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  return 0;
}

typedef enum 
{
  MENU_WINDOW,
} menu_closures;

static int menu_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scObject_t *self;
  menu_closures gettype = closure.n;
  menuDef_t *menu;
  
  self = in[0].data.object;
  menu =  self->data.data.userdata;
  switch (gettype)
  {
    case MENU_WINDOW:
      out[0].type = TYPE_OBJECT;
      out[0].data.object = WindowObjFromWindowDef_t(&menu->window);
      break;
    default:
      out[0].type = TYPE_UNDEF;
      return 1;
  }
  return 0;
}

static int menu_set ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  return 1;
}

static int menu_updateposition ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scObject_t *self;
  menuDef_t *menu;
  
  self = in[0].data.object;
  menu =  self->data.data.userdata;
  
  Menu_UpdatePosition( menu );
  return 0;
}

static int menu_close( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scObject_t *self;
  menuDef_t *menu;
  
  self = in[0].data.object;
  menu =  self->data.data.userdata;
  
  Menus_Close( menu );
  return 0;
}

static scLibObjectMember_t menu_members[] = {
  { "window", "", TYPE_OBJECT, menu_set, menu_get, { .n = MENU_WINDOW } },
};

static scField_t menu_fields[] = {
  { "name", "", TYPE_STRING, offsetof(menuDef_t, window.name) },
  { "" },
};

static scLibObjectMethod_t menu_methods[] = {
  { "UpdatePosition", "", menu_updateposition, { TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "Close", "", menu_close, { TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "" },
};

// Window Class

scClass_t *window_class;

static int window_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  return 0;
}

typedef enum 
{
  WINDOW_RECT,
} window_closures;

static int window_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scObject_t *self;
  window_closures gettype = closure.n;
  windowDef_t *window;
  
  self = in[0].data.object;
  window =  self->data.data.userdata;
  switch (gettype)
  {
    case WINDOW_RECT:
      out[0].type = TYPE_OBJECT;
      out[0].data.object = RectObjFromRectDef_t(&window->rect);
      break;
    default:
      out[0].type = TYPE_UNDEF;
      return 1;
  }
  return 0;
}

static int window_set ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  return 1;
}

static scLibObjectMember_t window_members[] = {
  { "rect", "", TYPE_OBJECT, window_set, window_get, { .n = WINDOW_RECT } },
};

static scField_t window_fields[] = {
  { "name", "", TYPE_STRING, offsetof(windowDef_t, name) },
  { "" },
};

static scLibObjectMethod_t window_methods[] = {
  { "" },
};


// itemDef_t class

scClass_t *item_class;

static int item_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  // TODO: error management
  scObject_t *self;
  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;
  menuDef_t *menu;
   
  self->data.type = TYPE_USERDATA;
  
  menu =  &Menus[ in[1].data.integer ];
  
  if(in[1].type == TYPE_INTEGER)
    menu = &Menus[ in[1].data.integer ];
  else if (in[1].type == TYPE_FLOAT) // damm you lua!!
    menu = &Menus[ atoi( va("%.0f",in[1].data.floating) ) ];
  else
    return 1;
  
  if(in[2].type == TYPE_INTEGER)
    self->data.data.userdata = (void*)menu->items[ in[2].data.integer ];
  else if (in[1].type == TYPE_FLOAT) // damm you lua!!
    self->data.data.userdata = (void*)menu->items[ atoi( va("%.0f",in[2].data.floating) ) ];
  else
    return 1;
  
  return 0;
}

static int item_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  return 0;
}

typedef enum 
{
  ITEM_WINDOW,
} item_closures;

static int item_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scObject_t *self;
  item_closures gettype = closure.n;
  itemDef_t *item;
  
  self = in[0].data.object;
  item =  self->data.data.userdata;
  switch (gettype)
  {
    case ITEM_WINDOW:
      out[0].type = TYPE_OBJECT;
      out[0].data.object = WindowObjFromWindowDef_t(&item->window);
      break;
    default:
      out[0].type = TYPE_UNDEF;
      return 1;
  }
  return 0;
}

static int item_set ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  return 1;
}

static int item_updateposition ( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scObject_t *self;
  itemDef_t *item;
  
  self = in[0].data.object;
  item =  self->data.data.userdata;
  
  Item_UpdatePosition( item );
  return 0;
}

static scLibObjectMember_t item_members[] = {
  { "window", "", TYPE_OBJECT, item_set, item_get, { .n = ITEM_WINDOW } },
};

static scField_t item_fields[] = {
  { "name", "", TYPE_STRING, offsetof(itemDef_t, window.name) },
  { "" },
};

static scLibObjectMethod_t item_methods[] = {
  { "UpdatePosition", "", item_updateposition, { TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "" },
};


static scObject_t *WindowObjFromWindowDef_t(windowDef_t *windowptr)
{
  scObject_t *windowobj;
  scDataTypeValue_t *data;
  
  windowobj = SC_ObjectNew( window_class );
  data = &windowobj->data;
  
  data->type = TYPE_USERDATA;
  data->data.userdata = (void*)windowptr;
  
  return windowobj;
}

static scLibObjectDef_t rect_def = { 
  "Rect", "",
  rect_constructor, { TYPE_UNDEF },
  rect_destructor,
  rect_members, 
  rect_methods, 
  NULL,
  NULL,                 // classMembers
  NULL,	                // classMethods
  { .v = NULL }
};

static scLibObjectDef_t menu_def = { 
  "Menu", "",
  menu_constructor, { TYPE_INTEGER, TYPE_UNDEF },
  menu_destructor,
  menu_members, 
  menu_methods, 
  menu_fields,
  NULL,                 // classMembers
  NULL,	                // classMethods
  { .v = NULL }
};

static scLibObjectDef_t item_def = { 
  "Item", "",
  item_constructor, { TYPE_INTEGER, TYPE_INTEGER, TYPE_UNDEF },
  item_destructor,
  item_members, 
  item_methods, 
  item_fields,
  NULL,                 // classMembers
  NULL,	                // classMethods
  { .v = NULL }
};

static scLibObjectDef_t window_def = { 
  "Window", "",
  0, { TYPE_UNDEF },
  window_destructor,
  window_members, 
  window_methods, 
  window_fields,
  NULL,                 // classMembers
  NULL,	                // classMethods
  { .v = NULL }
};

static scLibFunction_t ui_lib[] = {
  { "AddDrawFunc", ADD_DRAW_FUNC_DESC, add_draw_func, { TYPE_FUNCTION, TYPE_ANY, TYPE_UNDEF }, TYPE_INTEGER, { .v = NULL } },
  { "RemoveDrawFunc", REMOVE_DRAW_FUNC_DESC, remove_draw_func, { TYPE_INTEGER, TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "AddMoveFunc", ADD_MOVE_FUNC_DESC, add_move_func, { TYPE_FUNCTION, TYPE_ANY, TYPE_UNDEF }, TYPE_INTEGER, { .v = NULL } },
  { "RemoveMoveFunc", REMOVE_MOVE_FUNC_DESC, remove_move_func, { TYPE_INTEGER, TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "DrawText", "", draw_text, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_OBJECT, TYPE_STRING, TYPE_UNDEF }, TYPE_ANY, { .v = NULL } },
  { "DrawRect", "", draw_rect, { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT,
                                TYPE_FLOAT, TYPE_OBJECT, TYPE_UNDEF }, TYPE_ANY, { .v = NULL } },
  { "" }
};

static void SC_UIModuleInit( void )
{
  SC_AddLibrary( "ui", ui_lib );
  rect_class = SC_AddClass( "ui", &rect_def);
  menu_class = SC_AddClass( "ui", &menu_def);
  item_class = SC_AddClass( "ui", &item_def);
  window_class = SC_AddClass( "ui", &window_def);
}

