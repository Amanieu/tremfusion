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
  
  self->pointer = (void*)&g_entities[ args[0].data.integer ];
  
  self->type = type;
  return self;
}

typedef enum 
{
  ENTITY_CLASSNAME,
  ENTITY_LINK,
  ENTITY_MODEL,
  ENTITY_MODEL2,
  ENTITY_TARGET,
  ENTITY_TARGETNAME,
  ENTITY_TEAM,
  ENTITY_ORIGIN,
} ent_closures;

static void entity_set ( scObjectInstance_t *self, scDataTypeValue_t *value, void *closure)
{
  int settype = (int)closure;
  gentity_t *entity;
    
  entity = (gentity_t*)self->pointer;
  
  switch (settype)
  {
    case ENTITY_CLASSNAME:
      if (value->type != TYPE_STRING) return; // NOTE: Type checking should be 
                                              // preformed by the language specific code
      entity->classname = (char*)SC_StringToChar(value->data.string);
      break;
    case ENTITY_MODEL:
      if (value->type != TYPE_STRING) return; // NOTE: Type checking should be 
                                              // preformed by the language specific code
      entity->model = (char*)SC_StringToChar(value->data.string);
    case ENTITY_MODEL2:
      if (value->type != TYPE_STRING) return; // NOTE: Type checking should be 
                                              // preformed by the language specific code
      entity->model2 = (char*)SC_StringToChar(value->data.string);
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
  scObjectInstance_t *instance;
  gentity_t *entity;
  
  entity = (gentity_t*)self->pointer;
  value = BG_Alloc(sizeof(scDataTypeValue_t));
  
  switch (gettype)
  {
    case ENTITY_CLASSNAME:
      return SC_ValueStringFromChar( entity->classname );
    case ENTITY_MODEL:
      return SC_ValueStringFromChar( entity->model );
    case ENTITY_MODEL2:
      return SC_ValueStringFromChar( entity->model2 );
    case ENTITY_ORIGIN:
      instance = SC_Vec3_New( entity->r.currentOrigin);
      value->type = TYPE_OBJECTINSTANCE;
      value->data.objectinstance = instance;
      return value;
      //SC_Vec3_New
    default:
      value->type = TYPE_UNDEF;
      return value;
      // Error
  }
}

static scDataTypeValue_t *entity_method(scObjectInstance_t *self, scDataTypeValue_t *args, void *closure)
{
  int methodnum;
  scDataTypeValue_t *value;

  methodnum = (int)closure;
  value = BG_Alloc(sizeof(scDataTypeValue_t));
  value->type = TYPE_UNDEF;
  
  switch (methodnum) {
    case ENTITY_LINK:
      trap_LinkEntity( (gentity_t*)self->pointer );
      break;
    default:
      break;
  }
  return value;
}

static scObjectMember_t entity_members[] = {
    { "classname", "", TYPE_STRING, entity_set, entity_get,
        (void*)ENTITY_CLASSNAME },
    { "model", "", TYPE_STRING, entity_set, entity_get,
            (void*)ENTITY_MODEL },
    { "model2", "", TYPE_STRING, entity_set, entity_get,
            (void*)ENTITY_MODEL2 },
//    { "origin", "", TYPE_OBJECTINSTANCE, entity_set, entity_get,
//            (void*)ENTITY_ORIGIN },   
    { NULL },
};

static scObjectMethod_t entity_methods[] = {
  { "link", "", { TYPE_UNDEF }, entity_method, TYPE_UNDEF, (void*)ENTITY_LINK },
    { NULL },
};

static scObjectDef_t entity_object = { 
  "Entity", 
  entity_init, { TYPE_INTEGER, TYPE_UNDEF }, 
  entity_members, 
  entity_methods, 
};


void SC_game_init( void )
{
  SC_AddLibrary( "game", game_lib );
  SC_AddObjectType( "game", entity_object);
}

