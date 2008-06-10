/*
===========================================================================
Copyright (C) 2008 John Black

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

#include "python_local.h"

qboolean Python_Initialized;

// Module definitions
PyMethodDef server_methods[] = {
 {"printer",    FixedPrint,          METH_VARARGS, "Logs stdout"},
// {"addcommand", py_ServerCommandAdd, METH_VARARGS,  "set command callback for C"},
 {"command",    PY_ExecuteText,    METH_VARARGS,  "call command"},
// {"addbinding", PY_BindingAdd,    METH_VARARGS,  "set binding callback for C"},
 {NULL, NULL, 0, NULL}
};

PyMethodDef cvar_methods[] = {
    {"get",     PY_CvarGet,     METH_VARARGS,  "get cvar string"},
    {"set",     PY_CvarSet,     METH_VARARGS,  "set cvar string"},
//    {"list",    PY_CvarList,     METH_VARARGS,  "list all cvars with optional name filter"},
    {NULL, NULL, 0, NULL}      /* sentinel */
};

PyObject *PY_CvarGet(PyObject *self, PyObject *args)
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
    return Py_BuildValue("s", "");
}

cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force ); // Get rid of compiler warning

PyObject *PY_CvarSet(PyObject *self, PyObject *args)
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
  
  var = Cvar_Set2(s,  PyString_AsString(valuestring), qfalse);
  
  if (var)
    return Py_BuildValue("s", var->string);
  else
    return Py_BuildValue("s", "");
}

// big ugly buffer
static char PY_ExecuteTextOutBuf[ 32000 ];

void PY_FlushRedirect( char *outputbuf ) {
  Q_strcat( PY_ExecuteTextOutBuf, sizeof( PY_ExecuteTextOutBuf ), outputbuf );
}

PyObject* PY_ExecuteText(PyObject* self, PyObject* pArgs)
{
  char* command = NULL;
#define PY_OUTPUTBUF_LENGTH (10000 - 16) 
  char    py_outputbuf[PY_OUTPUTBUF_LENGTH];
  if (!PyArg_ParseTuple(pArgs, "s", &command)) return NULL;
  PY_ExecuteTextOutBuf[ 0 ] = '\0';
  
  Com_BeginRedirect (py_outputbuf, PY_OUTPUTBUF_LENGTH, PY_FlushRedirect); // Begin redirecting
  
// Com_Printf("%s\n", command);
  
  
  Cbuf_ExecuteText( EXEC_NOW, command);
  Com_EndRedirect ();
  Com_Printf(PY_ExecuteTextOutBuf);
  return Py_BuildValue("s", PY_ExecuteTextOutBuf);
}

/* 
 * All the fixedPrint, StdoutCatcher, StderrCatcher stuff 
 * is to make python use Com_Printf instead of just printf
 * which makes python's output coloured and also fixes the
 * messed up text caused by python printing something while
 * you are editing a line of text in the console.
 */

PyObject* FixedPrint(PyObject* self, PyObject* pArgs)
{
  char* LogStr = NULL;
  if (!PyArg_ParseTuple(pArgs, "s", &LogStr)) return NULL;
 // 
  Com_Printf("%s", LogStr); 

  Py_INCREF(Py_None);
  return Py_None;
}

char *serverModuleInitstring =  "import server\n"
                                "import sys\n"
                                "class StdoutCatcher:\n"
                                " def write(self, str):\n"
                                "  server.printer(str)\n"
                                "class StderrCatcher:\n"
                                " def write(self, str):\n"
                                "  server.printer(str)\n"
                                "sys.stdout = StdoutCatcher()\n"
                                "sys.stderr = StderrCatcher()\n";

char *Find_File( const char *filename )
{
  char  *basepath;
  char  *gamedir;
  char  *homepath;
  char  *cdpath;
  char  *filepath;
  FILE *fp;
  cvar_t    *fs_debug;
  
  homepath = Cvar_VariableString( "fs_homepath" );
  basepath = Cvar_VariableString( "fs_basepath" );
  cdpath = Cvar_VariableString( "fs_cdpath" );
  gamedir = Cvar_VariableString( "fs_game" );
  fs_debug = Cvar_Get( "fs_debug", "0", 0 );
  
  filepath = FS_BuildOSPath( homepath, gamedir, filename );
  if ( fs_debug->integer )
    Com_Printf("Trying to open: %s\n",filepath );
  fp = fopen (filepath, "r+");
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
  filepath = FS_BuildOSPath( cdpath, gamedir, filename );
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

int PY_ExecScript( const char *filename)
{
  FILE  *fp;
  char  *filepath;
  char    pythonfileexpanded[MAX_QPATH];
  Com_sprintf (pythonfileexpanded, sizeof(pythonfileexpanded), "python/%s.py", filename);
  filepath = Find_File( pythonfileexpanded );
  if(!filepath)
  {
    Com_Printf( S_COLOR_RED "ERROR: couldn't load script file %s\n", filename );
    return qfalse;
  }
  fp = fopen(filepath, "r+");
  PyRun_SimpleFile( fp, filepath);
  fclose(fp);
  return qtrue;
}
void PY_ExecScript_f( void )
{
  int i, numargv;
  if ( Cmd_Argc() < 2 ) {
    Com_Printf( "pyexec <file> (args)\n" );
    return;
  }
  numargv = Cmd_Argc()-2;
  PyRun_SimpleString("server.argv = []");
  PyRun_SimpleString( va("server.argv.append(\"%s\")", Cmd_Argv(1) ) );
  for (i = 0; i < numargv; i++){
    PyRun_SimpleString( va("server.argv.append(\"%s\")", Cmd_Argv(2+i) ) );
  }
  PY_ExecScript( Cmd_Argv(1) );
}

void PY_Frame( void )
{
  if(Python_Initialized)
    PyRun_SimpleString("time.sleep(0.0001)");
}
