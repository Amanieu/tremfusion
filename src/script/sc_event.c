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

/*
======================================================================

Event

======================================================================
*/

scClass_t *event_class;

typedef enum
{
  EVENT_INSIDE,
  EVENT_BEFORE,
  EVENT_AFTER,
  EVENT_TAG,
} loc_closure_t;

static void event_constructor( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event;

  SC_Common_Constructor(in, out, closure);

  event = BG_Alloc(sizeof(scEventNode_t));
  strcpy(event->skip_tag, "");
  event->root = SC_Event_NewNode("all");
  event->root->type = SC_EVENT_NODE_TYPE_NODE;
  out[0].data.object->data.data.userdata = event;
}

static void event_call( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  SC_Event_Call(event, in[1].data.hash);

  out[0].type = TYPE_UNDEF;
}

static void event_skip(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  SC_Event_Skip(event, SC_StringToChar(in[1].data.string));

  out[0].type = TYPE_UNDEF;
}

static void event_loc_method( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  int type = (int)closure;
  scDataTypeValue_t value;
  scEvent_t *event;
  scEventNode_t *node;
  scObject_t *object;
  scDataTypeHash_t *hash;

  object = SC_ObjectNew(loc_class);

  object->data.type = TYPE_HASH;
  object->data.data.hash = SC_HashNew();

  if(type == EVENT_TAG)
  {
    event = (scEvent_t*) in[0].data.object->data.data.userdata;
    node = SC_Event_FindChild(event->root, SC_StringToChar(in[1].data.string));
  }
  else
  {
    hash = in[0].data.object->data.data.hash;
    SC_HashGet(hash, "node", &value);
    node = value.data.userdata;
  }

  value.type = TYPE_USERDATA;
  value.data.userdata = node;
  SC_HashSet(object->data.data.hash, "node", &value);

  value.type = TYPE_INTEGER;
  value.data.integer = type;
  SC_HashSet(object->data.data.hash, "type", &value);

  out[0].type = TYPE_OBJECT;
  out[0].data.object = object;
}

static void event_dump(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  SC_Event_Dump(event);

  out->type = TYPE_UNDEF;
}

static scLibObjectMethod_t event_methods[] = {
  { "tag", "", event_loc_method, { TYPE_STRING, TYPE_UNDEF }, TYPE_OBJECT, (void*) EVENT_TAG },
  { "call", "", event_call, { TYPE_HASH, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "skip", "", event_skip, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "dump", "", event_dump, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

static scLibObjectDef_t event_def = {
  "Event", "",
  event_constructor, { TYPE_UNDEF },
  0, // An event should never be destroyed
  NULL,
  event_methods,
  NULL,
  NULL
};

/*
======================================================================

Location

======================================================================
*/

scClass_t *loc_class;

static void delete(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *args = in[0].data.object->data.data.userdata;
  scDataTypeValue_t value;
  scEventNode_t *node;
  int type;

  // get parent node from location object
  SC_HashGet(args, "node", &value);
  node = (scEventNode_t*) value.data.userdata;

  // get location type (after, before, inside or simple tag : implicit inside)
  SC_HashGet(args, "type", &value);
  type = value.data.integer;

  switch(type)
  {
    case EVENT_TAG:
      SC_Event_DeleteNode(node);
      break;
    case EVENT_BEFORE:
    case EVENT_INSIDE:
    case EVENT_AFTER:
      // Error
      break;
  }
}

static void add( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scDataTypeHash_t *args;
  scDataTypeValue_t value;
  scEventNode_t *parent;
  scEventNode_t *node;
  int type;

  args = in[0].data.object->data.data.userdata;
  node = in[1].data.object->data.data.userdata;

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
      SC_Event_AddNode(parent, parent->last, node);
      break;
    case EVENT_BEFORE:
      SC_Event_AddNode(parent->parent, parent->previous, node);
      break;
    case EVENT_AFTER:
      SC_Event_AddNode(parent->parent, parent, node);
      break;
  }
  node->parent = parent;
}

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
  loc_methods,
  NULL,
  NULL
};

/*
======================================================================

Node

======================================================================
*/

scClass_t *hook_class;
scClass_t *tag_class;

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
  {
    node->hook = in[2].data.function;
    node->hook->argument[0] = TYPE_HASH;
    node->hook->argument[1] = TYPE_UNDEF;
    node->hook->return_type = TYPE_UNDEF;
  }

  self->data.type = TYPE_USERDATA;
  self->data.data.userdata = node;
}

static void node_destructor( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
}

static scLibObjectDef_t tag_def = {
  "Tag", "",
  node_constructor, { TYPE_STRING, TYPE_UNDEF },
  node_destructor,
  NULL,
  NULL,
  NULL,
  (void*) SC_EVENT_NODE_TYPE_NODE,
};

static scLibObjectDef_t hook_def = {
  "Hook", "",
  node_constructor, { TYPE_STRING, TYPE_FUNCTION, TYPE_UNDEF },
  node_destructor,
  NULL,
  NULL,
  NULL,
  (void*) SC_EVENT_NODE_TYPE_HOOK,
};

/*
======================================================================

Global functions

======================================================================
*/

void event_call_rec(scEvent_t *event, scEventNode_t *node, scDataTypeHash_t *params)
{
  scEventNode_t *i;
  scDataTypeValue_t args[2];
  scDataTypeValue_t ret;

  args[0].type = TYPE_HASH;
  args[0].data.hash = params;
  args[1].type = TYPE_UNDEF;

  if(node == NULL)
    return;

  if(node->type == SC_EVENT_NODE_TYPE_NODE)
  {
    for(i = node->first; i != NULL; i = i->next)
    {
      event_call_rec(event, i, params);

      if(event->skip_tag[0] != '\0')
      {
        if(strcmp(node->tag, event->skip_tag) == 0)
          strcpy(event->skip_tag, "");
        break;
      }
    }
  }
  else
  {
    SC_RunFunction(node->hook, args, &ret);
  }
}

void SC_Event_Call(scEvent_t *event, scDataTypeHash_t *params)
{
  event_call_rec(event, event->root, params);
}

void SC_Event_AddNode(scEventNode_t *parent, scEventNode_t *previous, scEventNode_t *new)
{
  if(previous)
  {
    new->previous = previous;
    new->next = previous->next;
    previous->next = new;
  }
  else
  {
    new->next = parent->first;
    parent->first = new;
  }

  if(new->next)
    new->next->previous = new;
  else
    parent->last = new;

  new->parent = parent;
}

void SC_Event_DeleteNode(scEventNode_t *node)
{
  scEventNode_t *parent = node->parent;

  while(node->first)
    SC_Event_DeleteNode(node->first);

  if(node->previous)
    node->previous->next = node->next;
  else
    parent->first = node->next;

  if(node->next)
    node->next->previous = node->previous;
  else
    parent->last = node->previous;

  BG_Free(node);
}

scEventNode_t *SC_Event_NewNode(const char *tag)
{
  scEventNode_t *node = BG_Alloc(sizeof(scEventNode_t));
  memset(node, 0x00, sizeof(scEventNode_t));

  strcpy(node->tag, tag);

  return node;
}

scEventNode_t *SC_Event_NewTemporaryNode(const char *tag)
{
  scEventNode_t *node = SC_Event_NewNode(tag);
  node->ttl = 1;

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
    if(node->type == SC_EVENT_NODE_TYPE_NODE)
    {
      for(i = node->first; i != NULL; i = i->next)
      {
        found = SC_Event_FindChild(i, tag);
        if(found)
          return found;
      }
    }
  }

  return NULL;
}

void SC_Event_Skip(scEvent_t *event, const char *tag)
{
  strcpy(event->skip_tag, tag);
}

void SC_Event_Init(void)
{
  event_class = SC_AddClass("common", &event_def);
  hook_class = SC_AddClass("common", &hook_def);
  tag_class = SC_AddClass("common", &tag_def);
  loc_class = SC_AddClass("common", &loc_def);
}

static void dump_rec(scEventNode_t *node, char *name)
{
  char n[256];
  int len;
  scEventNode_t *tmpn;

  strcpy(n, name);
  strcat(n, ".");
  strcat(n, node->tag);

  len = strlen(n);

  Com_Printf("%s\n", n);
  if(node->type == SC_EVENT_NODE_TYPE_NODE)
  {
    tmpn = node->first;
    while(tmpn)
    {
      dump_rec(tmpn, n);
      tmpn = tmpn->next;
    }
    n[len] = '\0';
  }
}

void SC_Event_Dump(scEvent_t *event)
{
  Com_Printf("--------- SC_Event_Dump ----------\n");
  dump_rec(event->root, "");
  Com_Printf("----------------------------------\n");
}

