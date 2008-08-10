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
} loc_closure_t;

static void event_get(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  out[0].type = TYPE_INTEGER;
  out[0].data.integer = (int) closure;
}

static void event_constructor( scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event;

  SC_Common_Constructor(in, out, closure);

  event = SC_Event_New();

  out[0].data.object->data.data.userdata = event;
}

static void event_add(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event;
  scEventNode_t *node;
  scEventNode_t *parent;
  scDataTypeFunction_t *function;
  const char *tag;
  const char *dest;
  int type = (int) closure;
  int pos;

  event = in[0].data.object->data.data.userdata;
  tag = SC_StringToChar(in[1].data.string);

  node = SC_Event_NewNode(tag);
  node->type = type;

  pos = in[2].data.integer;
  dest = SC_StringToChar(in[3].data.string);

  if(type == SC_EVENT_NODE_TYPE_HOOK)
  {
    function = in[4].data.function;
    function->argument[0] = TYPE_HASH;
    function->argument[1] = TYPE_UNDEF;
    function->return_type = TYPE_UNDEF;
    node->hook = function;
  }

  parent = SC_Event_FindChild(event->root, dest);

  switch(pos)
  {
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

  out->type = TYPE_UNDEF;
}

static void event_delete(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event;
  const char *tag;
  scEventNode_t *node;

  event = in[0].data.object->data.data.userdata;
  tag = SC_StringToChar(in[1].data.string);

  node = SC_Event_FindChild(event->root, tag);
  SC_Event_DeleteNode(node);

  out->type = TYPE_UNDEF;
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

static void event_dump(scDataTypeValue_t *in, scDataTypeValue_t *out, void *closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  SC_Event_Dump(event);

  out->type = TYPE_UNDEF;
}

static scLibObjectMember_t event_members[] = {
  { "before", "", TYPE_INTEGER, 0, event_get, (void*) EVENT_BEFORE },
  { "inside", "", TYPE_INTEGER, 0, event_get, (void*) EVENT_INSIDE },
  { "after", "", TYPE_INTEGER, 0, event_get, (void*) EVENT_AFTER },
};

static scLibObjectMethod_t event_methods[] = {
  { "addTag", "", event_add, { TYPE_STRING, TYPE_INTEGER, TYPE_STRING, TYPE_UNDEF },
    TYPE_UNDEF, (void*) SC_EVENT_NODE_TYPE_NODE },
  { "addHook", "", event_add,
    { TYPE_STRING, TYPE_INTEGER, TYPE_STRING, TYPE_FUNCTION, TYPE_UNDEF },
    TYPE_UNDEF, (void*) SC_EVENT_NODE_TYPE_HOOK },
  { "delete", "", event_delete, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "call", "", event_call, { TYPE_HASH, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "skip", "", event_skip, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "dump", "", event_dump, { TYPE_UNDEF }, TYPE_UNDEF, NULL },
  { "" }
};

static scLibObjectDef_t event_def = {
  "Event", "",
  event_constructor, { TYPE_UNDEF },
  0, // An event should never be destroyed
  event_members,
  event_methods,
  NULL,
  NULL
};

/*
======================================================================

Global functions

======================================================================
*/

scEvent_t *SC_Event_New(void)
{
  scEvent_t *event;

  event = BG_Alloc(sizeof(scEvent_t));
  event->skip = 0;
  event->root = SC_Event_NewNode("all");
  event->root->type = SC_EVENT_NODE_TYPE_NODE;
  event->root->parent = event->root;
  event->current_node = NULL;

  return event;
}

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

  if(node->skip)
  {
    node->skip = qfalse;
    return;
  }

  event->current_node = node;

  if(node->type == SC_EVENT_NODE_TYPE_NODE)
  {
    for(i = node->first; i != NULL; i = i->next)
    {
      event_call_rec(event, i, params);

      if(event->skip)
      {
        if(node->skip)
        {
          node->skip = qfalse;
          event->skip = qfalse;
        }
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
  event->current_node = NULL;
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
  scEventNode_t *node;

  node = event->current_node;
  while(node)
  {
    if(strcmp(node->tag, tag) == 0)
    {
      event->skip = qtrue;
      break;
    }

    node = node->parent;
  }

  node = SC_Event_FindChild(event->root, tag);
  if(node)
    node->skip = qtrue;
}

void SC_Event_Init(void)
{
  event_class = SC_AddClass("common", &event_def);
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

