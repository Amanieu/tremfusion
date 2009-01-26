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

static char commandbuf[ 32000 ];
static PyObject *tremfusion_module;

char *Find_File( const char *filename )
{
        char  *basepath, *gamedir, *homepath, *filepath;
        FILE *fp;
        cvar_t *fs_debug;

        homepath = Cvar_VariableString( "fs_homepath" );
        basepath = Cvar_VariableString( "fs_basepath" );
        gamedir = Cvar_VariableString( "fs_game" );
        fs_debug = Cvar_Get( "fs_debug", "0", 0 );

        filepath = FS_BuildOSPath( homepath, gamedir, filename );
        if ( fs_debug->integer )
                Com_Printf("Trying to open: %s\n",filepath );
        fp = fopen(filepath, "r+");
        if (fp)
        {
                fclose(fp);
                return filepath;
        }
        filepath = FS_BuildOSPath( homepath, "base", filename );
        if ( fs_debug->integer )
                Com_Printf("Trying to open: %s\n",filepath );
        fp = fopen(filepath, "r+");
        if (fp)
        {
                fclose(fp);
                return filepath;
        }
        filepath = FS_BuildOSPath( basepath, gamedir, filename );
        if ( fs_debug->integer )
                Com_Printf("Trying to open: %s\n",filepath );
        fp = fopen (filepath, "r+");
        if (fp)
        {
                fclose(fp);
                return filepath;
        }
        // Must not have found it
        return NULL;
}

PyObject* P_Print(PyObject* self, PyObject* pArgs)
{
        char* s = NULL;
        if (!PyArg_ParseTuple(pArgs, "s", &s))
                return NULL;

        Com_Printf("%s", s);

        Py_RETURN_NONE;
}
void flush( char *outputbuf )
{
        Q_strcat(commandbuf, sizeof(commandbuf), outputbuf );
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

static PyMethodDef tremfusion_methods[] =
{
 {"p", P_Print, METH_VARARGS, "Prints using Com_Printf"},
 {"command", command, METH_VARARGS,  "call command"},
 {NULL}
};

char *stdout_catcher = "import tremfusion\n"
"import sys\n"
"class StdoutCatcher:\n"
"\tdef write(self, str):\n"
"\t\ttremfusion.p(str)\n"
"class StderrCatcher:\n"
"\tdef write(self, str):\n"
"\t\ttremfusion.p(str)\n"
"sys.stdout = StdoutCatcher()\n"
"sys.stderr = StderrCatcher()\n";

void Cmd_CompletePyName( char *args, int argNum ) {
        if( argNum == 2 ) {
                Field_CompleteFilename( "python", "py", qfalse );
        }
}

void P_script_f( void )
{
        int i;
        FILE *fp;
        char *filepath;
        PyObject *args;

        filepath = Find_File(va("python/%s", Cmd_Argv(1)));
        if(!filepath)
        {
                Com_Printf(S_COLOR_RED "ERROR: couldn't load script file %s\n",
                           Cmd_Argv(1));
                return;
        }
        args = PyList_New(Cmd_Argc() - 1);

        for (i = 0; i < Cmd_Argc() - 1; i++)
        {
                PyList_SET_ITEM(args, i,
                                PyString_FromString(Cmd_Argv( i + 1 )));
        }
        if(PyObject_SetAttrString(tremfusion_module, "args", args))
                PyErr_Print();

        fp = fopen(filepath, "r+");
        PyRun_SimpleFile(fp, filepath);
        fclose(fp);
}

void P_Init(void)
{
        Com_Printf("----- P_Init -----\n");
        /* Initialize python */
        Py_Initialize();
        /* Make python threads work at all */
        PyEval_InitThreads( );

        // Create a module for tremfusion stuff
        tremfusion_module = Py_InitModule("tremfusion", tremfusion_methods);

        PyRun_SimpleString(stdout_catcher);

        PyRun_SimpleString("import os, sys, time"); // Import some basic modules
        /* Make sys.version prettier and print it */
        PyRun_SimpleString("print \"Python version: \" + "
                           "sys.version.replace(\"\\n\", \"\\nBuilt with: \")");

        P_Cvar_Init();
        P_Event_Init();
        P_Configstring_Init();
        
        Cmd_AddCommand("script", P_script_f);
        Cmd_SetCommandCompletionFunc("script", Cmd_CompletePyName);
        Com_Printf("----- finished P_Init -----\n");
}

void P_Shutdown(void)
{
        Com_Printf("Shutdown python interpreter\n");
        Py_Finalize();
        Com_Printf("Shutdown of python interpreter complete\n");
}
