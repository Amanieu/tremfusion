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
  scDataTypeValue_t *args = in;

  while(args->type != TYPE_UNDEF)
  {
    if(in->type != TYPE_STRING)
      SC_EXEC_ERROR(va("argument #%d need a string, but have a %s", args-in, SC_DataTypeToString(in->type)));
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
  vec_t   *vect;       
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
  scObject_t *self;
  int settype = (int)closure;
  sc_vec3_t *data;
  vec_t *vec3;
  
  self = in[0].data.object;
  data = self->data.data.userdata;
    
  vec3 = data->vect;
  
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
      SC_EXEC_ERROR(va("internal error running `vec3.set' function in %s (%d) : %d: unknown closure", __FILE__, __LINE__, settype));
  }

  out->type = TYPE_UNDEF;
  return 0;
}

static int vec3_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scObject_t *self;
  int gettype = (int)closure;
  sc_vec3_t *data;
  vec_t *vec3;
  
  self = in[0].data.object;
  data = self->data.data.userdata;
  
  vec3 = data->vect;
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
      SC_EXEC_ERROR(va("internal error running `vec3.get' function in %s (%d) : %d: unknow closure", __FILE__, __LINE__, gettype));
  }

  return 0;
}

static int vec3_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scObject_t *self;
  sc_vec3_t *data;
  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;

  self->data.type = TYPE_USERDATA;
  data = BG_Alloc(sizeof(sc_vec3_t));
  data->sc_created = qtrue;
  data->vect = BG_Alloc(sizeof(vec3_t));
  memset(data->vect, 0x00, sizeof(vec3_t));
  self->data.data.userdata = data;

  return 0;
}

static int vec3_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scObject_t *self;
  sc_vec3_t *data;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;

  if(data->sc_created)
    BG_Free(data->vect);
  
  BG_Free(data);

  out->type = TYPE_UNDEF;
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
  vec_t   *vect;       
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
  scObject_t *self;
  int settype = (int)closure;
  sc_vec4_t *data;
  vec_t *vec4;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;
    
  vec4 = data->vect;
  
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
      SC_EXEC_ERROR(va("internal error running `vec4.set' function in %s (%d) : %d: unknow closure", __FILE__, __LINE__, settype));
  }
  out[0].type = TYPE_UNDEF;
  return 0;
}

static int vec4_get ( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scObject_t *self;
  int gettype = (int)closure;
  sc_vec3_t *data;
  vec_t *vec4;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;
  
  vec4 = data->vect;
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
      SC_EXEC_ERROR(va("internal error running `vec4.get' function in %s (%d) : %d: unknow closure", __FILE__, __LINE__, gettype));
  }

  return 0;
}

static int vec4_constructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
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

  out->type = TYPE_UNDEF;
  return 0;
}

static int vec4_destructor(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scObject_t *self;
  sc_vec4_t *data;
  
  self = in[0].data.object;
  data =  self->data.data.userdata;

  if(data->sc_created)
    BG_Free(data->vect);
  
  BG_Free(data);

  out->type = TYPE_UNDEF;
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

  return (vec4_t*)data->vect;
}

/*
======================================================================

Modules

======================================================================
*/

scClass_t *module_class;

static int add_autohooks(scDataTypeArray_t *autohook, scDataTypeValue_t *out)
{
  int i, ret;
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
      SC_EXEC_ERROR(va("invalid format in argument. Attempt an array but having a %s", SC_DataTypeToString(value.type)));

    h = value.data.array;

    SC_ArrayGet(h, 0, &value);
    if(value.type != TYPE_STRING)
      SC_EXEC_ERROR(va("invalid format in argument. Attempt a string but having a %s", SC_DataTypeToString(value.type)));
    SC_NamespaceGet(SC_StringToChar(value.data.string), &value);
    event = value.data.object->data.data.userdata;

    SC_ArrayGet(h, 1, &value);
    if(value.type != TYPE_STRING)
      SC_EXEC_ERROR(va("invalid format in argument. Attempt a string but having a %s", SC_DataTypeToString(value.type)));
    name = SC_StringToChar(value.data.string);

    SC_ArrayGet(h, 2, &value);
    if(value.type != TYPE_STRING)
      SC_EXEC_ERROR(va("invalid format in argument. Attempt a string but having a %s", SC_DataTypeToString(value.type)));
    location = SC_StringToChar(value.data.string);

    SC_ArrayGet(h, 3, &value);
    if(value.type != TYPE_STRING)
      SC_EXEC_ERROR(va("invalid format in argument. Attempt a string but having a %s", SC_DataTypeToString(value.type)));
    tag = SC_StringToChar(value.data.string);

    SC_ArrayGet(h, 4, &value);
    if(value.type == TYPE_UNDEF)
      function = NULL;
    else
    {
      if(value.type != TYPE_FUNCTION)
        SC_EXEC_ERROR(va("invalid format in argument. Attempt a function but having a %s", SC_DataTypeToString(value.type)));
      function = value.data.function;
    }

    parent = SC_Event_FindChild(event->root, tag);
    if(!parent)
      SC_EXEC_ERROR(va("can't load autohooks: unable to find hook by tag %s", tag));

    node = SC_Event_NewNode(name);
    if(function)
    {
      node->type = SC_EVENT_NODE_TYPE_HOOK;
      node->hook = function;
    }
    else
      node->type = SC_EVENT_NODE_TYPE_GROUP;

    if(strcmp(location, "inside") == 0)
      ret = SC_Event_AddNode(parent, parent->last, node, out);
    else if(strcmp(location, "before") == 0)
      ret = SC_Event_AddNode(parent->parent, parent->previous, node, out);
    else if(strcmp(location, "after") == 0)
      ret = SC_Event_AddNode(parent->parent, parent, node, out);
    else
      SC_EXEC_ERROR(va("invalid location in argument. Location should be `inside', `before' or `after', but having `%s'", location));

    if(ret != 0)
      return -1;
  }

  out->type = TYPE_UNDEF;
  return 0;
}

static int remove_autohooks(scDataTypeArray_t *autohook, scDataTypeValue_t *out)
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
      SC_EXEC_ERROR(va("invalid format in argument. Attempt an array but having a %s", SC_DataTypeToString(value.type)));

    h = value.data.array;

    SC_ArrayGet(h, 0, &value);
    if(value.type != TYPE_STRING)
      SC_EXEC_ERROR(va("invalid format in argument. Attempt a string but having a %s", SC_DataTypeToString(value.type)));
    SC_NamespaceGet(SC_StringToChar(value.data.string), &value);
    event = value.data.object->data.data.userdata;

    SC_ArrayGet(h, 1, &value);
    if(value.type != TYPE_STRING)
      SC_EXEC_ERROR(va("invalid format in argument. Attempt a string but having a %s", SC_DataTypeToString(value.type)));
    name = SC_StringToChar(value.data.string);

    node = SC_Event_FindChild(event->root, name);
    if(!node)
      SC_EXEC_ERROR(va("can't delete hook: can't find %s tag", name));

    if(SC_Event_DeleteNode(node, out) != 0)
      return -1;
  }

  return 0;
}

int SC_Module_Init(scObject_t *self)
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

  return 0;
}

int SC_Module_Register(scObject_t *self, scObject_t *toregister)
{
  scDataTypeValue_t value;
  scDataTypeArray_t *array;

  SC_HashGet(self->data.data.hash, "registered", &value);

  array = value.data.array;

  value.type = TYPE_OBJECT;
  value.data.object = toregister;
  SC_ArraySet(array, array->size, &value);

  return 0;
}

// FIXME: unloading don't work fine, have to fix it
int SC_Module_Load(scObject_t *self, scDataTypeValue_t *out)
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

  SC_HashGet(hash, "loaded", &value);
  if(value.data.boolean)
    SC_EXEC_ERROR(va("can't load module %s: allready loaded", name));

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
      const char *name;

      SC_ArrayGet(conflict, i, &value);

      name = SC_StringToChar(value.data.string);
      SC_NamespaceGet(va("module.%s", name), &value2);
      if(value2.type != TYPE_UNDEF)
      {
        SC_HashGet(value2.data.object->data.data.hash, "loaded", &value);
        if(value.data.boolean)
          SC_EXEC_ERROR(va("can't load module: conflicting with %s", name));
      }
    }
  }

  // Load all unloaded depends
  if(depend)
  {
    for(i = 0; i < depend->size; i++)
    {
      int ret;
      const char *name;

      SC_ArrayGet(depend, i, &value);
	  name = SC_StringToChar(value.data.string);
	  if(value.type == TYPE_UNDEF)
		  continue;

      SC_NamespaceGet(va("module.%s", name), &value2);
      if(value2.type != TYPE_UNDEF)
      {
        SC_HashGet(value2.data.object->data.data.hash, "loaded", &value);
        if(!value.data.boolean)
        {
          SC_Module_Register(value2.data.object, self);
          ret = SC_Module_Load(value2.data.object, out);
          if(ret == -1)
            return -1;
        }
      }
      else
        SC_EXEC_ERROR(va("can't load module: unknow depends (%s)", name));
    }
  }

  // Call "autoload" function
  if(autoload)
  {
    fin[0].type = TYPE_OBJECT;
    fin[0].data.object = self;
    fin[1].type = TYPE_UNDEF;
    if(SC_RunFunction(autoload, fin, &fout) == -1)
      SC_EXEC_ERROR(SC_StringToChar(fout.data.string))

    if(fout.data.boolean == qfalse)
      return 0;
  }

  // Add autohooks
  if(autohook)
  {
    int ret = add_autohooks(autohook, out);
    if(ret == -1)
      return -1;
  }

  // copy all module stuff to root namespace
  value.type = TYPE_OBJECT;
  value.data.object = self;
  SC_NamespaceSet(name, &value);

  // Set "loaded" flag
  value.type = TYPE_BOOLEAN;
  value.data.boolean = qtrue;
  SC_HashSet(hash, "loaded", &value);

  return 1;
}

int SC_Module_Unload(scObject_t *self, scDataTypeValue_t *out, int force)
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

  if(!force)
  {
    SC_HashGet(hash, "loaded", &value);
    if(value.data.boolean == qfalse)
      SC_EXEC_ERROR(va("can't unload module %s: not loaded", name));
  }

  SC_HashGet(hash, "registered", &value);
  using = value.data.array;

  SC_HashGet(hash, "autounload", &value);
  if(value.type == TYPE_FUNCTION)
    autounload = value.data.function;

  SC_HashGet(hash, "autohooks", &value);
  if(value.type == TYPE_ARRAY)
    autohook = value.data.array;

  // Check if able to unload (must not have dependent module loaded)
  if(!force)
  {
    if(using->size > 0)
    {
      SC_ArrayGet(using, 0, &value);
      SC_EXEC_ERROR(va("can't unload module %s: module %s depend on it", name, SC_StringToChar(value.data.string)));
    }
  }
 
  // Call "autounload" function
  if(autounload)
  {
    fin[0].type = TYPE_OBJECT;
    fin[0].data.object = self;
    fin[1].type = TYPE_UNDEF;
    SC_RunFunction(autounload, fin, &fout);
    if(force == qfalse && fout.data.boolean == qfalse)
      return 0;
  }

  // Remove autohooks
  if(autohook)
  {
    int ret = remove_autohooks(autohook, out);
    if(ret == -1)
      return -1;
  }

  // remove all module stuff in root namespace
  SC_NamespaceDelete(name);

  // Unset "loaded" flag
  value.type = TYPE_BOOLEAN;
  value.data.boolean = qfalse;
  SC_HashGet(hash, "loaded", &value);

  return 1;
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
  scObject_t *self;
  scDataTypeHash_t *args, *hash;
  scDataTypeValue_t value;
  const char *key;
  const char *name;

  SC_Common_Constructor(in, out, closure);
  self = out[0].data.object;
  SC_Module_Init(self);

  hash = self->data.data.hash;

  args = in[1].data.hash;
  key = SC_HashFirst(args, &value);
  while(key)
  {
    // if it's a valid key
    if( strcmp(key, "name") == 0 ||
        strcmp(key, "version") == 0 ||
        strcmp(key, "description") == 0 ||
        strcmp(key, "author") == 0 ||
        strcmp(key, "depend") == 0 ||
        strcmp(key, "conflict") == 0)
      SC_HashSet(hash, key, &value);
    key = SC_HashNext(args, key, &value);
  }

  SC_HashGet(hash, "name", &value);
  if(value.type == TYPE_UNDEF)
    SC_EXEC_ERROR(va("Can't create module: no name"));

  name = va("module.%s", SC_StringToChar(value.data.string));
  SC_NamespaceGet(name, &value);
  if(value.type != TYPE_UNDEF)
    SC_EXEC_ERROR(va("Can't create module %s: a module with this name allready exist", name));

  value.type = TYPE_OBJECT;
  value.data.object = self;
  SC_NamespaceSet(name, &value);

  return 0;
}

static int null_set(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  SC_EXEC_ERROR(va("Can't access field `%s' to write: access denied", (char*) closure));
}

static int module_set(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *hash;

  hash = in[0].data.object->data.data.hash;
  SC_HashSet(hash, (char*)closure, &in[1]);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_metaset(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *hash;

  hash = in[0].data.object->data.data.hash;
  SC_HashSet(hash, SC_StringToChar(in[1].data.string), &in[2]);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_get(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *hash;

  hash = in[0].data.object->data.data.hash;
  SC_HashGet(hash, (char*)closure, out);
  if(out->type == TYPE_UNDEF)
    SC_EXEC_ERROR(va("can't get `%s' module field: unknow value", (char*)closure));

  return 0;
}

static int module_metaget(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *hash;

  hash = in[0].data.object->data.data.hash;
  SC_HashGet(hash, SC_StringToChar(in[1].data.string), out);
  if(out->type == TYPE_UNDEF)
    SC_EXEC_ERROR(va("can't get `%s' module field: unknow value", (char*)closure));

  return 0;
}

static int module_load(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  int ret = SC_Module_Load(in[0].data.object, out);
  if(ret == -1)
    return -1;

  out->type = TYPE_BOOLEAN;
  out->data.boolean = ret;

  return 0;
}

static int module_register(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  int ret = SC_Module_Register(in[0].data.object, in[1].data.object);
  if(ret == -1)
    return -1;

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int module_unload(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  int ret = SC_Module_Unload(in[0].data.object, out, (int)closure);
  if(ret == -1)
    return -1;

  out->type = TYPE_BOOLEAN;
  out->data.boolean = ret;

  return 0;
}

static int module_dump(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  SC_ValueDump(&in[0].data.object->data);

  out[0].type = TYPE_UNDEF;

  return 0;
}

static scLibObjectMember_t module_members[] = {
  { "name", "", TYPE_STRING, null_set, module_get, (void*) "name" },
  { "version", "", TYPE_STRING, null_set, module_get, (void*) "version" },
  { "description", "", TYPE_STRING, null_set, module_get, (void*) "description" },
  { "author", "", TYPE_STRING, null_set, module_get, (void*) "author" },
  { "depend", "", TYPE_ARRAY, null_set, module_get, (void*) "depend" },
  { "conflict", "", TYPE_ARRAY, null_set, module_get, (void*) "conflict" },
  { "autoload", "", TYPE_FUNCTION, module_set, module_get, (void*) "autoload" },
  { "autounload", "", TYPE_FUNCTION, module_set, module_get, (void*) "autounload" },
  { "autohooks", "", TYPE_ARRAY, module_set, module_get, (void*) "autohooks" },
  { "loaded", "", TYPE_BOOLEAN, null_set, module_get, (void*) "loaded" },
  { "_", "", TYPE_ANY, module_metaset, module_metaget, NULL },
  { "" }
};

static scLibObjectMethod_t module_methods[] = {
  { "load", "", module_load, { TYPE_UNDEF }, TYPE_BOOLEAN, NULL },
  { "unload", "", module_unload, { TYPE_UNDEF }, TYPE_BOOLEAN, (void*) 0 },
  { "kill", "", module_unload, { TYPE_UNDEF }, TYPE_BOOLEAN, (void*) 1 },
  { "register", "", module_register, { TYPE_OBJECT, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "dump", "", module_dump, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

static scLibObjectDef_t module_def = {
  "Module", "",
  module_constructor, { TYPE_HASH, TYPE_UNDEF },
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

