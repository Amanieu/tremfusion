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

// cg_main.c -- initialization and primary entry point for cgame


#include "cg_local.h"

#include "../ui/ui_shared.h"
// display context for new ui stuff
displayContextDef_t cgDC;

void CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum );
void CG_Shutdown( void );

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
intptr_t vmMain( int command, int arg0, int arg1, int arg2, int arg3,
                              int arg4, int arg5, int arg6, int arg7,
                              int arg8, int arg9, int arg10, int arg11 )
{
  switch( command )
  {
    case CG_INIT:
      CG_Init( arg0, arg1, arg2 );
      return 0;

    case CG_SHUTDOWN:
      CG_Shutdown( );
      return 0;

    case CG_CONSOLE_COMMAND:
      return CG_ConsoleCommand( );

    case CG_CONSOLE_TEXT:
      CG_AddNotifyText( );
      return 0;

    case CG_DRAW_ACTIVE_FRAME:
      CG_DrawActiveFrame( arg0, arg1, arg2 );
      return 0;

    case CG_CROSSHAIR_PLAYER:
      return CG_CrosshairPlayer( );

    case CG_LAST_ATTACKER:
      return CG_LastAttacker( );

    case CG_KEY_EVENT:
      CG_KeyEvent( arg0, arg1 );
      return 0;

    case CG_MOUSE_EVENT:
      CG_MouseEvent( arg0, arg1 );
      return 0;

    case CG_EVENT_HANDLING:
      CG_EventHandling( arg0 );
      return 0;

    default:
      CG_Error( "vmMain: unknown command %i", command );
      break;
  }

  return -1;
}


cg_t        cg;
cgs_t       cgs;
centity_t   cg_entities[ MAX_GENTITIES ];

weaponInfo_t    cg_weapons[ 32 ];
upgradeInfo_t   cg_upgrades[ 32 ];

buildableInfo_t cg_buildables[ BA_NUM_BUILDABLES ];

vmCvar_t  cg_version;
vmCvar_t  cg_teslaTrailTime;
vmCvar_t  cg_centertime;
vmCvar_t  cg_runpitch;
vmCvar_t  cg_runroll;
vmCvar_t  cg_swingSpeed;
vmCvar_t  cg_shadows;
vmCvar_t  cg_drawTimer;
vmCvar_t  cg_drawClock;
vmCvar_t  cg_drawFPS;
vmCvar_t  cg_drawSpeed;
vmCvar_t  cg_drawDemoState;
vmCvar_t  cg_drawSnapshot;
vmCvar_t  cg_drawChargeBar;
vmCvar_t  cg_drawCrosshair;
vmCvar_t  cg_drawCrosshairNames;
vmCvar_t  cg_crosshairSize;
vmCvar_t  cg_draw2D;
vmCvar_t  cg_drawStatus;
vmCvar_t  cg_animSpeed;
vmCvar_t  cg_debugAnim;
vmCvar_t  cg_debugPosition;
vmCvar_t  cg_debugEvents;
vmCvar_t  cg_errorDecay;
vmCvar_t  cg_nopredict;
vmCvar_t  cg_debugMove;
vmCvar_t  cg_noPlayerAnims;
vmCvar_t  cg_showmiss;
vmCvar_t  cg_footsteps;
vmCvar_t  cg_addMarks;
vmCvar_t  cg_brassTime;
vmCvar_t  cg_viewsize;
vmCvar_t  cg_drawGun;
vmCvar_t  cg_gun_frame;
vmCvar_t  cg_gun_x;
vmCvar_t  cg_gun_y;
vmCvar_t  cg_gun_z;
vmCvar_t  cg_tracerChance;
vmCvar_t  cg_tracerWidth;
vmCvar_t  cg_tracerLength;
vmCvar_t  cg_autoswitch;
vmCvar_t  cg_thirdPerson;
vmCvar_t  cg_thirdPersonRange;
vmCvar_t  cg_thirdPersonAngle;
vmCvar_t  cg_stereoSeparation;
vmCvar_t  cg_lagometer;
vmCvar_t  cg_synchronousClients;
vmCvar_t  cg_stats;
vmCvar_t  cg_paused;
vmCvar_t  cg_blood;
vmCvar_t  cg_drawFriend;
vmCvar_t  cg_teamChatsOnly;
vmCvar_t  cg_noPrintDuplicate;
vmCvar_t  cg_noVoiceChats;
vmCvar_t  cg_noVoiceText;
vmCvar_t  cg_hudFiles;
vmCvar_t  cg_hudFilesEnable;
vmCvar_t  cg_smoothClients;
vmCvar_t  pmove_fixed;
vmCvar_t  pmove_msec;
vmCvar_t  cg_cameraMode;
vmCvar_t  cg_timescaleFadeEnd;
vmCvar_t  cg_timescaleFadeSpeed;
vmCvar_t  cg_timescale;
vmCvar_t  cg_noTaunt;
vmCvar_t  cg_drawSurfNormal;
vmCvar_t  cg_drawBBOX;
vmCvar_t  cg_wwSmoothTime;
vmCvar_t  cg_wwFollow;
vmCvar_t  cg_wwToggle;
vmCvar_t  cg_depthSortParticles;
vmCvar_t  cg_bounceParticles;
vmCvar_t  cg_consoleLatency;
vmCvar_t  cg_lightFlare;
vmCvar_t  cg_debugParticles;
vmCvar_t  cg_debugTrails;
vmCvar_t  cg_debugPVS;
vmCvar_t  cg_disableWarningDialogs;
vmCvar_t  cg_disableUpgradeDialogs;
vmCvar_t  cg_disableBuildDialogs;
vmCvar_t  cg_disableCommandDialogs;
vmCvar_t  cg_disableScannerPlane;
vmCvar_t  cg_tutorial;

vmCvar_t  cg_painBlendUpRate;
vmCvar_t  cg_painBlendDownRate;
vmCvar_t  cg_painBlendMax;
vmCvar_t  cg_painBlendScale;
vmCvar_t  cg_painBlendZoom;

vmCvar_t  cg_debugVoices;

vmCvar_t  cg_stickySpec;
vmCvar_t  cg_alwaysSprint;
vmCvar_t  cg_unlagged;

vmCvar_t  ui_currentClass;
vmCvar_t  ui_carriage;
vmCvar_t  ui_stages;
vmCvar_t  ui_dialog;
vmCvar_t  ui_voteActive;
vmCvar_t  ui_alienTeamVoteActive;
vmCvar_t  ui_humanTeamVoteActive;

vmCvar_t  cg_debugRandom;

vmCvar_t  cg_optimizePrediction;
vmCvar_t  cg_projectileNudge;

vmCvar_t  cg_suppressWAnimWarnings;

vmCvar_t  cg_voice;
vmCvar_t  cg_emoticons;
vmCvar_t  cg_drawAlienFeedback;


typedef struct
{
  vmCvar_t  *vmCvar;
  char      *cvarName;
  char      *defaultString;
  int       cvarFlags;
} cvarTable_t;

static cvarTable_t cvarTable[ ] =
{
  { &cg_version, "cg_version", PRODUCT_NAME, CVAR_ROM | CVAR_USERINFO },
  { &cg_autoswitch, "cg_autoswitch", "1", CVAR_ARCHIVE },
  { &cg_drawGun, "cg_drawGun", "1", CVAR_ARCHIVE },
  { &cg_viewsize, "cg_viewsize", "100", CVAR_ARCHIVE },
  { &cg_stereoSeparation, "cg_stereoSeparation", "0.4", CVAR_ARCHIVE  },
  { &cg_shadows, "cg_shadows", "1", CVAR_ARCHIVE  },
  { &cg_draw2D, "cg_draw2D", "1", CVAR_ARCHIVE  },
  { &cg_drawStatus, "cg_drawStatus", "1", CVAR_ARCHIVE  },
  { &cg_drawTimer, "cg_drawTimer", "1", CVAR_ARCHIVE  },
  { &cg_drawClock, "cg_drawClock", "1", CVAR_ARCHIVE  },
  { &cg_drawFPS, "cg_drawFPS", "1", CVAR_ARCHIVE  },
  { &cg_drawSpeed, "cg_drawSpeed", "0", CVAR_ARCHIVE  },
  { &cg_drawDemoState, "cg_drawDemoState", "1", CVAR_ARCHIVE  },
  { &cg_drawSnapshot, "cg_drawSnapshot", "0", CVAR_ARCHIVE  },
  { &cg_drawChargeBar, "cg_drawChargeBar", "1", CVAR_ARCHIVE  },
  { &cg_drawCrosshair, "cg_drawCrosshair", "1", CVAR_ARCHIVE },
  { &cg_drawCrosshairNames, "cg_drawCrosshairNames", "1", CVAR_ARCHIVE },
  { &cg_crosshairSize, "cg_crosshairSize", "1", CVAR_ARCHIVE },
  { &cg_brassTime, "cg_brassTime", "2500", CVAR_ARCHIVE },
  { &cg_addMarks, "cg_marks", "1", CVAR_ARCHIVE },
  { &cg_lagometer, "cg_lagometer", "1", CVAR_ARCHIVE },
  { &cg_teslaTrailTime, "cg_teslaTrailTime", "250", CVAR_ARCHIVE  },
  { &cg_gun_x, "cg_gunX", "0", CVAR_CHEAT },
  { &cg_gun_y, "cg_gunY", "0", CVAR_CHEAT },
  { &cg_gun_z, "cg_gunZ", "0", CVAR_CHEAT },
  { &cg_centertime, "cg_centertime", "3", CVAR_CHEAT },
  { &cg_runpitch, "cg_runpitch", "0.002", CVAR_ARCHIVE},
  { &cg_runroll, "cg_runroll", "0.005", CVAR_ARCHIVE },
  { &cg_swingSpeed, "cg_swingSpeed", "0.3", CVAR_CHEAT },
  { &cg_animSpeed, "cg_animspeed", "1", CVAR_CHEAT },
  { &cg_debugAnim, "cg_debuganim", "0", CVAR_CHEAT },
  { &cg_debugPosition, "cg_debugposition", "0", CVAR_CHEAT },
  { &cg_debugEvents, "cg_debugevents", "0", CVAR_CHEAT },
  { &cg_errorDecay, "cg_errordecay", "100", 0 },
  { &cg_nopredict, "cg_nopredict", "0", 0 },
  { &cg_debugMove, "cg_debugMove", "0", 0 },
  { &cg_noPlayerAnims, "cg_noplayeranims", "0", CVAR_CHEAT },
  { &cg_showmiss, "cg_showmiss", "0", 0 },
  { &cg_footsteps, "cg_footsteps", "1", CVAR_CHEAT },
  { &cg_tracerChance, "cg_tracerchance", "0.4", CVAR_CHEAT },
  { &cg_tracerWidth, "cg_tracerwidth", "1", CVAR_CHEAT },
  { &cg_tracerLength, "cg_tracerlength", "100", CVAR_CHEAT },
  { &cg_thirdPersonRange, "cg_thirdPersonRange", "40", CVAR_CHEAT },
  { &cg_thirdPersonAngle, "cg_thirdPersonAngle", "0", CVAR_CHEAT },
  { &cg_thirdPerson, "cg_thirdPerson", "0", CVAR_CHEAT },
  { &cg_stats, "cg_stats", "0", 0 },
  { &cg_drawFriend, "cg_drawFriend", "1", CVAR_ARCHIVE },
  { &cg_teamChatsOnly, "cg_teamChatsOnly", "0", CVAR_ARCHIVE },
  { &cg_noPrintDuplicate, "cg_noPrintDuplicate", "0", CVAR_ARCHIVE },
  { &cg_noVoiceChats, "cg_noVoiceChats", "0", CVAR_ARCHIVE },
  { &cg_noVoiceText, "cg_noVoiceText", "0", CVAR_ARCHIVE },
  { &cg_drawSurfNormal, "cg_drawSurfNormal", "0", CVAR_CHEAT },
  { &cg_drawBBOX, "cg_drawBBOX", "0", CVAR_CHEAT },
  { &cg_wwSmoothTime, "cg_wwSmoothTime", "300", CVAR_ARCHIVE },
  { &cg_wwFollow, "cg_wwFollow", "1", CVAR_ARCHIVE|CVAR_USERINFO },
  { &cg_wwToggle, "cg_wwToggle", "1", CVAR_ARCHIVE|CVAR_USERINFO },
  { &cg_stickySpec, "cg_stickySpec", "1", CVAR_ARCHIVE|CVAR_USERINFO },
  { &cg_alwaysSprint, "cg_alwaysSprint", "0", CVAR_ARCHIVE|CVAR_USERINFO },
  { &cg_unlagged, "cg_unlagged", "1", CVAR_ARCHIVE|CVAR_USERINFO },
  { &cg_depthSortParticles, "cg_depthSortParticles", "1", CVAR_ARCHIVE },
  { &cg_bounceParticles, "cg_bounceParticles", "0", CVAR_ARCHIVE },
  { &cg_consoleLatency, "cg_consoleLatency", "3000", CVAR_ARCHIVE },
  { &cg_lightFlare, "cg_lightFlare", "3", CVAR_ARCHIVE },
  { &cg_debugParticles, "cg_debugParticles", "0", CVAR_CHEAT },
  { &cg_debugTrails, "cg_debugTrails", "0", CVAR_CHEAT },
  { &cg_debugPVS, "cg_debugPVS", "0", CVAR_CHEAT },
  { &cg_disableWarningDialogs, "cg_disableWarningDialogs", "0", CVAR_ARCHIVE },
  { &cg_disableUpgradeDialogs, "cg_disableUpgradeDialogs", "0", CVAR_ARCHIVE },
  { &cg_disableBuildDialogs, "cg_disableBuildDialogs", "0", CVAR_ARCHIVE },
  { &cg_disableCommandDialogs, "cg_disableCommandDialogs", "0", CVAR_ARCHIVE },
  { &cg_disableScannerPlane, "cg_disableScannerPlane", "0", CVAR_ARCHIVE },
  { &cg_tutorial, "cg_tutorial", "1", CVAR_ARCHIVE },
  { &cg_hudFiles, "cg_hudFiles", "ui/hud.txt", CVAR_ARCHIVE},
  { &cg_hudFilesEnable, "cg_hudFilesEnable", "0", CVAR_ARCHIVE},

  { &cg_painBlendUpRate, "cg_painBlendUpRate", "10.0", 0 },
  { &cg_painBlendDownRate, "cg_painBlendDownRate", "0.5", 0 },
  { &cg_painBlendMax, "cg_painBlendMax", "0.7", 0 },
  { &cg_painBlendScale, "cg_painBlendScale", "7.0", 0 },
  { &cg_painBlendZoom, "cg_painBlendZoom", "0.65", 0 },

  { &cg_debugVoices, "cg_debugVoices", "0", 0 },
  
  { &ui_currentClass, "ui_currentClass", "0", 0 },
  { &ui_carriage, "ui_carriage", "", 0 },
  { &ui_stages, "ui_stages", "0 0", 0 },
  { &ui_dialog, "ui_dialog", "Text not set", 0 },
  { &ui_voteActive, "ui_voteActive", "0", 0 },
  { &ui_humanTeamVoteActive, "ui_humanTeamVoteActive", "0", 0 },
  { &ui_alienTeamVoteActive, "ui_alienTeamVoteActive", "0", 0 },

  { &cg_debugRandom, "cg_debugRandom", "0", 0 },
  
  { &cg_optimizePrediction, "cg_optimizePrediction", "1", CVAR_ARCHIVE },
  { &cg_projectileNudge, "cg_projectileNudge", "1", CVAR_ARCHIVE },

  // the following variables are created in other parts of the system,
  // but we also reference them here

  { &cg_paused, "cl_paused", "0", CVAR_ROM },
  { &cg_blood, "com_blood", "1", CVAR_ARCHIVE },
  { &cg_synchronousClients, "g_synchronousClients", "0", 0 }, // communicated by systeminfo
  { &cg_timescaleFadeEnd, "cg_timescaleFadeEnd", "1", 0},
  { &cg_timescaleFadeSpeed, "cg_timescaleFadeSpeed", "0", 0},
  { &cg_timescale, "timescale", "1", 0},
  { &cg_smoothClients, "cg_smoothClients", "0", CVAR_USERINFO | CVAR_ARCHIVE},
  { &cg_cameraMode, "com_cameraMode", "0", CVAR_CHEAT},

  { &pmove_fixed, "pmove_fixed", "0", 0},
  { &pmove_msec, "pmove_msec", "8", 0},
  { &cg_noTaunt, "cg_noTaunt", "0", CVAR_ARCHIVE},
  
  { &cg_suppressWAnimWarnings, "cg_suppressWAnimWarnings", "1", CVAR_ARCHIVE},

  { &cg_voice, "voice", "default", CVAR_USERINFO|CVAR_ARCHIVE},

  { &cg_emoticons, "cg_emoticons", "1", CVAR_LATCH|CVAR_ARCHIVE},
  { &cg_drawAlienFeedback, "cg_drawAlienFeedback", "1", 0}
};

static int   cvarTableSize = sizeof( cvarTable ) / sizeof( cvarTable[0] );

/*
=================
CG_RegisterCvars
=================
*/
void CG_RegisterCvars( void )
{
  int         i;
  cvarTable_t *cv;
  char        var[ MAX_TOKEN_CHARS ];

  for( i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++ )
  {
    trap_Cvar_Register( cv->vmCvar, cv->cvarName,
      cv->defaultString, cv->cvarFlags );
  }

  //repress standard Q3 console
  trap_Cvar_Set( "con_notifytime", "-2" );

  // see if we are also running the server on this machine
  trap_Cvar_VariableStringBuffer( "sv_running", var, sizeof( var ) );
  cgs.localServer = atoi( var );
  
  // override any existing version cvar
  trap_Cvar_Set( "cg_version", PRODUCT_NAME );
}


/*
===============
CG_SetUIVars

Set some cvars used by the UI
===============
*/
static void CG_SetUIVars( void )
{
  int   i, upgrades = 0;

  if( !cg.snap )
    return;

  //determine what the player is carrying
  for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
  {
    if( BG_InventoryContainsUpgrade( i, cg.snap->ps.stats ) &&
        BG_Upgrade( i )->purchasable )
      upgrades |= ( 1 << i );
  }

  trap_Cvar_Set( "ui_carriage", va( "%d %d %d", cg.snap->ps.stats[ STAT_WEAPON ],
                 upgrades, cg.snap->ps.persistant[ PERS_CREDIT ] ) );

  trap_Cvar_Set( "ui_stages", va( "%d %d", cgs.alienStage, cgs.humanStage ) );
}

/*
===============
CG_SetPVars

Set the p_* cvars
===============
*/
static void CG_SetPVars( void )
{
  playerState_t *ps;

  if( !cg.snap )
    return;

  ps = &cg.snap->ps;

  trap_Cvar_Set( "p_hp", va( "%d", ps->stats[ STAT_HEALTH ] ) );
  trap_Cvar_Set( "p_maxhp", va( "%d", ps->stats[ STAT_MAX_HEALTH ] ) );
  trap_Cvar_Set( "p_team", va( "%d", ps->stats[ STAT_TEAM ] ) );
  switch( ps->stats[ STAT_TEAM ] )
  {
  case TEAM_NONE:
    trap_Cvar_Set( "p_teamname", "^3Spectator" );
  case TEAM_ALIENS:
    trap_Cvar_Set( "p_teamname", "^1Alien" );
  case TEAM_HUMANS:
    trap_Cvar_Set( "p_teamname", "^4Human" );
  }
  trap_Cvar_Set( "p_class", va( "%d", ps->stats[ STAT_CLASS ] ) );
  trap_Cvar_Set( "p_classname", BG_ClassConfig( ps->stats[ STAT_CLASS ] )->humanName );
  trap_Cvar_Set( "p_weapon", va( "%d", ps->stats[ STAT_WEAPON ] ) );
  trap_Cvar_Set( "p_weaponname", BG_Weapon( ps->stats[ STAT_WEAPON ] )->humanName );
  trap_Cvar_Set( "p_ammo", va( "%d", ps->ammo ) );
  trap_Cvar_Set( "p_clips", va( "%d", ps->clips ) );
  trap_Cvar_Set( "p_credits", va( "%d", ps->persistant[ PERS_CREDIT ] ) );
  trap_Cvar_Set( "p_score", va( "%d", ps->persistant[ PERS_SCORE ] ) );
  trap_Cvar_Set( "p_attacker", va( "%d", CG_LastAttacker( ) ) );
  trap_Cvar_Set( "p_attackername", cgs.clientinfo[ CG_LastAttacker( ) ].name );
  trap_Cvar_Set( "p_crosshair", va( "%d", CG_CrosshairPlayer( ) ) );
  trap_Cvar_Set( "p_crosshairrname", cgs.clientinfo[ CG_CrosshairPlayer( ) ].name );
}

/*
=================
CG_UpdateCvars
=================
*/
void CG_UpdateCvars( void )
{
  int         i;
  cvarTable_t *cv;

  for( i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++ )
    trap_Cvar_Update( cv->vmCvar );

  // check for modications here

  CG_SetUIVars( );
  CG_SetPVars( );
}


int CG_CrosshairPlayer( void )
{
  if( cg.time > ( cg.crosshairClientTime + 1000 ) )
    return -1;

  return cg.crosshairClientNum;
}


int CG_LastAttacker( void )
{
  if( !cg.attackerTime )
    return -1;

  return cg.snap->ps.persistant[ PERS_ATTACKER ];
}


/*
=================
CG_RemoveNotifyLine
=================
*/
void CG_RemoveNotifyLine( void )
{
  int i, offset, totalLength;

  if( cg.numConsoleLines == 0 )
    return;

  offset = cg.consoleLines[ 0 ].length;
  totalLength = strlen( cg.consoleText ) - offset;

  //slide up consoleText
  for( i = 0; i <= totalLength; i++ )
    cg.consoleText[ i ] = cg.consoleText[ i + offset ];

  //pop up the first consoleLine
  for( i = 0; i < cg.numConsoleLines; i++ )
    cg.consoleLines[ i ] = cg.consoleLines[ i + 1 ];

  cg.numConsoleLines--;
}

/*
=================
CG_AddNotifyText
=================
*/
void CG_AddNotifyText( void )
{
  char buffer[ BIG_INFO_STRING ];
  int bufferLen, textLen;

  trap_LiteralArgs( buffer, BIG_INFO_STRING );

  if( !buffer[ 0 ] )
  {
    cg.consoleText[ 0 ] = '\0';
    cg.numConsoleLines = 0;
    return;
  }

  bufferLen = strlen( buffer );
  textLen = strlen( cg.consoleText );
  
  // Ignore console messages that were just printed
  if( cg_noPrintDuplicate.integer && textLen >= bufferLen &&
      !strcmp( cg.consoleText + textLen - bufferLen, buffer ) )
    return;
      
  if( cg.numConsoleLines == MAX_CONSOLE_LINES )
    CG_RemoveNotifyLine( );

  Q_strcat( cg.consoleText, MAX_CONSOLE_TEXT, buffer );
  cg.consoleLines[ cg.numConsoleLines ].time = cg.time;
  cg.consoleLines[ cg.numConsoleLines ].length = bufferLen;
  cg.numConsoleLines++;
}

void QDECL CG_Printf( const char *msg, ... )
{
  va_list argptr;
  char    text[ 1024 ];

  va_start( argptr, msg );
  Q_vsnprintf( text, sizeof( text ), msg, argptr );
  va_end( argptr );

  trap_Print( text );
}

void QDECL CG_Error( const char *msg, ... )
{
  va_list argptr;
  char    text[ 1024 ];

  va_start( argptr, msg );
  Q_vsnprintf( text, sizeof( text ), msg, argptr );
  va_end( argptr );

  trap_Error( text );
}

void QDECL Com_Error( int level, const char *error, ... )
{
  va_list argptr;
  char    text[1024];

  va_start( argptr, error );
  Q_vsnprintf( text, sizeof( text ), error, argptr );
  va_end( argptr );

  CG_Error( "%s", text );
}

void QDECL Com_Printf( const char *msg, ... ) {
  va_list   argptr;
  char    text[1024];

  va_start( argptr, msg );
  Q_vsnprintf( text, sizeof( text ), msg, argptr );
  va_end( argptr );

  CG_Printf ("%s", text);
}



/*
================
CG_Argv
================
*/
const char *CG_Argv( int arg )
{
  static char buffer[ MAX_STRING_CHARS ];

  trap_Argv( arg, buffer, sizeof( buffer ) );

  return buffer;
}


//========================================================================

/*
=================
CG_FileExists

Test if a specific file exists or not
=================
*/
qboolean CG_FileExists( char *filename )
{
  return trap_FS_FOpenFile( filename, NULL, FS_READ );
}

/*
=================
CG_RegisterSounds

called during a precache command
=================
*/
static void CG_RegisterSounds( void )
{
  int         i;
  char        name[ MAX_QPATH ];
  const char  *soundName;

  cgs.media.alienStageTransition  = trap_S_RegisterSound( "sound/announcements/overmindevolved.wav", qtrue );
  cgs.media.humanStageTransition  = trap_S_RegisterSound( "sound/announcements/reinforcement.wav", qtrue );

  cgs.media.alienOvermindAttack   = trap_S_RegisterSound( "sound/announcements/overmindattack.wav", qtrue );
  cgs.media.alienOvermindDying    = trap_S_RegisterSound( "sound/announcements/overminddying.wav", qtrue );
  cgs.media.alienOvermindSpawns   = trap_S_RegisterSound( "sound/announcements/overmindspawns.wav", qtrue );

  cgs.media.alienL1Grab           = trap_S_RegisterSound( "sound/player/level1/grab.wav", qtrue );
  cgs.media.alienL4ChargePrepare  = trap_S_RegisterSound( "sound/player/level4/charge_prepare.wav", qtrue );
  cgs.media.alienL4ChargeStart    = trap_S_RegisterSound( "sound/player/level4/charge_start.wav", qtrue );

  cgs.media.tracerSound           = trap_S_RegisterSound( "sound/weapons/tracer.wav", qfalse );
  cgs.media.selectSound           = trap_S_RegisterSound( "sound/weapons/change.wav", qfalse );
  cgs.media.turretSpinupSound     = trap_S_RegisterSound( "sound/buildables/mgturret/spinup.wav", qfalse );
  cgs.media.weaponEmptyClick      = trap_S_RegisterSound( "sound/weapons/click.wav", qfalse );

  cgs.media.talkSound             = trap_S_RegisterSound( "sound/misc/talk.wav", qfalse );
  cgs.media.alienTalkSound        = trap_S_RegisterSound( "sound/misc/alien_talk.wav", qfalse );
  cgs.media.humanTalkSound        = trap_S_RegisterSound( "sound/misc/human_talk.wav", qfalse );
  cgs.media.landSound             = trap_S_RegisterSound( "sound/player/land1.wav", qfalse );

  cgs.media.watrInSound           = trap_S_RegisterSound( "sound/player/watr_in.wav", qfalse );
  cgs.media.watrOutSound          = trap_S_RegisterSound( "sound/player/watr_out.wav", qfalse );
  cgs.media.watrUnSound           = trap_S_RegisterSound( "sound/player/watr_un.wav", qfalse );

  cgs.media.disconnectSound       = trap_S_RegisterSound( "sound/misc/disconnect.wav", qfalse );

  for( i = 0; i < 4; i++ )
  {
    Com_sprintf( name, sizeof( name ), "sound/player/footsteps/step%i.wav", i + 1 );
    cgs.media.footsteps[ FOOTSTEP_NORMAL ][ i ] = trap_S_RegisterSound( name, qfalse );

    Com_sprintf( name, sizeof( name ), "sound/player/footsteps/flesh%i.wav", i + 1 );
    cgs.media.footsteps[ FOOTSTEP_FLESH ][ i ] = trap_S_RegisterSound( name, qfalse );

    Com_sprintf( name, sizeof( name ), "sound/player/footsteps/splash%i.wav", i + 1 );
    cgs.media.footsteps[ FOOTSTEP_SPLASH ][ i ] = trap_S_RegisterSound( name, qfalse );

    Com_sprintf( name, sizeof( name ), "sound/player/footsteps/clank%i.wav", i + 1 );
    cgs.media.footsteps[ FOOTSTEP_METAL ][ i ] = trap_S_RegisterSound( name, qfalse );
  }

  for( i = 1 ; i < MAX_SOUNDS ; i++ )
  {
    soundName = CG_ConfigString( CS_SOUNDS + i );

    if( !soundName[ 0 ] )
      break;

    if( soundName[ 0 ] == '*' )
      continue; // custom sound

    cgs.gameSounds[ i ] = trap_S_RegisterSound( soundName, qfalse );
  }

  cgs.media.jetpackDescendSound     = trap_S_RegisterSound( "sound/upgrades/jetpack/low.wav", qfalse );
  cgs.media.jetpackIdleSound        = trap_S_RegisterSound( "sound/upgrades/jetpack/idle.wav", qfalse );
  cgs.media.jetpackAscendSound      = trap_S_RegisterSound( "sound/upgrades/jetpack/hi.wav", qfalse );

  cgs.media.medkitUseSound          = trap_S_RegisterSound( "sound/upgrades/medkit/medkit.wav", qfalse );

  cgs.media.alienEvolveSound        = trap_S_RegisterSound( "sound/player/alienevolve.wav", qfalse );

  cgs.media.alienBuildableExplosion = trap_S_RegisterSound( "sound/buildables/alien/explosion.wav", qfalse );
  cgs.media.alienBuildableDamage    = trap_S_RegisterSound( "sound/buildables/alien/damage.wav", qfalse );
  cgs.media.alienBuildablePrebuild  = trap_S_RegisterSound( "sound/buildables/alien/prebuild.wav", qfalse );

  cgs.media.humanBuildableExplosion = trap_S_RegisterSound( "sound/buildables/human/explosion.wav", qfalse );
  cgs.media.humanBuildablePrebuild  = trap_S_RegisterSound( "sound/buildables/human/prebuild.wav", qfalse );

  for( i = 0; i < 4; i++ )
    cgs.media.humanBuildableDamage[ i ] = trap_S_RegisterSound(
        va( "sound/buildables/human/damage%d.wav", i ), qfalse );

  cgs.media.hardBounceSound1        = trap_S_RegisterSound( "sound/misc/hard_bounce1.wav", qfalse );
  cgs.media.hardBounceSound2        = trap_S_RegisterSound( "sound/misc/hard_bounce2.wav", qfalse );

  cgs.media.repeaterUseSound        = trap_S_RegisterSound( "sound/buildables/repeater/use.wav", qfalse );

  cgs.media.buildableRepairSound    = trap_S_RegisterSound( "sound/buildables/human/repair.wav", qfalse );
  cgs.media.buildableRepairedSound  = trap_S_RegisterSound( "sound/buildables/human/repaired.wav", qfalse );

  cgs.media.lCannonWarningSound     = trap_S_RegisterSound( "models/weapons/lcannon/warning.wav", qfalse );
  cgs.media.lCannonWarningSound2    = trap_S_RegisterSound( "models/weapons/lcannon/warning2.wav", qfalse );
}


//===================================================================================


/*
=================
CG_RegisterGraphics

This function may execute for a couple of minutes with a slow disk.
=================
*/
static void CG_RegisterGraphics( void )
{
  int         i;
  static char *sb_nums[ 11 ] =
  {
    "gfx/2d/numbers/zero_32b",
    "gfx/2d/numbers/one_32b",
    "gfx/2d/numbers/two_32b",
    "gfx/2d/numbers/three_32b",
    "gfx/2d/numbers/four_32b",
    "gfx/2d/numbers/five_32b",
    "gfx/2d/numbers/six_32b",
    "gfx/2d/numbers/seven_32b",
    "gfx/2d/numbers/eight_32b",
    "gfx/2d/numbers/nine_32b",
    "gfx/2d/numbers/minus_32b",
  };
  static char *buildWeaponTimerPieShaders[ 8 ] =
  {
    "ui/assets/neutral/1_5pie",
    "ui/assets/neutral/3_0pie",
    "ui/assets/neutral/4_5pie",
    "ui/assets/neutral/6_0pie",
    "ui/assets/neutral/7_5pie",
    "ui/assets/neutral/9_0pie",
    "ui/assets/neutral/10_5pie",
    "ui/assets/neutral/12_0pie",
  };
   static char *alienAttackFeedbackShaders[ 11 ] =
  {
        "ui/assets/alien/feedback/scratch_00",
        "ui/assets/alien/feedback/scratch_01",
        "ui/assets/alien/feedback/scratch_02",
        "ui/assets/alien/feedback/scratch_03",
        "ui/assets/alien/feedback/scratch_04",
        "ui/assets/alien/feedback/scratch_05",
        "ui/assets/alien/feedback/scratch_06",
        "ui/assets/alien/feedback/scratch_07",
        "ui/assets/alien/feedback/scratch_08",
        "ui/assets/alien/feedback/scratch_09",
        "ui/assets/alien/feedback/scratch_10"
  };

  // clear any references to old media
  memset( &cg.refdef, 0, sizeof( cg.refdef ) );
  trap_R_ClearScene( );

  trap_R_LoadWorldMap( cgs.mapname );
  CG_UpdateMediaFraction( 0.66f );

  for( i = 0; i < 11; i++ )
    cgs.media.numberShaders[ i ] = trap_R_RegisterShader( sb_nums[ i ] );

  cgs.media.viewBloodShader           = trap_R_RegisterShader( "gfx/damage/fullscreen_painblend" );

  cgs.media.connectionShader          = trap_R_RegisterShader( "gfx/2d/net" );

  cgs.media.creepShader               = trap_R_RegisterShader( "creep" );

  cgs.media.scannerBlipShader         = trap_R_RegisterShader( "gfx/2d/blip" );
  cgs.media.scannerLineShader         = trap_R_RegisterShader( "gfx/2d/stalk" );

  cgs.media.tracerShader              = trap_R_RegisterShader( "gfx/misc/tracer" );

  cgs.media.backTileShader            = trap_R_RegisterShader( "console" );


  // building shaders
  cgs.media.greenBuildShader          = trap_R_RegisterShader("gfx/misc/greenbuild" );
  cgs.media.redBuildShader            = trap_R_RegisterShader("gfx/misc/redbuild" );
  cgs.media.humanSpawningShader       = trap_R_RegisterShader("models/buildables/telenode/rep_cyl" );

  for( i = 0; i < 8; i++ )
    cgs.media.buildWeaponTimerPie[ i ] = trap_R_RegisterShader( buildWeaponTimerPieShaders[ i ] );
  for( i = 0; i < 11; i++ )
    cgs.media.alienAttackFeedbackShaders[i] = trap_R_RegisterShader( alienAttackFeedbackShaders[i] );

  // player health cross shaders
  cgs.media.healthCross               = trap_R_RegisterShader( "ui/assets/neutral/cross.tga" );
  cgs.media.healthCross2X             = trap_R_RegisterShader( "ui/assets/neutral/cross2.tga" );
  cgs.media.healthCross3X             = trap_R_RegisterShader( "ui/assets/neutral/cross3.tga" );
  cgs.media.healthCrossMedkit         = trap_R_RegisterShader( "ui/assets/neutral/cross_medkit.tga" );
  cgs.media.healthCrossPoisoned       = trap_R_RegisterShader( "ui/assets/neutral/cross_poison.tga" );
  
  // squad markers
  cgs.media.squadMarkerH              = trap_R_RegisterShader( "ui/assets/neutral/squad_h" );
  cgs.media.squadMarkerV              = trap_R_RegisterShader( "ui/assets/neutral/squad_v" );

  cgs.media.upgradeClassIconShader    = trap_R_RegisterShader( "icons/icona_upgrade.tga" );

  cgs.media.balloonShader             = trap_R_RegisterShader( "gfx/sprites/chatballoon" );

  cgs.media.disconnectPS              = CG_RegisterParticleSystem( "disconnectPS" );

  CG_UpdateMediaFraction( 0.7f );

  memset( cg_weapons, 0, sizeof( cg_weapons ) );
  memset( cg_upgrades, 0, sizeof( cg_upgrades ) );

  cgs.media.shadowMarkShader          = trap_R_RegisterShader( "gfx/marks/shadow" );
  cgs.media.wakeMarkShader            = trap_R_RegisterShader( "gfx/marks/wake" );

  cgs.media.poisonCloudPS             = CG_RegisterParticleSystem( "firstPersonPoisonCloudPS" );
  cgs.media.poisonCloudedPS           = CG_RegisterParticleSystem( "poisonCloudedPS" );
  cgs.media.alienEvolvePS             = CG_RegisterParticleSystem( "alienEvolvePS" );
  cgs.media.alienAcidTubePS           = CG_RegisterParticleSystem( "alienAcidTubePS" );

  cgs.media.jetPackDescendPS          = CG_RegisterParticleSystem( "jetPackDescendPS" );
  cgs.media.jetPackHoverPS            = CG_RegisterParticleSystem( "jetPackHoverPS" );
  cgs.media.jetPackAscendPS           = CG_RegisterParticleSystem( "jetPackAscendPS" );

  cgs.media.humanBuildableDamagedPS   = CG_RegisterParticleSystem( "humanBuildableDamagedPS" );
  cgs.media.alienBuildableDamagedPS   = CG_RegisterParticleSystem( "alienBuildableDamagedPS" );
  cgs.media.humanBuildableHitSmallPS   = CG_RegisterParticleSystem( "humanBuildableHitSmallPS" );
  cgs.media.alienBuildableHitSmallPS   = CG_RegisterParticleSystem( "alienBuildableHitSmallPS" );
  cgs.media.humanBuildableHitLargePS   = CG_RegisterParticleSystem( "humanBuildableHitLargePS" );
  cgs.media.alienBuildableHitLargePS   = CG_RegisterParticleSystem( "alienBuildableHitLargePS" );
  cgs.media.humanBuildableDestroyedPS = CG_RegisterParticleSystem( "humanBuildableDestroyedPS" );
  cgs.media.alienBuildableDestroyedPS = CG_RegisterParticleSystem( "alienBuildableDestroyedPS" );

  cgs.media.alienBleedPS              = CG_RegisterParticleSystem( "alienBleedPS" );
  cgs.media.humanBleedPS              = CG_RegisterParticleSystem( "humanBleedPS" );

  CG_BuildableStatusParse( "ui/assets/human/buildstat.cfg", &cgs.humanBuildStat );
  CG_BuildableStatusParse( "ui/assets/alien/buildstat.cfg", &cgs.alienBuildStat );
 
  // register the inline models
  cgs.numInlineModels = trap_CM_NumInlineModels( );

  for( i = 1; i < cgs.numInlineModels; i++ )
  {
    char    name[ 10 ];
    vec3_t  mins, maxs;
    int     j;

    Com_sprintf( name, sizeof( name ), "*%i", i );

    cgs.inlineDrawModel[ i ] = trap_R_RegisterModel( name );
    trap_R_ModelBounds( cgs.inlineDrawModel[ i ], mins, maxs );

    for( j = 0 ; j < 3 ; j++ )
      cgs.inlineModelMidpoints[ i ][ j ] = mins[ j ] + 0.5 * ( maxs[ j ] - mins[ j ] );
  }

  // register all the server specified models
  for( i = 1; i < MAX_MODELS; i++ )
  {
    const char *modelName;

    modelName = CG_ConfigString( CS_MODELS + i );

    if( !modelName[ 0 ] )
      break;

    cgs.gameModels[ i ] = trap_R_RegisterModel( modelName );
  }

  CG_UpdateMediaFraction( 0.8f );

  // register all the server specified shaders
  for( i = 1; i < MAX_GAME_SHADERS; i++ )
  {
    const char *shaderName;

    shaderName = CG_ConfigString( CS_SHADERS + i );

    if( !shaderName[ 0 ] )
      break;

    cgs.gameShaders[ i ] = trap_R_RegisterShader( shaderName );
  }

  CG_UpdateMediaFraction( 0.9f );

  // register all the server specified particle systems
  for( i = 1; i < MAX_GAME_PARTICLE_SYSTEMS; i++ )
  {
    const char *psName;

    psName = CG_ConfigString( CS_PARTICLE_SYSTEMS + i );

    if( !psName[ 0 ] )
      break;

    cgs.gameParticleSystems[ i ] = CG_RegisterParticleSystem( (char *)psName );
  }
}


/*
=======================
CG_BuildSpectatorString

=======================
*/
void CG_BuildSpectatorString( void )
{
  int i;

  cg.spectatorList[ 0 ] = 0;

  for( i = 0; i < MAX_CLIENTS; i++ )
  {
    if( cgs.clientinfo[ i ].infoValid && cgs.clientinfo[ i ].team == TEAM_NONE )
      Q_strcat( cg.spectatorList, sizeof( cg.spectatorList ), va( "%s     " S_COLOR_WHITE, cgs.clientinfo[ i ].name ) );
  }

  i = strlen( cg.spectatorList );

  if( i != cg.spectatorLen )
  {
    cg.spectatorLen = i;
    cg.spectatorWidth = -1;
  }
}



/*
===================
CG_RegisterClients

===================
*/
static void CG_RegisterClients( void )
{
  int   i;

  cg.charModelFraction = 0.0f;

  //precache all the models/sounds/etc
  for( i = PCL_NONE + 1; i < PCL_NUM_CLASSES;  i++ )
  {
    CG_PrecacheClientInfo( i, BG_ClassConfig( i )->modelName,
                              BG_ClassConfig( i )->skinName );

    cg.charModelFraction = (float)i / (float)PCL_NUM_CLASSES;
    trap_UpdateScreen( );
  }

  cgs.media.larmourHeadSkin    = trap_R_RegisterSkin( "models/players/human_base/head_light.skin" );
  cgs.media.larmourLegsSkin    = trap_R_RegisterSkin( "models/players/human_base/lower_light.skin" );
  cgs.media.larmourTorsoSkin   = trap_R_RegisterSkin( "models/players/human_base/upper_light.skin" );

  cgs.media.jetpackModel       = trap_R_RegisterModel( "models/players/human_base/jetpack.md3" );
  cgs.media.jetpackFlashModel  = trap_R_RegisterModel( "models/players/human_base/jetpack_flash.md3" );
  cgs.media.battpackModel      = trap_R_RegisterModel( "models/players/human_base/battpack.md3" );

  cg.charModelFraction = 1.0f;
  trap_UpdateScreen( );

  //load all the clientinfos of clients already connected to the server
  for( i = 0; i < MAX_CLIENTS; i++ )
  {
    const char  *clientInfo;

    clientInfo = CG_ConfigString( CS_PLAYERS + i );
    if( !clientInfo[ 0 ] )
      continue;

    CG_NewClientInfo( i );
  }

  CG_BuildSpectatorString( );
}

//===========================================================================

/*
=================
CG_ConfigString
=================
*/
const char *CG_ConfigString( int index )
{
  if( index < 0 || index >= MAX_CONFIGSTRINGS )
    CG_Error( "CG_ConfigString: bad index: %i", index );

  return cgs.gameState.stringData + cgs.gameState.stringOffsets[ index ];
}

//==================================================================

/*
======================
CG_StartMusic

======================
*/
void CG_StartMusic( void )
{
  char  *s;
  char  parm1[ MAX_QPATH ], parm2[ MAX_QPATH ];

  // start the background music
  s = (char *)CG_ConfigString( CS_MUSIC );
  Q_strncpyz( parm1, COM_Parse( &s ), sizeof( parm1 ) );
  Q_strncpyz( parm2, COM_Parse( &s ), sizeof( parm2 ) );

  trap_S_StartBackgroundTrack( parm1, parm2 );
}

/*
======================
CG_PlayerCount
======================
*/
int CG_PlayerCount( void )
{
  int i, count = 0;

  CG_RequestScores( );

  for( i = 0; i < cg.numScores; i++ )
  {
    if( cg.scores[ i ].team == TEAM_ALIENS ||
        cg.scores[ i ].team == TEAM_HUMANS )
      count++;
  }

  return count;
}

//
// ==============================
// new hud stuff ( mission pack )
// ==============================
//
char *CG_GetMenuBuffer( const char *filename )
{
  int           len;
  fileHandle_t  f;
  static char   buf[ MAX_MENUFILE ];

  len = trap_FS_FOpenFile( filename, &f, FS_READ );

  if( !f )
  {
    trap_Print( va( S_COLOR_RED "menu file not found: %s, using default\n", filename ) );
    return NULL;
  }

  if( len >= MAX_MENUFILE )
  {
    trap_Print( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i",
                    filename, len, MAX_MENUFILE ) );
    trap_FS_FCloseFile( f );
    return NULL;
  }

  trap_FS_Read( buf, len, f );
  buf[len] = 0;
  trap_FS_FCloseFile( f );

  return buf;
}

qboolean CG_Asset_Parse( int handle )
{
  pc_token_t token;
  const char *tempStr;

  if( !trap_Parse_ReadToken( handle, &token ) )
    return qfalse;

  if( Q_stricmp( token.string, "{" ) != 0 )
    return qfalse;

  while( 1 )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
      return qfalse;

    if( Q_stricmp( token.string, "}" ) == 0 )
      return qtrue;

    // font
    if( Q_stricmp( token.string, "font" ) == 0 )
    {
      int pointSize;

      if( !PC_String_Parse( handle, &tempStr ) || !PC_Int_Parse( handle, &pointSize ) )
        return qfalse;

      cgDC.registerFont( tempStr, pointSize, &cgDC.Assets.textFont );
      continue;
    }

    // smallFont
    if( Q_stricmp( token.string, "smallFont" ) == 0 )
    {
      int pointSize;

      if( !PC_String_Parse( handle, &tempStr ) || !PC_Int_Parse( handle, &pointSize ) )
        return qfalse;

      cgDC.registerFont( tempStr, pointSize, &cgDC.Assets.smallFont );
      continue;
    }

    // font
    if( Q_stricmp( token.string, "bigfont" ) == 0 )
    {
      int pointSize;

      if( !PC_String_Parse( handle, &tempStr ) || !PC_Int_Parse( handle, &pointSize ) )
        return qfalse;

      cgDC.registerFont( tempStr, pointSize, &cgDC.Assets.bigFont );
      continue;
    }

    // gradientbar
    if( Q_stricmp( token.string, "gradientbar" ) == 0 )
    {
      if( !PC_String_Parse( handle, &tempStr ) )
        return qfalse;

      cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip( tempStr );
      continue;
    }

    // enterMenuSound
    if( Q_stricmp( token.string, "menuEnterSound" ) == 0 )
    {
      if( !PC_String_Parse( handle, &tempStr ) )
        return qfalse;

      cgDC.Assets.menuEnterSound = trap_S_RegisterSound( tempStr, qfalse );
      continue;
    }

    // exitMenuSound
    if( Q_stricmp( token.string, "menuExitSound" ) == 0 )
    {
      if( !PC_String_Parse( handle, &tempStr ) )
        return qfalse;

      cgDC.Assets.menuExitSound = trap_S_RegisterSound( tempStr, qfalse );
      continue;
    }

    // itemFocusSound
    if( Q_stricmp( token.string, "itemFocusSound" ) == 0 )
    {
      if( !PC_String_Parse( handle, &tempStr ) )
        return qfalse;

      cgDC.Assets.itemFocusSound = trap_S_RegisterSound( tempStr, qfalse );
      continue;
    }

    // menuBuzzSound
    if( Q_stricmp( token.string, "menuBuzzSound" ) == 0 )
    {
      if( !PC_String_Parse( handle, &tempStr ) )
        return qfalse;

      cgDC.Assets.menuBuzzSound = trap_S_RegisterSound( tempStr, qfalse );
      continue;
    }

    if( Q_stricmp( token.string, "cursor" ) == 0 )
    {
      if( !PC_String_Parse( handle, &cgDC.Assets.cursorStr ) )
        return qfalse;

      cgDC.Assets.cursor = trap_R_RegisterShaderNoMip( cgDC.Assets.cursorStr );
      continue;
    }

    if( Q_stricmp( token.string, "fadeClamp" ) == 0 )
    {
      if( !PC_Float_Parse( handle, &cgDC.Assets.fadeClamp ) )
        return qfalse;

      continue;
    }

    if( Q_stricmp( token.string, "fadeCycle" ) == 0 )
    {
      if( !PC_Int_Parse( handle, &cgDC.Assets.fadeCycle ) )
        return qfalse;

      continue;
    }

    if( Q_stricmp( token.string, "fadeAmount" ) == 0 )
    {
      if( !PC_Float_Parse( handle, &cgDC.Assets.fadeAmount ) )
        return qfalse;

      continue;
    }

    if( Q_stricmp( token.string, "shadowX" ) == 0 )
    {
      if( !PC_Float_Parse( handle, &cgDC.Assets.shadowX ) )
        return qfalse;

      continue;
    }

    if( Q_stricmp( token.string, "shadowY" ) == 0 )
    {
      if( !PC_Float_Parse( handle, &cgDC.Assets.shadowY ) )
        return qfalse;

      continue;
    }

    if( Q_stricmp( token.string, "shadowColor" ) == 0 )
    {
      if( !PC_Color_Parse( handle, &cgDC.Assets.shadowColor ) )
        return qfalse;

      cgDC.Assets.shadowFadeClamp = cgDC.Assets.shadowColor[ 3 ];
      continue;
    }
  }

  return qfalse;
}

void CG_ParseMenu( const char *menuFile )
{
  pc_token_t  token;
  int         handle;

  handle = trap_Parse_LoadSource( menuFile );

  if( !handle )
    handle = trap_Parse_LoadSource( "ui/testhud.menu" );

  if( !handle )
    return;

  while( 1 )
  {
    if( !trap_Parse_ReadToken( handle, &token ) )
      break;

    //if ( Q_stricmp( token, "{" ) ) {
    //  Com_Printf( "Missing { in menu file\n" );
    //  break;
    //}

    //if ( menuCount == MAX_MENUS ) {
    //  Com_Printf( "Too many menus!\n" );
    //  break;
    //}

    if( token.string[ 0 ] == '}' )
      break;

    if( Q_stricmp( token.string, "assetGlobalDef" ) == 0 )
    {
      if( CG_Asset_Parse( handle ) )
        continue;
      else
        break;
    }


    if( Q_stricmp( token.string, "menudef" ) == 0 )
    {
      // start a new menu
      Menu_New( handle );
    }
  }

  trap_Parse_FreeSource( handle );
}

qboolean CG_Load_Menu( char **p )
{
  char *token;

  token = COM_ParseExt( p, qtrue );

  if( token[ 0 ] != '{' )
    return qfalse;

  while( 1 )
  {
    token = COM_ParseExt( p, qtrue );

    if( Q_stricmp( token, "}" ) == 0 )
      return qtrue;

    if( !token || token[ 0 ] == 0 )
      return qfalse;

    CG_ParseMenu( token );
  }
  return qfalse;
}



void CG_LoadMenus( const char *menuFile )
{
  char          *token;
  char          *p;
  int           len, start;
  fileHandle_t  f;
  static char   buf[ MAX_MENUDEFFILE ];

  start = trap_Milliseconds( );

  len = trap_FS_FOpenFile( menuFile, &f, FS_READ );

  if( !f )
  {
    trap_Error( va( S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile ) );
    len = trap_FS_FOpenFile( "ui/hud.txt", &f, FS_READ );

    if( !f )
      trap_Error( va( S_COLOR_RED "default menu file not found: ui/hud.txt, unable to continue!\n" ) );
  }

  if( len >= MAX_MENUDEFFILE )
  {
    trap_Error( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i",
                menuFile, len, MAX_MENUDEFFILE ) );
    trap_FS_FCloseFile( f );
    return;
  }

  trap_FS_Read( buf, len, f );
  buf[ len ] = 0;
  trap_FS_FCloseFile( f );

  COM_Compress( buf );

  Menu_Reset( );

  p = buf;

  while( 1 )
  {
    token = COM_ParseExt( &p, qtrue );

    if( !token || token[ 0 ] == 0 || token[ 0 ] == '}' )
      break;

    if( Q_stricmp( token, "}" ) == 0 )
      break;

    if( Q_stricmp( token, "loadmenu" ) == 0 )
    {
      if( CG_Load_Menu( &p ) )
        continue;
      else
        break;
    }
  }

  //Com_Printf( "UI menu load time = %d milli seconds\n", trap_Milliseconds( ) - start );
}



static qboolean CG_OwnerDrawHandleKey( int ownerDraw, int flags, float *special, int key )
{
  return qfalse;
}


static int CG_FeederCount( float feederID )
{
  int i, count = 0;

  if( feederID == FEEDER_ALIENTEAM_LIST )
  {
    for( i = 0; i < cg.numScores; i++ )
    {
      if( cg.scores[ i ].team == TEAM_ALIENS )
        count++;
    }
  }
  else if( feederID == FEEDER_HUMANTEAM_LIST )
  {
    for( i = 0; i < cg.numScores; i++ )
    {
      if( cg.scores[ i ].team == TEAM_HUMANS )
        count++;
    }
  }

  return count;
}


void CG_SetScoreSelection( void *p )
{
  menuDef_t     *menu = (menuDef_t*)p;
  playerState_t *ps = &cg.snap->ps;
  int           i, alien, human;
  int           feeder;

  alien = human = 0;

  for( i = 0; i < cg.numScores; i++ )
  {
    if( cg.scores[ i ].team == TEAM_ALIENS )
      alien++;
    else if( cg.scores[ i ].team == TEAM_HUMANS )
      human++;

    if( ps->clientNum == cg.scores[ i ].client )
      cg.selectedScore = i;
  }

  if( menu == NULL )
    // just interested in setting the selected score
    return;

  feeder = FEEDER_ALIENTEAM_LIST;
  i = alien;

  if( cg.scores[ cg.selectedScore ].team == TEAM_HUMANS )
  {
    feeder = FEEDER_HUMANTEAM_LIST;
    i = human;
  }

  Menu_SetFeederSelection(menu, feeder, i, NULL);
}

// FIXME: might need to cache this info
static clientInfo_t * CG_InfoFromScoreIndex( int index, int team, int *scoreIndex )
{
  int i, count;
  count = 0;

  for( i = 0; i < cg.numScores; i++ )
  {
    if( cg.scores[ i ].team == team )
    {
      if( count == index )
      {
        *scoreIndex = i;
        return &cgs.clientinfo[ cg.scores[ i ].client ];
      }
      count++;
    }
  }

  *scoreIndex = index;
  return &cgs.clientinfo[ cg.scores[ index ].client ];
}

static qboolean CG_ClientIsReady( int clientNum )
{
  // CS_CLIENTS_READY is a hex string, each character of which is 4 bits
  // the highest bit of the first char is a toggle for client 0, the second
  // highest for client 1, etc.
  // there are exactly four bits of information in each character
  int val;
  const char *s = CG_ConfigString( CS_CLIENTS_READY );

  // select the appropriate character without passing the end of the string
  for( val = clientNum / 4; *s && val > 0; s++, val-- );

  // convert hex -> int
  // FIXME: replace with sscanf when it supports width conversions (or some
  // other appropriate library function)
  if( isdigit( *s ) )
    val = *s - '0';
  else if( *s >= 'a' && *s <= 'f' )
    val = 10 + *s - 'a';
  else if( *s >= 'A' && *s <= 'F' )
    val = 10 + *s - 'A';
  else
    return qfalse;

  // select appropriate bit
  return ( ( val & 1 << ( 3 - clientNum % 4 ) ) != 0 );
}

static const char *CG_FeederItemText( float feederID, int index, int column, qhandle_t *handle )
{
  int           scoreIndex = 0;
  clientInfo_t  *info = NULL;
  int           team = -1;
  score_t       *sp = NULL;
  qboolean      showIcons = qfalse;

  *handle = -1;

  if( feederID == FEEDER_ALIENTEAM_LIST )
    team = TEAM_ALIENS;
  else if( feederID == FEEDER_HUMANTEAM_LIST )
    team = TEAM_HUMANS;

  info = CG_InfoFromScoreIndex( index, team, &scoreIndex );
  sp = &cg.scores[ scoreIndex ];

  if( cg.intermissionStarted && CG_ClientIsReady( sp->client ) )
    showIcons = qfalse;
  else if( cg.snap->ps.pm_type == PM_SPECTATOR || cg.snap->ps.pm_flags & PMF_FOLLOW ||
    team == cg.snap->ps.stats[ STAT_TEAM ] || cg.intermissionStarted )
    showIcons = qtrue;

  if( info && info->infoValid )
  {
    switch( column )
    {
      case 0:
        if( showIcons )
        {
          if( sp->weapon != WP_NONE )
            *handle = cg_weapons[ sp->weapon ].weaponIcon;
        }
        break;

      case 1:
        if( showIcons )
        {
          if( sp->team == TEAM_HUMANS && sp->upgrade != UP_NONE )
            *handle = cg_upgrades[ sp->upgrade ].upgradeIcon;
          else if( sp->team == TEAM_ALIENS )
          {
            switch( sp->weapon )
            {
              case WP_ABUILD2:
              case WP_ALEVEL1_UPG:
              case WP_ALEVEL2_UPG:
              case WP_ALEVEL3_UPG:
                *handle = cgs.media.upgradeClassIconShader;
                break;

              default:
                break;
            }
          }
        }
        break;

      case 2:
        if( cg.intermissionStarted && CG_ClientIsReady( sp->client ) )
          return "Ready";
        break;

      case 3:
        return info->name;
        break;

      case 4:
        return va( "%d", sp->score );
        break;

      case 5:
        return va( "%4d", sp->time );
        break;

      case 6:
        if( sp->ping == -1 )
          return "";

        return va( "%4d", sp->ping );
        break;
    }
  }

  return "";
}

static qhandle_t CG_FeederItemImage( float feederID, int index )
{
  return 0;
}

static void CG_FeederSelection( float feederID, int index )
{
  int i, count;
  int team = ( feederID == FEEDER_ALIENTEAM_LIST ) ? TEAM_ALIENS : TEAM_HUMANS;
  count = 0;

  for( i = 0; i < cg.numScores; i++ )
  {
    if( cg.scores[ i ].team == team )
    {
      if( index == count )
        cg.selectedScore = i;

      count++;
    }
  }
}

static float CG_Cvar_Get( const char *cvar )
{
  char buff[ 128 ];

  memset( buff, 0, sizeof( buff ) );
  trap_Cvar_VariableStringBuffer( cvar, buff, sizeof( buff ) );
  return atof( buff );
}

void CG_Text_PaintWithCursor( float x, float y, float scale, vec4_t color, const char *text,
                              int cursorPos, char cursor, int limit, int style )
{
  UI_Text_Paint( x, y, scale, color, text, 0, limit, style );
}

static int CG_OwnerDrawWidth( int ownerDraw, float scale )
{
  switch( ownerDraw )
  {
    case CG_KILLER:
      return UI_Text_Width( CG_GetKillerText( ), scale, 0 );
      break;
  }

  return 0;
}

static int CG_PlayCinematic( const char *name, float x, float y, float w, float h )
{
  return trap_CIN_PlayCinematic( name, x, y, w, h, CIN_loop );
}

static void CG_StopCinematic( int handle )
{
  trap_CIN_StopCinematic( handle );
}

static void CG_DrawCinematic( int handle, float x, float y, float w, float h )
{
  trap_CIN_SetExtents( handle, x, y, w, h );
  trap_CIN_DrawCinematic( handle );
}

static void CG_RunCinematicFrame( int handle )
{
  trap_CIN_RunCinematic( handle );
}

// hack to prevent warning
static qboolean CG_OwnerDrawVisible( int parameter )
{
  return qfalse;
}

/*
=================
CG_LoadHudMenu
=================
*/
void CG_LoadHudMenu( void )
{
  char        buff[ 1024 ];
  const char  *hudSet;

  cgDC.aspectScale = ( ( 640.0f * cgs.glconfig.vidHeight ) /
                       ( 480.0f * cgs.glconfig.vidWidth ) );
  cgDC.xscale = cgs.glconfig.vidWidth / 640.0f;
  cgDC.yscale = cgs.glconfig.vidHeight / 480.0f;

  cgDC.registerShaderNoMip  = &trap_R_RegisterShaderNoMip;
  cgDC.setColor             = &trap_R_SetColor;
  cgDC.drawHandlePic        = &CG_DrawPic;
  cgDC.drawStretchPic       = &trap_R_DrawStretchPic;
  cgDC.registerModel        = &trap_R_RegisterModel;
  cgDC.modelBounds          = &trap_R_ModelBounds;
  cgDC.fillRect             = &CG_FillRect;
  cgDC.drawRect             = &CG_DrawRect;
  cgDC.drawSides            = &CG_DrawSides;
  cgDC.drawTopBottom        = &CG_DrawTopBottom;
  cgDC.clearScene           = &trap_R_ClearScene;
  cgDC.addRefEntityToScene  = &trap_R_AddRefEntityToScene;
  cgDC.renderScene          = &trap_R_RenderScene;
  cgDC.registerFont         = &trap_R_RegisterFont;
  cgDC.ownerDrawItem        = &CG_OwnerDraw;
  cgDC.getValue             = &CG_GetValue;
  cgDC.ownerDrawVisible     = &CG_OwnerDrawVisible;
  cgDC.runScript            = &CG_RunMenuScript;
  cgDC.setCVar              = trap_Cvar_Set;
  cgDC.getCVarString        = trap_Cvar_VariableStringBuffer;
  cgDC.getCVarValue         = CG_Cvar_Get;
  //cgDC.setOverstrikeMode    = &trap_Key_SetOverstrikeMode;
  //cgDC.getOverstrikeMode    = &trap_Key_GetOverstrikeMode;
  cgDC.startLocalSound      = &trap_S_StartLocalSound;
  cgDC.ownerDrawHandleKey   = &CG_OwnerDrawHandleKey;
  cgDC.feederCount          = &CG_FeederCount;
  cgDC.feederItemImage      = &CG_FeederItemImage;
  cgDC.feederItemText       = &CG_FeederItemText;
  cgDC.feederSelection      = &CG_FeederSelection;
  //cgDC.setBinding           = &trap_Key_SetBinding;
  //cgDC.getBindingBuf        = &trap_Key_GetBindingBuf;
  //cgDC.keynumToStringBuf    = &trap_Key_KeynumToStringBuf;
  //cgDC.executeText          = &trap_Cmd_ExecuteText;
  cgDC.Error                = &Com_Error;
  cgDC.Print                = &Com_Printf;
  cgDC.ownerDrawWidth       = &CG_OwnerDrawWidth;
  //cgDC.ownerDrawText        = &CG_OwnerDrawText;
  //cgDC.Pause                = &CG_Pause;
  cgDC.registerSound        = &trap_S_RegisterSound;
  cgDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
  cgDC.stopBackgroundTrack  = &trap_S_StopBackgroundTrack;
  cgDC.playCinematic        = &CG_PlayCinematic;
  cgDC.stopCinematic        = &CG_StopCinematic;
  cgDC.drawCinematic        = &CG_DrawCinematic;
  cgDC.runCinematicFrame    = &CG_RunCinematicFrame;
  cgDC.hudloading           = qtrue;
  Init_Display( &cgDC );

  Menu_Reset( );

  trap_Cvar_VariableStringBuffer( "cg_hudFiles", buff, sizeof( buff ) );
  hudSet = buff;

  if( !cg_hudFilesEnable.integer || hudSet[ 0 ] == '\0' )
    hudSet = "ui/hud.txt";

  CG_LoadMenus( hudSet );
  cgDC.hudloading = qfalse;
}

void CG_AssetCache( void )
{
  int i;

  cgDC.Assets.gradientBar         = trap_R_RegisterShaderNoMip( ASSET_GRADIENTBAR );
  cgDC.Assets.scrollBar           = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR );
  cgDC.Assets.scrollBarArrowDown  = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWDOWN );
  cgDC.Assets.scrollBarArrowUp    = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWUP );
  cgDC.Assets.scrollBarArrowLeft  = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWLEFT );
  cgDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip( ASSET_SCROLLBAR_ARROWRIGHT );
  cgDC.Assets.scrollBarThumb      = trap_R_RegisterShaderNoMip( ASSET_SCROLL_THUMB );
  cgDC.Assets.sliderBar           = trap_R_RegisterShaderNoMip( ASSET_SLIDER_BAR );
  cgDC.Assets.sliderThumb         = trap_R_RegisterShaderNoMip( ASSET_SLIDER_THUMB );

  if( cg_emoticons.integer )
  {
    cgDC.Assets.emoticonCount = BG_LoadEmoticons( cgDC.Assets.emoticons,
      cgDC.Assets.emoticonWidths );
  }
  else
    cgDC.Assets.emoticonCount = 0;

  for( i = 0; i < cgDC.Assets.emoticonCount; i++ )
  {
    cgDC.Assets.emoticonShaders[ i ] = trap_R_RegisterShaderNoMip(
      va( "emoticons/%s_%dx1.tga", cgDC.Assets.emoticons[ i ],
          cgDC.Assets.emoticonWidths[ i ] ) );
  }
}

/*
=================
CG_Init

Called after every level change or subsystem restart
Will perform callbacks to make the loading info screen update.
=================
*/
void CG_Init( int serverMessageNum, int serverCommandSequence, int clientNum )
{
  const char  *s;

  // clear everything
  memset( &cgs, 0, sizeof( cgs ) );
  memset( &cg, 0, sizeof( cg ) );
  memset( &cg.pmext, 0, sizeof( cg.pmext ) );
  memset( cg_entities, 0, sizeof( cg_entities ) );

  cg.clientNum = clientNum;

  cgs.processedSnapshotNum = serverMessageNum;
  cgs.serverCommandSequence = serverCommandSequence;

  // get the rendering configuration from the client system
  trap_GetGlconfig( &cgs.glconfig );
  cgs.screenXScale = cgs.glconfig.vidWidth / 640.0f;
  cgs.screenYScale = cgs.glconfig.vidHeight / 480.0f;

  // load a few needed things before we do any screen updates
  cgs.media.whiteShader     = trap_R_RegisterShader( "white" );
  cgs.media.charsetShader   = trap_R_RegisterShader( "gfx/2d/bigchars" );
  cgs.media.outlineShader   = trap_R_RegisterShader( "outline" );

  // load overrides
  BG_InitClassConfigs( );
  BG_InitBuildableConfigs( );
  BG_InitAllowedGameElements( );

  // Dynamic memory
  BG_InitMemory( );

  CG_RegisterCvars( );

  CG_InitConsoleCommands( );

  String_Init( );

  CG_AssetCache( );
  CG_LoadHudMenu( );
  cg.feedbackAnimation = 0;
  cg.feedbackAnimationType = 0;

  cg.weaponSelect = WP_NONE;

  // old servers

  // get the gamestate from the client system
  trap_GetGameState( &cgs.gameState );

  // check version
  s = CG_ConfigString( CS_GAME_VERSION );

  if( strcmp( s, GAME_VERSION ) )
    CG_Error( "Client/Server game mismatch: %s/%s", GAME_VERSION, s );

  s = CG_ConfigString( CS_LEVEL_START_TIME );
  cgs.levelStartTime = atoi( s );

  CG_ParseServerinfo( );

  // load the new map
  trap_CM_LoadMap( cgs.mapname );

  cg.loading = qtrue;   // force players to load instead of defer

  CG_LoadTrailSystems( );
  CG_UpdateMediaFraction( 0.05f );

  CG_LoadParticleSystems( );
  CG_UpdateMediaFraction( 0.05f );

  CG_RegisterSounds( );
  CG_UpdateMediaFraction( 0.60f );

  CG_RegisterGraphics( );
  CG_UpdateMediaFraction( 0.90f );

  CG_InitWeapons( );
  CG_UpdateMediaFraction( 0.95f );

  CG_InitUpgrades( );
  CG_UpdateMediaFraction( 1.0f );

  CG_InitBuildables( );

  cgs.voices = BG_VoiceInit( );
  BG_PrintVoices( cgs.voices, cg_debugVoices.integer );
 
  CG_RegisterClients( );   // if low on memory, some clients will be deferred

  cg.loading = qfalse;  // future players will be deferred

  CG_InitMarkPolys( );

  // remove the last loading update
  cg.infoScreenText[ 0 ] = 0;

  // Make sure we have update values (scores)
  CG_SetConfigValues( );

  CG_StartMusic( );

  CG_ShaderStateChanged( );

  trap_S_ClearLoopingSounds( qtrue );
}

/*
=================
CG_Shutdown

Called before every level change or subsystem restart
=================
*/
void CG_Shutdown( void )
{
  // some mods may need to do cleanup work here,
  // like closing files or archiving session data

  // Reset cg_version
  trap_Cvar_Set( "cg_version", "" );
}
