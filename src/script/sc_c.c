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

#include "sc_script.h"

void SC_AddLibrary( const char *namespace, scLib_t lib[] )
{
  scLib_t *l = lib;
  scDataTypeValue_t var;
  char name[ MAX_PATH_LENGTH + 1 ];
  int nslen = strlen( namespace );

  Q_strncpyz( name, namespace, sizeof( name ) );
  name[nslen] = '.';

  var.type = TYPE_FUNCTION;
  while( strcmp( l->name, "" ) != 0 )
  {
    SC_FunctionNew( & var.data.function );
    var.data.function->langage = LANGAGE_C;
    memcpy( var.data.function->argument, l->argument, sizeof( var.data.function->argument ) );
    var.data.function->return_type = lib->return_type;
    var.data.function->data.ref = lib->ref;

    Q_strncpyz( name + nslen + 1, l->name, sizeof( l->name ) );

    SC_NamespaceSet( name, & var );
    l++;
  }
}

