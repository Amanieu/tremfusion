/*
===========================================================================
Copyright (C) 2008 John Black

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "p_local.h"

static PyObject *callback_dict;

static void call_callbacks(const char *eventname)
{
  PyObject *callback_list;
  PyObject *callback, *args, *res;
  int i, j;
  Com_Printf("Calling callbacks for: %s\n", eventname);
  callback_list = PyDict_GetItemString(callback_dict, eventname);
  if(!callback_list || !PyList_CheckExact(callback_list)) {
    Com_Printf("Bad callback_list\n");
    return;
  }
  j = PyList_GET_SIZE(callback_list);
  args = PyTuple_New(0);
  for(i=0; i<j; i++) {
    callback = PyList_GET_ITEM(callback_list, i);
    if (!PyCallable_Check(callback)) {
      Com_Printf("Uncallable callback :x\n");
      goto error;
    }
    res = PyObject_CallObject(callback, args);
    if(!res) {
      /* TODO: Make our own exception type so that callbacks can 
       * remove themselves without printing a traceback */
      PyErr_Print();
      goto error;
    }
    Py_XDECREF(res);
    continue;
    error:
    /* Set the bad callback to none so we can remove it later */
    Py_INCREF(Py_None);
    PyList_SetItem(callback_list, i, Py_None);
  }
  Py_XDECREF(args);
}

void P_Event_Newmap(void)
{
  call_callbacks("new_map");
}

void P_Event_Maprestart(void)
{
  call_callbacks("map_restart");
}

static PyObject *connect(PyObject *self, PyObject *args)
{
  char *name;
  PyObject *callback, *callback_list;
  if(!PyArg_ParseTuple(args, "sO", &name, &callback))
  {
    return NULL;
  }
  if (!PyCallable_Check(callback))
  {
    PyErr_SetString(PyExc_StandardError,
                    "callback must be callable");
    return NULL;
  }
  callback_list = PyDict_GetItemString(callback_dict, name);
  if(!callback_list) {
    // TODO: Make our own Error class
    PyErr_SetString(PyExc_StandardError,
                    "name does not exist");
    return NULL;
  }
  PyList_Append(callback_list, callback);
  Py_RETURN_NONE;
}

void P_test_event_f(void)
{
  call_callbacks(Cmd_Argv(1));
}

static PyMethodDef P_event_methods[] =
{
 {"connect", connect, METH_VARARGS,  "connect callback to event"},
 {NULL}      /* sentinel */
};

void P_Event_Init(void)
{
  callback_dict = PyDict_New();
  
  PyDict_SetItemString(callback_dict, "new_map", PyList_New(0));
  PyDict_SetItemString(callback_dict, "map_restart", PyList_New(0));
  
#ifndef NDEBUG
  Cmd_AddCommand("testevent", P_test_event_f);
#endif
  
  Py_InitModule("tremfusion.event", P_event_methods);
}