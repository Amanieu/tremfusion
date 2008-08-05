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

#include "sc_public.h"

void SC_AddLibrary( const char *namespace, scLibFunction_t lib[] )
{
  scLibFunction_t *l = lib;
  scDataTypeValue_t var;
  char name[ MAX_PATH_LENGTH + 1 ];
  int nslen = strlen( namespace );

  Q_strncpyz( name, namespace, sizeof( name ) );
  name[nslen] = '.';

  var.type = TYPE_FUNCTION;
  while( strcmp( l->name, "" ) != 0 )
  {
    var.data.function = SC_FunctionNew();
    var.data.function->langage = LANGAGE_C;
    memcpy( var.data.function->argument, l->argument, sizeof( var.data.function->argument ) );
    var.data.function->return_type = lib->return_type;
    var.data.function->data.ref = lib->ref;

    Q_strncpyz( name + nslen + 1, l->name, strlen( l->name ) + 1);

    SC_NamespaceSet( name, & var );
    l++;
  }
}

scClass_t *SC_AddClass( const char *namespace, scLibObjectDef_t *def )
{
  char name[ MAX_PATH_LENGTH + 1 ];
  scDataTypeValue_t var;
  scClass_t    *class;
  scLibObjectMember_t  *member;
  scLibObjectMethod_t  *method;
  int nslen = strlen( namespace );
  int i, cnt;
  
  class = BG_Alloc( sizeof(scClass_t) );
  strcpy(class->name, def->name);
  strcpy(class->desc, def->desc);

  class->constructor.gc.count = 0;
  class->constructor.langage = LANGAGE_C;
  class->constructor.argument[0] = TYPE_CLASS;
  memcpy(class->constructor.argument+1, def->constructor_arguments, ( MAX_FUNCTION_ARGUMENTS ) * sizeof(scDataType_t));
  class->constructor.return_type = TYPE_OBJECT;
  class->constructor.data.ref = def->constructor;

  class->destructor.gc.count = 0;
  class->destructor.langage = LANGAGE_C;
  class->destructor.argument[0] = TYPE_CLASS;
  class->destructor.argument[1] = TYPE_UNDEF;
  class->destructor.return_type = TYPE_UNDEF;
  class->destructor.data.ref = def->destructor;

  cnt = 0;
  if(def->members != NULL)
    for(member = def->members; member->name[0] != '\0'; member++)
      cnt++;
  class->memcount = cnt;
  class->members = BG_Alloc(sizeof(scObjectMember_t) * (cnt+1));
  for(i = 0; i < cnt; i++)
  {
    strcpy(class->members[i].name, def->members[i].name);
    strcpy(class->members[i].desc, def->members[i].desc);
    class->members[i].type = def->members[i].type;

    class->members[i].set.gc.count = 0;
    class->members[i].set.langage = LANGAGE_C;
    class->members[i].set.data.ref = def->members[i].set;
    class->members[i].set.argument[0] = class->members[i].type;
    class->members[i].set.argument[1] = TYPE_UNDEF;
    class->members[i].set.return_type = TYPE_UNDEF;
    class->members[i].set.closure = def->members[i].closure;

    class->members[i].get.gc.count = 0;
    class->members[i].get.langage = LANGAGE_C;
    class->members[i].get.data.ref = def->members[i].get;
    class->members[i].get.argument[0] = TYPE_UNDEF;
    class->members[i].get.return_type = class->members[i].type;
    class->members[i].get.closure = def->members[i].closure;
  }
  class->members[i].name[0] = '\0';

  cnt = 0;
  if(def->methods != NULL)
    for(method = def->methods; method->name[0] != '\0'; method++)
      cnt++;
  class->methcount = cnt;
  class->methods = BG_Alloc(sizeof(scObjectMethod_t) * (cnt+1));
  for(i = 0; i < cnt; i++)
  {
    strcpy(class->methods[i].name, def->methods[i].name);
    strcpy(class->methods[i].desc, def->methods[i].desc);

    class->methods[i].method.gc.count = 0;
    class->methods[i].method.langage = LANGAGE_C;
    class->methods[i].method.data.ref = def->methods[i].method;
    class->methods[i].method.argument[0] = TYPE_OBJECT;
    memcpy( class->methods[i].method.argument+1, def->methods[i].argument,
            sizeof(scDataType_t) * MAX_FUNCTION_ARGUMENTS);
    class->methods[i].method.return_type = def->methods[i].return_type;
    class->methods[i].method.closure = def->methods[i].closure;
  }
  class->methods[i].name[0] = '\0';

  var.type = TYPE_CLASS;
  var.data.class = class;
  SC_ValueGCInc(&var);

  Q_strncpyz( name, namespace, sizeof( name ) );
  name[nslen] = '.';
  Q_strncpyz( name + nslen + 1, def->name, strlen( def->name ) + 1);
  SC_NamespaceSet(name, &var);
  
  SC_InitClass(class);
  return class;
}

