/*
   ===========================================================================
   Copyright (C) 2008 Maurice Doison <mdoison@linux62.org>

   This file is part of Tremulous source code.

   Tremulous source code is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the License,
   or (at your option) any later version.

   Tremulous source code is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tremulous source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
   ===========================================================================
   */
// lua_player.c -- player library for Lua

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif

#include "g_local.h"

int G_CallPlayerHooks(const char *event, gentity_t *ent)
{
#ifdef USE_LUA
  lua_State *L = g_luaState;
  lua_Entity *lent;
  int ret;

  lua_getglobal(L, "game");
  lua_getfield(L, -1, "entity");
  lua_remove(L, -2);
  lua_getfield(L, -1, "player");
  lua_remove(L, -2);

  lent = lua_newuserdata(L, sizeof(lua_Entity));
  lent->e = ent;

  ret = G_CallHooks(event);

  lua_pop(L, 2);

  return ret;
#else
  return 1;
#endif
}
#ifdef USE_LUA
static int player_AddHook(lua_State *L)
{
  lua_getglobal(L, "game");
  lua_getfield(L, -1, "entity");
  lua_remove(L, -2);
  lua_getfield(L, -1, "player");
  lua_remove(L, -2);

  G_AddHook(L, lua_tostring(L, -3));

  return 0;
}

static const luaL_reg entity_meta[] = {
  {"AddHook", player_AddHook},

  {NULL, NULL}
};

int luaopen_player(lua_State * L)
{
  lua_getglobal(L, "game");
  lua_getfield(L, -1, "entity");
  lua_remove(L, -2);
  luaL_register(L, "game.entity.player", entity_meta);

  lua_newtable(L);

  // Events from lua_entity
  G_CreateHooksTable(L, "on_init");
  G_CreateHooksTable(L, "on_damage");
  G_CreateHooksTable(L, "on_think");
  G_CreateHooksTable(L, "on_use");
  G_CreateHooksTable(L, "on_pain");
  G_CreateHooksTable(L, "on_die");

  // New player events
  G_CreateHooksTable(L, "on_primary_attack");
  G_CreateHooksTable(L, "on_secondary_attack");
  G_CreateHooksTable(L, "on_third_attack");
  G_CreateHooksTable(L, "on_spawn");
  G_CreateHooksTable(L, "on_change_team");
  G_CreateHooksTable(L, "on_evolve");
  G_CreateHooksTable(L, "on_build");
  G_CreateHooksTable(L, "on_inventory_changed");

  lua_setfield(L, -2, "hooks");

  return 1;
}

#endif

