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

// sc_event.c
// contain event managment structures

#include "sc_public.h"

static scClass_t *event_class;
static scClass_t *hook_class;
static scClass_t *tag_class;
static scClass_t *loc_class;

typedef enum
{
  EVENT_INSIDE,
  EVENT_BEFORE,
  EVENT_AFTER,
  EVENT_TAG,
} loc_closure_t;

void SC_Event_Call(scEventNode_t *node, scDataTypeHash_t *params)
{
  scEventNode_t *i;
  scDataTypeValue_t args[2];
  scDataTypeValue_t ret;

  args[0].type = TYPE_HASH;
  args[0].data.hash = params;
  args[1].type = TYPE_UNDEF;

  if(node == NULL)
    return;

  for(i = node->before; i != NULL; i = i->next)
    SC_Event_Call(i, params);

  if(node->type == SC_EVENT_NODE_TYPE_NODE)
  {
    for(i = node->inside.node; i != NULL; i = i->next)
      SC_Event_Call(i, params);
  }
  else
    SC_RunFunction(node->inside.hook, args, &ret);

  for(i = node->after; i != NULL; i = i->next)
    SC_Event_Call(i, params);
}

void SC_Event_AddNode(scEventNode_t **list, scEventNode_t *node)
{
  node->next = *list;
  *list = node;
}

void SC_Event_DeleteNode(scEventNode_t **list)
{
  scEventNode_t *node;
  while(*list)
  {
    node = *list;
    *list = (*list)->next;

    SC_Event_DeleteNode(&node->after);
    SC_Event_DeleteNode(&node->before);
    if(node->type == SC_EVENT_NODE_TYPE_NODE)
      SC_Event_DeleteNode(&node->inside.node);
    else
      SC_FunctionGCDec(node->inside.hook);

    BG_Free(node);
  }
}

scEventNode_t *SC_Event_NewNode(const char *tag)
{
  scEventNode_t *node = BG_Alloc(sizeof(scEventNode_t));
  memset(node, 0x00, sizeof(scEventNode_t));

  strcpy(node->tag, tag);

  return node;
}

scEventNode_t *SC_Event_FindChild(scEventNode_t *node, const char *tag)
{
  scEventNode_t *found;
  scEventNode_t *i;

  if(node == NULL)
    return NULL;

  if(strcmp(node->tag, tag) == 0)
    return node;
  else
  {
    for(i = node->before; i != NULL; i = i->next)
    {
      found = SC_Event_FindChild(i, tag);
      if(found)
        return found;
    }

    if(node->type == SC_EVENT_NODE_TYPE_NODE)
    {
      for(i = node->inside.node; i != NULL; i = i->next)
      {
        found = SC_Event_FindChild(i, tag);
        if(found)
          return found;
      }
    }

    for(i = node->after; i != NULL; i = i->next)
    {
      found = SC_Event_FindChild(i, tag);
      if(found)
        return found;
    }
  }

  return NULL;
}

static void event_loc_method( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  int type = (int)closure;
  scDataTypeValue_t value;
  scEventNode_t *node = (scEventNode_t*) in[0].data.object->data.data.userdata;
  scObject_t *object = SC_ObjectNew(loc_class);

  if(type == EVENT_TAG)
    node = SC_Event_FindChild(node, SC_StringToChar(in[1].data.string));

  value.type = TYPE_USERDATA;
  value.data.userdata = node;
  SC_HashSet(object->data.data.hash, "node", &value);

  value.type = TYPE_INTEGER;
  value.data.integer = type;
  SC_HashSet(object->data.data.hash, "type", &value);

  out[0].type = TYPE_OBJECT;
  out[0].data.object = object;
}

static void event_call( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEventNode_t *node = in[0].data.object->data.data.userdata;

  SC_Event_Call(node, in[1].data.hash);
}

/*static void event_constructor( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  SC_Common_Constructor(in, out, closure);
}*/

static void delete(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *args = in[0].data.object->data.data.userdata;
  scDataTypeValue_t value;
  scEventNode_t *parent;
  int type;

  // get parent node from location object
  SC_HashGet(args, "node", &value);
  parent = (scEventNode_t*) value.data.userdata;

  // get location type (after, before, inside or simple tag : implicit inside)
  SC_HashGet(args, "type", &value);
  type = value.data.integer;

  switch(type)
  {
    case EVENT_TAG:
    case EVENT_INSIDE:
      SC_Event_DeleteNode(&parent->inside.node);
      break;
    case EVENT_BEFORE:
      SC_Event_DeleteNode(&parent->before);
      break;
    case EVENT_AFTER:
      SC_Event_DeleteNode(&parent->after);
      break;
  }
}

static void add( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *args = in[0].data.object->data.data.userdata;
  scDataTypeValue_t value;
  scEventNode_t *parent;
  scEventNode_t *node = in[1].data.object->data.data.userdata;
  int type;

  // closure have info about node type (node or leaf)
  node->type = (scEventNodeType_t) closure;
  
  // get parent node from location object
  SC_HashGet(args, "node", &value);
  parent = (scEventNode_t*) value.data.userdata;

  // get location type (after, before, inside or simple tag : implicit inside)
  SC_HashGet(args, "type", &value);
  type = value.data.integer;

  switch(type)
  {
    case EVENT_TAG:
    case EVENT_INSIDE:
      SC_Event_AddNode(&parent->inside.node, node);
      break;
    case EVENT_BEFORE:
      SC_Event_AddNode(&parent->before, node);
      break;
    case EVENT_AFTER:
      SC_Event_AddNode(&parent->after, node);
      break;
  }
  node->parent = parent;
}

static void node_constructor( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  int type = (int) closure;
  scObject_t *self;
  scEventNode_t *node;

  SC_Common_Constructor(in, out, closure);

  self = out->data.object;
  node = SC_Event_NewNode(SC_StringToChar(in[1].data.string));

  node->type = type;
  if(type == SC_EVENT_NODE_TYPE_HOOK)
    node->inside.hook = in[2].data.function;

  self->data.type = TYPE_USERDATA;
  self->data.data.userdata = node;
}

static void node_destructor( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
}

static scLibObjectMethod_t event_methods[] = {
  { "tag", "", event_loc_method, { TYPE_STRING }, TYPE_OBJECT, (void*) EVENT_TAG },
  { "call", "", event_call, { TYPE_HASH }, TYPE_UNDEF, NULL },
  { "" }
};

static scLibObjectDef_t event_def = {
  "Event", "",
  SC_Common_Constructor, { TYPE_UNDEF },
  0, // An event should never be destroyed
  NULL,
  event_methods
};

static scLibObjectDef_t tag_def = {
  "Tag", "",
  node_constructor, { TYPE_UNDEF },
  node_destructor,
  NULL,
  (void*) SC_EVENT_NODE_TYPE_NODE,
};

static scLibObjectDef_t hook_def = {
  "Hook", "",
  0, { TYPE_UNDEF },
  node_destructor,
  NULL,
  (void*) SC_EVENT_NODE_TYPE_HOOK,
};

static scLibObjectMethod_t loc_methods[] = {
  { "inside", "", event_loc_method, { TYPE_UNDEF }, TYPE_OBJECT, (void*) EVENT_INSIDE },
  { "after", "", event_loc_method, { TYPE_UNDEF }, TYPE_OBJECT, (void*) EVENT_AFTER },
  { "before", "", event_loc_method, { TYPE_UNDEF }, TYPE_OBJECT, (void*) EVENT_BEFORE },
  { "add", "", add, { TYPE_OBJECT, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "delete", "", delete, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

static scLibObjectDef_t loc_def = {
  "Location", "",
  0, { TYPE_UNDEF },
  0,
  NULL,
  loc_methods
};

void SC_Event_Init( void )
{
  event_class = SC_AddObjectType("common", &event_def);
  hook_class = SC_AddObjectType("common", &hook_def);
  tag_class = SC_AddObjectType("common", &tag_def);
  loc_class = SC_AddObjectType("common", &loc_def);
}

