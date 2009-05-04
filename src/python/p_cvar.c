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
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "p_local.h"

PyObject *update_functions_dict;

static PyObject *get(PyObject *self, PyObject *args)
{
        //char cvar_
        const char *s;
        cvar_t  *var;
        if (!PyArg_ParseTuple(args, "s", &s) )
                return NULL;
        var = Cvar_Get(s, "", 0);

        if (var)
                return Py_BuildValue("s", var->string);
        else
                Py_RETURN_NONE;
}
// Get rid of compiler warning
cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force );

static PyObject *set(PyObject *self, PyObject *args)
{
        //char cvar_
        const char *s;
        PyObject *value, *valuestring;
        cvar_t  *var;
        if (!PyArg_ParseTuple(args, "sO", &s, &value) )
                return NULL;
        valuestring = PyObject_Str( value );
        if (!valuestring)
                return NULL;

        var = Cvar_Set2(s, PyString_AsString(valuestring), qfalse);

        if (var)
                return Py_BuildValue("s", var->string);
        else
                return Py_BuildValue("s", "");
}

void P_Cvar_UpdateFunctionWrapper(char *var_name)
{
        PyObject *function, *args, *ret;
        function = PyDict_GetItemString(update_functions_dict, var_name);
        if(!function || !PyCallable_Check(function)) {
                Cvar_SetUpdateFunction(var_name, NULL);
                return;
        }
        args = Py_BuildValue("(s)", var_name);
        ret = PyObject_Call(function, args, NULL);
        Py_XDECREF(args);
        if(!ret) {
                PyErr_Print();
                Cvar_SetUpdateFunction(var_name, NULL);
                return;
        }
        Py_DECREF(ret);
}

static PyObject *P_Cvar_register(PyObject *self, PyObject *args)
{
        char *var_name;
        PyObject *update_function;
        
        if(!PyArg_ParseTuple(args, "sO", &var_name, &update_function)) {
                return NULL;
        } if (!PyCallable_Check(update_function)) {
                PyErr_SetString(PyExc_StandardError,
                                "update function must be callable");
                return NULL;
        }
        Cvar_SetUpdateFunction(var_name, &P_Cvar_UpdateFunctionWrapper);
        PyDict_SetItemString(update_functions_dict, var_name, update_function);
        Py_RETURN_NONE;
}

static PyMethodDef P_cvar_methods[] =
{
 {"get", get, METH_VARARGS,  "get cvar string"},
 {"set", set, METH_VARARGS,  "set cvar string"},
 {"register", P_Cvar_register, METH_VARARGS,  "set cvar update function"},
 {NULL}      /* sentinel */
};



void P_Cvar_Init(void)
{
  Py_InitModule("cvar", P_cvar_methods);
  update_functions_dict = PyDict_New();
}
