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
// lua_game.c -- qagame library for Lua


#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif

#include "g_local.h"

int G_CallGameHooks(const char *event)
{
#ifdef USE_LUA
  lua_State *L = g_luaState;
  int ret;

  lua_getglobal(L, "game");

  lua_pushnil(L);

  ret = G_CallHooks(event);

  lua_pop(L, 2);

  return ret;
#else
  return 1;
#endif
}

#ifdef USE_LUA
static int game_AddHook(lua_State *L)
{
  lua_getglobal(L, "game");
  G_AddHook(L, lua_tostring(L, -3));

  return 0;
}

static int game_Time(lua_State * L)
{
	lua_pushinteger(L, level.time - level.startTime);

	return 1;
}

static int game_StartTime(lua_State * L)
{
	lua_pushinteger(L, level.startTime);

	return 1;
}

static int game_Print(lua_State * L)
{
	int             i;
	char            buf[MAX_STRING_CHARS];
	int             n = lua_gettop(L);	// number of arguments

	memset(buf, 0, sizeof(buf));

	lua_getglobal(L, "tostring");
	for(i = 1; i <= n; i++)
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

	G_Printf("%s\n", buf);
	return 0;
}

static int game_Cp(lua_State * L)
{
	int             i;
	char            buf[MAX_STRING_CHARS];
	int             n = lua_gettop(L);	// number of arguments

	memset(buf, 0, sizeof(buf));

	lua_getglobal(L, "tostring");
	for(i = 1; i <= n; i++)
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

	trap_SendServerCommand(-1, va("cp \"" S_COLOR_WHITE "%s\n\"", buf));
	return 0;
}

static int game_Exec(lua_State * L)
{
	const char *s;

	if(lua_gettop(L) != 1)
		return luaL_error(L, "game.Exec: function take 1 argument");

	s = lua_tostring(L, -1);
	if(s == NULL)
		return luaL_error(L, "game.Exec: invalid arguments");

	trap_SendServerCommand(-1, s);

	return 0;
}

static int game_CvarInteger(lua_State *L)
{
	const char *cvar;
	lua_Integer lval;

	if(lua_gettop(L) != 1)
		return luaL_error(L, "game.CvarInt: function take 1 argument");

	cvar = lua_tostring(L, -1);
	if(cvar == NULL)
		return luaL_error(L, "game.CvarInt: invalid arguments");

	lua_pop(L, 1);

	lval = trap_Cvar_VariableIntegerValue(cvar);
	lua_pushinteger(L, lval);

	return 1;
}

static int game_CvarString(lua_State *L)
{
	const char *cvar;
	char buf[MAX_STRING_CHARS];

	if(lua_gettop(L) != 1)
		return luaL_error(L, "game.CvarString: function take 1 argument");

	cvar = lua_tostring(L, -1);
	if(cvar == NULL)
		return luaL_error(L, "game.CvarString: invalid arguments");

	lua_pop(L, 1);

	trap_Cvar_VariableStringBuffer(cvar, buf, sizeof(buf));

	lua_pushstring(L, buf);
	
	return 1;
}

static int game_CvarFloat(lua_State *L)
{
	const char *cvar;
	lua_Number lval;

	if(lua_gettop(L) != 1)
		return luaL_error(L, "game.CvarFloat: function take 1 argument");

	cvar = lua_tostring(L, -1);
	if(cvar == NULL)
		return luaL_error(L, "game.CvarFloat: invalid arguments");

	lua_pop(L, 1);

	lval = (lua_Number) trap_Cvar_VariableValue(cvar);

	lua_pushvalue(L, lval);

	return 1;
}

static int game_CvarSet(lua_State *L)
{
	const char *cvar;
	const char *value;

	if(lua_gettop(L) != 2)
		return luaL_error(L, "game.CvarSet: function take 2 arguments");

	cvar = lua_tostring(L, -2);

	lua_getglobal(L, "tostring");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	value = lua_tostring(L, -1);

	if(value == NULL)
		return luaL_error(L, "game.CvarSet: function take 2 arguments");

	trap_Cvar_Set(cvar, value);

	return 0;
}

static const luaL_reg gamelib[] = {
	{"AddHook", game_AddHook},
	{"Print", game_Print},
	{"Cp", game_Cp},
	{"Exec", game_Exec},
	{"CvarInteger", game_CvarInteger},
	{"CvarString", game_CvarString},
	{"CvarFloat", game_CvarFloat},
	{"CvarSet", game_CvarSet},
	{"Time", game_Time},
	{"StartTime", game_StartTime},
	{NULL, NULL}
};

int luaopen_game(lua_State * L)
{
	luaL_register(L, "game", gamelib);

	lua_newtable(L);

	G_CreateHooksTable(L, "on_think");
	G_CreateHooksTable(L, "on_init");
	G_CreateHooksTable(L, "on_exit");
	G_CreateHooksTable(L, "on_shutdown");
	G_CreateHooksTable(L, "on_player_connect");
	G_CreateHooksTable(L, "on_player_disconnect");
	G_CreateHooksTable(L, "on_player_begin");
	G_CreateHooksTable(L, "on_stage_up");
	G_CreateHooksTable(L, "on_sudden_death");

	lua_setfield(L, -2, "hooks");

	lua_pushliteral(L, "_GAMEVERSION");
	lua_pushliteral(L, GAME_VERSION);

	return 1;
}
#endif

