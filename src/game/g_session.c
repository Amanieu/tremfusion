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


/*
=======================================================================

  SESSION DATA

Session data is the only data that stays persistant across level loads
and tournament restarts.
=======================================================================
*/

/*
================
G_WriteClientSessionData

Called on game shutdown
================
*/
void G_WriteClientSessionData( gclient_t *client )
{
  const char  *s;
  const char  *var;

  s = va( "%i %i %i %s",
    client->sess.spectatorTime,
    client->sess.spectatorState,
    client->sess.spectatorClient,
    BG_ClientListString( &client->sess.ignoreList )
    );

  var = va( "session%i", client - level.clients );

  trap_Cvar_Set( var, s );
}

/*
================
G_ReadSessionData

Called on a reconnect
================
*/
void G_ReadSessionData( gclient_t *client )
{
  char  s[ MAX_STRING_CHARS ];
  const char  *var;
  int spectatorState;

  var = va( "session%i", client - level.clients );
  trap_Cvar_VariableStringBuffer( var, s, sizeof(s) );

  // FIXME: should be using BG_ClientListParse() for ignoreList, but
  //        bg_lib.c's sscanf() currently lacks %s
  sscanf( s, "%i %i %i %x%x",
    &client->sess.spectatorTime,
    &spectatorState,
    &client->sess.spectatorClient,
    &client->sess.ignoreList.hi,
    &client->sess.ignoreList.lo
    );

  client->sess.spectatorState = (spectatorState_t)spectatorState;
}


/*
================
G_InitSessionData

Called on a first-time connect
================
*/
void G_InitSessionData( gclient_t *client, char *userinfo )
{
  client->sess.spectatorState = SPECTATOR_FREE;
  client->sess.spectatorTime = level.time;
  client->sess.spectatorClient = -1;
  memset( &client->sess.ignoreList, 0, sizeof( client->sess.ignoreList ) );

  G_WriteClientSessionData( client );
}


/*
==================
G_WriteSessionData

==================
*/
void G_WriteSessionData( void )
{
  int    i;

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
      G_WriteClientSessionData( &level.clients[ i ] );
  }
}
