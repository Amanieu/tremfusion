/*
   ===========================================================================
   Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>

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
// lua_entity.c -- entity library for Lua


#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif

#include "g_local.h"

int G_CallEntityHooks(const char *event, gentity_t *ent)
{
#ifdef USE_LUA
  lua_State *L = g_luaState;
  lua_Entity *lent;
  int ret;

  lua_getglobal(L, "game");
  lua_getfield(L, -1, "entity");
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
static int entity_AddHook(lua_State *L)
{
  lua_getglobal(L, "game");
  lua_getfield(L, -1, "entity");
  lua_remove(L, -2);

  G_AddHook(L, lua_tostring(L, -3));

  return 0;
}

static int entity_Target(lua_State * L)
{
  lua_Entity     *lent;
  lua_Entity     *target;
  gentity_t      *t = NULL;

  target = lua_newuserdata(L, sizeof(lua_Entity));
  luaL_getmetatable(L, "game.entity");
  lua_setmetatable(L, -2);

  lent = lua_getentity(L, 1);


  if(!lent->e)
  {
    Com_Printf("entity_Target: invalid entity!\n");
    return 0;
  }
  if(!lent->e->target)
  {
    Com_Printf("entity_Target: no target!\n");
    return 0;
  }

  t = G_PickTarget(lent->e->target);
  if(!t)
  {
    G_Printf("entity_Target: Couldn't find target %s\n", lent->e->target);
    return 0;
  }

  target->e = t;

  return 1;


}

/*static int entity_Find(lua_State * L)
  {
  char           *s;
  lua_Entity     *lent;
  int             i;
  gentity_t      *t;

  lent = lua_newuserdata(L, sizeof(lua_Entity));

  luaL_getmetatable(L, "game.entity");
  lua_setmetatable(L, -2);

  s = (char *)luaL_checkstring(L, 1);

  lent->e = NULL;

  for(i = 0; i < level.num_entities; i++)
  {
// Here we use tent to point to potential targets
t = &g_entities[i];

if(!t->inuse)
continue;

if(Q_stricmp(t->name, s) == 0)
{
lent->e = t;
break;
}

}

if(!lent->e)
{
Com_Printf("entity_Find: entity '%s' not found!\n", s);
return 0;
}

return 1;
}*/


/*static int entity_Teleport(lua_State * L)
  {
  lua_Entity     *lent;
  lua_Entity     *target;


  lent = lua_getentity(L, 1);
  target = lua_getentity(L, 2);

  if(!lent->e)
  {
  Com_Printf("entity_Teleport: invalid entity!\n");
  return 0;
  }
  if(!target->e)
  {
  Com_Printf("entity_Teleport: invalid target!\n");
  return 0;
  }

  if(lent->e->client)
  TeleportPlayer(lent->e, target->e->s.origin, target->e->s.angles);
  else
  TeleportEntity(lent->e, target->e->s.origin, target->e->s.angles);
  return 1;
  }*/



/*static int entity_IsRocket(lua_State * L)
  {
  lua_Entity     *lent;
  qboolean        rocket = qfalse;

  lent = lua_getentity(L, 1);

  if(lent->e->classname == "rocket")
  rocket = qtrue;

  lua_pushboolean(L, rocket);

  return 1;
  }
  static int entity_IsGrenade(lua_State * L)
  {
  lua_Entity     *lent;
  qboolean        grenade = qfalse;

  lent = lua_getentity(L, 1);

  if(lent->e->classname == "grenade")
  grenade = qtrue;

  lua_pushboolean(L, grenade);

  return 1;
  }*/


static int entity_Spawn(lua_State * L)
{
  lua_Entity     *lent;

  lent = lua_newuserdata(L, sizeof(lua_Entity));

  luaL_getmetatable(L, "game.entity");
  lua_setmetatable(L, -2);

  lent->e = G_Spawn();

  return 1;
}

/*static int entity_GetNumber(lua_State * L)
  {
  lua_Entity     *lent;

  lent = lua_getentity(L, 1);
  lua_pushnumber(L, lent->e - g_entities);
//  lua_pushnumber(L, lent->e->s.number);

return 1;
}

static int entity_IsClient(lua_State * L)
{
lua_Entity     *lent;

lent = lua_getentity(L, 1);
lua_pushboolean(L, lent->e->client != NULL);

return 1;
}

static int entity_GetClientName(lua_State * L)
{
lua_Entity     *lent;

lent = lua_getentity(L, 1);
lua_pushstring(L, lent->e->client->pers.netname);

return 1;
}

static int entity_Print(lua_State * L)
{
lua_Entity     *lent;
int             i;
char            buf[MAX_STRING_CHARS];
int             n = lua_gettop(L);	// number of arguments

lent = lua_getentity(L, 1);
if(!lent->e->client)
return luaL_error(L, "`Print' must be used with a client entity");

memset(buf, 0, sizeof(buf));

lua_getglobal(L, "tostring");
for(i = 2; i <= n; i++)
{
const char     *s;

lua_pushvalue(L, -1);	// function to be called
lua_pushvalue(L, i);	// value to print
lua_call(L, 1, 1);
s = lua_tostring(L, -1);	// get result

if(s == NULL)
return luaL_error(L, "`tostring' must return a string to `print'");

Q_strcat(buf, sizeof(buf), s);

lua_pop(L, 1);			// pop result
}

trap_SendServerCommand(lent->e - g_entities, va("print \"%s\n\"", buf));

return 0;
}

static int entity_CenterPrint(lua_State * L)
{
lua_Entity     *lent;
int             i;
char            buf[MAX_STRING_CHARS];
int             n = lua_gettop(L);	// number of arguments

lent = lua_getentity(L, 1);
if(!lent->e->client)
  return luaL_error(L, "`CenterPrint' must be used with a client entity");

  memset(buf, 0, sizeof(buf));

  lua_getglobal(L, "tostring");
  for(i = 2; i <= n; i++)
{
  const char     *s;

  lua_pushvalue(L, -1);	// function to be called
  lua_pushvalue(L, i);	// value to print
  lua_call(L, 1, 1);
  s = lua_tostring(L, -1);	// get result

  if(s == NULL)
    return luaL_error(L, "`tostring' must return a string to `print'");

  Q_strcat(buf, sizeof(buf), s);

  lua_pop(L, 1);			// pop result
}

trap_SendServerCommand(lent->e - g_entities, va("cp \"" S_COLOR_WHITE "%s\n\"", buf));

return 0;
}

static int entity_GetClassName(lua_State * L)
{
  lua_Entity     *lent;

  lent = lua_getentity(L, 1);
  lua_pushstring(L, lent->e->classname);

  return 1;
}

static int entity_SetClassName(lua_State * L)
{
  lua_Entity     *lent;

  //  char           *classname;

  lent = lua_getentity(L, 1);
  lent->e->classname = (char *)luaL_checkstring(L, 2);

  //  lent->e->classname = classname;

  return 1;
}*/

/*static int entity_GetTargetName(lua_State * L)
  {
  lua_Entity     *lent;

  lent = lua_getentity(L, 1);
  lua_pushstring(L, lent->e->name);

  return 1;
  }*/

/*static int entity_Rotate(lua_State * L)
  {
  lua_Entity     *lent;
  vec_t          *vec;

  lent = lua_getentity(L, 1);
  vec = lua_getvector(L, 2);

  lent->e->s.apos.trType = TR_LINEAR;
  lent->e->s.apos.trDelta[0] = vec[0];
  lent->e->s.apos.trDelta[1] = vec[1];
  lent->e->s.apos.trDelta[2] = vec[2];

  return 1;
  }

  static int entity_GC(lua_State * L)
  {
//  G_Printf("Lua says bye to entity = %p\n", lua_getentity(L));

return 0;
}*/

/*static int entity_ToString(lua_State * L)
  {
  lua_Entity     *lent;
  gentity_t      *gent;
  char            buf[MAX_STRING_CHARS];

  lent = lua_getentity(L, 1);
  gent = lent->e;
  Com_sprintf(buf, sizeof(buf), "entity: class=%s name=%s id=%i pointer=%p\n", gent->classname, gent->name, gent - g_entities,
  gent);
  lua_pushstring(L, buf);

  return 1;
  }*/

static const luaL_reg entity_ctor[] = {
  {"Spawn", entity_Spawn},

  /*{"Find", entity_Find},		//find an entity by name e.g ent = entity.Find("myentity");*/
  {"Target", entity_Target},	//find an entitys target, e.g target = entity.Target(ent);
  {"AddHook", entity_AddHook},

  {NULL, NULL}
};

static const luaL_reg entity_meta[] = {
  /*{"__gc", entity_GC},
    {"__tostring", entity_ToString},
    {"GetNumber", entity_GetNumber},
    {"IsClient", entity_IsClient},
    {"GetClientName", entity_GetClientName},
    {"Print", entity_Print},
    {"CenterPrint", entity_CenterPrint},
    {"GetClassName", entity_GetClassName},
    {"SetClassName", entity_SetClassName},
    {"GetTargetName", entity_GetTargetName},
    {"Rotate", entity_Rotate},*/
  {"AddHook", entity_AddHook},

  {NULL, NULL}
};

int luaopen_entity(lua_State * L)
{
  lua_getglobal(L, "game");
  luaL_register(L, "game.entity", entity_meta);

  lua_newtable(L);

  G_CreateHooksTable(L, "on_init");
  G_CreateHooksTable(L, "on_damage");

  G_CreateHooksTable(L, "on_think");
  //G_CreateHooksTable(L, "on_reached");
  //G_CreateHooksTable(L, "on_blocked");
  //G_CreateHooksTable(L, "on_touch");
  G_CreateHooksTable(L, "on_use");
  G_CreateHooksTable(L, "on_pain");
  G_CreateHooksTable(L, "on_die");

  lua_setfield(L, -2, "hooks");

  /*luaL_newmetatable(L, "game.entity");

    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);		// pushes the metatable
    lua_settable(L, -3);		// metatable.__index = metatable

    luaL_register(L, NULL, entity_meta);
    luaL_register(L, "entity", entity_ctor);

    lua_pushstring(L, "hooks");
    lua_newtable(L);
    lua_settable(L, -3);*/

  return 1;
}

void lua_pushentity(lua_State * L, gentity_t * ent)
{
  lua_Entity     *lent;

  lent = lua_newuserdata(L, sizeof(lua_Entity));

  luaL_getmetatable(L, "game.entity");
  lua_setmetatable(L, -2);

  lent->e = ent;
}

lua_Entity     *lua_getentity(lua_State * L, int argNum)
{
  void           *ud;

  ud = luaL_checkudata(L, argNum, "game.entity");
  luaL_argcheck(L, ud != NULL, argNum, "`entity' expected");
  return (lua_Entity *) ud;
}

#endif
