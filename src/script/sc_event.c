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

static int event_get(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  out[0].type = TYPE_INTEGER;
  out[0].data.integer = closure.n;
  return 0;
}

static int event_constructor( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scEvent_t *event;

  SC_Common_Constructor(in, out, closure);

  event = SC_Event_New();

  out[0].data.object->data.data.userdata = event;

  return 0;
}

static int event_add(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scEvent_t *event;
  scEventNode_t *node;
  scEventNode_t *parent;
  scDataTypeFunction_t *function;
  const char *tag;
  const char *dest;
  int type = closure.n;
  int pos, ret;

  event = in[0].data.object->data.data.userdata;
  tag = SC_StringToChar(in[1].data.string);

  node = SC_Event_NewNode(tag);
  node->type = type;

  pos = in[2].data.integer;
  dest = SC_StringToChar(in[3].data.string);

  if(type == SC_EVENT_NODE_TYPE_HOOK)
  {
    function = in[4].data.function;
    function->argument[0] = TYPE_OBJECT;
    function->argument[1] = TYPE_HASH;
    function->argument[2] = TYPE_UNDEF;
    function->return_type = TYPE_UNDEF;
    SC_FunctionGCInc(function);
    node->hook = function;
  }

  parent = SC_Event_Find(event, dest);
  if(!parent)
	  SC_EXEC_ERROR(va("can't find hook: %s", dest));

  switch(pos)
  {
    case EVENT_INSIDE:
      ret = SC_Event_AddNode(parent, parent->last, node, out);
      break;
    case EVENT_BEFORE:
      ret = SC_Event_AddNode(parent->parent, parent->previous, node, out);
      break;
    case EVENT_AFTER:
      ret = SC_Event_AddNode(parent->parent, parent, node, out);
      break;
    default:
      SC_EXEC_ERROR(va("can't add hook: %d: unknow location", pos));
  }

  if(ret != 0)
    return ret;

  out->type = TYPE_UNDEF;
  return 0;
}

static int event_delete(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scEvent_t *event;
  const char *tag;
  scEventNode_t *node;

  event = in[0].data.object->data.data.userdata;
  tag = SC_StringToChar(in[1].data.string);

  node = SC_Event_Find(event, tag);
  if(!node)
    SC_EXEC_ERROR(va("Can't delete hook: Can't find hook %s", tag));

  if(SC_Event_DeleteNode(node, out) != 0)
    return -1;

  out->type = TYPE_UNDEF;
  return 0;
}

static int event_call( scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  int ret = SC_Event_Call(in[0].data.object, in[1].data.hash, out);
  if(ret < 0)
    return -1;

  out[0].type = TYPE_BOOLEAN;
  out[0].data.boolean = ret;
  return 0;
}

static int event_skip(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  if(SC_Event_Skip(event, SC_StringToChar(in[1].data.string), out) != 0)
    return -1;

  out[0].type = TYPE_UNDEF;

  return 0;
}

static int event_dump(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  SC_Event_Dump(event);

  out->type = TYPE_UNDEF;
  return 0;
}

static int event_addmaingroups(scDataTypeValue_t *in, scDataTypeValue_t *out, scClosure_t closure)
{
  scEvent_t *event = in[0].data.object->data.data.userdata;

  if(SC_Event_AddMainGroups(event, out) != 0)
    return -1;

  out->type = TYPE_UNDEF;
  return 0;
}

static scLibObjectMember_t event_members[] = {
  { "before", "", TYPE_INTEGER, 0, event_get, { .n = EVENT_BEFORE } },
  { "inside", "", TYPE_INTEGER, 0, event_get, { .n = EVENT_INSIDE } },
  { "after", "", TYPE_INTEGER, 0, event_get, { .n = EVENT_AFTER } },
};

static scLibObjectMethod_t event_methods[] = {
  { "addGroup", "", event_add, { TYPE_STRING, TYPE_INTEGER, TYPE_STRING, TYPE_UNDEF },
    TYPE_UNDEF, { .n = SC_EVENT_NODE_TYPE_GROUP } },
  { "addMainGroups", "", event_addmaingroups, { TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "addHook", "", event_add,
    { TYPE_STRING, TYPE_INTEGER, TYPE_STRING, TYPE_FUNCTION, TYPE_UNDEF },
    TYPE_UNDEF, { .n = SC_EVENT_NODE_TYPE_HOOK } },
  { "delete", "", event_delete, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "call", "", event_call, { TYPE_HASH, TYPE_UNDEF }, TYPE_BOOLEAN, { .v = NULL } },
  { "skip", "", event_skip, { TYPE_STRING, TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "dump", "", event_dump, { TYPE_UNDEF }, TYPE_UNDEF, { .v = NULL } },
  { "" }
};

static scLibObjectDef_t event_def = {
  "Event", "",
  event_constructor, { TYPE_UNDEF },
  0, // An event should never be destroyed
  event_members,
  event_methods,
  NULL,
  { .v = NULL }
};

/*
======================================================================

Global functions

======================================================================
*/

scObject_t *SC_Event_NewObject(void)
{
  scObject_t *self = SC_ObjectNew(event_class);
  self->data.type = TYPE_USERDATA;
  self->data.data.userdata = SC_Event_New();

  return self;
}

scEvent_t *SC_Event_New(void)
{
  scEvent_t *event;

  event = BG_Alloc(sizeof(scEvent_t));
  event->skip = 0;
  event->root = SC_Event_NewNode("all");
  event->root->type = SC_EVENT_NODE_TYPE_GROUP;
  event->root->parent = event->root;
  event->current_node = NULL;

  return event;
}

int event_call_rec(scObject_t *event, scEventNode_t *node, scDataTypeHash_t *params, scDataTypeValue_t *out)
{
  scEventNode_t *i;
  scDataTypeValue_t in[3];
  scEvent_t *data;
  qboolean ret = qtrue;
  int tret;

  data = event->data.data.userdata;

  in[0].type = TYPE_OBJECT;
  in[0].data.object = event;
  in[1].type = TYPE_HASH;
  in[1].data.hash = params;
  in[2].type = TYPE_UNDEF;

  if(node == NULL)
    return 0;

  if(node->skip)
  {
    node->skip = qfalse;
    return 0;
  }

  data->current_node = node;

  if(node->type == SC_EVENT_NODE_TYPE_GROUP)
  {
    for(i = node->first; i != NULL; i = i->next)
    {
      tret = event_call_rec(event, i, params, out);
      if(tret < 0)
        return -1;

      ret &= tret;

      if(data->skip)
      {
        if(node->skip)
        {
          node->skip = qfalse;
          data->skip = qfalse;
        }
        break;
      }
    }
  }
  else
  {
    if(SC_RunFunction(node->hook, in, out) != 0)
      SC_EXEC_ERROR(va("error running event function: %s", SC_StringToChar(out->data.string)));
    if(out[0].type == TYPE_BOOLEAN)
      ret &= out[0].data.boolean;
  }

  return ret;
}

int SC_Event_Call(scObject_t *event, scDataTypeHash_t *params, scDataTypeValue_t *out)
{
  scEvent_t *data = event->data.data.userdata;
  int ret;

  ret = event_call_rec(event, data->root, params, out);
  if(ret < 0)
    return -1;

  data->current_node = NULL;

  return ret;
}

int SC_Event_AddMainGroups(scEvent_t *event, scDataTypeValue_t *out)
{
  scEventNode_t *node, *lnode;

  node = SC_Event_NewNode("check");
  node->type = SC_EVENT_NODE_TYPE_GROUP;
  node->parent = event->root;
  if(SC_Event_AddNode(node->parent, node->parent->last, node, out) != 0)
    return -1;
  lnode = node;

  node = SC_Event_NewNode("system");
  node->type = SC_EVENT_NODE_TYPE_GROUP;
  node->parent = lnode;
  if(SC_Event_AddNode(node->parent, node->parent->last, node, out) != 0)
    return -1;

  node = SC_Event_NewNode("game");
  node->type = SC_EVENT_NODE_TYPE_GROUP;
  node->parent = lnode;
  if(SC_Event_AddNode(node->parent, node->parent->last, node, out) != 0)
    return -1;

  node = SC_Event_NewNode("action");
  node->type = SC_EVENT_NODE_TYPE_GROUP;
  node->parent = event->root;
  if(SC_Event_AddNode(node->parent, lnode, node, out) != 0)
    return -1;
  lnode = node;

  node = SC_Event_NewNode("init");
  node->type = SC_EVENT_NODE_TYPE_GROUP;
  node->parent = event->root;
  if(SC_Event_AddNode(node->parent, lnode->previous, node, out) != 0)
    return -1;

  return 0;
}

int SC_Event_AddNode(scEventNode_t *parent, scEventNode_t *previous, scEventNode_t *new, scDataTypeValue_t *out)
{
  scEventNode_t *node = parent->first;

  // error if tag allready used
  while(node)
  {
    if(strcmp(node->tag, new->tag) == 0)
      SC_EXEC_ERROR(va("Can't add hook: %s tag allready exist", new->tag));
    node = node->next;
  }
 
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

  return 0;
}

int SC_Event_DeleteNode(scEventNode_t *node, scDataTypeValue_t *out)
{
  scEventNode_t *parent = node->parent;

  while(node->first)
    if(SC_Event_DeleteNode(node->first, out) != 0)
      return -1;

  if(node->previous)
    node->previous->next = node->next;
  else
    parent->first = node->next;

  if(node->next)
    node->next->previous = node->previous;
  else
    parent->last = node->previous;

  BG_Free(node);

  return 0;
}

scEventNode_t *SC_Event_NewCHook(const char *tag, scCRef_t cfunc)
{
  scDataTypeFunction_t *function = SC_FunctionNew();

  function->argument[0] = TYPE_OBJECT;
  function->argument[1] = TYPE_HASH;
  function->argument[2] = TYPE_UNDEF;
  function->return_type = TYPE_UNDEF;
  function->langage = LANGAGE_C;
  function->data.ref = cfunc;
  function->closure.v = NULL;

  return SC_Event_NewHook(tag, function);
}

scEventNode_t *SC_Event_NewHook(const char *tag, scDataTypeFunction_t *function)
{
  scEventNode_t *node = SC_Event_NewNode(tag);
  node->type = SC_EVENT_NODE_TYPE_HOOK;
  SC_FunctionGCInc(function);
  node->hook = function;
  return node;
}

scEventNode_t *SC_Event_NewGroup(const char *tag)
{
  scEventNode_t *node = SC_Event_NewNode(tag);
  node->type = SC_EVENT_NODE_TYPE_GROUP;
  return node;
}

scEventNode_t *SC_Event_NewNode(const char *tag)
{
  scEventNode_t *node = BG_Alloc(sizeof(scEventNode_t));
  memset(node, 0x00, sizeof(scEventNode_t));

  strcpy(node->tag, tag);

  return node;
}

scEventNode_t *SC_Event_Find(scEvent_t *event, const char *tag)
{
  scEventNode_t *node, *i;
  char tmp[MAX_NAMESPACE_LENGTH];
  char *idx, *lidx;

  node = event->root;

  Q_strncpyz(tmp, tag, sizeof(tmp));
  lidx = tmp;

  // Skip any 'all' tag founded
  if(Q_strncmp(lidx, "all", 3) == 0)
  {
    idx = strchr(lidx, '.');
    if(!idx)
      return node;
    lidx = idx + 1;
  }

  while(lidx)
  {
    idx = strchr(lidx, '.');
    if(idx)
      *idx = '\0';
    if(node->type == SC_EVENT_NODE_TYPE_GROUP)
    {
      i = node->first;
      node = NULL;
      while(i != NULL)
      {
        if(strcmp(i->tag, lidx) == 0)
        {
          node = i;
          break;
        }
        i = i->next;
      }
      if(!node)
        return NULL;
    }
    else
      return NULL;

    if(idx)
      lidx = idx +1;
    else
      lidx = NULL;
  }
  
  return node;
}

int SC_Event_Skip(scEvent_t *event, const char *tag, scDataTypeValue_t *out)
{
  scEventNode_t *node, *fnode;

  node = SC_Event_Find(event, tag);
  if(!node)
    SC_EXEC_ERROR(va("can't find hook: %s", tag));

  node->skip = qtrue;

  fnode = event->current_node;
  while(fnode != fnode->parent)
  {
    if(fnode == node)
    {
      event->skip = qtrue;
      break;
    }

    fnode = fnode->parent;
  }
  if(fnode == node)
    event->skip = qtrue;

  return 0;
}

void SC_Event_Init(void)
{
  scDataTypeValue_t value;

  event_class = SC_AddClass("script", &event_def);

  value.type = TYPE_NAMESPACE;
  value.data.namespace = SC_HashNew();
  SC_NamespaceSet("event", &value);
}

static void dump_rec(scEventNode_t *node, char *name)
{
  char n[256];
  int len;
  scEventNode_t *tmpn;

  strcpy(n, name);
  strcat(n, node->tag);

  len = strlen(n);

  Com_Printf("%s\n", n);
  if(node->type == SC_EVENT_NODE_TYPE_GROUP)
  {
    tmpn = node->first;
    strcat(n, ".");
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

