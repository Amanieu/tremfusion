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
#define AS_OVER_RT3         ((ALIENSENSE_RANGE*0.5f)/M_ROOT3)
typedef enum{
	AS_SPAWN,
	AS_EVO,
	AS_ATTACK
}astates_t;

static void ABotUpdateInventory(bot_state_t* bs);
static float ABotFindWidth(int weapon);
static qboolean ABotFindEnemy(bot_state_t* bs);
static float ABotFindRange(int weapon);

static float ABotFindWidth(int weapon){ //, float width, float range){
	float width, range;
	switch(weapon){		
		case WP_ABUILD2:
			range = ABUILDER_CLAW_RANGE;
			width = ABUILDER_CLAW_WIDTH;
			break;
		case WP_ALEVEL0:
			range = LEVEL0_BITE_RANGE;
			width = LEVEL0_BITE_WIDTH;
			break;
		case WP_ALEVEL1:
		case WP_ALEVEL1_UPG:
			range = LEVEL1_CLAW_RANGE;
			width = LEVEL1_CLAW_WIDTH;
			break;
		case WP_ALEVEL2:
		case WP_ALEVEL2_UPG:
			range = LEVEL2_CLAW_RANGE;
			width = LEVEL2_CLAW_WIDTH;
			break;
		case WP_ALEVEL3:
		case WP_ALEVEL3_UPG:
			range = LEVEL3_CLAW_RANGE;
			width = LEVEL3_CLAW_WIDTH;
			break;
		case WP_ALEVEL4:
			range = LEVEL4_CLAW_RANGE;
			width = LEVEL4_CLAW_WIDTH;
			break;
		default:
			range = 0.0f;
			width = 0.0f;
			break;
	}
	//Bot_Print( BPERROR, "weapon #: %d width: %f range: %f \n", weapon, width, range);
	return width;
}

static float ABotFindRange(int weapon){ //, float width, float range){
	float width, range;
	switch(weapon){		
		case WP_ABUILD2:
			range = ABUILDER_CLAW_RANGE;
			width = ABUILDER_CLAW_WIDTH;
			break;
		case WP_ALEVEL0:
			range = LEVEL0_BITE_RANGE;
			width = LEVEL0_BITE_WIDTH;
			break;
		case WP_ALEVEL1:
		case WP_ALEVEL1_UPG:
			range = LEVEL1_CLAW_RANGE;
			width = LEVEL1_CLAW_WIDTH;
			break;
		case WP_ALEVEL2:
		case WP_ALEVEL2_UPG:
			range = LEVEL2_CLAW_RANGE;
			width = LEVEL2_CLAW_WIDTH;
			break;
		case WP_ALEVEL3:
		case WP_ALEVEL3_UPG:
			range = LEVEL3_CLAW_RANGE;
			width = LEVEL3_CLAW_WIDTH;
			break;
		case WP_ALEVEL4:
			range = LEVEL4_CLAW_RANGE;
			width = LEVEL4_CLAW_WIDTH;
			break;
		default:
			range = 0.0f;
			width = 0.0f;
			break;
	}
	//Bot_Print( BPERROR, "weapon #: %d width: %f range: %f \n", weapon, width, range);
	return range;
}

/*
==================
BotAimAtEnemy
==================
*/
void ABotAimAtEnemy(bot_state_t *bs) {
	int i;
	vec3_t dir, bestorigin, start;
	vec3_t mins = {-4,-4,-4}, maxs = {4, 4, 4};
	aas_entityinfo_t entinfo;
	bsp_trace_t trace;
	vec3_t target;

	//if the bot has no enemy
	if (bs->enemy < 0) {
		return;
	}
	//get the enemy entity information
	BotEntityInfo(bs->enemy, &entinfo);
	//if this is not a player
	if (bs->enemy >= MAX_CLIENTS) {
		
		//if the buildable is visible
		VectorCopy(entinfo.origin, target);

		//aim at the building
		VectorSubtract(target, bs->eye, dir);
		vectoangles(dir, bs->ideal_viewangles);
		
		//set the aim target before trying to attack
		VectorCopy(target, bs->aimtarget);
		return;
	}
	
	// todo: add reaction delay (enemysight time)

	//get the enemy entity information
	BotEntityInfo(bs->enemy, &entinfo);

	// todo, predict enemy movement (velocity... lastvisorigin)

	VectorCopy(entinfo.origin, bestorigin);
	
	//get the start point shooting from
	//NOTE: the x and y projectile start offsets are ignored
	VectorCopy(bs->origin, start);
	start[2] += bs->cur_ps.viewheight;

	BotAI_Trace(&trace, start, mins, maxs, bestorigin, bs->entitynum, MASK_SHOT);
	
	//if the enemy is NOT hit
	if (trace.fraction <= 1 && trace.ent != entinfo.number) {
		bestorigin[2] += 16;
	}

	VectorCopy(bestorigin, bs->aimtarget);
	
	//get aim direction
	VectorSubtract(bestorigin, bs->eye, dir);

	//add some randomness
	for (i = 0; i < 3; i++) dir[i] += 0.06 * crandom();

	//set the ideal view angles
	vectoangles(dir, bs->ideal_viewangles);
	bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
	bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);	

	/*
	//set the view angles directly
	if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
	VectorCopy(bs->ideal_viewangles, bs->viewangles);
	trap_EA_View(bs->client, bs->viewanglescd );
	*/
}

/*
==================
BotCheckAttack
==================
*/
void ABotCheckAttack(bot_state_t *bs) {
  float fov;
  int attackentity;
  bsp_trace_t bsptrace;
  vec3_t forward, start, end, dir, angles;
  bsp_trace_t trace;
  aas_entityinfo_t entinfo;
  vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

  attackentity = bs->enemy;
  //
  BotEntityInfo(attackentity, &entinfo);
  
  // if not attacking a player
  if (attackentity >= MAX_CLIENTS) {

  }

  VectorSubtract(bs->aimtarget, bs->eye, dir);

  if (VectorLengthSquared(dir) < Square(100))
          fov = 120;
  else
          fov = 50;
  //
  vectoangles(dir, angles);
  if (!BotInFieldOfVision(bs->viewangles, fov, angles))
          return;
  BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->aimtarget, bs->client, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
  if (bsptrace.fraction < 1 && bsptrace.ent != attackentity)
          return;

  //get the start point shooting from
  VectorCopy(bs->origin, start);
  start[2] += bs->cur_ps.viewheight;

  AngleVectors(bs->viewangles, forward, NULL, NULL);
  //end point aiming at
  VectorMA(start, 1000, forward, end);
  //a little back to make sure not inside a very close enemy
  VectorMA(start, -12, forward, start);
  BotAI_Trace(&trace, start, mins, maxs, end, bs->entitynum, MASK_SHOT);
  //if the entity is a client
  if (trace.ent > 0 && trace.ent <= MAX_CLIENTS) {
          if (trace.ent != attackentity) {
                  //if a teammate is hit
                  if (BotSameTeam(bs, trace.ent))
                          return;
          }
  }
  
  // TODO avoid radial damage
  
  /*
  //if fire has to be release to activate weapon
  if (wi.flags & WFL_FIRERELEASED) {
          if (bs->flags & BFL_ATTACKED) {
                  trap_EA_Attack(bs->client);
          }
  }
  else {
  */
          trap_EA_Attack(bs->client);
  /*}
  bs->flags ^= BFL_ATTACKED;
  */
}

qboolean HumansNearby(bot_state_t* bs){
	int i, num;
	gentity_t *other;
	int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { AS_OVER_RT3, AS_OVER_RT3, AS_OVER_RT3 };
  vec3_t    mins, maxs;
	//check there are no humans nearby
  VectorAdd( bs->origin , range, maxs );
  VectorSubtract( bs->origin, range, mins );

  num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    other = &g_entities[ entityList[ i ] ];

    if( ( other->client && other->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) ||
        ( other->s.eType == ET_BUILDABLE && BG_Buildable( other->s.modelindex )->team ==  TEAM_HUMANS ) )
    {
      return qtrue;
    }
  }
  return qfalse;
}
 
// armoury AI: buy stuff depending on credits (stage, situation)
#define CanEvoTo(x) -1 != BG_ClassCanEvolveFromTo( currentClass, x, \
                                             bs->cur_ps.persistant[ PERS_CREDIT ], \
                                             g_alienStage.integer, 0 )

qboolean ABotClassOK(bot_state_t* bs){
  int numEvos;
  int  currentClass = bs->ent->client->ps.stats[ STAT_CLASS ];
  if(!HumansNearby(bs)){  
    numEvos = bs->inventory[BI_CREDITS] + BG_Class(currentClass)->value;
    switch(g_alienStage.integer){
      case 0:
      case 1:
        if( CanEvoTo( PCL_ALIEN_LEVEL3 ) && currentClass != PCL_ALIEN_LEVEL3) return qfalse;
        break;
      case 2:
        if( CanEvoTo( PCL_ALIEN_LEVEL4 ) && currentClass != PCL_ALIEN_LEVEL4) return qfalse;
        break;
    }
  } else {
    return qtrue;
  }
  return qtrue;
}

void ABotEvolve(bot_state_t* bs){
  int numEvos;
  int  currentClass = bs->ent->client->ps.stats[ STAT_CLASS ];
  numEvos = bs->inventory[BI_CREDITS] + BG_Class(currentClass)->cost;
  switch(g_alienStage.integer){
    case 0:
    case 1:
      if( CanEvoTo( PCL_ALIEN_LEVEL3 ) )
        trap_EA_Command(bs->client, "class level3" );
        break;
    case 2:
      if( CanEvoTo( PCL_ALIEN_LEVEL4 ) )
        trap_EA_Command(bs->client, "class level4" );
      break;
  } 
  //now update your inventory
  ABotUpdateInventory(bs);
}

void ABotCheckRespawn(bot_state_t* bs){
	char buf[144];

  if( bs->cur_ps.pm_type == PM_DEAD ||
      bs->cur_ps.pm_type == PM_SPECTATOR ||
      bs->cur_ps.pm_type == PM_FREEZE )
  {
    //on the spawn queue?
    if( bs->cur_ps.pm_flags & PMF_QUEUED)
      return;
    Com_sprintf(buf, sizeof(buf), "class %s", "level0");
    trap_EA_Command(bs->client, buf);   	
  }
}

// inventory becomes quite useless, thanks to the BG_Inventory functions..
static void ABotUpdateInventory(bot_state_t* bs){
  
  bs->inventory[BI_HEALTH] = bs->cur_ps.stats[ STAT_HEALTH ];
  bs->inventory[BI_CREDITS] = bs->cur_ps.persistant[ PERS_CREDIT ];
  bs->inventory[BI_CLASS] = bs->ent->client->pers.classSelection;
  bs->inventory[BI_WEAPON] = bs->ent->client->ps.weapon;
  //Bot_Print(BPMSG, "\nI have %d health, %d credits and %d stamina", bs->cur_ps.stats[STAT_HEALTH],
  //    bs->cur_ps.persistant[ PERS_CREDIT ],
  //    bs->cur_ps.stats[ STAT_STAMINA ]);
}

// ABotFindEnemy
// if enemy is found: set bs->goal and bs->enemy and return qtrue

static qboolean ABotFindEnemy(bot_state_t* bs){
	// return 0 if no alien building can be found
	
	//find alien in FOV
	if(BotGoalForEnemy( bs, &bs->goal ) ) return qtrue;
	//see if there's an enemy in range, and go for it
	if( BotGoalForClosestBuildable(  bs, &bs->goal, BA_H_MGTURRET, Nullcheckfuct ) ) return qtrue;	
	// go for eggs
	if( BotGoalForClosestBuildable(  bs, &bs->goal, BA_H_SPAWN, Nullcheckfuct ) ) return qtrue;	
	// go for OM
	if( BotGoalForClosestBuildable(  bs, &bs->goal, BA_H_REACTOR, Nullcheckfuct ) )return qtrue;	
	// haven't returned yet so no enemy found > dont do anything
	BotAddInfo(bs, "enemy", "none");
	return qfalse;
	
}

qboolean ABotAttack(bot_state_t* bs){
	bot_moveresult_t moveresult;
	vec3_t target, dir;
	
	//state transitions
	// dead -> spawn
	if( !BotIsAlive(bs) ){
		bs->state = AS_SPAWN;
		return qfalse;
	}
	
	// not satisfied with current class -> EVO
	if( !ABotClassOK(bs) ){
		bs->state = AS_EVO;
		return qfalse;
	}
	
	// report
	BotAddInfo(bs, "task", "attack");
	
	
	// find target
	if( !ABotFindEnemy(bs) )
		return qtrue;
	// shoot if target in sight
	if( BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy) ){
		// aim and check attack
		BotAddInfo(bs, "action", "shoot");
		ABotAimAtEnemy(bs);
		ABotCheckAttack(bs);
	}
	//if we are in range of the target, but can't see it, move around a bit
	// move to target
	bs->tfl = TFL_DEFAULT;
	BotAddInfo(bs, "action", va("movetogoal %d", trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->cur_ps.origin, bs->goal.areanum, bs->tfl)) );
	BotSetupForMovement(bs);
	//move towards the goal
	trap_BotMoveToGoal(&moveresult, bs->ms, &bs->goal, bs->tfl);
	BotAIBlocked(bs, &moveresult, qtrue);
	//BotMovementViewTarget(int movestate, bot_goal_t *goal, int travelflags, float lookahead, vec3_t target)
	if (trap_BotMovementViewTarget(bs->ms, &bs->goal, bs->tfl, 300, target)) {
		//Bot_Print(BPMSG, "I'm uh...fixing my view angle?");
		VectorSubtract(target, bs->origin, dir);
		vectoangles(dir, bs->ideal_viewangles);
		
	}
	
	return qtrue;
}


// go for arm and gear up
qboolean ABotEvo(bot_state_t* bs){
	//bot_goal_t goal;
	
	//state transitions
	
	// dead -> spawn
	if( !BotIsAlive(bs) ){
		bs->state = AS_SPAWN;
		return qfalse;
	}
	
	// satisfied with current equip? -> attack!
	if( ABotClassOK(bs) ){
		bs->state = AS_ATTACK;
		return qfalse;
	}
	ABotEvolve(bs);
	BotAddInfo(bs, "task", "evolving!");
	//now that we've bought ammo, move to the next state
	bs->state = AS_ATTACK;
	return qfalse;
}
	
qboolean ABotSpawn(bot_state_t* bs){
	//state transitions
	if( BotIsAlive(bs) ){
		bs->state = AS_EVO;
		return qfalse;
	}
	
	ABotCheckRespawn(bs);
	return qtrue;
}

// ABotrunstate: return qfalse if state is changed, 
// so new state can be executed within the same frame
qboolean ABotRunState(bot_state_t* bs){
	switch(bs->state){
		case AS_SPAWN:
			return ABotSpawn(bs);
		case AS_EVO:
			return ABotEvo(bs);
		case AS_ATTACK:
			return ABotAttack(bs);
		default:
			Bot_Print( BPERROR, "bs->state irregular value %d \n", bs->state);
			bs->state = AS_SPAWN;
			return qtrue;
	}
}

/*
==================
BotAlienAI
==================
*/
void BotAlienAI(bot_state_t *bs, float thinktime) {
	//char buf[144];
	
	/*bot_goal_t goal;
	bot_moveresult_t moveresult;
	int tt;
	vec3_t target, dir;
	*/
	//char userinfo[MAX_INFO_STRING];

	//if the bot has just been setup
	if (bs->setupcount > 0) {
		bs->setupcount--;
		if (bs->setupcount > 0) return;

		trap_EA_Command(bs->client, "team aliens");
		//
		//bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];
		//bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
		bs->setupcount = 0;
	}	
	
	// update knowledge base and inventory
	ABotUpdateInventory(bs);
	
	// run the FSM
	bs->statecycles = 0;
	while( !ABotRunState(bs) ){
		if( ++(bs->statecycles) > 5){
			BotAddInfo(bs, "botstates", "loop");
			break;
		}
	}
	
	// update the hud
	if(bot_developer.integer){
		// fsm state
		BotAddInfo(bs, "state", va("%d",bs->state) );
		// weapon
		//BotAddInfo(bs, "weapon", va("%d",bs->inventory[BI_WEAPON]) );
		// ammo
		//BotAddInfo(bs, "ammo", va("%d",bs->inventory[BI_AMMO]) );
		//target
		BotAddInfo(bs, "goal", va("%d",bs->goal.entitynum) );
		//Enemy Info
		if( bs->enemyent && bs->enemyent->client)
			BotAddInfo(bs, "enemyname", va("%s",bs->enemyent->client->pers.netname) );
		// copy config string
		trap_SetConfigstring( CS_BOTINFOS + bs->client, bs->hudinfo);
	}
}
