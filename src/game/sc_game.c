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

static scLibObjectDef_t entity_def;

static int game_Print( scDataTypeValue_t *args, scDataTypeValue_t *ret, void *closure )
{
  G_Printf(SC_StringToChar(args[0].data.string));
  ret->type = TYPE_UNDEF;

  return 0;
}

static int game_Command( scDataTypeValue_t *args, scDataTypeValue_t *ret, void* closure )
{
  trap_SendConsoleCommand(EXEC_APPEND, SC_StringToChar(args[0].data.string));
  ret->type = TYPE_UNDEF;

  return 0;
}

static scLibFunction_t game_lib[] = {
  { "Print", "", game_Print, { TYPE_STRING , TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "Command", "", game_Command, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

static int entity_init ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure )
{
  // TODO: error management
  scObject_t *self;

  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;
  

  self->data.type = TYPE_USERDATA;
  if(in[1].type == TYPE_INTEGER)
    self->data.data.userdata = (void*)&g_entities[ in[1].data.integer ];
  else if (in[1].type == TYPE_FLOAT) // damm you lua!!
    self->data.data.userdata = (void*)&g_entities[ atoi( va("%.0f",in[1].data.floating) ) ]; // Hax

  return 0;
}

static int entity_destroy ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure )
{
  return 0;
}

typedef enum 
{
  // Vectors
  ENTITY_ORIGIN,
  // Methods
  ENTITY_LINK,
} ent_closures;

#define ENT_SET_STR(x) x = (char*) SC_StringToChar(in[1].data.string); \
  break

static int entity_set( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: Error management
  int settype = (int)closure;
  gentity_t *entity;
  scObject_t *self = in[0].data.object;
    
  entity = (gentity_t*)self->data.data.userdata;
  
  switch (settype)
  {
    case ENTITY_ORIGIN:
      // TODO: Error : read only value Champion: Have read only values have a NULL setter?
    default:
      // Error
      break;
  }

  out->type = TYPE_UNDEF;

  return 0;
}

#define ENT_GET_STR(x) \
  out[0].type = TYPE_STRING; \
  if (x) \
    out[0].data.string = SC_StringNewFromChar(x); \
  else \
    out[0].data.string = SC_StringNewFromChar(""); \
  break

static int entity_get(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  int gettype = (int)closure;
  scObject_t *instance;
  gentity_t *entity;
  scObject_t *self = in[0].data.object;
  entity = (gentity_t*)self->data.data.userdata;
  
  switch (gettype)
  {
    case ENTITY_ORIGIN:
      instance = SC_Vec3FromVec3_t( entity->r.currentOrigin);
      out[0].type = TYPE_OBJECT;
      out[0].data.object = instance;
      break;
    default:
      out[0].type = TYPE_UNDEF;
      break;
      // Error
  }
  out[1].type = TYPE_UNDEF;

  return 0;
}

static int entity_method(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  int methodnum = (int)closure;
  gentity_t *entity;
  scObject_t *self = in[0].data.object;
  entity = (gentity_t*)self->data.data.userdata;

  switch (methodnum)
  {
    case ENTITY_LINK:
      trap_LinkEntity( entity );
      break;
    default:
      break;
  }
  out[0].type = TYPE_UNDEF;

  return 0;
}

static scLibObjectMember_t entity_members[] = {
  // Vectors
  { "origin",    "", TYPE_OBJECT,  entity_set, entity_get, (void*)ENTITY_ORIGIN },
  // Booleans
//  { "inuse",     "", TYPE_BOOLEAN, entity_set, entity_get, (void*)ENTITY_INUSE },
  { "" },
};

static scLibObjectMethod_t entity_methods[] = {
  { "link", "", entity_method, { TYPE_UNDEF }, TYPE_UNDEF, (void*)ENTITY_LINK },
  { "" },
};

static scField_t entity_fields[] = {
  { "classname",  "", TYPE_STRING,  FOFS(classname)  },
  { "model",      "", TYPE_STRING,  FOFS(model)      },
  { "model2",     "", TYPE_STRING,  FOFS(model2)     },
  { "target",     "", TYPE_STRING,  FOFS(target)     },
  { "targetname", "", TYPE_STRING,  FOFS(targetname) },
  { "team",       "", TYPE_STRING,  FOFS(team)       },
  { "inuse",      "", TYPE_BOOLEAN, FOFS(inuse)      },
  { "" },
};

static scLibObjectDef_t entity_def = { 
  "Entity", "",
  entity_init, { TYPE_INTEGER, TYPE_UNDEF },
  entity_destroy,
  entity_members, 
  entity_methods, 
  entity_fields,
  NULL
};

static scConstant_t constants[] = {
  { "TEAM_NONE", TYPE_INTEGER, (void*) TEAM_NONE },
  { "TEAM_ALIENS", TYPE_INTEGER, (void*) TEAM_ALIENS },
  { "TEAM_HUMANS", TYPE_INTEGER, (void*) TEAM_HUMANS },
  { "" }
};

void SC_game_init( void )
{
  SC_AddLibrary( "game", game_lib );
  SC_AddClass( "game", &entity_def );
  SC_Constant_Add(constants);
}

