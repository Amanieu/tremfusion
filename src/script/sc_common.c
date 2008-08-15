/*
===========================================================================
Copyright (C) 2008 John Black
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

// sc_common.c
// common function and objects

#include "../game/g_local.h"
#include "../qcommon/q_shared.h"

#include "sc_public.h"

/*
======================================================================

Global Functions

======================================================================
*/

static int common_Print( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure )
{
  while(in->type != TYPE_UNDEF)
  {
    Com_Printf(SC_StringToChar(in->data.string));
    in++;
  }
  out->type = TYPE_UNDEF;

  return 0;
}

static scLibFunction_t common_lib[] = {
  { "Print", "", common_Print, { TYPE_ANY , TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

/*
======================================================================

Vec3

======================================================================
*/

scClass_t *vec3_class;

typedef struct
{
  qboolean sc_created; // qtrue if created from python or lua, false if created by SC_Vec3FromVec3_t
                       // Prevents call of BG_Free on a vec3_t 
  float   *vect;       
}sc_vec3_t;

typedef enum 
{
  VEC3_X,
  VEC3_Y,
  VEC3_Z,
} vec3_closures;

void SC_Common_Constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scObject_t *self = SC_ObjectNew(in[0].data.class);

  out->type = TYPE_OBJECT;
  out->data.object = self;
}

static int vec3_set ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  int settype = (int)closure;
  sc_vec3_t *data;
  vec3_t vec3;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;
    
  memcpy( vec3, data->vect, sizeof( vec3 ) );
  
  switch (settype)
  {
    case VEC3_X:
      vec3[0] = in[1].data.floating ;
      break;
    case VEC3_Y:
      vec3[1] = in[1].data.floating ;
      break;
    case VEC3_Z:
      vec3[2] = in[1].data.floating ;
      break;
    default:
      return -1;
  }
  memcpy( data->vect, vec3, sizeof( vec3 ) );

  return 0;
}

static int vec3_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  int gettype = (int)closure;
  sc_vec3_t *data;
  vec3_t vec3;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;
  
  memcpy( vec3, data->vect, sizeof( vec3 ) );
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

  return 0;
}

static int vec3_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  sc_vec3_t *data;
  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;

  self->data.type = TYPE_USERDATA;
  data = BG_Alloc(sizeof(sc_vec3_t));
  data->sc_created = qtrue;
  data->vect = BG_Alloc(sizeof(float) * 3);
  memset(data->vect, 0x00, sizeof(float) * 3);
  self->data.data.userdata = data;

  return 0;
}

static int vec3_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  sc_vec3_t *data;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;

  if(data->sc_created)
    BG_Free(data->vect);
  
  BG_Free(data);

  return 0;
}

static scLibObjectMember_t vec3_members[] = {
    { "x", "", TYPE_FLOAT, vec3_set, vec3_get, (void*)VEC3_X },
    { "y", "", TYPE_FLOAT, vec3_set, vec3_get, (void*)VEC3_Y },
    { "z", "", TYPE_FLOAT, vec3_set, vec3_get, (void*)VEC3_Z },
    { "" },
};

static scLibObjectMethod_t vec3_methods[] = {
    { "" },
};

static scLibObjectDef_t vec3_def = { 
  "Vec3", "",
  vec3_constructor, { TYPE_UNDEF },
  vec3_destructor,
  vec3_members, 
  vec3_methods, 
  NULL,
  NULL
};

scObject_t *SC_Vec3FromVec3_t( float *vect )
{
  scObject_t *instance;
  sc_vec3_t *data;
  instance = SC_ObjectNew(vec3_class);

  instance->data.type = TYPE_USERDATA;
  data = BG_Alloc(sizeof(sc_vec3_t));
  data->sc_created = qfalse;
  data->vect = vect;
  instance->data.data.userdata = data;

  return instance;
}

/*
======================================================================

Vec4

======================================================================
*/

scClass_t *vec4_class;

typedef struct
{
  qboolean sc_created; // qtrue if created from python or lua, false if created by SC_Vec4FromVec4_t
                       // Prevents call of BG_Free on a vec3_t 
  float   *vect;       
}sc_vec4_t;

typedef enum 
{
  VEC4_R,
  VEC4_G,
  VEC4_B,
  VEC4_A,
} vec4_closures;

static int vec4_set ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  int settype = (int)closure;
  sc_vec4_t *data;
  vec4_t vec4;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;
    
  memcpy( vec4, data->vect, sizeof( vec4 ) );
  
  switch (settype)
  {
    case VEC4_R:
      vec4[0] = in[1].data.floating ;
      break;
    case VEC4_G:
      vec4[1] = in[1].data.floating ;
      break;
    case VEC4_B:
      vec4[2] = in[1].data.floating ;
      break;
    case VEC4_A:
      vec4[3] = in[1].data.floating ;
      break;
    default:
      return -1;
  }
  memcpy( data->vect, vec4, sizeof( vec4 ) );
  out[0].type = TYPE_UNDEF;
  return 0;
}

static int vec4_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  int gettype = (int)closure;
  sc_vec3_t *data;
  vec4_t vec4;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;
  
  memcpy( vec4, data->vect, sizeof( vec4 ) );
  out[0].type = TYPE_FLOAT;
  
  switch (gettype)
  {
    case VEC4_R:
      out[0].data.floating = vec4[0];
      break;
    case VEC4_G:
      out[0].data.floating = vec4[1];
      break;
    case VEC4_B:
      out[0].data.floating = vec4[2];
      break;
    case VEC4_A:
      out[0].data.floating = vec4[3];
      break;
    default:
      out[0].type = TYPE_UNDEF;
      break;
      // Error
  }

  return 0;
}

static int vec4_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  sc_vec4_t *data;
  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;

  self->data.type = TYPE_USERDATA;
  data = BG_Alloc(sizeof(sc_vec4_t));
  data->sc_created = qtrue;
  data->vect = BG_Alloc(sizeof(float) * 4);
  memset(data->vect, 0x00, sizeof(float) * 4);
  self->data.data.userdata = data;

  return 0;
}

static int vec4_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;
  sc_vec4_t *data;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;

  if(data->sc_created)
    BG_Free(data->vect);
  
  BG_Free(data);

  return 0;
}

static scLibObjectMember_t vec4_members[] = {
    { "r", "", TYPE_FLOAT, vec4_set, vec4_get, (void*)VEC4_R },
    { "g", "", TYPE_FLOAT, vec4_set, vec4_get, (void*)VEC4_G },
    { "b", "", TYPE_FLOAT, vec4_set, vec4_get, (void*)VEC4_B },
    { "a", "", TYPE_FLOAT, vec4_set, vec4_get, (void*)VEC4_A },
    { "" },
};

static scLibObjectMethod_t vec4_methods[] = {
    { "" },
};

static scLibObjectDef_t vec4_def = { 
  "Vec4", "",
  vec4_constructor, { TYPE_UNDEF },
  vec4_destructor,
  vec4_members, 
  vec4_methods, 
  NULL,
  NULL
};

scObject_t *SC_Vec4FromVec4_t( float *vect )
{
  scObject_t *instance;
  sc_vec4_t *data;
  instance = SC_ObjectNew(vec4_class);

  instance->data.type = TYPE_USERDATA;
  data = BG_Alloc(sizeof(sc_vec4_t));
  data->sc_created = qfalse;
  data->vect = vect;
  instance->data.data.userdata = data;

  return instance;
}

vec4_t *SC_Vec4t_from_Vec4( scObject_t *vectobject )
{
  sc_vec4_t *data;
  
  data = vectobject->data.data.userdata;

  return data->vect;
}

/*
======================================================================

Modules

======================================================================
*/

scClass_t *module_class;

static void add_autohooks(scDataTypeArray_t *autohook)
{
  int i;
  scDataTypeValue_t value;
  scDataTypeArray_t *h;
  const char *name;
  const char *location;
  const char *tag;
  scDataTypeFunction_t *function;
  scEvent_t *event;
  scEventNode_t *node;
  scEventNode_t *parent;

  for(i = 0; i < autohook->size; i++)
  {
    SC_ArrayGet(autohook, i, &value);
    if(value.type != TYPE_ARRAY)
    {
      // TODO: error
      continue;
    }

    h = value.data.array;

    // TODO: check all types
    SC_ArrayGet(h, 0, &value);
    SC_NamespaceGet(SC_StringToChar(value.data.string), &value);
    event = value.data.object->data.data.userdata;

    SC_ArrayGet(h, 1, &value);
    name = SC_StringToChar(value.data.string);

    SC_ArrayGet(h, 2, &value);
    location = SC_StringToChar(value.data.string);

    SC_ArrayGet(h, 3, &value);
    tag = SC_StringToChar(value.data.string);

    SC_ArrayGet(h, 4, &value);
    if(value.type == TYPE_UNDEF)
      function = NULL;
    else
      function = value.data.function;

    parent = SC_Event_FindChild(event->root, tag);

    node = SC_Event_NewNode(name);
    if(function)
    {
      node->type = SC_EVENT_NODE_TYPE_HOOK;
      node->hook = function;
    }
    else
      node->type = SC_EVENT_NODE_TYPE_NODE;

    if(strcmp(location, "inside") == 0)
      SC_Event_AddNode(parent, parent->last, node);
    else if(strcmp(location, "before") == 0)
      SC_Event_AddNode(parent->parent, parent->previous, node);
    else if(strcmp(location, "after") == 0)
      SC_Event_AddNode(parent->parent, parent, node);
    else
    {
      // Error: invalid location
    }
  }
}

static void remove_autohooks(scDataTypeArray_t *autohook)
{
  int i;
  scDataTypeValue_t value;
  scDataTypeArray_t *h;
  const char *name;
  scEvent_t *event;
  scEventNode_t *node;

  for(i = autohook->size - 1; i >= 0; i--)
  {
    SC_ArrayGet(autohook, i, &value);
    if(value.type != TYPE_ARRAY)
    {
      // TODO: error
      continue;
    }

    h = value.data.array;

    // TODO: check all types
    SC_ArrayGet(h, 0, &value);
    SC_NamespaceGet(SC_StringToChar(value.data.string), &value);
    event = value.data.object->data.data.userdata;

    SC_ArrayGet(h, 1, &value);
    name = SC_StringToChar(value.data.string);

    node = SC_Event_FindChild(event->root, name);
    SC_Event_DeleteNode(node);
  }
}

void SC_Module_Init(scObject_t *self)
{
  scDataTypeValue_t value;
  scDataTypeHash_t *hash;

  hash = SC_HashNew();

  value.type = TYPE_BOOLEAN;
  value.data.boolean = qfalse;
  SC_HashSet(hash, "loaded", &value);

  value.type = TYPE_ARRAY;
  value.data.array = SC_ArrayNew();
  SC_HashSet(hash, "registered", &value);

  self->data.type = TYPE_HASH;
  self->data.data.hash = hash;
}

void SC_Module_Register(scObject_t *self, scObject_t *toregister)
{
  scDataTypeValue_t value;
  scDataTypeArray_t *array;

  SC_HashGet(self->data.data.hash, "registered", &value);

  array = value.data.array;

  value.type = TYPE_OBJECT;
  value.data.object = toregister;
  SC_ArraySet(array, array->size, &value);
}

void SC_Module_Load(scObject_t *self)
{
  scDataTypeHash_t *hash;
  scDataTypeFunction_t *autoload = NULL;
  scDataTypeArray_t *autohook = NULL;
  scDataTypeArray_t *conflict = NULL;
  scDataTypeArray_t *depend = NULL;
  const char* name;
  scDataTypeValue_t value, value2;
  scDataTypeValue_t fin[2];
  scDataTypeValue_t fout;
  int i;

  hash = self->data.data.hash;

  SC_HashGet(hash, "name", &value);
  name = SC_StringToChar(value.data.string);

  SC_HashGet(hash, "conflict", &value);
  if(value.type == TYPE_ARRAY)
    conflict = value.data.array;

  SC_HashGet(hash, "depend", &value);
  if(value.type == TYPE_ARRAY)
    depend = value.data.array;

  SC_HashGet(hash, "autoload", &value);
  if(value.type == TYPE_FUNCTION)
    autoload = value.data.function;

  SC_HashGet(hash, "autohooks", &value);
  if(value.type == TYPE_ARRAY)
    autohook = value.data.array;

  // Make error if conflicted module is loaded
  if(conflict)
  {
    for(i = 0; i < conflict->size; i++)
    {
      SC_ArrayGet(conflict, i, &value);
      if(value.type != TYPE_STRING)
      {
        // TODO: error
      }

      SC_NamespaceGet(va("module.%s", SC_StringToChar(value.data.string)), &value2);
      if(value2.type != TYPE_UNDEF)
      {
        SC_HashGet(value2.data.object->data.data.hash, "loaded", &value);
        if(value.data.boolean)
        {
          // TODO: conflict error
        }
      }
    }
  }

  // Load all unloaded depends
  if(depend)
  {
    for(i = 0; i < depend->size; i++)
    {
      SC_ArrayGet(depend, i, &value);
      if(value.type != TYPE_STRING)
      {
        // TODO: error
      }

      SC_NamespaceGet(va("module.%s", SC_StringToChar(value.data.string)), &value2);
      if(value2.type != TYPE_UNDEF)
      {
        SC_HashGet(value2.data.object->data.data.hash, "loaded", &value);
        if(!value.data.boolean)
        {
          SC_Module_Register(value2.data.object, self);
          SC_Module_Load(value2.data.object);
        }
      }
      else
      {
        // TODO: error : can't autoload unknow module
      }
    }
  }

  // Call "autoload" function
  if(autoload)
  {
    fin[0].type = TYPE_OBJECT;
    fin[0].data.object = self;
    fin[1].type = TYPE_UNDEF;
    SC_RunFunction(autoload, fin, &fout);
  }

  // Add autohooks
  if(autohook)
    add_autohooks(autohook);

  // TODO: move all module stuff to root namespace

  // Set "loaded" flag
  value.type = TYPE_BOOLEAN;
  value.data.boolean = qtrue;
  SC_HashSet(hash, "loaded", &value);
}

void SC_Module_Unload(scObject_t *self)
{
  scDataTypeHash_t *hash;
  scDataTypeFunction_t *autounload = NULL;
  scDataTypeArray_t *autohook = NULL;
  scDataTypeArray_t *using;
  scDataTypeValue_t value;
  const char *name;
  scDataTypeValue_t fin[2];
  scDataTypeValue_t fout;

  hash = self->data.data.hash;

  SC_HashGet(hash, "name", &value);
  name = SC_StringToChar(value.data.string);

  SC_HashGet(hash, "registered", &value);
  using = value.data.array;

  SC_HashGet(hash, "autounload", &value);
  if(value.type == TYPE_FUNCTION)
    autounload = value.data.function;

  SC_HashGet(hash, "autohooks", &value);
  if(value.type == TYPE_ARRAY)
    autohook = value.data.array;

  // Check if able to unload (must not have dependent module loaded)
  if(using->size > 0)
  {
    // TODO: can't unload module : other modules depend on it
  }
 
  // Remove autohooks
  if(autohook)
    remove_autohooks(autohook);

  // Call "autounload" function
  if(autounload)
  {
    fin[0].type = TYPE_OBJECT;
    fin[0].data.object = self;
    fin[1].type = TYPE_UNDEF;
    SC_RunFunction(autounload, fin, &fout);
  }

  // TODO: remove all module stuff in root namespace

  // Unset "loaded" flag
  value.type = TYPE_BOOLEAN;
  value.data.boolean = qfalse;
  SC_HashGet(hash, "loaded", &value);
}

typedef enum
{
  MODULE_NAME,
  MODULE_VERSION,
  MODULE_AUTHOR,
  MODULE_DEPEND,
  MODULE_CONFLICT,
  MODULE_AUTOLOAD,
  MODULE_AUTOUNLOAD,
  MODULE_AUTOTAG,
  MODULE_LOADED
} module_closures;

static int module_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scObject_t *self;

  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;

  SC_Module_Init(self);
  SC_HashSet(self->data.data.hash, "name", &in[1]);

  return 0;
}

static int module_set(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scDataTypeHash_t *hash;

  hash = in[0].data.object->data.data.hash;
  SC_HashSet(hash, (char*)closure, &in[1]);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_get(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  scDataTypeHash_t *hash;

  hash = in[0].data.object->data.data.hash;
  SC_HashGet(hash, (char*)closure, out);

  return 0;
}

static int module_load(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  SC_Module_Load(in[0].data.object);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_register(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  SC_Module_Register(in[0].data.object, in[1].data.object);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_unload(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  SC_Module_Unload(in[0].data.object);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_dump(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  // TODO: error management
  SC_ValueDump(&in[0].data.object->data);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static scLibObjectMember_t module_members[] = {
  { "name", "", TYPE_STRING, module_set, module_get, (void*) "name" },
  { "version", "", TYPE_STRING, module_set, module_get, (void*) "version" },
  { "author", "", TYPE_STRING, module_set, module_get, (void*) "author" },
  { "depend", "", TYPE_ARRAY, module_set, module_get, (void*) "depend" },
  { "conflict", "", TYPE_ARRAY, module_set, module_get, (void*) "conflict" },
  { "autoload", "", TYPE_FUNCTION, module_set, module_get, (void*) "autoload" },
  { "autounload", "", TYPE_FUNCTION, module_set, module_get, (void*) "autounload" },
  { "autohooks", "", TYPE_ARRAY, module_set, module_get, (void*) "autohooks" },
  { "loaded", "", TYPE_BOOLEAN, 0, module_get, (void*) "loaded" },
  { "" }
};

static scLibObjectMethod_t module_methods[] = {
  { "load", "", module_load, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "unload", "", module_unload, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "register", "", module_register, { TYPE_OBJECT, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "dump", "", module_dump, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

static scLibObjectDef_t module_def = {
  "Module", "",
  module_constructor, { TYPE_STRING },
  0, // a module should never be destroyed
  module_members,
  module_methods,
  NULL,
  NULL
};

void SC_Common_Init( void )
{
  SC_AddLibrary( "common", common_lib );
  vec3_class = SC_AddClass( "common", &vec3_def);
  vec4_class = SC_AddClass( "common", &vec4_def);
  module_class = SC_AddClass("script", &module_def);
}

