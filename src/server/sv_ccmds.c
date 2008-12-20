/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

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

#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
SV_CompleteDemoName
====================
*/
static void SV_CompleteDemoName( char *args, int argNum )
{
	if( argNum == 2 )
	{
		char demoExt[ 16 ];

		Com_sprintf( demoExt, sizeof( demoExt ), ".svdm_%d", PROTOCOL_VERSION );
		Field_CompleteFilename( "svdemos", demoExt, qtrue );
	}
}

/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByHandle( void ) {
	client_t	*cl;
	int			i;
	char		*s;
	char		cleanName[64];

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);
	
	if(!s[i])
	{
		int plid = atoi(s);

		// Check for numeric playerid match
		if(plid >= 0 && plid < sv_maxclients->integer)
		{
			cl = &svs.clients[plid];
			
			if(cl->state)
				return cl;
		}
	}

	// check for a name match
	for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
		if ( !cl->state ) {
			continue;
		}
		if ( !Q_stricmp( cl->name, s ) ) {
			return cl;
		}

		Q_strncpyz( cleanName, cl->name, sizeof(cleanName) );
		Q_CleanStr( cleanName );
		if ( !Q_stricmp( cleanName, s ) ) {
			return cl;
		}
	}

	Com_Printf( "Player %s is not on the server\n", s );

	return NULL;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	char		*cmd;
	char		*map;
	qboolean	killBots, cheat;
	char		expanded[MAX_QPATH];
	char		mapname[MAX_QPATH];
	int			i;

	map = Cmd_Argv(1);
	if ( !map ) {
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);
	if ( FS_ReadFile (expanded, NULL) == -1 ) {
		Com_Printf ("Can't find map %s\n", expanded);
		return;
	}

	cmd = Cmd_Argv(0);
	if ( !Q_stricmp( cmd, "devmap" ) ) {
		cheat = qtrue;
		killBots = qtrue;
	} else {
		cheat = qfalse;
		killBots = qfalse;
	}

	// stop any demos
	if (sv.demoState == DS_RECORDING)
		SV_DemoStopRecord();
	if (sv.demoState == DS_PLAYBACK)
		SV_DemoStopPlayback();

	// save the map name here cause on a map restart we reload the autogen.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	SV_SpawnServer( mapname, killBots );

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( cheat ) {
		Cvar_Set( "sv_cheats", "1" );
	} else {
		Cvar_Set( "sv_cheats", "0" );
	}

	// This forces the local master server IP address cache
	// to be updated on sending the next heartbeat
	for( i = 0; i < MAX_MASTER_SERVERS; i++ )
		sv_master[ i ]->modified  = qtrue;
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int			i;
	client_t	*client;
	char		*denied;
	int			delay;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( sv.restartTime ) {
		return;
	}

	if (Cmd_Argc() > 1 ) {
		delay = atoi( Cmd_Argv(1) );
	}
	else {
		delay = 5;
	}
	if( delay && !Cvar_VariableValue("g_doWarmup") ) {
		sv.restartTime = sv.time + delay * 1000;
		SV_SetConfigstring( CS_WARMUP, va("%i", sv.restartTime) );
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients and democlients change
	if ( sv_maxclients->modified || sv_democlients->modified ) {
		char	mapname[MAX_QPATH];

		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );

		SV_SpawnServer( mapname, qfalse );
		return;
	}

	// stop any demos
	if (sv.demoState == DS_RECORDING)
		SV_DemoStopRecord();
	if (sv.demoState == DS_PLAYBACK)
		SV_DemoStopPlayback();

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid	
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for (i = 0; i < 3; i++)
	{
		VM_Call (gvm, GAME_RUN_FRAME, sv.time);
		sv.time += 100;
		svs.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for (i=0 ; i<sv_maxclients->integer ; i++) {
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED) {
			continue;
		}

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		denied = VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

		client->state = CS_ACTIVE;

		SV_ClientEnterWorld( client, &client->lastUsercmd );
	}	

	// run another frame to allow things to look at all the players
	VM_Call (gvm, GAME_RUN_FRAME, sv.time);
	sv.time += 100;
	svs.time += 100;

	// start recording a demo
	if ( sv_autoDemo->integer ) {
		qtime_t	now;
		Com_RealTime( &now );
		Cbuf_AddText( va( "demo_record %04d%02d%02d%02d%02d%02d-%s\n",
			1900 + now.tm_year,
			1 + now.tm_mon,
			now.tm_mday,
			now.tm_hour,
			now.tm_min,
			now.tm_sec,
			Cvar_VariableString( "mapname" ) ) );
	}
}

//===============================================================


/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int			i, j, l;
	client_t	*cl;
	playerState_t	*ps;
	const char		*s;
	int			ping;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("map: %s\n", sv_mapname->string );

	Com_Printf ("num score ping name            lastmsg address               qport rate\n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++)
	{
		if (!cl->state)
			continue;
		Com_Printf ("%3i ", i);
		ps = SV_GameClientNum( i );
		Com_Printf ("%5i ", ps->persistant[PERS_SCORE]);

		if (cl->state == CS_CONNECTED)
			Com_Printf ("CNCT ");
		else if (cl->state == CS_ZOMBIE)
			Com_Printf ("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", ping);
		}

		Com_Printf ("%s", cl->name);
    // TTimo adding a ^7 to reset the color
    // NOTE: colored names in status breaks the padding (WONTFIX)
    Com_Printf ("^7");
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");

		Com_Printf ("%7i ", svs.time - cl->lastPacketTime );

		s = NET_AdrToString( cl->netchan.remoteAddress );
		Com_Printf ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");
		
		Com_Printf ("%5i", cl->netchan.qport);

		Com_Printf (" %5i", cl->rate);

		Com_Printf ("\n");
	}
	Com_Printf ("\n");
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SYSTEMINFO ) );
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}


/*
=================
SV_StartRedirect_f

Redirect console output to a client
=================
*/
static client_t *redirect_client = NULL;
static void SV_ClientRedirect( char *outputbuf ) {
	SV_SendServerCommand( redirect_client, "%s", outputbuf );
}
static void SV_StartRedirect_f( void ) {
#define SV_OUTPUTBUF_LENGTH (1024 - 16)
	int clientNum;
	static char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

	clientNum = atoi( Cmd_Argv(1) );
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer )
		return;
	redirect_client = svs.clients + clientNum;
	Com_EndRedirect( );
	Com_BeginRedirect( sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_ClientRedirect );
}


/*
=================
SV_Demo_Record_f
=================
*/
static void SV_Demo_Record_f( void ) {
	// make sure server is running
	if (!com_sv_running->integer) {
		Com_Printf("Server is not running.\n");
		return;
	}

	if (Cmd_Argc() > 2) {
		Com_Printf("Usage: demo_record <demoname>\n");
		return;
	}

	if (sv.demoState != DS_NONE) {
		Com_Printf("A demo is already being recorded/played.\n");
		return;
	}

	if (sv_maxclients->integer == MAX_CLIENTS) {
		Com_Printf("Too many slots, reduce sv_maxclients.\n");
		return;
	}

	if (Cmd_Argc() == 2)
		sprintf(sv.demoName, "svdemos/%s.svdm_%d", Cmd_Argv(1), PROTOCOL_VERSION);
	else {
		int	number;
		// scan for a free demo name
		for (number = 0 ; number >= 0 ; number++) {
			Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%d.svdm_%d", number, PROTOCOL_VERSION);
			if (!FS_FileExists(sv.demoName))
				break;	// file doesn't exist
		}
		if (number < 0) {
			Com_Printf("Couldn't generate a filename for the demo, try deleting some old ones.\n");
			return;
		}
	}

	sv.demoFile = FS_FOpenFileWrite(sv.demoName);
	if (!sv.demoFile) {
		Com_Printf("ERROR: Couldn't open %s for writing.\n", sv.demoName);
		return;
	}
	SV_DemoStartRecord();
}


/*
=================
SV_Demo_Play_f
=================
*/
static void SV_Demo_Play_f( void ) {
	char *arg;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: demo_play <demoname>\n");
		return;
	}

	if (sv.demoState != DS_NONE) {
		Com_Printf("A demo is already being recorded/played.\n");
		return;
	}

	if (sv_democlients->integer <= 0) {
		Com_Printf("You need to set sv_democlients to a value greater than 0.\n");
		return;
	}
	
	// check for an extension .svdm_?? (?? is protocol)
	arg = Cmd_Argv(1);
	if (!strcmp(arg + strlen(arg) - 6, va(".svdm_%d", PROTOCOL_VERSION)))
		Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%s", arg);
	else
		Com_sprintf(sv.demoName, sizeof(sv.demoName), "svdemos/%s.svdm_%d", arg, PROTOCOL_VERSION);

	FS_FOpenFileRead(sv.demoName, &sv.demoFile, qtrue);
	if (!sv.demoFile) {
		Com_Printf("ERROR: Couldn't open %s for reading.\n", sv.demoName);
		return;
	}
	SV_DemoStartPlayback();
}


/*
=================
SV_Demo_Stop_f
=================
*/
static void SV_Demo_Stop_f( void ) {
	if (sv.demoState == DS_NONE) {
		Com_Printf("No demo is currently being recorded or played.\n");
		return;
	}

	// Close the demo file
	if (sv.demoState == DS_PLAYBACK)
		SV_DemoStopPlayback();
	else
		SV_DemoStopRecord();
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "maps", "bsp", qtrue );
	}
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands( void ) {
	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand ("map_restart", SV_MapRestart_f);
	Cmd_AddCommand ("sectorlist", SV_SectorList_f);
	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
	Cmd_AddCommand ("devmap", SV_Map_f);
	Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
	Cmd_AddCommand ("killserver", SV_KillServer_f);
	Cmd_AddCommand ("startRedirect", SV_StartRedirect_f);
	Cmd_AddCommand ("endRedirect", Com_EndRedirect);
	Cmd_AddCommand ("demo_record", SV_Demo_Record_f);
	Cmd_AddCommand ("demo_play", SV_Demo_Play_f);
	Cmd_SetCommandCompletionFunc( "demo_play", SV_CompleteDemoName );
	Cmd_AddCommand ("demo_stop", SV_Demo_Stop_f);
}

