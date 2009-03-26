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

#include "g_local.h"
#include "ai_main.h"

/*
==================
ClientName
==================
 */
char *ClientName(int client, char *name, int size) {
  char buf[MAX_INFO_STRING];
  
  if (client < 0 || client >= MAX_CLIENTS) {
    ////BotAI_Print(PRT_ERROR, "ClientName: client out of range\n");
    return "[client out of range]";
  }
  trap_GetConfigstring(CS_PLAYERS+client, buf, sizeof(buf));
  strncpy(name, Info_ValueForKey(buf, "n"), size-1);
  name[size-1] = '\0';
  Q_CleanStr( name );
  return name;
}

/*
==================
ClientSkin
==================
 */
char *ClientSkin(int client, char *skin, int size) {
  char buf[MAX_INFO_STRING];
  
  if (client < 0 || client >= MAX_CLIENTS) {
    BotAI_Print(PRT_ERROR, "ClientSkin: client out of range\n");
    return "[client out of range]";
  }
  trap_GetConfigstring(CS_PLAYERS+client, buf, sizeof(buf));
  strncpy(skin, Info_ValueForKey(buf, "model"), size-1);
  skin[size-1] = '\0';
  return skin;
}

/*
==============
AngleDifference
==============
 */
float AngleDifference(float ang1, float ang2) {
  float diff;
  
  diff = ang1 - ang2;
  if (ang1 > ang2) {
    if (diff > 180.0) diff -= 360.0;
  }
  else {
    if (diff < -180.0) diff += 360.0;
  }
  return diff;
}

/*
==============
BotChangeViewAngle
==============
 */
float BotChangeViewAngle(float angle, float ideal_angle, float speed) {
  float move;
  
  angle = AngleMod(angle);
  ideal_angle = AngleMod(ideal_angle);
  if (angle == ideal_angle) return angle;
  move = ideal_angle - angle;
  if (ideal_angle > angle) {
    if (move > 180.0) move -= 360.0;
  }
  else {
    if (move < -180.0) move += 360.0;
  }
  if (move > 0) {
    if (move > speed) move = speed;
  }
  else {
    if (move < -speed) move = -speed;
  }
  return AngleMod(angle + move);
}

/*
==============
BotChangeViewAngles
==============
 */
void BotChangeViewAngles(bot_state_t *bs, float thinktime) {
  float diff, factor, maxchange, anglespeed;
  int i;
  
  if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
  //
  if (bs->enemy >= 0) {
    factor = 0.8f;
    maxchange = 360;
  }
  else {
    factor = 0.2f;
    maxchange = 360;
  }
  if (maxchange < 240) maxchange = 240;
  maxchange *= thinktime;
  for (i = 0; i < 2; i++) {
    //
    //if (bot_challenge.integer) {
    //smooth slowdown view model
    diff = abs(AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]));
    anglespeed = diff * factor;
    if (anglespeed > maxchange) anglespeed = maxchange;
    bs->viewangles[i] = BotChangeViewAngle(bs->viewangles[i],
                                           bs->ideal_viewangles[i], anglespeed);
    //}
    /*else {
			//over reaction view model
			bs->viewangles[i] = AngleMod(bs->viewangles[i]);
			bs->ideal_viewangles[i] = AngleMod(bs->ideal_viewangles[i]);
			diff = AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]);
			disired_speed = diff * factor;
			bs->viewanglespeed[i] += (bs->viewanglespeed[i] - disired_speed);
			if (bs->viewanglespeed[i] > 180) bs->viewanglespeed[i] = maxchange;
			if (bs->viewanglespeed[i] < -180) bs->viewanglespeed[i] = -maxchange;
			anglespeed = bs->viewanglespeed[i];
			if (anglespeed > maxchange) anglespeed = maxchange;
			if (anglespeed < -maxchange) anglespeed = -maxchange;
			bs->viewangles[i] += anglespeed;
			bs->viewangles[i] = AngleMod(bs->viewangles[i]);
			//demping
			bs->viewanglespeed[i] *= 0.45 * (1 - factor);
		}*/
    //Bot_Print(PRT_MESSAGE, "ideal_angles %f %f\n", bs->ideal_viewangles[0], bs->ideal_viewangles[1], bs->ideal_viewangles[2]);`
    //bs->viewangles[i] = bs->ideal_viewangles[i];
  }
  //bs->viewangles[PITCH] = 0;
  if (bs->viewangles[PITCH] > 180) bs->viewangles[PITCH] -= 360;
  //elementary action: view
  trap_EA_View(bs->client, bs->viewangles);
}

/*
==================
BotPointAreaNum
==================
 */
int BotPointAreaNum(vec3_t origin) {
  int areanum, numareas, areas[10];
  vec3_t end;
  
  areanum = trap_AAS_PointAreaNum(origin);
  if (areanum) return areanum;
  VectorCopy(origin, end);
  end[2] += 10;
  numareas = trap_AAS_TraceAreas(origin, end, areas, NULL, 10);
  if (numareas > 0) return areas[0];
  return 0;
}


//========================================================================
//			visualizition and debugging tools
//========================================================================



//==================
// Bot_Print
//==================
static char last_msg[2048];
void QDECL Bot_Print(botprint_t type, char *fmt, ...) {
  char str[2048];
  va_list ap;
  
  va_start(ap, fmt);
  Q_vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);
  
  //check if the message is the same. If it is, just print a dot.
  if(Q_stricmp(str, last_msg)) {
    //messages differ, so copy the latest message over to the last_msg
    Q_strncpyz(last_msg, str, 2048);
  } else {
    return;
  }
  switch(type) {
    case BPMSG: {
      G_Printf("%s", str);
      break;
    }
    case BPERROR: {
      G_Printf( S_COLOR_YELLOW "Warning: %s", str );
      break;
    }
    case BPDEBUG: {
      if( bot_developer.integer)
        G_Printf( S_COLOR_GREEN "%s", str);
      break;
    }
    default: {
      G_Printf( "unknown print type\n" );
      break;
    }
  }
}

// append key-value pair to bot's ConfigString
void BotAddInfo(bot_state_t* bs, char* key, char* value ){    
  if( !bot_developer.integer ) return;
  Info_SetValueForKey(bs->hudinfo, key, value );
}

void BotShowViewAngles(bot_state_t* bs){
  vec3_t forward, end;
  AngleVectors( bs->ideal_viewangles, forward, NULL, NULL );
  //VectorScale(forward, 500, forward);
  //VectorAdd(bs->origin, forward, end);
  VectorMA(bs->origin, 300, forward, end);
  DebugLineDouble(bs->origin, end, 4);
}

void BotTestAAS(vec3_t origin) {
  int areanum;
  aas_areainfo_t info;
  
  //	trap_Cvar_Update(&bot_testclusters);	
  //	if (bot_testclusters.integer) {
  if (!trap_AAS_Initialized()) return;
  areanum = BotPointAreaNum(origin);
  if (!areanum)
    trap_SendServerCommand( -1, va("cp \"Solid! \n\"") ); 
  else {
    trap_AAS_AreaInfo(areanum, &info);
    trap_SendServerCommand( -1, va("cp \"area %d, cluster %d \n\"", areanum, info.cluster) ); 
  }
  //	}
}

/*
==================
BotAI_GetClientState
==================
 */
int BotAI_GetClientState( int clientNum, playerState_t *state ) {
  gentity_t	*ent;
  
  ent = &g_entities[clientNum];
  if ( !ent->inuse ) {
    return qfalse;
  }
  if ( !ent->client ) {
    return qfalse;
  }
  
  memcpy( state, &ent->client->ps, sizeof(playerState_t) );
  return qtrue;
}

/*
==================
BotSetupForMovement
==================
 */
void BotSetupForMovement(bot_state_t *bs) {
  bot_initmove_t initmove;
  
  memset(&initmove, 0, sizeof(bot_initmove_t));
  VectorCopy(bs->cur_ps.origin, initmove.origin);
  VectorCopy(bs->cur_ps.velocity, initmove.velocity);
  VectorClear(initmove.viewoffset);
  initmove.viewoffset[2] += bs->cur_ps.viewheight;
  initmove.entitynum = bs->entitynum;
  initmove.client = bs->client;
  initmove.thinktime = bs->thinktime;
  //set the onground flag
  if (bs->cur_ps.groundEntityNum != ENTITYNUM_NONE) initmove.or_moveflags |= MFL_ONGROUND;
  //set the teleported flag
  if ((bs->cur_ps.pm_flags & PMF_TIME_KNOCKBACK) && (bs->cur_ps.pm_time > 0)) {
    initmove.or_moveflags |= MFL_TELEPORTED;
  }
  //set the waterjump flag
  if ((bs->cur_ps.pm_flags & PMF_TIME_WATERJUMP) && (bs->cur_ps.pm_time > 0)) {
    initmove.or_moveflags |= MFL_WATERJUMP;
  }
  //set presence type
  if (bs->cur_ps.pm_flags & PMF_DUCKED) initmove.presencetype = PRESENCE_CROUCH;
  else initmove.presencetype = PRESENCE_NORMAL;
  //
  VectorCopy(bs->viewangles, initmove.viewangles);
  //
  trap_BotInitMoveState(bs->ms, &initmove);
}

/*
==================
BotSameTeam
==================
 */
int BotSameTeam(bot_state_t *bs, int entnum) {
  char info1[1024], info2[1024];
  
  if (bs->client < 0 || bs->client >= MAX_CLIENTS) {
    //BotAI_Print(PRT_ERROR, "BotSameTeam: client out of range\n");
    return qfalse;
  }
  if (entnum < 0 || entnum >= MAX_CLIENTS) {
    //BotAI_Print(PRT_ERROR, "BotSameTeam: client out of range\n");
    return qfalse;
  }
  
  trap_GetConfigstring(CS_PLAYERS+bs->client, info1, sizeof(info1));
  trap_GetConfigstring(CS_PLAYERS+entnum, info2, sizeof(info2));
  
  if (atoi(Info_ValueForKey(info1, "t")) == atoi(Info_ValueForKey(info2, "t"))) return qtrue;
  
  return qfalse;
}

qboolean BotIsAlive(bot_state_t* bs){
  int t = bs->cur_ps.pm_type;
  
  return ( t == PM_NORMAL || t == PM_JETPACK || t == PM_GRABBED) ;
}

qboolean BotIntermission(bot_state_t *bs) {
  if (level.intermissiontime) return qtrue;
  return (bs->cur_ps.pm_type == PM_FREEZE || bs->cur_ps.pm_type == PM_INTERMISSION);
}

int CheckAreaForGoal(vec3_t origin, vec3_t bestorigin){	// copy of BotFuzzyPointReachabilityArea
  int firstareanum, j, x, y, z;
  int areas[10], numareas, areanum, bestareanum;
  float dist, bestdist;
  vec3_t points[10], v, end;
  
  firstareanum = 0;
  areanum = trap_AAS_PointAreaNum(origin);
  if (areanum){
    firstareanum = areanum;
    if (trap_AAS_AreaReachability(areanum)){
      return areanum;
    }
  } //end if
  VectorCopy(origin, end);
  end[2] += 4;
  numareas = trap_AAS_TraceAreas(origin, end, areas, points, 10);
  for (j = 0; j < numareas; j++)
  {
    if (trap_AAS_AreaReachability(areas[j])) return areas[j];
  } //end for
  bestdist = 999999;
  bestareanum = 0;
  for (z = 1; z >= -1; z -= 1){
    for (x = 1; x >= -1; x -= 1){
      for (y = 1; y >= -1; y -= 1){
        VectorCopy(origin, end);
        end[0] += x * 8;
        end[1] += y * 8;
        end[2] += z * 12;
        numareas = trap_AAS_TraceAreas(origin, end, areas, points, 10);
        for (j = 0; j < numareas; j++)
        {
          if (trap_AAS_AreaReachability(areas[j]))
          {
            VectorSubtract(points[j], origin, v);
            dist = VectorLength(v);
            if (dist < bestdist)
            {
              bestareanum = areas[j];
              bestdist = dist;
              VectorCopy(points[j], bestorigin);
            } //end if
          } //end if
          if (!firstareanum) firstareanum = areas[j];
        } //end for
      } //end for
    } //end for
    if (bestareanum){
      return bestareanum;
      //VectorCopy(bestorigin, origin);
    }
  } //end for
  return firstareanum;
} //end of the function BotFuzzyPointReachabilityArea

qboolean CheckReachability(bot_goal_t* goal){
  // not reachable? check below and above
  if( !trap_AAS_AreaReachability(goal->areanum) ){
    vec3_t bestorigin;
    goal->areanum = CheckAreaForGoal(goal->origin, bestorigin);
    //Com_Printf( "Area num is %d\n", goal->areanum );
    // now reachable?
    if( trap_AAS_AreaReachability(goal->areanum) ){
      //Com_Printf( "Entity is harder to reach\n" );
      VectorCopy(bestorigin, goal->origin);			
    } else {
      return qfalse;
    }
  }
  return qtrue;
}
void TraceDownToGround(bot_state_t* bs, vec3_t origin, vec3_t out){
  bsp_trace_t trace;
  vec3_t down;
  VectorCopy( origin, down);
  down[2] = down[2] - 1000;
  BotAI_Trace(&trace, origin, NULL, NULL, down, bs->entitynum, CONTENTS_SOLID);	
  VectorCopy( trace.endpos, out);
  out[2] += 10.0;
  //Com_Printf( "Original origin is at %f, %f, %f\n", origin[0], origin[1], origin[2] );
  //Com_Printf( "Neworigin is at %f, %f, %f\n", out[0], out[1], out[2] );
}
void OrgToGoal(vec3_t org, bot_goal_t* goal){
  VectorCopy( org, goal->origin);
  goal->areanum = BotPointAreaNum(goal->origin);
  VectorSet(goal->mins, -10, -10, -10);
  VectorSet(goal->maxs, 10, 10, 10);
}

int GetWalkingDist( bot_state_t* bs, vec3_t origin ){
  bot_goal_t goal;
  OrgToGoal(origin, &goal);
  if(!CheckReachability(&goal)){
    //    Bot_Print( BPDEBUG, "cant find a goal for entity %s \n", ent->classname);
    return 10;
  }
  //  arenanum = BotPointAreaNum( bestorigin );
  return trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->cur_ps.origin, goal.areanum, bs->tfl);
}

// sets goal for the first entity of the given type. CHEAT
qboolean BotGoalForBuildable(bot_state_t* bs, bot_goal_t* goal, int bclass){
  int i;
  gentity_t* ent;
  
  for( i = 1, ent = g_entities + i ; i < level.num_entities ; i++, ent++ ){
    // filter with s.eType = ET_BUILDABLE ?
    if( BG_Buildable(ent->s.modelindex)->team ==  TEAM_HUMANS && !ent->powered )
      continue;
    if( ent->s.modelindex != bclass)
      continue;
    // skip dead buildings
    if(ent->health <= 0) continue;
    
    // create a goal
    //Com_Printf( "Buildable goal checking ---\n");
    OrgToGoal(ent->s.origin, goal);
    
    VectorCopy( BG_BuildableConfig( ent->s.modelindex)->mins, goal->mins  );
    VectorCopy( BG_BuildableConfig( ent->s.modelindex)->maxs, goal->maxs  );
    
    //Com_Printf( "Goal is at %f, %f, %f\n", goal->origin[0], goal->origin[1], goal->origin[2] );
    //Com_Printf( "Min of goal is %f, %f, %f\n", goal->mins[0], goal->mins[1], goal->mins[2] );
    //Com_Printf( "Max of goal is %f, %f, %f\n", goal->maxs[0], goal->maxs[1], goal->maxs[2] );
    goal->entitynum = i;
    
    if(!CheckReachability(goal)){
      Bot_Print( BPDEBUG, "cant find a goal for entity %s \n", ent->classname);
      continue;
    }
    
    bs->enemyent = ent;
    return qtrue;
  }
  return qfalse;
}
qboolean Nullcheckfuct( bot_state_t* bs, int entnum, float dist ){
  return qtrue;
}
// sets goal for the closest entity of the given type. CHEAT
qboolean BotGoalForClosestBuildable(bot_state_t* bs, bot_goal_t* goal, int bclass, qboolean(*check)(bot_state_t* bs, int entnum, float dist )){
  int i, closest=-1;
  gentity_t* ent;
  gentity_t* closesttarget;
  float dist, closestdist = 100000000;
  vec3_t origin;
  
  for( i = 1, ent = g_entities + i ; i < level.num_entities ; i++, ent++ ){
    if( ent->s.eType != ET_BUILDABLE) continue;
    // filter with s.eType = ET_BUILDABLE ?
    if( ent->s.modelindex == bclass ){
      
      // skip dead buildings
      if(ent->health <= 0) continue;
      //calculate the distance towards the enemy
      //			VectorSubtract(ent->s.origin, bs->origin, dir);
      //			dist = VectorLength(dir);
      dist = GetWalkingDist( bs, ent->s.origin);
      if ( !check(bs, i, dist) ) continue;
      if(dist < closestdist){
        //closestentinfo = entinfo;
        closestdist = dist;
        //closesttarget = &target;
        closest = i;
      }
    }
  }
  if(closest < 0) return qfalse; //No emenys within distance
  //	BotAddInfo(bs, "closest enemy building", ent->classname);	
  closesttarget =  &g_entities[ closest ];
  // create a goal
  //TraceDownToGround(bs, closesttarget->s.origin, origin);
  
  VectorCopy(closesttarget->s.origin, origin);
  //VectorCopy(temp, origin);
  if( closesttarget->s.modelindex == BA_H_MEDISTAT )
    origin[2] += 5.0;
  if( closesttarget->s.modelindex == BA_H_SPAWN )
    origin[2] += 10.0;
  if( closesttarget->s.modelindex == BA_A_BOOSTER )
    origin[2] += 10.0;
  OrgToGoal(origin, goal);
  //Com_Printf( "Goal is at %f, %f, %f\n", goal->origin[0], goal->origin[1], goal->origin[2] );
  VectorCopy( BG_BuildableConfig( ent->s.modelindex)->mins, goal->mins  );
  VectorCopy( BG_BuildableConfig( ent->s.modelindex)->maxs, goal->maxs  );
  goal->entitynum = closest;
  //TraceDownToGround(bs, goal);
  if(CheckReachability(goal)){
    return qtrue;			
  }
  Bot_Print( BPDEBUG, "cant find a goal for buildable %s \n", BG_Buildable(bclass)->entityName);
  return qfalse;
}
/*
==================
BotFindEnemy
==================
 */

int BotFindEnemy(bot_state_t *bs, int curenemy) {
  int i, closest=-1;
  gentity_t* target;
  gentity_t* closesttarget;
  aas_entityinfo_t entinfo, closestentinfo;
  //int closestdist;
  float dist, closestdist = 1000;
  vec3_t dir;
  
  //i = BotFindEnemy(bs, bs->enemy);
  // check list for enemies
  for (i = 0; i < MAX_CLIENTS; i++) {
    target = &g_entities[ i ];
    if( target->health <= 0 ) continue;
    //for (o = 0; o < total_entities; i =  entityList[ o++ ]) {
    if(i > MAX_CLIENTS) break;
    
    if (i == bs->client) continue;
    //if it's the current enemy
    //if (i == curenemy) return i;
    
    BotEntityInfo(i, &entinfo);
    //
    if (!entinfo.valid) continue;
    //if the enemy isn't dead and the enemy isn't the bot self
    if (entinfo.number == bs->entitynum) continue;
    if( !BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 90, i) && !bs->inventory[BI_HELMET] ) continue;
    //if(bs->inventory[BI_HELMET] || )
    
    //calculate the distance towards the enemy
    VectorSubtract(entinfo.origin, bs->origin, dir);
    dist = VectorLength(dir);
    BotAddInfo(bs, "dist", va("%f",dist) );
    //if on the same team
    if (BotSameTeam(bs, i)) continue;
    if(dist < closestdist){
      //closestentinfo = entinfo;
      closestdist = dist;
      //closesttarget = &target;
      closest = i;
    }
  }
  if(closest < 0) return -1; //No emenys within distance
  BotEntityInfo(closest, &closestentinfo);	
  closesttarget =  &g_entities[ closest ];
  //	BotAddInfo(bs, "closest enemy", closesttarget->client->pers.netname);	
  return closest;
}

qboolean BotGoalForEnemy(bot_state_t *bs, bot_goal_t* goal){
  int i, closest=-1;
  gentity_t* target;
  gentity_t* closesttarget;
  aas_entityinfo_t entinfo, closestentinfo;
  //int closestdist;
  float dist, closestdist = 1000;
  vec3_t dir;
  
  //i = BotFindEnemy(bs, bs->enemy);
  // check list for enemies
  for (i = 0; i < MAX_CLIENTS; i++) {
    target = &g_entities[ i ];
    if( target->health <= 0 ) continue;
    //for (o = 0; o < total_entities; i =  entityList[ o++ ]) {
    if(i > MAX_CLIENTS) break;
    
    if (i == bs->client) continue;
    //if it's the current enemy
    //if (i == curenemy) return i;
    
    BotEntityInfo(i, &entinfo);
    //
    if (!entinfo.valid) continue;
    //if the enemy isn't dead and the enemy isn't the bot self
    if (entinfo.number == bs->entitynum) continue;
    //if( !BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy) ){
    //if(bs->inventory[BI_HELMET] || )
    
    //calculate the distance towards the enemy
    VectorSubtract(entinfo.origin, bs->origin, dir);
    dist = VectorLength(dir);
    BotAddInfo(bs, "dist", va("%f",dist) );
    //if on the same team
    if (BotSameTeam(bs, i)) continue;
    if(dist < closestdist){
      //closestentinfo = entinfo;
      closestdist = dist;
      //closesttarget = &target;
      closest = i;
    }
  }
  if(closest < 0) return qfalse; //No emenys within distance
  BotEntityInfo(closest, &closestentinfo);	
  closesttarget =  &g_entities[ closest ];
  //	BotAddInfo(bs, "closest enemy", closesttarget->client->pers.netname);
  
  // create a goal
  OrgToGoal(closesttarget->s.origin, goal);
  //BG_FindBBoxForBuildable( ent->s.modelindex, goal->mins, goal->maxs );
  VectorCopy( BG_BuildableConfig( closesttarget->client->pers.classSelection )->mins, goal->mins  );
  VectorCopy( BG_BuildableConfig( closesttarget->client->pers.classSelection )->maxs, goal->maxs  );
  goal->entitynum = bs->enemy = closest;
  // not reachable? check below and above
  if ( !trap_AAS_AreaReachability(goal->areanum) ) {
    vec3_t bestorigin;
    goal->areanum = CheckAreaForGoal(goal->origin, bestorigin);
    // now reachable?
    if (trap_AAS_AreaReachability(goal->areanum) ) {
      VectorCopy(bestorigin, goal->origin);
    } else {
      Bot_Print(BPDEBUG, "cant find a goal for player %s \n",
                closesttarget->client->pers.netname);
      return qfalse;
    }
    bs->enemyent = closesttarget;
    return qtrue;
  }
  //Bot_Print( BPDEBUG, "didn't find an enemy\n");
  return qtrue;
}
// sets goal for the first player of the given team. CHEAT
qboolean BotGoalForNearestEnemy(bot_state_t* bs, bot_goal_t* goal) {
  int              enemy;
  aas_entityinfo_t entinfo;
  gentity_t* enemyent;
  
  enemy = BotFindEnemy(bs, bs->enemy);
  if(enemy<0) return qfalse;
  
  BotEntityInfo(enemy, &entinfo);
  enemyent =  &g_entities[ enemy ];
  
  OrgToGoal(entinfo.origin, goal);
  VectorCopy( BG_BuildableConfig( enemyent->s.modelindex)->mins, goal->mins  );
  VectorCopy( BG_BuildableConfig( enemyent->s.modelindex)->maxs, goal->maxs  );
  goal->entitynum = enemy;
  if(CheckReachability(goal)){
    return qtrue;			
  }
  Bot_Print( BPDEBUG, "cant find a goal for entity %s \n", enemyent->classname);
  return qfalse;
}

//////
// VISIBILITY
/////

qboolean BotInFieldOfVision(vec3_t viewangles, float fov, vec3_t angles)
{
  int i;
  float diff, angle;
  
  for (i = 0; i < 2; i++) {
    angle = AngleMod(viewangles[i]);
    angles[i] = AngleMod(angles[i]);
    diff = angles[i] - angle;
    if (angles[i] > angle) {
      if (diff > 180.0) diff -= 360.0;
    }
    else {
      if (diff < -180.0) diff += 360.0;
    }
    if (diff > 0) {
      if (diff > fov * 0.5) return qfalse;
    }
    else {
      if (diff < -fov * 0.5) return qfalse;
    }
  }
  return qtrue;
}

float BotEntityVisible(int viewer, vec3_t eye, vec3_t viewangles, float fov, int ent) {
  int i, contents_mask, passent, hitent, infog, inwater, otherinfog, pc;
  float squaredfogdist, waterfactor, vis, bestvis;
  bsp_trace_t trace;
  aas_entityinfo_t entinfo;
  vec3_t dir, entangles, start, end, middle;
  
  //calculate middle of bounding box
  BotEntityInfo(ent, &entinfo);
  VectorAdd(entinfo.mins, entinfo.maxs, middle);
  VectorScale(middle, 0.5, middle);
  VectorAdd(entinfo.origin, middle, middle);
  //check if entity is within field of vision
  VectorSubtract(middle, eye, dir);
  vectoangles(dir, entangles);
  if (!BotInFieldOfVision(viewangles, fov, entangles)) return 0;
  //
  pc = trap_AAS_PointContents(eye);
  infog = (pc & CONTENTS_FOG);
  inwater = (pc & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER));
  //
  bestvis = 0;
  for (i = 0; i < 3; i++) {
    //if the point is not in potential visible sight
    //if (!AAS_inPVS(eye, middle)) continue;
    //
    contents_mask = CONTENTS_SOLID|CONTENTS_PLAYERCLIP;
    passent = viewer;
    hitent = ent;
    VectorCopy(eye, start);
    VectorCopy(middle, end);
    //if the entity is in water, lava or slime
    if (trap_AAS_PointContents(middle) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER)) {
      contents_mask |= (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
    }
    //if eye is in water, lava or slime
    if (inwater) {
      if (!(contents_mask & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))) {
        passent = ent;
        hitent = viewer;
        VectorCopy(middle, start);
        VectorCopy(eye, end);
      }
      contents_mask ^= (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
    }
    //trace from start to end
    BotAI_Trace(&trace, start, NULL, NULL, end, passent, contents_mask);
    //if water was hit
    waterfactor = 1.0;
    if (trace.contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER)) {
      //if the water surface is translucent
      if (1) {
        //trace through the water
        contents_mask &= ~(CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
        BotAI_Trace(&trace, trace.endpos, NULL, NULL, end, passent, contents_mask);
        waterfactor = 0.5;
      }
    }
    //if a full trace or the hitent was hit
    if (trace.fraction >= 1 || trace.ent == hitent) {
      //check for fog, assuming there's only one fog brush where
      //either the viewer or the entity is in or both are in
      otherinfog = (trap_AAS_PointContents(middle) & CONTENTS_FOG);
      if (infog && otherinfog) {
        VectorSubtract(trace.endpos, eye, dir);
        squaredfogdist = VectorLengthSquared(dir);
      }
      else if (infog) {
        VectorCopy(trace.endpos, start);
        BotAI_Trace(&trace, start, NULL, NULL, eye, viewer, CONTENTS_FOG);
        VectorSubtract(eye, trace.endpos, dir);
        squaredfogdist = VectorLengthSquared(dir);
      }
      else if (otherinfog) {
        VectorCopy(trace.endpos, end);
        BotAI_Trace(&trace, eye, NULL, NULL, end, viewer, CONTENTS_FOG);
        VectorSubtract(end, trace.endpos, dir);
        squaredfogdist = VectorLengthSquared(dir);
      }
      else {
        //if the entity and the viewer are not in fog assume there's no fog in between
        squaredfogdist = 0;
      }
      //decrease visibility with the view distance through fog
      vis = 1 / ((squaredfogdist * 0.001) < 1 ? 1 : (squaredfogdist * 0.001));
      //if entering water visibility is reduced
      vis *= waterfactor;
      //
      if (vis > bestvis) bestvis = vis;
      //if pretty much no fog
      if (bestvis >= 0.95) return bestvis;
    }
    //check bottom and top of bounding box as well
    if (i == 0) middle[2] += entinfo.mins[2];
    else if (i == 1) middle[2] += entinfo.maxs[2] - entinfo.mins[2];
  }
  return bestvis;
}
/*
==================
BotEntityDistance
==================
 */
float BotEntityDistance(bot_state_t* bs, int ent){
  aas_entityinfo_t entinfo;
  float dist;
  vec3_t dir;
  BotEntityInfo(ent, &entinfo);
  //calculate the distance towards the entity
  VectorSubtract(entinfo.origin, bs->origin, dir);
  dist = VectorLength(dir);
  return dist;	
}
/*
==================
BotAI_Trace
==================
 */
void BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask) {
  trace_t trace;
  
  trap_Trace(&trace, start, mins, maxs, end, passent, contentmask);
  //copy the trace information
  bsptrace->allsolid = trace.allsolid;
  bsptrace->startsolid = trace.startsolid;
  bsptrace->fraction = trace.fraction;
  VectorCopy(trace.endpos, bsptrace->endpos);
  bsptrace->plane.dist = trace.plane.dist;
  VectorCopy(trace.plane.normal, bsptrace->plane.normal);
  bsptrace->plane.signbits = trace.plane.signbits;
  bsptrace->plane.type = trace.plane.type;
  bsptrace->surface.value = trace.surfaceFlags;
  bsptrace->ent = trace.entityNum;
  bsptrace->exp_dist = 0;
  bsptrace->sidenum = 0;
  bsptrace->contents = 0;
}


/*
==================
EntityIsInvisible
==================
 */
qboolean EntityIsInvisible(aas_entityinfo_t *entinfo) {
  return qfalse;
}

/*
===============
G_BuildableRange

Check whether a point is within some range of a type of buildable
===============
 */
qboolean AI_BuildableWalkingRange( bot_state_t* bs, int r, buildable_t buildable )
{
  //  int       entityList[ MAX_GENTITIES ];
  int       i;
  gentity_t *ent;
  int dist;
  vec3_t origin;
  
  for( i = 1, ent = g_entities + i ; i < level.num_entities ; i++, ent++ )
  {
    if( ent->s.eType != ET_BUILDABLE )
      continue;
    
    if( BG_Buildable(ent->s.modelindex)->team ==  TEAM_HUMANS && !ent->powered )
      continue;
    if( ent->s.modelindex != buildable)
      continue;
    if(!ent->spawned )
      continue;
    VectorCopy(ent->s.origin, origin);
    if( buildable == BA_H_MEDISTAT )
      origin[2] += 5.0;
    dist = GetWalkingDist( bs, origin);
    if(dist == 0)
      continue;
    //    Com_Printf("%d ", dist);
    if( dist < r){
      //      Com_Printf("\n");
      return qtrue;
    }
    
  } 
  //  Com_Printf("\n");
  return qfalse;
}
/*
==================
EntityIsDead
==================
 */
qboolean EntityIsDead(aas_entityinfo_t *entinfo) {
  playerState_t ps;
  
  if (entinfo->number >= 0 && entinfo->number < MAX_CLIENTS) {
    //retrieve the current client state
    BotAI_GetClientState( entinfo->number, &ps );
    if (ps.pm_type != PM_NORMAL) return qtrue;
  }
  return qfalse;
}
/*
==================
EntityIsShooting
==================
 */
qboolean EntityIsShooting(aas_entityinfo_t *entinfo) {
  if (entinfo->flags & EF_FIRING) {
    return qtrue;
  }
  return qfalse;
}
/*
==================
EntityIsChatting
==================
 */
qboolean EntityIsChatting(aas_entityinfo_t *entinfo) {
  //  if (entinfo->flags & EF_TALK) {
  //    return qtrue;
  //  }
  return qfalse;
}
/*
==================
ClientOnSameTeamFromName
==================
 */
int ClientOnSameTeamFromName(bot_state_t *bs, char *name) {
  int i;
  char buf[MAX_INFO_STRING];
  static int maxclients;
  
  if (!maxclients)
    maxclients = trap_Cvar_VariableIntegerValue("sv_maxclients");
  for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
    if (!BotSameTeam(bs, i))
      continue;
    trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
    Q_CleanStr( buf );
    if (!Q_stricmp(Info_ValueForKey(buf, "n"), name)) return i;
  }
  return -1;
}

/*
==================
BotSetTeleportTime
==================
 */
void BotSetTeleportTime(bot_state_t *bs) {
  if ((bs->cur_ps.eFlags ^ bs->last_eFlags) & EF_TELEPORT_BIT) {
    bs->teleport_time = FloatTime();
  }
  bs->last_eFlags = bs->cur_ps.eFlags;
}

/*
==================
BotIsDead
==================
 */
qboolean BotIsDead(bot_state_t *bs) {
  return (bs->cur_ps.pm_type == PM_DEAD);
}

/*
==================
BotIsObserver
==================
 */
qboolean BotIsObserver(bot_state_t *bs) {
  char buf[MAX_INFO_STRING];
  if (bs->cur_ps.pm_type == PM_SPECTATOR) return qtrue;
  trap_GetConfigstring(CS_PLAYERS+bs->client, buf, sizeof(buf));
  if (atoi(Info_ValueForKey(buf, "t")) == TEAM_NONE) return qtrue;
  return qfalse;
}

/*
==================
BotInLavaOrSlime
==================
 */
qboolean BotInLavaOrSlime(bot_state_t *bs) {
  vec3_t feet;
  
  VectorCopy(bs->origin, feet);
  feet[2] -= 23;
  return (trap_AAS_PointContents(feet) & (CONTENTS_LAVA|CONTENTS_SLIME));
}

/*
==================
BotCreateWayPoint
==================
 */
bot_waypoint_t *BotCreateWayPoint(char *name, vec3_t origin, int areanum) {
  bot_waypoint_t *wp;
  vec3_t waypointmins = {-8, -8, -8}, waypointmaxs = {8, 8, 8};
  
  wp = botai_freewaypoints;
  if ( !wp ) {
    BotAI_Print( PRT_WARNING, "BotCreateWayPoint: Out of waypoints\n" );
    return NULL;
  }
  botai_freewaypoints = botai_freewaypoints->next;
  
  Q_strncpyz( wp->name, name, sizeof(wp->name) );
  VectorCopy(origin, wp->goal.origin);
  VectorCopy(waypointmins, wp->goal.mins);
  VectorCopy(waypointmaxs, wp->goal.maxs);
  wp->goal.areanum = areanum;
  wp->next = NULL;
  wp->prev = NULL;
  return wp;
}

/*
==================
BotFindWayPoint
==================
 */
bot_waypoint_t *BotFindWayPoint(bot_waypoint_t *waypoints, char *name) {
  bot_waypoint_t *wp;
  
  for (wp = waypoints; wp; wp = wp->next) {
    if (!Q_stricmp(wp->name, name)) return wp;
  }
  return NULL;
}

/*
==================
BotFreeWaypoints
==================
 */
void BotFreeWaypoints(bot_waypoint_t *wp) {
  bot_waypoint_t *nextwp;
  
  for (; wp; wp = nextwp) {
    nextwp = wp->next;
    wp->next = botai_freewaypoints;
    botai_freewaypoints = wp;
  }
}

/*
==================
BotInitWaypoints
==================
 */
void BotInitWaypoints(void) {
  int i;
  
  botai_freewaypoints = NULL;
  for (i = 0; i < MAX_WAYPOINTS; i++) {
    botai_waypoints[i].next = botai_freewaypoints;
    botai_freewaypoints = &botai_waypoints[i];
  }
}

/*
==================
TeamPlayIsOn
==================
 */
qboolean TeamPlayIsOn(void) {
  return qtrue;
}

bot_waypoint_t botai_waypoints[MAX_WAYPOINTS];
bot_waypoint_t *botai_freewaypoints;

/*
==================
BotSetEntityNumForGoalWithModel
==================
 */
void BotSetEntityNumForGoalWithModel(bot_goal_t *goal, int eType, char *modelname) {
  gentity_t *ent;
  int i, modelindex;
  vec3_t dir;
  
  modelindex = G_ModelIndex( modelname );
  ent = &g_entities[0];
  for (i = 0; i < level.num_entities; i++, ent++) {
    if ( !ent->inuse ) {
      continue;
    }
    if ( eType && ent->s.eType != eType) {
      continue;
    }
    if (ent->s.modelindex != modelindex) {
      continue;
    }
    VectorSubtract(goal->origin, ent->s.origin, dir);
    if (VectorLengthSquared(dir) < Square(10)) {
      goal->entitynum = i;
      return;
    }
  }
}

/*
==================
BotReachedGoal
==================
 */
int BotReachedGoal(bot_state_t *bs, bot_goal_t *goal) {
  if (goal->flags & GFL_ITEM) {
    //if touching the goal
    if (trap_BotTouchingGoal(bs->origin, goal)) {
      if (!(goal->flags & GFL_DROPPED)) {
        trap_BotSetAvoidGoalTime(bs->gs, goal->number, -1);
      }
      return qtrue;
    }
    //if the goal isn't there
    if (trap_BotItemGoalInVisButNotVisible(bs->entitynum, bs->eye, bs->viewangles, goal)) {
      /*
      float avoidtime;
      int t;

      avoidtime = trap_BotAvoidGoalTime(bs->gs, goal->number);
      if (avoidtime > 0) {
        t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, goal->areanum, bs->tfl);
        if ((float) t * 0.009 < avoidtime)
          return qtrue;
      }
       */
      return qtrue;
    }
    //if in the goal area and below or above the goal and not swimming
    if (bs->areanum == goal->areanum) {
      if (bs->origin[0] > goal->origin[0] + goal->mins[0] && bs->origin[0] < goal->origin[0] + goal->maxs[0]) {
        if (bs->origin[1] > goal->origin[1] + goal->mins[1] && bs->origin[1] < goal->origin[1] + goal->maxs[1]) {
          if (!trap_AAS_Swimming(bs->origin)) {
            return qtrue;
          }
        }
      }
    }
  }
  else if (goal->flags & GFL_AIR) {
    //if touching the goal
    if (trap_BotTouchingGoal(bs->origin, goal)) return qtrue;
    //if the bot got air
    if (bs->lastair_time > FloatTime() - 1) return qtrue;
  }
  else {
    //if touching the goal
    if (trap_BotTouchingGoal(bs->origin, goal)) return qtrue;
  }
  return qfalse;
}

/*
==================
EasyClientName
==================
 */
char *EasyClientName(int client, char *buf, int size) {
  int i;
  char *str1, *str2, *ptr, c;
  char name[128];
  
  strcpy(name, ClientName(client, name, sizeof(name)));
  for (i = 0; name[i]; i++) name[i] &= 127;
  //remove all spaces
  for (ptr = strstr(name, " "); ptr; ptr = strstr(name, " ")) {
    memmove(ptr, ptr+1, strlen(ptr+1)+1);
  }
  //check for [x] and ]x[ clan names
  str1 = strstr(name, "[");
  str2 = strstr(name, "]");
  if (str1 && str2) {
    if (str2 > str1) memmove(str1, str2+1, strlen(str2+1)+1);
    else memmove(str2, str1+1, strlen(str1+1)+1);
  }
  //check for {x} and }x{ clan names
  str1 = strstr(name, "{");
  str2 = strstr(name, "}");
  if (str1 && str2) {
    if (str2 > str1) memmove(str1, str2+1, strlen(str2+1)+1);
    else memmove(str2, str1+1, strlen(str1+1)+1);
  }
  //remove Mr prefix
  if ((name[0] == 'm' || name[0] == 'M') &&
      (name[1] == 'r' || name[1] == 'R')) {
    memmove(name, name+2, strlen(name+2)+1);
  }
  //only allow lower case alphabet characters
  ptr = name;
  while(*ptr) {
    c = *ptr;
    if ((c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '_') {
      ptr++;
    }
    else if (c >= 'A' && c <= 'Z') {
      *ptr += 'a' - 'A';
      ptr++;
    }
    else {
      memmove(ptr, ptr+1, strlen(ptr + 1)+1);
    }
  }
  strncpy(buf, name, size-1);
  buf[size-1] = '\0';
  return buf;
}
