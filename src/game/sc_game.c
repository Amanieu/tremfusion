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

static scLibObjectDef_t entity_object;

static void entity_init ( scObject_t *self, void *closure , scDataTypeValue_t *in, scDataTypeValue_t *out)
{
  self->data.type = TYPE_USERDATA;
  self->data.data.userdata = (void*)&g_entities[ in[0].data.integer ];
  out[0].type = TYPE_UNDEF;
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

static void entity_set( scObject_t *self, void *closure, scDataTypeValue_t *in, scDataTypeValue_t *out)
{
  int settype = (int)closure;
  gentity_t *entity;
    
  entity = (gentity_t*)self->data.data.userdata;
  
  switch (settype)
  {
    case ENTITY_CLASSNAME:
      if (in->type != TYPE_STRING) return; // FIXME: Type checking should be 
                                           // preformed by the language specific code
      strcpy(entity->classname, SC_StringToChar(in->data.string));
      break;
    case ENTITY_MODEL:
      if (in->type != TYPE_STRING) return; // FIXME: Type checking should be 
                                              // preformed by the language specific code
      strcpy(entity->model, SC_StringToChar(in->data.string));
      break;
    case ENTITY_MODEL2:
      if (in->type != TYPE_STRING) return; // FIXME: Type checking should be 
                                              // preformed by the language specific code
      strcpy(entity->model2, SC_StringToChar(in->data.string));
      break;
    default:
      // Error
      break;
  }

  out->type = TYPE_UNDEF;
}

static void entity_get(scObject_t *self, void *closure, scDataTypeValue_t *in, scDataTypeValue_t *out)
{
  int gettype = (int)closure;
  scObject_t *instance;
  gentity_t *entity;
  
  entity = (gentity_t*)self->data.data.userdata;
  
  switch (gettype)
  {
    case ENTITY_CLASSNAME:
      out[0].type = TYPE_STRING;
      out[0].data.string = SC_StringNewFromChar(entity->classname);
      break;
    case ENTITY_MODEL:
      out[0].type = TYPE_STRING;
      out[0].data.string = SC_StringNewFromChar(entity->model);
      break;
    case ENTITY_MODEL2:
      out[0].type = TYPE_STRING;
      out[0].data.string = SC_StringNewFromChar(entity->model2);
      break;
    case ENTITY_ORIGIN:
      instance = SC_Vec3FromVec3_t( entity->r.currentOrigin);
      out[0].type = TYPE_OBJECT;
      out[0].data.object = instance;
      break;
      //SC_Vec3FromVec3_t
    default:
      out[0].type = TYPE_UNDEF;
      break;
      // Error
  }
  out[1].type = TYPE_UNDEF;
}

static void entity_method(scObject_t *self, void *closure, scDataTypeValue_t *in, scDataTypeValue_t *out)
{
  int methodnum;
  methodnum = (int)closure;

  switch (methodnum)
  {
    case ENTITY_LINK:
      trap_LinkEntity( (gentity_t*)self->data.data.userdata );
      break;
    default:
      break;
  }
  out[0].type = TYPE_UNDEF;
}

static scLibObjectMember_t entity_members[] = {
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

static scLibObjectMethod_t entity_methods[] = {
  { "link", "", entity_method, { TYPE_UNDEF }, TYPE_UNDEF, (void*)ENTITY_LINK },
    { NULL },
};

static scLibObjectDef_t entity_object = { 
  "Entity", 
  { entity_init, { TYPE_INTEGER, TYPE_UNDEF } },
  { NULL },
  entity_members, 
  entity_methods, 
};


void SC_game_init( void )
{
  SC_AddLibrary( "game", game_lib );
  SC_AddObjectType( "game", &entity_object);
}

