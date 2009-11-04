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

/**********************************************************************
  UI_ATOMS.C
 
  User interface building blocks and support functions.
**********************************************************************/
#include "ui_local.h"

qboolean    m_entersound;    // after a frame, so caching won't disrupt the sound

void QDECL Com_Error( int level, const char *error, ... )
{
  va_list    argptr;
  char    text[1024];

  va_start( argptr, error );
  Q_vsnprintf( text, sizeof( text ), error, argptr );
  va_end( argptr );

  trap_Error( text );
}

void QDECL Com_Printf( const char *msg, ... )
{
  va_list    argptr;
  char    text[1024];

  va_start( argptr, msg );
  Q_vsnprintf( text, sizeof( text ), msg, argptr );
  va_end( argptr );

  trap_Print( text );
}


/*
=================
UI_ClampCvar
=================
*/
float UI_ClampCvar( float min, float max, float value )
{
  if( value < min ) return min;

  if( value > max ) return max;

  return value;
}

/*
=================
UI_StartDemoLoop
=================
*/
void UI_StartDemoLoop( void )
{
  trap_Cmd_ExecuteText( EXEC_APPEND, "d1\n" );
}

char *UI_Argv( int arg )
{
  static char  buffer[MAX_STRING_CHARS];

  trap_Argv( arg, buffer, sizeof( buffer ) );

  return buffer;
}


char *UI_Cvar_VariableString( const char *var_name )
{
  static char  buffer[MAX_STRING_CHARS];

  trap_Cvar_VariableStringBuffer( var_name, buffer, sizeof( buffer ) );

  return buffer;
}

static void  UI_Cache_f( void )
{
  Display_CacheAll();
}

static void UI_Menu_f( void )
{
    if( Menu_Count( ) > 0 )
    {
      trap_Key_SetCatcher( KEYCATCH_UI );
      Menus_ActivateByName( UI_Argv( 1 ) );
    }
}

static void UI_CloseMenus_f( void )
{
    if( Menu_Count( ) )
    {
      Menus_CloseAll( qfalse );
      if( !Menu_Count( ) )
      {
        trap_Key_SetCatcher( trap_Key_GetCatcher( ) & ~KEYCATCH_UI );
        trap_Key_ClearStates( );
        trap_Cvar_Set( "cl_paused", "0" );
      }
    }
}

static void UI_MessageMode_f( void )
{
  char *arg = UI_Argv( 0 );
  char buffer[ MAX_SAY_TEXT ] = "";
  int i;
  for ( i = 1; i < trap_Argc( ); i++ )
      Q_strcat( buffer, sizeof( buffer ), UI_Argv( i ) );
  trap_Cvar_Set( "ui_sayBuffer", buffer );
  uiInfo.chatTargetClientNum = -1;
  uiInfo.chatTeam = qfalse;
  uiInfo.chatAdmins = qfalse;
  uiInfo.chatClan = qfalse;
  uiInfo.chatPrompt = qfalse;
  trap_Key_SetCatcher( KEYCATCH_UI );
  Menus_CloseByName( "say" );
  Menus_CloseByName( "say_team" );
  Menus_CloseByName( "say_crosshair" );
  Menus_CloseByName( "say_attacker" );
  Menus_CloseByName( "say_admins" );
  Menus_CloseByName( "say_prompt" );
  Menus_CloseByName( "say_clan" );
  switch( arg[ 11 ] )
  {
    default:
    case '\0':
      Menus_ActivateByName( "say" );
      break;
    case '2':
      uiInfo.chatTeam = qtrue;
      Menus_ActivateByName( "say_team" );
      break;
    case '3':
      uiInfo.chatTargetClientNum = trap_CrosshairPlayer();
      Menus_ActivateByName( "say_crosshair" );
      break;
    case '4':
      uiInfo.chatTargetClientNum = trap_LastAttacker();
      Menus_ActivateByName( "say_attacker" );
      break;
    case '5':
      uiInfo.chatAdmins = qtrue;
      Menus_ActivateByName( "say_admins" );
      break;
    case '6':
      uiInfo.chatClan = qtrue;
      Menus_ActivateByName( "say_clan" );
      break;
  }
}

static void UI_Prompt_f( void )
{
  static char buffer[ MAX_SAY_TEXT ];
  itemDef_t *item;
  if ( trap_Argc( ) < 3 )
  {
    Com_Printf( "prompt <callback> [prompt]: Opens the chatbox, store the text in ui_sayBuffer and then vstr callback\n" );
    return;
  }
  trap_Argv( 1, uiInfo.chatPromptCallback, sizeof( uiInfo.chatPromptCallback ) );
  trap_Argv( 2, buffer, sizeof( buffer ) );
  trap_Cvar_Set( "ui_sayBuffer", "" );
  uiInfo.chatTargetClientNum = -1;
  uiInfo.chatTeam = qfalse;
  uiInfo.chatAdmins = qfalse;
  uiInfo.chatClan = qfalse;
  uiInfo.chatPrompt = qtrue;
  trap_Key_SetCatcher( KEYCATCH_UI );
  Menus_CloseByName( "say" );
  Menus_CloseByName( "say_team" );
  Menus_CloseByName( "say_crosshair" );
  Menus_CloseByName( "say_attacker" );
  Menus_CloseByName( "say_admins" );
  Menus_CloseByName( "say_prompt" );
  Menus_CloseByName( "say_clan" );
  item = Menu_FindItemByName( Menus_ActivateByName( "say_prompt" ), "say_field" );
  if ( item )
  {
    trap_Argv( 2, buffer, sizeof( buffer ) );
    item->text = buffer;
  }
}

struct uicmd
{
  char *cmd;
  void ( *function )( void );
} commands[ ] = {
  { "closemenus", UI_CloseMenus_f },
  { "menu", UI_Menu_f },
  { "messagemode", UI_MessageMode_f },
  { "messagemode2", UI_MessageMode_f },
  { "messagemode3", UI_MessageMode_f },
  { "messagemode4", UI_MessageMode_f },
  { "messagemode5", UI_MessageMode_f },
  { "messagemode6", UI_MessageMode_f },
  { "prompt", UI_Prompt_f },
  { "ui_load", UI_Load },
  { "ui_report", UI_Report },
  { "ui_cache", UI_Cache_f }
};

/*
=================
UI_ConsoleCommand
=================
*/
qboolean UI_ConsoleCommand( int realTime )
{
  struct uicmd *cmd = bsearch( UI_Argv( 0 ), commands,
    sizeof( commands ) / sizeof( commands[ 0 ] ), sizeof( commands[ 0 ] ),
    cmdcmp );

  uiInfo.uiDC.frameTime = realTime - uiInfo.uiDC.realTime;
  uiInfo.uiDC.realTime = realTime;

  if( cmd )
  {
    cmd->function( );
    return qtrue;
  }

  return qfalse;
}

void UI_DrawNamedPic( float x, float y, float width, float height, const char *picname )
{
  qhandle_t  hShader;

  hShader = trap_R_RegisterShaderNoMip( picname );
  UI_AdjustFrom640( &x, &y, &width, &height );
  trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

void UI_DrawHandlePic( float x, float y, float w, float h, qhandle_t hShader )
{
  float  s0;
  float  s1;
  float  t0;
  float  t1;

  if( w < 0 )
  {  // flip about vertical
    w  = -w;
    s0 = 1;
    s1 = 0;
  }
  else
  {
    s0 = 0;
    s1 = 1;
  }

  if( h < 0 )
  {  // flip about horizontal
    h  = -h;
    t0 = 1;
    t1 = 0;
  }
  else
  {
    t0 = 0;
    t1 = 1;
  }

  UI_AdjustFrom640( &x, &y, &w, &h );
  trap_R_DrawStretchPic( x, y, w, h, s0, t0, s1, t1, hShader );
}

void UI_SetColor( const float *rgba )
{
  trap_R_SetColor( rgba );
}
