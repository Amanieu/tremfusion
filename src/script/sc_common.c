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

/*static scLibObjectDef *vec3_object;*/
static scClass_t *vec3_class;

typedef enum 
{
  VEC3_X,
  VEC3_Y,
  VEC3_Z,
} vec3_closures;

static void vec3_set ( scObject_t *self, void *closure, scDataTypeValue_t *in, scDataTypeValue_t *out)
{
  int settype = (int)closure;
  vec3_t vec3;
    
  memcpy( vec3, self->data.data.userdata, sizeof( vec3 ) );
  
  switch (settype)
  {
    case VEC3_X:
      vec3[0] = in[0].data.floating ;
      break;
    case VEC3_Y:
      vec3[1] = in[0].data.floating ;
      break;
    case VEC3_Z:
      vec3[2] = in[0].data.floating ;
      break;
    default:
      return;
  }
  memcpy( self->data.data.userdata, vec3, sizeof( vec3 ) );
}

static void vec3_get ( scObject_t *self, void *closure, scDataTypeValue_t *in, scDataTypeValue_t *out )
{
  int gettype = (int)closure;
  vec3_t vec3;
  
  memcpy( vec3, self->data.data.userdata, sizeof( vec3 ) );
  out[0].type = TYPE_FLOAT;
  
  switch (gettype)
  {
    case VEC3_X:
      out[0].data.floating = vec3[0];
      break;
    case VEC3_Y:
      out[0].data.floating = vec3[1];
      break;
    case VEC3_Z:
      out[0].data.floating = vec3[2];
      break;
    default:
      out[0].type = TYPE_UNDEF;
      break;
      // Error
  }
}

static scLibObjectMember_t vec3_members[] = {
    { "x", "", TYPE_FLOAT, vec3_set, vec3_get, (void*)VEC3_X },
    { "y", "", TYPE_FLOAT, vec3_set, vec3_get, (void*)VEC3_Y },
    { "z", "", TYPE_FLOAT, vec3_set, vec3_get, (void*)VEC3_Z },
    { NULL },
};

static scLibObjectMethod_t vec3_methods[] = {
    { NULL },
};

static scLibObjectDef_t vec3_def = { 
  "Vec3", 
  { 0 },
  { 0 },
  vec3_members, 
  vec3_methods, 
};

scObject_t *SC_Vec3FromVec3_t( float *vect )
{
  scObject_t *instance;
  instance = SC_ObjectNew(vec3_class);
  instance->data.type = TYPE_USERDATA;
  instance->data.data.userdata = (void*)vect;

  return instance;
}

void SC_Common_Init( void )
{
  vec3_class = SC_AddObjectType( "common", &vec3_def);
}

