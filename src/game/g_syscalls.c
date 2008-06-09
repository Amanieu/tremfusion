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

// this file is only included when building a dll
// g_syscalls.asm is included instead when building a qvm

static intptr_t (QDECL *syscall)( intptr_t arg, ... ) = (intptr_t (QDECL *)( intptr_t, ...))-1;


void dllEntry( intptr_t (QDECL *syscallptr)( intptr_t arg,... ) )
{
  syscall = syscallptr;
}

int PASSFLOAT( float x )
{
  float floatTemp;
  floatTemp = x;
  return *(int *)&floatTemp;
}

void  trap_Print( const char *fmt )
{
  syscall( G_PRINT, fmt );
}

void  trap_Error( const char *fmt )
{
  syscall( G_ERROR, fmt );
}

int   trap_Milliseconds( void )
{
  return syscall( G_MILLISECONDS );
}
int   trap_Argc( void )
{
  return syscall( G_ARGC );
}

void  trap_Argv( int n, char *buffer, int bufferLength )
{
  syscall( G_ARGV, n, buffer, bufferLength );
}

int   trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode )
{
  return syscall( G_FS_FOPEN_FILE, qpath, f, mode );
}

void  trap_FS_Read( void *buffer, int len, fileHandle_t f )
{
  syscall( G_FS_READ, buffer, len, f );
}

void  trap_FS_Write( const void *buffer, int len, fileHandle_t f )
{
  syscall( G_FS_WRITE, buffer, len, f );
}

void  trap_FS_FCloseFile( fileHandle_t f )
{
  syscall( G_FS_FCLOSE_FILE, f );
}

int trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize )
{
  return syscall( G_FS_GETFILELIST, path, extension, listbuf, bufsize );
}

void  trap_SendConsoleCommand( int exec_when, const char *text )
{
  syscall( G_SEND_CONSOLE_COMMAND, exec_when, text );
}

void  trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags )
{
  syscall( G_CVAR_REGISTER, cvar, var_name, value, flags );
}

void  trap_Cvar_Update( vmCvar_t *cvar )
{
  syscall( G_CVAR_UPDATE, cvar );
}

void trap_Cvar_Set( const char *var_name, const char *value )
{
  syscall( G_CVAR_SET, var_name, value );
}

int trap_Cvar_VariableIntegerValue( const char *var_name )
{
  return syscall( G_CVAR_VARIABLE_INTEGER_VALUE, var_name );
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize )
{
  syscall( G_CVAR_VARIABLE_STRING_BUFFER, var_name, buffer, bufsize );
}


void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
                          playerState_t *clients, int sizeofGClient )
{
  syscall( G_LOCATE_GAME_DATA, gEnts, numGEntities, sizeofGEntity_t, clients, sizeofGClient );
}

void trap_DropClient( int clientNum, const char *reason )
{
  syscall( G_DROP_CLIENT, clientNum, reason );
}

void trap_SendServerCommand( int clientNum, const char *text )
{
  syscall( G_SEND_SERVER_COMMAND, clientNum, text );
}

void trap_SetConfigstring( int num, const char *string )
{
  syscall( G_SET_CONFIGSTRING, num, string );
}

void trap_GetConfigstring( int num, char *buffer, int bufferSize )
{
  syscall( G_GET_CONFIGSTRING, num, buffer, bufferSize );
}

void trap_GetUserinfo( int num, char *buffer, int bufferSize )
{
  syscall( G_GET_USERINFO, num, buffer, bufferSize );
}

void trap_SetUserinfo( int num, const char *buffer )
{
  syscall( G_SET_USERINFO, num, buffer );
}

void trap_GetServerinfo( char *buffer, int bufferSize )
{
  syscall( G_GET_SERVERINFO, buffer, bufferSize );
}

void trap_SetBrushModel( gentity_t *ent, const char *name )
{
  syscall( G_SET_BRUSH_MODEL, ent, name );
}

void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins,
                 const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask )
{
  syscall( G_TRACE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

void trap_TraceCapsule( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask )
{
  syscall( G_TRACECAPSULE, results, start, mins, maxs, end, passEntityNum, contentmask );
}

int trap_PointContents( const vec3_t point, int passEntityNum )
{
  return syscall( G_POINT_CONTENTS, point, passEntityNum );
}


qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 )
{
  return syscall( G_IN_PVS, p1, p2 );
}

qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 )
{
  return syscall( G_IN_PVS_IGNORE_PORTALS, p1, p2 );
}

void trap_AdjustAreaPortalState( gentity_t *ent, qboolean open )
{
  syscall( G_ADJUST_AREA_PORTAL_STATE, ent, open );
}

qboolean trap_AreasConnected( int area1, int area2 )
{
  return syscall( G_AREAS_CONNECTED, area1, area2 );
}

void trap_LinkEntity( gentity_t *ent )
{
  syscall( G_LINKENTITY, ent );
}

void trap_UnlinkEntity( gentity_t *ent )
{
  syscall( G_UNLINKENTITY, ent );
}


int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount )
{
  return syscall( G_ENTITIES_IN_BOX, mins, maxs, list, maxcount );
}

qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent )
{
  return syscall( G_ENTITY_CONTACT, mins, maxs, ent );
}

qboolean trap_EntityContactCapsule( const vec3_t mins, const vec3_t maxs, const gentity_t *ent )
{
  return syscall( G_ENTITY_CONTACTCAPSULE, mins, maxs, ent );
}

void trap_GetUsercmd( int clientNum, usercmd_t *cmd )
{
  syscall( G_GET_USERCMD, clientNum, cmd );
}

qboolean trap_GetEntityToken( char *buffer, int bufferSize )
{
  return syscall( G_GET_ENTITY_TOKEN, buffer, bufferSize );
}

int trap_RealTime( qtime_t *qtime )
{
  return syscall( G_REAL_TIME, qtime );
}

void trap_SnapVector( float *v )
{
  syscall( G_SNAPVECTOR, v );
  return;
}

void trap_SendGameStat( const char *data )
{
  syscall( G_SEND_GAMESTAT, data );
  return;
}

int trap_Parse_AddGlobalDefine( char *define )
{
  return syscall( G_PARSE_ADD_GLOBAL_DEFINE, define );
}

int trap_Parse_LoadSource( const char *filename )
{
  return syscall( G_PARSE_LOAD_SOURCE, filename );
}

int trap_Parse_FreeSource( int handle )
{
  return syscall( G_PARSE_FREE_SOURCE, handle );
}

int trap_Parse_ReadToken( int handle, pc_token_t *pc_token )
{
  return syscall( G_PARSE_READ_TOKEN, handle, pc_token );
}

int trap_Parse_SourceFileAndLine( int handle, char *filename, int *line )
{
  return syscall( G_PARSE_SOURCE_FILE_AND_LINE, handle, filename, line );
}

int trap_BotAllocateClient( void )
{
  return syscall( G_BOT_ALLOCATE_CLIENT );
}

void trap_BotFreeClient( int clientNum )
{
  syscall( G_BOT_FREE_CLIENT, clientNum );
}

int trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points)
{
  return syscall( G_DEBUG_POLYGON_CREATE, color, numPoints, points );
}

void trap_DebugPolygonDelete(int id)
{
  syscall( G_DEBUG_POLYGON_DELETE, id );
}



// BotLib traps start here
void trap_EA_Say(int client, char *str) {
	syscall( BOTLIB_EA_SAY, client, str );
}

void trap_EA_SayTeam(int client, char *str) {
	syscall( BOTLIB_EA_SAY_TEAM, client, str );
}

void trap_EA_Command(int client, char *command) {
	syscall( BOTLIB_EA_COMMAND, client, command );
}

void trap_EA_Action(int client, int action) {
	syscall( BOTLIB_EA_ACTION, client, action );
}

void trap_EA_Gesture(int client) {
	syscall( BOTLIB_EA_GESTURE, client );
}

void trap_EA_Talk(int client) {
	syscall( BOTLIB_EA_TALK, client );
}

void trap_EA_Attack(int client) {
	syscall( BOTLIB_EA_ATTACK, client );
}

void trap_EA_Use(int client) {
	syscall( BOTLIB_EA_USE, client );
}

void trap_EA_Respawn(int client) {
	syscall( BOTLIB_EA_RESPAWN, client );
}

void trap_EA_Crouch(int client) {
	syscall( BOTLIB_EA_CROUCH, client );
}

void trap_EA_MoveUp(int client) {
	syscall( BOTLIB_EA_MOVE_UP, client );
}

void trap_EA_MoveDown(int client) {
	syscall( BOTLIB_EA_MOVE_DOWN, client );
}

void trap_EA_MoveForward(int client) {
	syscall( BOTLIB_EA_MOVE_FORWARD, client );
}

void trap_EA_MoveBack(int client) {
	syscall( BOTLIB_EA_MOVE_BACK, client );
}

void trap_EA_MoveLeft(int client) {
	syscall( BOTLIB_EA_MOVE_LEFT, client );
}

void trap_EA_MoveRight(int client) {
	syscall( BOTLIB_EA_MOVE_RIGHT, client );
}

void trap_EA_SelectWeapon(int client, int weapon) {
	syscall( BOTLIB_EA_SELECT_WEAPON, client, weapon );
}

void trap_EA_Jump(int client) {
	syscall( BOTLIB_EA_JUMP, client );
}

void trap_EA_DelayedJump(int client) {
	syscall( BOTLIB_EA_DELAYED_JUMP, client );
}

void trap_EA_Move(int client, vec3_t dir, float speed) {
	syscall( BOTLIB_EA_MOVE, client, dir, PASSFLOAT(speed) );
}

void trap_EA_View(int client, vec3_t viewangles) {
	syscall( BOTLIB_EA_VIEW, client, viewangles );
}

void trap_EA_EndRegular(int client, float thinktime) {
	syscall( BOTLIB_EA_END_REGULAR, client, PASSFLOAT(thinktime) );
}

void trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input) {
	syscall( BOTLIB_EA_GET_INPUT, client, PASSFLOAT(thinktime), input );
}

void trap_EA_ResetInput(int client) {
	syscall( BOTLIB_EA_RESET_INPUT, client );
}

int trap_BotLibSetup( void ) {
	return syscall( BOTLIB_SETUP );
}

int trap_BotLibShutdown( void ) {
	return syscall( BOTLIB_SHUTDOWN );
}

int trap_BotLibLoadMap(const char *mapname) {
	return syscall( BOTLIB_LOAD_MAP, mapname );
}

float trap_AAS_Time(void) {
	int temp;
	temp = syscall( BOTLIB_AAS_TIME );
	return (*(float*)&temp);
}
int trap_AAS_Initialized(void) {
	return syscall( BOTLIB_AAS_INITIALIZED );
}

int trap_AAS_PointAreaNum(vec3_t point) {
	return syscall( BOTLIB_AAS_POINT_AREA_NUM, point );
}

int trap_AAS_AreaReachability(int areanum) {
	return syscall( BOTLIB_AAS_AREA_REACHABILITY, areanum );
}

int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas) {
	return syscall( BOTLIB_AAS_TRACE_AREAS, start, end, areas, points, maxareas );
}

int trap_AAS_AreaInfo( int areanum, void /* struct aas_areainfo_s */ *info ) {
	return syscall( BOTLIB_AAS_AREA_INFO, areanum, info );
}

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags) {
	return syscall( BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA, areanum, origin, goalareanum, travelflags );
}

void trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info) {
	syscall( BOTLIB_AAS_ENTITY_INFO, entnum, info );
}

int trap_BotLibStartFrame(float time) {
	return syscall( BOTLIB_START_FRAME, PASSFLOAT( time ) );
}

int trap_BotLibVarSet(char *var_name, char *value) {
	return syscall( BOTLIB_LIBVAR_SET, var_name, value );
}

int trap_BotLibVarGet(char *var_name, char *value, int size) {
	return syscall( BOTLIB_LIBVAR_GET, var_name, value, size );
}

int trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue) {
	return syscall( BOTLIB_UPDATENTITY, ent, bue );
}

int trap_BotGetServerCommand(int clientNum, char *message, int size) {
	return syscall( BOTLIB_GET_CONSOLE_MESSAGE, clientNum, message, size );
}

void trap_BotUserCommand(int clientNum, usercmd_t *ucmd) {
	syscall( BOTLIB_USER_COMMAND, clientNum, ucmd );
}
void trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags) {
	syscall( BOTLIB_AI_MOVE_TO_GOAL, result, movestate, goal, travelflags );
}

void trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove) {
	syscall( BOTLIB_AI_INIT_MOVE_STATE, handle, initmove );
}

int trap_BotAllocMoveState(void) {
	return syscall( BOTLIB_AI_ALLOC_MOVE_STATE );
}

void trap_BotFreeMoveState(int handle) {
	syscall( BOTLIB_AI_FREE_MOVE_STATE, handle );
}

int trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target) {
	return syscall( BOTLIB_AI_MOVEMENT_VIEW_TARGET, movestate, goal, travelflags, PASSFLOAT(lookahead), target );
}


