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

// sc_utils.c

#include "sc_public.h"

static scConstant_t *sc_constants;
static int sc_buf_const;
static int sc_num_const;

void SC_Constant_Init(void)
{
  sc_buf_const = 32;
  sc_constants = BG_Alloc(sc_buf_const * sizeof(scConstant_t));
  sc_num_const = 0;
}

void SC_Constant_Add(scConstant_t *constants)
{
  int n;
  scConstant_t *cst = constants;
  while(cst->name[0] != '\0')
    cst++;

  n = (cst - constants);

  if(n >= sc_num_const + sc_buf_const)
  {
    cst = sc_constants;

    do
    {
      sc_buf_const *= 2;
    } while(sc_buf_const <= sc_num_const + n);

    sc_constants = BG_Alloc(sc_buf_const * sizeof(scConstant_t));

    memcpy(sc_constants, cst, sc_num_const * sizeof(scConstant_t));

    BG_Free(cst);
  }

  cst = constants;
  memcpy(sc_constants + sc_num_const, constants, n);
  sc_num_const += n;
}

scConstant_t *SC_Constant_Get(const char *name)
{
  scConstant_t *cst = sc_constants;

  while(cst->name[0] != '\0')
  {
    if(strcmp(cst->name, name) == 0)
      return cst;
    cst++;
  }

  return NULL;
}

