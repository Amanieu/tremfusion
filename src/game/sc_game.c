/*
===========================================================================
Copyright (C) 2008 Maurice Doison

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

#include "g_local.h"
static void game_Print( scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  G_Printf(SC_StringToChar(args[0].data.string));
  ret->type = TYPE_UNDEF;
}

static void game_Command( scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  trap_SendConsoleCommand(EXEC_APPEND, SC_StringToChar(args[0].data.string));
  ret->type = TYPE_UNDEF;
}

static scLib_t game_lib[] = {
  { "Print", game_Print, { TYPE_STRING , TYPE_UNDEF }, TYPE_UNDEF },
  { "Command", game_Command, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF },
  { "" }
};

static scObjectDef_t entity_object;

static scObjectInstance_t *entity_init ( scObjectType_t *type, scDataTypeValue_t *args ){
  scObjectInstance_t *self;
  self = BG_Alloc( sizeof( scObjectInstance_t ) );
  
  self->type = type;
  Com_Printf("entity_init called\n");
//  self->pointer =
  return self;
}

typedef enum 
{
  ENTITY_CLASSNAME,
} ent_getset_t;

static void entity_set ( scObjectInstance_t *self, scDataTypeValue_t *value, void *closure)
{
  int settype = (int)closure;
  gentity_t *entity;
    
  entity = (gentity_t*)self->pointer;
  
  switch (settype)
  {
    case ENTITY_CLASSNAME:
      if (value->type != TYPE_STRING)
      {
        //TODO: Raise a Type Error
        return;
      }
      entity->classname = (char*)SC_StringToChar(value->data.string);
      break;
    default:
      // Error
      break;
  }
}

static scDataTypeValue_t *entity_get ( scObjectInstance_t *self, void *closure )
{
  scDataTypeValue_t *value;
  int gettype = (int)closure;
  scDataTypeString_t *string;
  gentity_t *entity;
  
  entity = (gentity_t*)self->pointer;
  value = BG_Alloc(sizeof(scDataTypeValue_t));
  
  
  switch (gettype)
  {
    case ENTITY_CLASSNAME:
      string = SC_StringNewFromChar( entity->classname );
      value->type = TYPE_STRING;
      value->data.string = string;
      return value;
    default:
      value->type = TYPE_UNDEF;
      return value;
      // Error
  }
}

static scObjectMember_t entity_members[] = {
    { "classname", "", TYPE_STRING, entity_set, entity_get, (void*)ENTITY_CLASSNAME},
    { NULL },
};

static scObjectDef_t entity_object = {
  "Entity",
  entity_init,
  { TYPE_INTEGER, TYPE_UNDEF },
  entity_members,
};


void SC_game_init( void )
{
  SC_AddLibrary( "game", game_lib );
  SC_AddObjectType( "game", entity_object);
}

