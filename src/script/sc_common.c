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

// sc_common.c
// common function and objects

#include "../game/g_local.h"
#include "../qcommon/q_shared.h"

#include "sc_public.h"

static scObjectDef_t vec3_object;
static scObjectType_t *vec3_type;

typedef enum 
{
  VEC3_X,
  VEC3_Y,
  VEC3_Z,
} vec3_closures;

static void vec3_set ( scObjectInstance_t *self, scDataTypeValue_t *value, void *closure)
{
  int settype = (int)closure;
  vec3_t vec3;
    
  memcpy( vec3, self->pointer, sizeof( vec3 ) );
  
  switch (settype)
  {
    case VEC3_X:
      vec3[0] = value->data.floating ;
      break;
    case VEC3_Y:
      vec3[1] = value->data.floating ;
      break;
    case VEC3_Z:
      vec3[2] = value->data.floating ;
      break;
    default:
      return;
  }
  memcpy( self->pointer, vec3, sizeof( vec3 ) );
}

static scDataTypeValue_t *vec3_get ( scObjectInstance_t *self, void *closure )
{
  scDataTypeValue_t *value;
  int gettype = (int)closure;
  vec3_t vec3;
  
  memcpy( vec3, self->pointer, sizeof( vec3 ) );
  value = BG_Alloc(sizeof(scDataTypeValue_t));
  value->type = TYPE_FLOAT;
  
  switch (gettype)
  {
    case VEC3_X:
      value->data.floating = vec3[0];
      break;
    case VEC3_Y:
      value->data.floating = vec3[1];
      break;
    case VEC3_Z:
      value->data.floating = vec3[2];
      break;
    default:
      value->type = TYPE_UNDEF;
      break;
      // Error
  }
  SC_ValueGCInc( value );
  return value;
}

static scObjectMember_t vec3_members[] = {
    { "x", "", TYPE_FLOAT, vec3_set, vec3_get,
        (void*)VEC3_X },
    { "y", "", TYPE_FLOAT, vec3_set, vec3_get,
        (void*)VEC3_Y },
    { "z", "", TYPE_FLOAT, vec3_set, vec3_get,
        (void*)VEC3_Z },
    { NULL },
};

static scObjectMethod_t vec3_methods[] = {
    { NULL },
};

static scObjectDef_t vec3_object = { 
  "Vec3", 
  0, { TYPE_UNDEF }, 
  vec3_members, 
  vec3_methods, 
};

scObjectInstance_t *SC_Vec3_New( float *vect )
{
  scObjectInstance_t *instance;
  instance = BG_Alloc( sizeof( scObjectInstance_t ) );
  
  instance->pointer = (void*)vect;
  
  instance->type = vec3_type;
  return instance;
}

void SC_Common_Init( void )
{
  scDataTypeValue_t vec3typeobject;
//  SC_AddLibrary( "game", game_lib );
  SC_AddObjectType( "common", vec3_object);
  SC_NamespaceGet( "common.Vec3", &vec3typeobject );
  vec3_type = vec3typeobject.data.objecttype;
}
