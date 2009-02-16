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

static PyMethodDef P_cvar_methods[] =
{
 {"get", get, METH_VARARGS,  "get cvar string"},
 {"set", set, METH_VARARGS,  "set cvar string"},
 {NULL}      /* sentinel */
};

void P_Cvar_Init(void)
{
  Py_InitModule("cvar", P_cvar_methods);
}
