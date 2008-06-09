/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

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

#include "g_local.h"
#include "../botlib/botlib.h"

extern vmCvar_t bot_minaliens;
extern vmCvar_t bot_minhumans;

void G_AddBot( const char *name, const char *team) {
	int				clientNum;
	gentity_t		*bot;
	//char			*botname;
	char			userinfo[MAX_INFO_STRING];

	// create the bot's userinfo
	userinfo[0] = '\0';

	Info_SetValueForKey( userinfo, "name", name );
	Info_SetValueForKey( userinfo, "rate", "25000" );
	Info_SetValueForKey( userinfo, "snaps", "20" );

	// have the server allocate a client slot
	clientNum = trap_BotAllocateClient();
	if ( clientNum == -1 ) {
		G_Printf( S_COLOR_RED "Unable to add bot.  All player slots are in use.\n" );
		G_Printf( S_COLOR_RED "Start server with more 'open' slots (or check setting of sv_maxclients cvar).\n" );
		return;
	}

	// initialize the bot settings
	if( !team || !*team ) {
		team = "balanceme";
	}
	Info_SetValueForKey( userinfo, "team", team );
	bot = &g_entities[ clientNum ];
	bot->r.svFlags |= SVF_BOT;
	bot->inuse = qtrue;

	// register the userinfo
	trap_SetUserinfo( clientNum, userinfo );

	// have it connect to the game as a normal client
	if ( ClientConnect( clientNum, qtrue, qtrue ) ) {
		return;
	}

	ClientBegin( clientNum );


	//AddBotToSpawnQueue( clientNum, delay );
}

#define BOT_SPAWN_QUEUE_DEPTH	16

typedef struct {
	int		clientNum;
	int		spawnTime;
} botSpawnQueue_t;

static botSpawnQueue_t	botSpawnQueue[BOT_SPAWN_QUEUE_DEPTH];

/*
===============
AddBotToSpawnQueue
===============
*/
static void AddBotToSpawnQueue( int clientNum, int delay ) {
	int		n;

	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( !botSpawnQueue[n].spawnTime ) {
			botSpawnQueue[n].spawnTime = level.time + delay;
			botSpawnQueue[n].clientNum = clientNum;
			return;
		}
	}

	G_Printf( S_COLOR_YELLOW "Unable to delay spawn\n" );
	ClientBegin( clientNum );
}


/*
===============
G_RemoveQueuedBotBegin

Called on client disconnect to make sure the delayed spawn
doesn't happen on a freed index
===============
*/
void G_RemoveQueuedBotBegin( int clientNum ) {
	int		n;

	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( botSpawnQueue[n].clientNum == clientNum ) {
			botSpawnQueue[n].spawnTime = 0;
			return;
		}
	}
}

/*
===============
G_AddRandomBot
===============
*/
void G_AddRandomBot( int team ) {
	char	netname[36], *teamstr;



	if (team == TEAM_ALIENS) teamstr = "aliens";
	else if (team == TEAM_HUMANS) teamstr = "humans";
	else teamstr = "";
	
	strncpy(netname, "bot", sizeof(netname)-1);		// todo
	netname[sizeof(netname)-1] = '\0';
	Q_CleanStr(netname);
	
	trap_SendConsoleCommand( EXEC_INSERT, va("addbot %s %s\n", netname, teamstr) );
	return;
}

/*
===============
G_RemoveRandomBot
===============
*/
int G_RemoveRandomBot( int team ) {
	gclient_t	*cl;
	int i;

	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) ) {
			continue;
		}
		if ( team >= 0 && cl->ps.stats[ STAT_PTEAM ] != team ) {
			continue;
		}
		//strcpy(netname, cl->pers.netname);
		//Q_CleanStr(netname);
		//trap_SendConsoleCommand( EXEC_INSERT, va("kick %s\n", netname) );
		trap_SendConsoleCommand( EXEC_INSERT, va("clientkick %d\n", cl->ps.clientNum) );
		return qtrue;
	}
	return qfalse;
}

/*
===============
G_CountHumanPlayers
===============
*/
int G_CountHumanPlayers( int team ) {
	int i, num;
	gclient_t	*cl;

	num = 0;
	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT ) {
			continue;
		}
		if ( team >= 0 && cl->ps.stats[ STAT_PTEAM ] != team ) {
			continue;
		}
		num++;
	}
	return num;
}

/*
===============
G_CountBotPlayers
===============
*/
int G_CountBotPlayers( int team ) {
	int i, n, num;
	gclient_t	*cl;

	num = 0;
	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) ) {
			continue;
		}
		if ( team >= 0 && cl->ps.stats[ STAT_PTEAM ] != team ) {
			continue;
		}
		num++;
	}
	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( !botSpawnQueue[n].spawnTime ) {
			continue;
		}
		if ( botSpawnQueue[n].spawnTime > level.time ) {
			continue;
		}
		num++;
	}
	return num;
}

void G_CheckMinimumPlayers( void ) {
	int minaliens, minhumans;
	int humanplayers, botplayers;
	static int checkminimumplayers_time;

	if (level.intermissiontime) return;
	
	//only check once each 5 seconds
	if (checkminimumplayers_time > level.time - 1000) {
		return;
	}
	checkminimumplayers_time = level.time;
	
	// check alien team
	trap_Cvar_Update(&bot_minaliens);
	minaliens = bot_minaliens.integer;
	if (minaliens <= 0) return;
	
	if (minaliens >= g_maxclients.integer / 2) {
		minaliens = (g_maxclients.integer / 2) -1;
	}

	humanplayers = G_CountHumanPlayers( TEAM_ALIENS );
	botplayers = G_CountBotPlayers(	TEAM_ALIENS );
	//
	if (humanplayers + botplayers < minaliens ) {
		G_AddRandomBot( TEAM_ALIENS );
	} else if (humanplayers + botplayers > minaliens && botplayers) {
		G_RemoveRandomBot( TEAM_ALIENS );
	}
	
	// check human team
	trap_Cvar_Update(&bot_minhumans);
	minhumans = bot_minhumans.integer;
	if (minhumans <= 0) return;
	
	if (minhumans >= g_maxclients.integer / 2) {
		minhumans = (g_maxclients.integer / 2) -1;
	}
	humanplayers = G_CountHumanPlayers( TEAM_HUMANS );
	botplayers = G_CountBotPlayers( TEAM_HUMANS );
	//
	if (humanplayers + botplayers < minhumans ) {
		G_AddRandomBot( TEAM_HUMANS );
	} else if (humanplayers + botplayers > minhumans && botplayers) {
		G_RemoveRandomBot( TEAM_HUMANS );
	}
}


void G_CheckBotSpawn( void ) {
	int		n;

	G_CheckMinimumPlayers();

	for( n = 0; n < BOT_SPAWN_QUEUE_DEPTH; n++ ) {
		if( !botSpawnQueue[n].spawnTime ) {
			continue;
		}
		if ( botSpawnQueue[n].spawnTime > level.time ) {
			continue;
		}
		ClientBegin( botSpawnQueue[n].clientNum );
		botSpawnQueue[n].spawnTime = 0;
	}
}

/*
===============
G_BotConnect
===============
*/
qboolean G_BotConnect( int clientNum, qboolean restart ) {
	//bot_settings_t	settings;
	char			teamstr[MAX_FILEPATH];
	char			userinfo[MAX_INFO_STRING];
	int 			team;

	trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );

	Q_strncpyz( teamstr, Info_ValueForKey( userinfo, "team" ), sizeof(teamstr) );
	
	if( !Q_stricmp( teamstr, "aliens" ) )
		team = TEAM_ALIENS;
	else if( !Q_stricmp( teamstr, "humans" ) )
		team = TEAM_HUMANS;
	else{
		//TODO auto select
		team = TEAM_HUMANS;
	}
		

	if (!BotAISetupClient( clientNum, team, restart )) {
		trap_DropClient( clientNum, "BotAISetupClient failed" );
		return qfalse;
	}

	return qtrue;
}
