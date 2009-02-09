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

#define P_OUTPUT_LENGTH (10000 - 16)

static PyObject *commands_dict;
static char commandbuf[ 32000 ];

void flush( char *outputbuf )
{
        Q_strcat(commandbuf, sizeof(commandbuf), outputbuf );
}

PyObject* command_register(PyObject* self, PyObject* pArgs) {
        char *command;
        PyObject *function;
        if (!PyArg_ParseTuple(pArgs, "sO", &command, &function))
                return NULL;
        if(!PyCallable_Check(function)) {
                PyErr_SetString(PyExc_StandardError,
                              "callback must be callable");
                return NULL;
        }
        PyDict_SetItemString(commands_dict, command, function);
        Cmd_AddCommand(command, NULL);
        Py_RETURN_NONE;
}

PyObject* command(PyObject* self, PyObject* pArgs)
{
        char *command;
        char output[P_OUTPUT_LENGTH];

        if (!PyArg_ParseTuple(pArgs, "s", &command))
                return NULL;
        commandbuf[ 0 ] = '\0';

        /* Redirect output to return to python */
        Com_BeginRedirect(output, P_OUTPUT_LENGTH, flush);

        Cbuf_ExecuteText( EXEC_NOW, command);
        Com_EndRedirect();
        Com_Printf("%s", commandbuf);
        return Py_BuildValue("s", commandbuf);
}

qboolean P_Call_Command( void )
{
        PyObject *args = NULL, *ret, *command_function;
        
        if(!p_initilized) return qfalse;
        command_function = PyDict_GetItemString(commands_dict, Cmd_Argv(0));
        if(!command_function) return qfalse;
        if(!PyCallable_Check(command_function)) goto error;
        
        args = P_ArgTuple();
        ret = PyObject_Call(command_function, args, NULL);
        if(!ret) goto error;
        Py_DECREF(args);
        return qtrue;
error:
        Py_XDECREF(args);
        PyDict_DelItemString(commands_dict, Cmd_Argv(0));
        return qfalse;
}

static PyMethodDef tremfusion_command_methods[] =
{
 {"call", command, METH_VARARGS,  "call command"},
 {"register", command_register, METH_VARARGS,  "register command"},
 {NULL}
};

void P_Command_Init( void )
{
        commands_dict = PyDict_New();
        
        Py_InitModule("tremfusion.command", tremfusion_command_methods);
}

void P_Command_Shutdown( void )
{
        Py_CLEAR(commands_dict);
}