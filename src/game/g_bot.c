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
#include "ai_main.h"
#include "g_admin.h"

static int    g_numBots;
static char   *g_botInfos[MAX_BOTS];

vmCvar_t bot_minplayers;

#define MAX_NUMBER_BOTS 20

#define BOT_SPAWN_QUEUE_DEPTH 16

typedef struct {
  int   clientNum;
  int   spawnTime;
} botSpawnQueue_t;

static botSpawnQueue_t  botSpawnQueue[BOT_SPAWN_QUEUE_DEPTH];




void G_BotDel(gentity_t *ent, int clientNum ) {
  gentity_t *bot;
  const char  *teamstring;
  bot = &g_entities[clientNum];
  if( !( bot->r.svFlags & SVF_BOT ) ) {
    ADMP( va("'^7%s^7' is not a bot\n", bot->client->pers.netname) );
    return;
  }
  switch(bot->client->pers.teamSelection)
  {
    case TEAM_ALIENS:
      teamstring = "^1Alien team^7";
      break;
    case TEAM_HUMANS:
      teamstring = "^4Human team^7";
      break;
    case TEAM_NONE:
      teamstring = "^2Spectator team^7";
      break;
    default:
      teamstring = "Team error - you shouldn't see this";
      break;
  }
  level.numBots--;
  ClientDisconnect( clientNum );
  //this needs to be done to free up the client slot - I think - Ender Feb 18 2008
  trap_DropClient( clientNum, va( "was deleted by ^7%s^7 from the %s^7\n\"", ( ent ) ? ent->client->pers.netname : "The Console" , teamstring ) );
}

void G_BotRemoveAll( gentity_t *ent ) {
  int i;
  
  for( i = 0; i < level.num_entities; i++ ) {
    if( g_entities[ i ].r.svFlags & SVF_BOT ) {
      //G_BotDel(ent, i);
      G_BotDel(ent, i);
    }
  }
  AP( va( "print \"^3!bot removeall: ^7%s^7 deleted all bots\n\"", ( ent ) ? ent->client->pers.netname : "The Console"));
}

/*
===============
G_RemoveQueuedBotBegin

Called on client disconnect to make sure the delayed spawn
doesn't happen on a freed index
===============
*/
void G_RemoveQueuedBotBegin( int clientNum ) {
  int   n;

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
void G_AddRandomBot(gentity_t *ent, int team , char *name, float *skill) {
  int   i, n, num;
  //float skill;
  char  *value, netname[36], *teamstr;
  char      *botinfo;
  gclient_t *cl;

  num = 0;
  for ( n = 0; n < g_numBots ; n++ ) {
    value = Info_ValueForKey( g_botInfos[n], "name" );
    //
    for ( i=0 ; i< g_maxclients.integer ; i++ ) {
      cl = level.clients + i;
      if ( cl->pers.connected != CON_CONNECTED ) {
        continue;
      }
      if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) ) {
        continue;
      }
      if ( team >= 0 && cl->pers.teamSelection != team ) {
        continue;
      }
      if ( !Q_stricmp( value, cl->pers.netname ) ) {
        break;
      }
    }
    if (i >= g_maxclients.integer) {
      num++;
    }
  }
  num = random() * num;
  for ( n = 0; n < g_numBots ; n++ ) {
    value = Info_ValueForKey( g_botInfos[n], "name" );
    //
    for ( i=0 ; i< g_maxclients.integer ; i++ ) {
      cl = level.clients + i;
      if ( cl->pers.connected != CON_CONNECTED ) {
        continue;
      }
      if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) ) {
        continue;
      }
      if ( team >= 0 && cl->pers.teamSelection != team ) {
        continue;
      }
      if ( !Q_stricmp( value, cl->pers.netname ) ) {
        break;
      }
    }
    if (i >= g_maxclients.integer) {
      num--;
      if (num <= 0) {
        if (*skill == -1.0)
          *skill = 2.0;
        //trap_Cvar_VariableValue( "g_spSkill" );
        if (team == TEAM_ALIENS) teamstr = "aliens";
        else if (team == TEAM_HUMANS) teamstr = "humans";
        else teamstr = "";
        
        strncpy(netname, value, sizeof(netname)-1);
        netname[sizeof(netname)-1] = '\0';
        Q_CleanStr(netname);
        botinfo = G_GetBotInfoByName( netname );
        G_BotAdd(ent, (name) ? name: netname, team, skill, botinfo);
        //trap_SendConsoleCommand( EXEC_INSERT, va("addbot %s %f %s %i\n", netname, skill, teamstr, 0) );
        return;
      }
    }
  }
} 

/*
===============
G_RemoveRandomBot
===============
*/
int G_RemoveRandomBot( int team ) {
  gclient_t *cl;
  int i;

  for ( i=0 ; i< g_maxclients.integer ; i++ ) {
    cl = level.clients + i;
    if ( cl->pers.connected != CON_CONNECTED ) {
      continue;
    }
    if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) ) {
      continue;
    }
    if ( team >= 0 && cl->ps.stats[ STAT_TEAM ] != team ) {
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
  gclient_t *cl;

  num = 0;
  for ( i=0 ; i< g_maxclients.integer ; i++ ) {
    cl = level.clients + i;
    if ( cl->pers.connected != CON_CONNECTED ) {
      continue;
    }
    if ( g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT ) {
      continue;
    }
    if ( team >= 0 && cl->ps.stats[ STAT_TEAM ] != team ) {
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
  gclient_t *cl;

  num = 0;
  for ( i=0 ; i< g_maxclients.integer ; i++ ) {
    cl = level.clients + i;
    if ( cl->pers.connected != CON_CONNECTED ) {
      continue;
    }
    if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) ) {
      continue;
    }
    if ( team >= 0 && cl->ps.stats[ STAT_TEAM ] != team ) {
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
  float skill_float;

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
  botplayers = G_CountBotPlayers( TEAM_ALIENS );
  //
  if (humanplayers + botplayers < minaliens ) {
    skill_float = -1;
    G_AddRandomBot(NULL, TEAM_ALIENS, NULL, &skill_float );
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
    skill_float = -1;
    G_AddRandomBot(NULL, TEAM_HUMANS , NULL, &skill_float);
  } else if (humanplayers + botplayers > minhumans && botplayers) {
    G_RemoveRandomBot( TEAM_HUMANS );
  }
}


void G_CheckBotSpawn( void ) {
  int   n;

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
  bot_settings_t  settings;
  char      teamstr[MAX_FILEPATH];
  char      userinfo[MAX_INFO_STRING];
  int       team;

  trap_GetUserinfo( clientNum, userinfo, sizeof(userinfo) );

  Q_strncpyz( settings.characterfile, Info_ValueForKey( userinfo, "characterfile" ), sizeof(settings.characterfile) );
  settings.skill = atof( Info_ValueForKey( userinfo, "skill" ) );
  Q_strncpyz( settings.team, Info_ValueForKey( userinfo, "team" ), sizeof(settings.team) );
  
  // At this point, it's quite possible that the bot doesn't have a team, as in it
  // was just created
  if( !Q_stricmp( teamstr, "aliens" ) )
    team = TEAM_ALIENS;
  else if( !Q_stricmp( teamstr, "humans" ) )
    team = TEAM_HUMANS;
  else{
    team = TEAM_NONE;
  }
    
//  Com_Printf("Trying BotAISetupClient\n");
  if (!BotAISetupClient( clientNum, &settings, restart )) {
    trap_DropClient( clientNum, "BotAISetupClient failed" );
    return qfalse;
  }

  return qtrue;
}

// Start of bot charactor stuff

/*
===============
G_ParseInfos
===============
*/
int G_ParseInfos( char *buf, int max, char *infos[] ) {
  char  *token;
  int   count;
  char  key[MAX_TOKEN_CHARS];
  char  info[MAX_INFO_STRING];

  count = 0;

  while ( 1 ) {
    token = COM_Parse( &buf );
    if ( !token[0] ) {
      break;
    }
    if ( strcmp( token, "{" ) ) {
      Com_Printf( "Missing { in info file\n" );
      break;
    }

    if ( count == max ) {
      Com_Printf( "Max infos exceeded\n" );
      break;
    }

    info[0] = '\0';
    while ( 1 ) {
      token = COM_ParseExt( &buf, qtrue );
      if ( !token[0] ) {
        Com_Printf( "Unexpected end of info file\n" );
        break;
      }
      if ( !strcmp( token, "}" ) ) {
        break;
      }
      Q_strncpyz( key, token, sizeof( key ) );

      token = COM_ParseExt( &buf, qfalse );
      if ( !token[0] ) {
        strcpy( token, "<NULL>" );
      }
      Info_SetValueForKey( info, key, token );
    }
    //NOTE: extra space for arena number
    infos[count] = BG_Alloc(strlen(info) + strlen("\\num\\") + strlen(va("%d", MAX_ARENAS)) + 1);
    if (infos[count]) {
      strcpy(infos[count], info);
      count++;
    }
  }
  return count;
}

/*
===============
G_LoadBotsFromFile
===============
*/
static void G_LoadBotsFromFile( char *filename ) {
  int       len;
  fileHandle_t  f;
  char      buf[MAX_BOTS_TEXT];

  len = trap_FS_FOpenFile( filename, &f, FS_READ );
  if ( !f ) {
    G_Printf( S_COLOR_RED "file not found: %s\n", filename );
    return;
  }
  if ( len >= MAX_BOTS_TEXT ) {
    G_Printf( S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_BOTS_TEXT );
    trap_FS_FCloseFile( f );
    return;
  }

  trap_FS_Read( buf, len, f );
  buf[len] = 0;
  trap_FS_FCloseFile( f );

  g_numBots += G_ParseInfos( buf, MAX_BOTS - g_numBots, &g_botInfos[g_numBots] );
}

/*
===============
G_LoadBots
===============
*/
static void G_LoadBots( void ) {
  vmCvar_t  botsFile;
  int     numdirs;
  char    filename[128];
  char    dirlist[1024];
  char*   dirptr;
  int     i;
  int     dirlen;

  if ( !trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
    return;
  }

  g_numBots = 0;

  trap_Cvar_Register( &botsFile, "g_botsFile", "", CVAR_INIT|CVAR_ROM );
  if( *botsFile.string ) {
    G_LoadBotsFromFile(botsFile.string);
  }
  else {
    G_LoadBotsFromFile("scripts/bots.txt");
  }

  // get all bots from .bot files
  numdirs = trap_FS_GetFileList("scripts", ".bot", dirlist, 1024 );
  dirptr  = dirlist;
  for (i = 0; i < numdirs; i++, dirptr += dirlen+1) {
    dirlen = strlen(dirptr);
    strcpy(filename, "scripts/");
    strcat(filename, dirptr);
    G_LoadBotsFromFile(filename);
  }
  G_Printf( "%i bots parsed\n", g_numBots );
}



/*
===============
G_GetBotInfoByNumber
===============
*/
char *G_GetBotInfoByNumber( int num ) {
  if( num < 0 || num >= g_numBots ) {
    G_Printf( S_COLOR_RED "Invalid bot number: %i\n", num );
    return NULL;
  }
  return g_botInfos[num];
}


/*
===============
G_GetBotInfoByName
===============
*/
char *G_GetBotInfoByName( const char *name ) {
  int   n;
  char  *value;

  G_Printf(" Searching %i bot infos for '%s'...\n", g_numBots, name);
  for ( n = 0; n < g_numBots ; n++ ) {
    value = Info_ValueForKey( g_botInfos[n], "name" );
    if ( !Q_stricmp( value, name ) ) {
      return g_botInfos[n];
    }
  }

  return NULL;
}

/*
===============
G_InitBots
===============
*/
void G_InitBots( qboolean restart ) {
  G_Printf("------------- Loading Bots --------------\n");
  G_LoadBots();
  G_Printf("-----------------------------------------\n");
  //G_LoadArenas();
}

void G_BotAdd( gentity_t *ent, char *name, int team, float *skill, char *botinfo ) {
  int botsuffixnumber;
  int clientNum;
  //char      *botinfo;
  char userinfo[MAX_INFO_STRING];
  char err[ MAX_STRING_CHARS ];
  char newname[ MAX_NAME_LENGTH ];
  int reservedSlots = 0;
  char  *teamstring, *teamstr;
  gentity_t *bot;
  
  if(bot_developer.integer)
    G_Printf("Trying to add bot '%s'...\n", name);
  reservedSlots = trap_Cvar_VariableIntegerValue( "sv_privateclients" );
  /*
  // get the botinfo from bots.txt
  botinfo = G_GetBotInfoByName( name );
  if ( !botinfo ) {
    G_Printf( S_COLOR_RED "Error: Bot '%s' not defined\n", name );
    return;
  }*/
  
  // find what clientNum to use for bot
  clientNum = trap_BotAllocateClient();
  if ( clientNum == -1 ) {
    G_Printf( S_COLOR_RED "Unable to add bot.  All player slots are in use.\n" );
    G_Printf( S_COLOR_RED "Start server with more 'open' slots (or check setting of sv_maxclients cvar).\n" );
    return;
  }
  newname[0] = '\0';
  switch(team)
  {
    case TEAM_ALIENS:
      teamstring = "^1Alien team^7";      
      teamstr = "aliens";
      Q_strcat( newname, sizeof(newname), "^1[BOT]^7");
      Q_strcat( newname, MAX_NAME_LENGTH, name );
      break;
    case TEAM_HUMANS:
      teamstring = "^4Human team";
      teamstr = "humans";
      Q_strcat( newname, sizeof(newname), "^4[BOT]^7");
      Q_strcat( newname, MAX_NAME_LENGTH, name );
      break;
    default:
      return;
  }
  //now make sure that we can add bots of the same name, but just incremented
  //numerically. We'll now use name as a temp buffer, since we have the
  //real name in newname.
  botsuffixnumber = 1;
    if (!G_admin_name_check( NULL, newname, err, sizeof( err ) )){
      while( botsuffixnumber < MAX_NUMBER_BOTS )
        {
          strcpy( name, va( "%s%d", newname, botsuffixnumber ) );
          if ( G_admin_name_check( NULL, name, err, sizeof( err ) ) )
          {
            strcpy( newname, name );
              break;
          }
          botsuffixnumber++; // Only increments if the last requested name was used.
        }
  }
  
  bot = &g_entities[ clientNum ];
  bot->r.svFlags |= SVF_BOT;
  bot->inuse = qtrue;
  
  // register user information
  userinfo[0] = '\0';
  Info_SetValueForKey( userinfo, "characterfile", Info_ValueForKey( botinfo, "aifile" ) );
  Info_SetValueForKey( userinfo, "name", newname );
  Info_SetValueForKey( userinfo, "rate", "25000" );
  Info_SetValueForKey( userinfo, "snaps", "20" );
  Info_SetValueForKey( userinfo, "skill", va("%f", *skill ) );
  Info_SetValueForKey( userinfo, "teamstr", teamstr );

  trap_SetUserinfo( clientNum, userinfo );

  // have it connect to the game as a normal client
  if(ClientConnect(clientNum, qtrue, qtrue) != NULL ) {
    // won't let us join
    
    return;
  }
  if( team == TEAM_HUMANS)
    AP( va( "print \"^3!bot add: ^7%s^7 added bot: %s^7 to the %s^7 with character: %s and with skill level: %.0f\n\"", ( ent ) ? ent->client->pers.netname : "The Console" , bot->client->pers.netname, teamstring,Info_ValueForKey( botinfo, "name" ) , *skill ));
  else {
    AP( va( "print \"^3!bot add: ^7%s^7 added bot: %s^7 to the %s^7\n\"", ( ent ) ? ent->client->pers.netname : "The Console" , bot->client->pers.netname, teamstring));
  }
  BotBegin( clientNum );
  G_ChangeTeam( bot, team );
  level.numBots++;
}
/*
===============
G_ListChars
===============
*/
qboolean G_ListChars(gentity_t *ent, int skiparg) {
  int   i, handle;
  char  charname[ MAX_NAME_LENGTH ];
  char  *botinfo;
  char  skill[4];
  float   skill_float=2.0;
  ADMBP_begin();
  if( G_SayArgc() < 2 + skiparg ){
    ADMBP( va("^3!listchars: ^7 List of bot characters\n") );
    for ( i = 0; i < g_numBots ; i++ ) {
      ADMBP( va( "%s \n", Info_ValueForKey( g_botInfos[i], "name" ) ) );
      //ADMBP( va( "%s \n", g_botInfos[i] ) );
    }
    ADMBP( va("To get information on a specific character type: \n^3!listchars [name] (skill) ^7\n") );
  }
  else{
    G_SayArgv( 1 + skiparg, charname, sizeof( charname ) );
    if(G_SayArgc() < 3 + skiparg) {
      skill_float = 2;
    } else {
      G_SayArgv( 2 + skiparg, skill, sizeof( skill ) );
      skill_float = atof(skill);
    }
    if(skill_float > 5.0 || skill_float < 1.0){
      ADMP( "^3!listchars: ^7bot skill level must be between 1 and 5\n" );
      return qfalse;
    }
    botinfo = G_GetBotInfoByName( charname );
    if ( !botinfo ) {
      ADMP( va( S_COLOR_RED "Error: Bot character '%s' not defined\n", charname ) );
      return qfalse;
    }
    ADMBP( va("^3!listchars: ^7 character info for '%s' with skill level: %.1f \n", charname, skill_float ) );
    handle = trap_BotLoadCharacter (Info_ValueForKey( botinfo, "aifile" ), skill_float);
    ADMBP( va("^3 Aim skill:^7 %.2f \n", trap_Characteristic_BFloat(handle, CHARACTERISTIC_AIM_SKILL, 0, 1) ) );
    ADMBP( va("^3 Aim Accuracy:^7 %.2f \n", trap_Characteristic_BFloat(handle, CHARACTERISTIC_AIM_ACCURACY, 0, 1) ) );
    ADMBP( va("^3 Attack skill:^7 %.2f \n", trap_Characteristic_BFloat(handle, CHARACTERISTIC_ATTACK_SKILL, 0, 1) ) );
    ADMBP( va("^3 Reaction time:^7 %.2fs \n", trap_Characteristic_BFloat(handle, CHARACTERISTIC_REACTIONTIME, 0, 1) ) );
    //ADMBP( va("^3 Reaction time:^7 %.2fs \n", trap_Characteristic_BFloat(handle, CHARACTERISTIC_REACTIONTIME, 0, 1) ) );
    trap_BotFreeCharacter(handle);
  }
  ADMBP_end();
   //Info_ValueForKey( g_botInfos[n], "name" );
  return qtrue;
}
