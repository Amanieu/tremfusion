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
#include "ai_chat.h"

#ifndef MAX_PATH
#define MAX_PATH		144
#endif
//bot states
bot_state_t	*botstates[MAX_CLIENTS];
//number of bots
int numbots;
//floating point time
float floattime;

/*
==================
BotAI_Print
==================
*/
void QDECL BotAI_Print(int type, char *fmt, ...) {
  char str[2048];
  va_list ap;

  va_start(ap, fmt);
  Q_vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  switch(type) {
    case PRT_MESSAGE: {
      G_Printf("%s", str);
      break;
    }
    case PRT_WARNING: {
      G_Printf( S_COLOR_YELLOW "Warning: %s", str );
      break;
    }
    case PRT_ERROR: {
      G_Printf( S_COLOR_RED "Error: %s", str );
      break;
    }
    case PRT_FATAL: {
      G_Printf( S_COLOR_RED "Fatal: %s", str );
      break;
    }
    case PRT_EXIT: {
      G_Error( S_COLOR_RED "Exit: %s", str );
      break;
    }
    default: {
      G_Printf( "unknown print type\n" );
      break;
    }
  }
}


//time to do a regular update
//float regularupdate_time;
/*
==============
BotInputToUserCommand
==============
*/
void BotInputToUserCommand(bot_input_t *bi, usercmd_t *ucmd, int delta_angles[3], int time) {
	vec3_t angles, forward, right;
	short temp;
		int j, forwardmv, rightmv, upmv; 

	//clear the whole structure
	memset(ucmd, 0, sizeof(usercmd_t));
	//
	//Com_Printf("dir = %f %f %f speed = %f\n", bi->dir[0], bi->dir[1], bi->dir[2], bi->speed);
	//the duration for the user command in milli seconds
	ucmd->serverTime = time;

	//set the buttons
	if (bi->actionflags & ACTION_RESPAWN) ucmd->buttons = BUTTON_ATTACK;
	if (bi->actionflags & ACTION_ATTACK) ucmd->buttons |= BUTTON_ATTACK;
	if (bi->actionflags & ACTION_TALK) ucmd->buttons |= BUTTON_TALK;
	if (bi->actionflags & ACTION_GESTURE) ucmd->buttons |= BUTTON_GESTURE;
	if (bi->actionflags & ACTION_USE) ucmd->buttons |= BUTTON_USE_HOLDABLE;
	if (bi->actionflags & ACTION_WALK) ucmd->buttons |= BUTTON_WALKING;
	//
	ucmd->weapon = bi->weapon;
	//set the view angles
	//NOTE: the ucmd->angles are the angles WITHOUT the delta angles
	ucmd->angles[PITCH] = ANGLE2SHORT(bi->viewangles[PITCH]);
	ucmd->angles[YAW] = ANGLE2SHORT(bi->viewangles[YAW]);
	ucmd->angles[ROLL] = ANGLE2SHORT(bi->viewangles[ROLL]);
	//subtract the delta angles
	for (j = 0; j < 3; j++) {
		temp = ucmd->angles[j] - delta_angles[j];
		/*NOTE: disabled because temp should be mod first
		if ( j == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if ( temp > 16000 ) temp = 16000;
			else if ( temp < -16000 ) temp = -16000;
		}
		*/
		ucmd->angles[j] = temp;
	}
	//NOTE: movement is relative to the REAL view angles
	//get the horizontal forward and right vector
	//get the pitch in the range [-180, 180]
	if (bi->dir[2]) angles[PITCH] = bi->viewangles[PITCH];
	else angles[PITCH] = 0;
	angles[YAW] = bi->viewangles[YAW];
	angles[ROLL] = 0;
	AngleVectors(angles, forward, right, NULL);
	//bot input speed is in the range [0, 400]
	bi->speed = bi->speed * 127 / 400;

	//set the view independent movement
	forwardmv = DotProduct(forward, bi->dir) * bi->speed;
	rightmv = DotProduct(right, bi->dir) * bi->speed;
	upmv = abs(forward[2]) * bi->dir[2] * bi->speed;
	//normal keyboard movement
	if (bi->actionflags & ACTION_MOVEFORWARD) forwardmv += 127;
	if (bi->actionflags & ACTION_MOVEBACK) forwardmv -= 127;
	if (bi->actionflags & ACTION_MOVELEFT) rightmv -= 127;
	if (bi->actionflags & ACTION_MOVERIGHT) rightmv += 127;
	//jump/moveup
	if (bi->actionflags & ACTION_JUMP) upmv += 127;
	//crouch/movedown
	if (bi->actionflags & ACTION_CROUCH) upmv -= 127;
	
	ucmd->forwardmove = (forwardmv > 127) ? 127 : forwardmv;
	ucmd->rightmove = (rightmv > 127) ? 127 : rightmv;
	ucmd->upmove = (upmv > 127) ? 127 : upmv;
	//
	//Com_Printf("forward = %d right = %d up = %d\n", ucmd->forwardmove, ucmd->rightmove, ucmd->upmove);
	//Com_Printf("ucmd->serverTime = %d\n", ucmd->serverTime);
}

void BotUpdateInput(bot_state_t *bs, int time, int elapsed_time) {
	bot_input_t bi;
	int j;

	//add the delta angles to the bot's current view angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	//change the bot view angles
	BotChangeViewAngles(bs, (float) elapsed_time / 1000);
	//retrieve the bot input
	trap_EA_GetInput(bs->client, (float) time / 1000, &bi);

	//convert the bot input to a usercmd
	BotInputToUserCommand(&bi, &bs->lastucmd, bs->cur_ps.delta_angles, time);

	//subtract the delta angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
}

/*
==================
BotAI_GetEntityState
==================
*/
int BotAI_GetEntityState( int entityNum, entityState_t *state ) {
	gentity_t	*ent;

	ent = &g_entities[entityNum];
	memset( state, 0, sizeof(entityState_t) );
	if (!ent->inuse) return qfalse;
	if (!ent->r.linked) return qfalse;
	if (ent->r.svFlags & SVF_NOCLIENT) return qfalse;
	memcpy( state, &ent->s, sizeof(entityState_t) );
	return qtrue;
}

/*
==================
BotAI_GetSnapshotEntity
==================
*/
int BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state ) {
	int		entNum;

	entNum = trap_BotGetSnapshotEntity( clientNum, sequence );
	if ( entNum == -1 ) {
		memset(state, 0, sizeof(entityState_t));
		return -1;
	}

	BotAI_GetEntityState( entNum, state );

	return sequence + 1;
}



/*
==================
BotCheckEvents
==================
*/
void BotCheckEvents(bot_state_t *bs, entityState_t *state) {
	int event;
	//char buf[128];
	vec3_t lastteleport_origin;		//last teleport event origin
	float lastteleport_time;		//last teleport event time
	float dist;
	vec3_t dir;

	//NOTE: this sucks, we're accessing the gentity_t directly
	//but there's no other fast way to do it right now
	if (bs->entityeventTime[state->number] == g_entities[state->number].eventTime) {
		return;
	}
	bs->entityeventTime[state->number] = g_entities[state->number].eventTime;
	//if it's an event only entity
	if (state->eType > ET_EVENTS) {
		event = (state->eType - ET_EVENTS) & ~EV_EVENT_BITS;
	}
	else {
		event = state->event & ~EV_EVENT_BITS;
	}
	//
	switch(event) {
		//client obituary event
		case EV_OBITUARY:
		{
			int target, attacker, mod;

			target = state->otherEntityNum;
			attacker = state->otherEntityNum2;
			Bot_Print(BPERROR, "%d killed me\n", attacker);
			mod = state->eventParm;
			//
			if (target == bs->client) {
				bs->botdeathtype = mod;
				bs->lastkilledby = attacker;
				BotChat_Death(bs); // talk 
				HBotEnterChat(bs); 
				//trap_BotEnterChat(bs->cs, 0, bs->chatto);
				//
				if (target == attacker ||
					target == ENTITYNUM_NONE ||
					target == ENTITYNUM_WORLD) bs->botsuicide = qtrue;
				else bs->botsuicide = qfalse;
				//
				bs->num_deaths++;
			}
			//else if this client was killed by the bot
			else if (attacker == bs->client) {
				bs->enemydeathtype = mod;
				bs->lastkilledplayer = target;
				bs->killedenemy_time = FloatTime();
				BotChat_Kill(bs);
				HBotEnterChat(bs);
				//trap_BotEnterChat(bs->cs, 0, bs->chatto);
				//
				bs->num_kills++;
				
			}
			else if (attacker == bs->enemy && target == attacker) {
				bs->enemysuicide = qtrue;
			}
			break;
		}
		case EV_GLOBAL_SOUND:
		{
			break;
		}
		case EV_PLAYER_TELEPORT_IN:
		{
			VectorCopy(state->origin, lastteleport_origin);
			lastteleport_time = FloatTime();
			break;
		}
		case EV_GENERAL_SOUND:
		{
			break;
		}
		//case EV_FOOTSTEP:
		//case EV_FOOTSTEP_METAL:
		//case EV_FOOTSPLASH:
		//case EV_FOOTWADE:
		case EV_SWIM:
		//case EV_FALL_SHORT:
		//case EV_FALL_MEDIUM:
		//case EV_FALL_FAR:
		//case EV_STEP_4:
		//case EV_STEP_8:
		//case EV_STEP_12:
		//case EV_STEP_16:
		//case EV_JUMP_PAD:
		case EV_JUMP:
		case EV_TAUNT:
		//case EV_WATER_TOUCH:
		//case EV_WATER_LEAVE:
		//case EV_WATER_UNDER:
		//case EV_WATER_CLEAR:
		//case EV_ITEM_PICKUP:
		//case EV_GLOBAL_ITEM_PICKUP:
		case EV_NOAMMO:
		case EV_CHANGE_WEAPON:
		case EV_FIRE_WEAPON:
		case EV_FIRE_WEAPON2:
		case EV_FIRE_WEAPON3:
			VectorSubtract(state->origin, bs->origin, dir);
			dist = VectorLength(dir);
			if(dist < 300.0){
				VectorCopy(state->origin, bs->lastheardorigin);
				bs->lastheardtime = FloatTime();
//				Bot_Print(BPMSG, "Event happend %0.0f units away at time \n", dist, FloatTime());
				//vectoangles(dir, bs->ideal_viewangles); 
			}			
			break;
	}
}

void BotCheckSnapshot(bot_state_t *bs) {
	int ent;
	entityState_t state;

	//remove all avoid spots
	//trap_BotAddAvoidSpot(bs->ms, vec3_origin, 0, AVOID_CLEAR);
	//reset kamikaze body
	//bs->kamikazebody = 0;
	//reset number of proxmines
	//bs->numproxmines = 0;
	//
	ent = 0;
	while( ( ent = BotAI_GetSnapshotEntity( bs->client, ent, &state ) ) != -1 ) {
		//check the entity state for events
		BotCheckEvents(bs, &state);
		//check for grenades the bot should avoid
		//BotCheckForGrenades(bs, &state);
	}
	//check the player state for events
	BotAI_GetEntityState(bs->client, &state);
	//copy the player state events to the entity state
	state.event = bs->cur_ps.externalEvent;
	state.eventParm = bs->cur_ps.externalEventParm;
	//
	BotCheckEvents(bs, &state);
}

char *BotGetMenuText( int menu, int arg )
{
  switch( menu )
  {
    case MN_TEAM: return"menu tremulous_teamselect";
    case MN_A_CLASS: return "menu tremulous_alienclass";
    case MN_H_SPAWN: return "menu tremulous_humanitem";
    case MN_A_BUILD: return "menu tremulous_alienbuild";
    case MN_H_BUILD: return "menu tremulous_humanbuild";
    case MN_H_ARMOURY: return "menu tremulous_humanarmoury";
    //===============================
    case MN_H_UNKNOWNITEM: return "Unknown item";
    case MN_A_TEAMFULL: return "The alien team has too many players";
    case MN_H_TEAMFULL: return "The human team has too many players";
    case MN_A_TEAMCHANGEBUILDTIMER: return "You cannot change teams until your build timer expires.";
    case MN_H_TEAMCHANGEBUILDTIMER: return "You cannot change teams until your build timer expires.";
    case MN_CMD_CHEAT: return "Cheats are not enabled on this server";
    case MN_CMD_TEAM: return "Join a team first";
    case MN_CMD_SPEC: return "You can only use this command when spectating";
    case MN_CMD_ALIEN: return "Must be alien to use this command";
    case MN_CMD_HUMAN: return "Must be human to use this command";
    case MN_CMD_LIVING: return "Must be living to use this command";
    case MN_B_NOROOM: return "There is no room to build here";
    case MN_B_NORMAL: return "Cannot build on this surface";
    case MN_B_CANNOT: return "You cannot build that structure";
    case MN_B_LASTSPAWN: return "You may not deconstruct the last spawn";
    case MN_B_SUDDENDEATH: return "Cannot build during Sudden Death";
    case MN_B_REVOKED: return "Your building rights have been revoked";
    case MN_B_SURRENDER: return "Building is denied to traitorous cowards";
//      longMsg   = "Your team has decided to admit defeat and concede the game:"
//                  "traitors and cowards are not allowed to build.";
//                  // too harsh? <- LOL
    case MN_H_NOBP: return "There is no power remaining";
    case MN_H_NOTPOWERED: return "This buildable is not powered";
    case MN_H_ONEREACTOR: return "There can only be one Reactor";
    case MN_H_NOPOWERHERE: return "There is no power here";
    case MN_H_NODCC: return "There is no Defense Computer";
    case MN_H_RPTPOWERHERE: return "This area already has power";
    case MN_H_NOSLOTS: return "You have no room to carry this";
    case MN_H_NOFUNDS: return "Insufficient funds";
    case MN_H_ITEMHELD: return "You already hold this item";
    case MN_H_NOARMOURYHERE: return "You must be near a powered Armoury";
    case MN_H_NOENERGYAMMOHERE: return "You must be near a Reactor or a powered Armoury or Repeater";
    case MN_H_NOROOMBSUITON: return "Not enough room here to put on a Battle Suit";
    case MN_H_NOROOMBSUITOFF: return "Not enough room here to take off your Battle Suit";
    case MN_H_ARMOURYBUILDTIMER: return "You can not buy or sell weapons until your build timer expires";
    case MN_H_DEADTOCLASS: return "You must be dead to use the class command";
    case MN_H_UNKNOWNSPAWNITEM: return "Unknown starting item";
    //===============================
    case MN_A_NOCREEP: return "There is no creep here";
    case MN_A_NOOVMND: return "There is no Overmind";
    case MN_A_ONEOVERMIND: return "There can only be one Overmind";
    case MN_A_ONEHOVEL: return "There can only be one Hovel";
    case MN_A_NOBP: return "The Overmind cannot control any more structures";
    case MN_A_NOEROOM: return "There is no room to evolve here";
    case MN_A_TOOCLOSE: return "This location is too close to the enemy to evolve";
    case MN_A_NOOVMND_EVOLVE: return "There is no Overmind";
    case MN_A_EVOLVEBUILDTIMER: return "You cannot Evolve until your build timer expires";
    case MN_A_HOVEL_OCCUPIED: return "This Hovel is already occupied by another builder";
    case MN_A_HOVEL_BLOCKED: return "The exit to this Hovel is currently blocked";
    case MN_A_HOVEL_EXIT: return "The exit to this Hovel would always be blocked";
    case MN_A_INFEST: return "menu tremulous_alienupgrade\n";
    case MN_A_CANTEVOLVE: return va( "You cannot evolve into a %s", 
                                     BG_ClassConfig( arg )->humanName );
    case MN_A_EVOLVEWALLWALK: return "You cannot evolve while wallwalking";
    case MN_A_UNKNOWNCLASS: return "Unknown class";
    case MN_A_CLASSNOTSPAWN: return va( "You cannot spawn as a %s", 
                                        BG_ClassConfig( arg )->humanName );
    case MN_A_CLASSNOTALLOWED: return va( "The %s is not allowed", 
                                          BG_ClassConfig( arg )->humanName );
    case MN_A_CLASSNOTATSTAGE: return va( "The %s is not allowed at Stage %d",
                                          BG_ClassConfig( arg )->humanName,
                                          g_alienStage.integer + 1 );
    default:
      return va( "BotGetMenuText: debug: no such menu %d\n", menu );
  }
}

int BotAI(int client, float thinktime) {
	bot_state_t *bs;
	char buf[1024], *args;
	int j;

	trap_EA_ResetInput(client);
	//
	bs = botstates[client];
	if (!bs || !bs->inuse) {
		Bot_Print(BPERROR, "BotAI: client %d is not setup\n", client);
		return qfalse;
	}
	BotCheckSnapshot(bs);
	//retrieve the current client state
	BotAI_GetClientState( client, &bs->cur_ps );
	//retrieve any waiting server commands
	while( trap_BotGetServerCommand(client, buf, sizeof(buf)) ) {
		//have buf point to the command and args to the command arguments
		args = strchr( buf, ' ');
		if (!args) continue;
		*args++ = '\0';

		
		//remove color espace sequences from the arguments
		//RemoveColorEscapeSequences( args );

		if (!Q_stricmp(buf, "print")) {
			//remove first and last quote from the chat message
			if( bot_developer.integer){
				memmove(args, args+1, strlen(args));
				args[strlen(args)-1] = '\0';
				Bot_Print(BPDEBUG, "[BOT_MSG] %s\n", args);
			}
			//trap_BotQueueConsoleMessage(bs->cs, CMS_NORMAL, args);
		}
		else if (!Q_stricmp(buf, "chat")) {
			//remove first and last quote from the chat message
			//memmove(args, args+1, strlen(args));
			//args[strlen(args)-1] = '\0';
			//trap_BotQueueConsoleMessage(bs->cs, CMS_CHAT, args);
		}
		else if (!Q_stricmp(buf, "tchat")) {
			//remove first and last quote from the chat message
			//memmove(args, args+1, strlen(args));
			//args[strlen(args)-1] = '\0';
			//trap_BotQueueConsoleMessage(bs->cs, CMS_CHAT, args);
		}
		else if (!Q_stricmp(buf, "servermenu")) {
		  char arg1[ 4 ], arg2[ 4 ];
		  char *menu_text;
		  if( trap_Argc( ) == 2  ) {
		    trap_Argv( 1, arg1, sizeof(arg1));
		    menu_text = BotGetMenuText( atoi( arg1 ), 0 );
		  }
		  if( trap_Argc( ) == 3 ) {
		    trap_Argv( 1, arg1, sizeof(arg1) );
		    trap_Argv( 2, arg2, sizeof(arg2) );
		    menu_text = BotGetMenuText( atoi( arg1), atoi( arg2 ) );
		  }
		  Bot_Print(BPDEBUG, "[BOT_MENU] %s\n", menu_text);
		}
		else if (!Q_stricmp(buf, "scores"))	// parse scores?
			{  }
		else if (!Q_stricmp(buf, "clientLevelShot"))
			{ }
		else if (!Q_stricmp(buf, "cp "))	// CenterPrint
			{ }	
		else if (!Q_stricmp(buf, "cs"))	// confing string modified
			{ }

	}
	
	//add the delta angles to the bot's current view angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	//increase the local time of the bot
	bs->ltime += thinktime;
	//
	bs->thinktime = thinktime;
	//origin of the bot
	VectorCopy(bs->cur_ps.origin, bs->origin);
	//eye coordinates of the bot
	VectorCopy(bs->cur_ps.origin, bs->eye);
	bs->eye[2] += bs->cur_ps.viewheight;
	//get the area the bot is in
	bs->areanum = BotPointAreaNum(bs->origin);
	// invalidate the hud config string
	bs->hudinfo[0] = '\0';
	// update the team info
	if(bs->team == 0)
		bs->team = g_entities[ bs->entitynum ].client->pers.teamSelection;
	
	//the real AI	
	if(bs->team == TEAM_ALIENS)
		BotAlienAI(bs, thinktime);
	else if (bs->team == TEAM_HUMANS)
		BotHumanAI(bs, thinktime);
	else
		Bot_Print(BPERROR, "BotAI: odd bs->team value %d\n", bs->team);

	//set the weapon selection every AI frame
	//trap_EA_SelectWeapon(bs->client, bs->weaponnum);
	
	//subtract the delta angles
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	bs->lastframe_health = bs->inventory[BI_HEALTH];
	//everything was ok
	return qtrue;
}

void BotCheckDeath(int target, int attacker, int mod){
	bot_state_t *tbs, *abs;
	tbs = botstates[target];
	abs = botstates[attacker];
	if ( (!tbs || !tbs->inuse) && (!abs || !abs->inuse) ) {
		// Both aren't bots so return
		return;
	}
	if (tbs && tbs->client && target == tbs->client) {
		tbs->botdeathtype = mod;
		tbs->lastkilledby = attacker;
		if(tbs->team == TEAM_HUMANS){
			if(BotChat_Death(tbs)) // talk 
				HBotEnterChat(tbs);
			//trap_BotEnterChat(tbs->cs, 0, tbs->chatto);
		}
		//
		if (target == attacker ||
			target == ENTITYNUM_NONE ||
			target == ENTITYNUM_WORLD) tbs->botsuicide = qtrue;
		else tbs->botsuicide = qfalse;
		//
		tbs->num_deaths++;
	}
	//else if this client was killed by the bot
	if (abs && abs->client && attacker == abs->client) {
		abs->enemydeathtype = mod;
		abs->lastkilledplayer = target;
		abs->killedenemy_time = FloatTime();
		if(abs->team == TEAM_HUMANS){
			if(BotChat_Kill(abs))
				HBotEnterChat(abs);
			//trap_BotEnterChat(abs->cs, 0, abs->chatto);
		}
		//
		abs->num_kills++;		
	}
}

void BotScheduleBotThink(void) {
	int i, botnum;

	botnum = 0;

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		//initialize the bot think residual time
		botstates[i]->botthink_residual = bot_thinktime.integer * botnum / numbots;
		botnum++;
	}
}

int BotAIStartFrame(int time) {
	int elapsed_time;
	static int local_time = 0;
	static int lastbotthink_time;
	int thinktime;
	static int botlib_residual;
	int i;
	bot_entitystate_t state;
	gentity_t	*ent;

	G_CheckBotSpawn();
	
	trap_Cvar_Update(&bot_developer);
	
	// reschedule if thinktime was changed
	trap_Cvar_Update(&bot_thinktime);	
	if (bot_thinktime.integer > 200) trap_Cvar_Set("bot_thinktime", "200");
	if (bot_thinktime.integer != lastbotthink_time) {
		lastbotthink_time = bot_thinktime.integer;
		BotScheduleBotThink();
	}
	
	elapsed_time = time - local_time;
	local_time = time;
	
	botlib_residual += elapsed_time;

	if (elapsed_time > bot_thinktime.integer) thinktime = elapsed_time;
	else thinktime = bot_thinktime.integer;
	
	 // update the bot library
	if ( botlib_residual >= thinktime ) {
		botlib_residual -= thinktime;

		trap_BotLibStartFrame((float) time / 1000);

		if (!trap_AAS_Initialized()) return qfalse;
		
		//update entities in the botlib
		for (i = 0; i < MAX_GENTITIES; i++) {
			ent = &g_entities[i];
			if (!ent->inuse) {
				trap_BotLibUpdateEntity(i, NULL);
				continue;
			}
			if (!ent->r.linked) {
				trap_BotLibUpdateEntity(i, NULL);
				continue;
			}
			if (ent->r.svFlags & SVF_NOCLIENT) {
				trap_BotLibUpdateEntity(i, NULL);
				continue;
			}
			// do not update missiles
			if (ent->s.eType == ET_MISSILE ) {
				trap_BotLibUpdateEntity(i, NULL);
				continue;
			}
			// do not update event only entities
			if (ent->s.eType > ET_EVENTS) {
				trap_BotLibUpdateEntity(i, NULL);
				continue;
			}
			//
			memset(&state, 0, sizeof(bot_entitystate_t));
			//
			VectorCopy(ent->r.currentOrigin, state.origin);
			if (i < MAX_CLIENTS) {
				VectorCopy(ent->s.apos.trBase, state.angles);
			} else {
				VectorCopy(ent->r.currentAngles, state.angles);
			}
			VectorCopy(ent->s.origin2, state.old_origin);
			VectorCopy(ent->r.mins, state.mins);
			VectorCopy(ent->r.maxs, state.maxs);
			state.type = ent->s.eType;
			state.flags = ent->s.eFlags;
			if (ent->r.bmodel) state.solid = SOLID_BSP;
			else state.solid = SOLID_BBOX;
			state.groundent = ent->s.groundEntityNum;
			state.modelindex = ent->s.modelindex;
			state.modelindex2 = ent->s.modelindex2;
			state.frame = ent->s.frame;
			state.event = ent->s.event;
			state.eventParm = ent->s.eventParm;
//			state.powerups = ent->s.powerups;
			state.legsAnim = ent->s.legsAnim;
			state.torsoAnim = ent->s.torsoAnim;
			state.weapon = ent->s.weapon;
			//
			trap_BotLibUpdateEntity(i, &state);
		}

		//BotAIRegularUpdate();		// update the ai itemlist, see BotUpdateEntityItems
	}
	
	floattime = trap_AAS_Time();

	// execute scheduled bot AI
	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		//
		botstates[i]->botthink_residual += elapsed_time;
		//
		if ( botstates[i]->botthink_residual >= thinktime ) {
			botstates[i]->botthink_residual -= thinktime;

			if (!trap_AAS_Initialized()) return qfalse;

			if (g_entities[i].client->pers.connected == CON_CONNECTED) {
				//botstates[i]->frametime = time;		// cyr, clock frame duration
				BotAI(i, (float) thinktime / 1000);
			}
		}
	}
	
	DeleteDebugLines();
	
	 // execute bot user commands every frame
	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		if( g_entities[i].client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		
		BotShowViewAngles(botstates[i]);
		BotUpdateInput(botstates[i], time, elapsed_time);
		trap_BotUserCommand(botstates[i]->client, &botstates[i]->lastucmd);
	}
	
	return qtrue;
}

int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart) {
	char filename[MAX_PATH], name[MAX_PATH],  csname[MAX_PATH];
	bot_state_t *bs;
	int team;
	int errnum;
	if( !Q_stricmp( settings->team, "aliens" ) )
		team = TEAM_ALIENS;
	else if( !Q_stricmp( settings->team, "humans" ) )
		team = TEAM_HUMANS;
	else{
		team = TEAM_NONE;
	}
	if (!botstates[client]) botstates[client] = BG_Alloc(sizeof(bot_state_t));
	bs = botstates[client];

	if (bs && bs->inuse) {
		Bot_Print(BPERROR, "BotAISetupClient: client %d already setup\n", client);
		return qfalse;
	}

	if (!trap_AAS_Initialized()) {
		Bot_Print(BPERROR, "AAS not initialized\n");
		return qfalse;
	}
	//allocate a weapon state
	bs->ws = trap_BotAllocWeaponState();
	//Bot_Print(BPMSG, "loading skill %f from %s\n", settings->skill, settings->characterfile);
	//load the bot character
	//if (team == TEAM_HUMANS){
	bs->character = trap_BotLoadCharacter(settings->characterfile, settings->skill);
	//Bot_Print(BPMSG, "loaded skill %f from %s\n", settings->skill, settings->characterfile);
	if (!bs->character && team == TEAM_HUMANS) {
		Bot_Print(BPERROR, "couldn't load skill %f from %s\n", settings->skill, settings->characterfile);
		return qfalse;
	}
	//allocate a chat state
	bs->cs = trap_BotAllocChatState();
	//load the chat file
	trap_Characteristic_String(bs->character, CHARACTERISTIC_CHAT_FILE, filename, MAX_PATH);
	trap_Characteristic_String(bs->character, CHARACTERISTIC_CHAT_NAME, name, MAX_PATH);
	errnum = trap_BotLoadChatFile(bs->cs, filename, name);
	if (errnum != BLERR_NOERROR && team == TEAM_HUMANS) {
		trap_BotFreeChatState(bs->cs);
		//trap_BotFreeGoalState(bs->gs);
		trap_BotFreeWeaponState(bs->ws);
		return qfalse;
	}
	//copy the settings
  memcpy(&bs->settings, settings, sizeof(bot_settings_t));
	//set the chat name
	ClientName(client, csname, sizeof(csname));
	trap_BotSetChatName(bs->cs, csname, client);
	//}
	bs->inuse = qtrue;
	bs->client = client;
	bs->entitynum = client;
	bs->setupcount = 4;
	bs->entergame_time = FloatTime();
	bs->ms = trap_BotAllocMoveState();
	bs->team = team;
	bs->ent = &g_entities[ bs->client ];
	//bs->ent->bs = bs;
	bs->tfl = TFL_DEFAULT;
	numbots++;

	//NOTE: reschedule the bot thinking
	BotScheduleBotThink();
	if(BotChat_EnterGame(bs)) // talk 
		HBotEnterChat(bs);
	
	//bot has been setup succesfully
	return qtrue;
}

/*
==============
BotAIShutdownClient
==============
*/
int BotAIShutdownClient(int client, qboolean restart) {
	bot_state_t *bs;

	bs = botstates[client];
	if (!bs || !bs->inuse) {
		Bot_Print(BPERROR, "BotAIShutdownClient: client %d already shutdown\n", client);
		return qfalse;
	}

	trap_BotFreeMoveState(bs->ms);
	
	//free the goal state`			
	//trap_BotFreeGoalState(bs->gs);
	//free the chat file
	trap_BotFreeChatState(bs->cs);
	//free the weapon weights
	trap_BotFreeWeaponState(bs->ws);
	//free the bot character
	trap_BotFreeCharacter(bs->character);
	//
	//BotFreeWaypoints(bs->checkpoints);
	//BotFreeWaypoints(bs->patrolpoints);
	//clear activate goal stack
	//BotClearActivateGoalStack(bs);
	
	//clear the bot state
	memset(bs, 0, sizeof(bot_state_t));
	//set the inuse flag to qfalse
	bs->inuse = qfalse;
	//there's one bot less
	numbots--;
	//everything went ok
	return qtrue;
}

int BotAILoadMap( int restart ) {
	vmCvar_t	mapname;

	if (!restart) {
		trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		trap_BotLibLoadMap( mapname.string );
	}

	return qtrue;
}

int BotAISetup( int restart ) {
	vmCvar_t	mapname;
	char buf[144];

	// todo: register cvars
	/*
	trap_Cvar_Register(&bot_thinktime, "bot_thinktime", "100", CVAR_CHEAT);
	trap_Cvar_Register( &bot_minaliens, "bot_minaliens", "0", CVAR_SERVERINFO );
	trap_Cvar_Register( &bot_minhumans, "bot_minhumans", "0", CVAR_SERVERINFO );
	trap_Cvar_Register( &bot_developer, "bot_developer", "0", CVAR_SERVERINFO );
	trap_Cvar_Register(&bot_challenge, "bot_challenge", "0", 0);
	*/
	//if the game is restarted for a tournament
	if (restart) {
		return qtrue;
	}
	
	//reset the bot states
	memset( botstates, 0, sizeof(botstates) );
	
	// set up the botlib
	trap_Cvar_VariableStringBuffer("sv_maxclients", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "8");
	trap_BotLibVarSet("maxclients", buf);
	Com_sprintf(buf, sizeof(buf), "%d", MAX_GENTITIES);
	trap_BotLibVarSet("maxentities", buf);
	//bsp checksum
	trap_Cvar_VariableStringBuffer("sv_mapChecksum", buf, sizeof(buf));
	if (strlen(buf)) trap_BotLibVarSet("sv_mapChecksum", buf);
	//maximum number of aas links
	trap_Cvar_VariableStringBuffer("max_aaslinks", buf, sizeof(buf));
	if (strlen(buf)) trap_BotLibVarSet("max_aaslinks", buf);
	//base directory
	trap_Cvar_VariableStringBuffer("fs_basepath", buf, sizeof(buf));
	if (strlen(buf)) trap_BotLibVarSet("basedir", buf);
	//game directory
	trap_Cvar_VariableStringBuffer("fs_game", buf, sizeof(buf));
	if (strlen(buf)) trap_BotLibVarSet("gamedir", buf);
	//home directory
	trap_Cvar_VariableStringBuffer("fs_homepath", buf, sizeof(buf));
	if (strlen(buf)) trap_BotLibVarSet("homedir", buf);

	if ( trap_BotLibSetup() != BLERR_NOERROR) return qfalse;
	
	// load the map's aas
	trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
	trap_BotLibLoadMap( mapname.string );
	return qtrue;
}

int BotAIShutdown( int restart ) {
	int i;

	//if the game is restarted for a tournament
	if ( restart ) {
		//shutdown all the bots in the botlib
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (botstates[i] && botstates[i]->inuse) {
				BotAIShutdownClient(botstates[i]->client, restart);
			}
		}
		//don't shutdown the bot library
	}
	else {
		trap_BotLibShutdown();
	}
	return qtrue;
}
/*
==============
BotEntityInfo
==============
*/
void BotEntityInfo(int entnum, aas_entityinfo_t *info) {
	trap_AAS_EntityInfo(entnum, info);
}
/*
==================
BotRandomMove
==================
*/
void BotRandomMove(bot_state_t *bs, bot_moveresult_t *moveresult) {
	vec3_t dir, angles;

	angles[0] = 0;
	angles[1] = random() * 360;
	angles[2] = 0;
	AngleVectors(angles, dir, NULL, NULL);

	trap_BotMoveInDirection(bs->ms, dir, 400, MOVE_WALK);

	moveresult->failure = qfalse;
	VectorCopy(dir, moveresult->movedir);
}
/*
==================
BotAIBlocked

Very basic handling of bots being blocked by other entities.
Check what kind of entity is blocking the bot and try to activate
it. If that's not an option then try to walk around or over the entity.
Before the bot ends in this part of the AI it should predict which doors to
open, which buttons to activate etc.
==================
*/
void BotAIBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate) {
	int movetype;//, bspent;
	vec3_t hordir, start, end, mins, maxs, sideward, angles, up = {0, 0, 1};
	aas_entityinfo_t entinfo;
	gentity_t* blockent = &g_entities[ moveresult->blockentity ];

	// if the bot is not blocked by anything
	if (!moveresult->blocked) {
		bs->notblocked_time = FloatTime();
		return;
	}
 	if(blockent->s.eType == ET_BUILDABLE)
 		trap_EA_Jump(bs->client);
	// if stuck in a solid area
	if ( moveresult->type == RESULTTYPE_INSOLIDAREA ) {
		// move in a random direction in the hope to get out
		BotRandomMove(bs, moveresult);
		//
		return;
	}
	// get info for the entity that is blocking the bot
	BotEntityInfo(moveresult->blockentity, &entinfo);
	
#ifdef OBSTACLEDEBUG
	ClientName(bs->client, netname, sizeof(netname));
	BotAI_Print(PRT_MESSAGE, "%s: I'm blocked by model %d\n", netname, entinfo.modelindex);
#endif // OBSTACLEDEBUG
	
	// just some basic dynamic obstacle avoidance code
	hordir[0] = moveresult->movedir[0];
	hordir[1] = moveresult->movedir[1];
	hordir[2] = 0;
	// if no direction just take a random direction
	if (VectorNormalize(hordir) < 0.1) {
		VectorSet(angles, 0, 360 * random(), 0);
		AngleVectors(angles, hordir, NULL, NULL);
	}
	//
	//if (moveresult->flags & MOVERESULT_ONTOPOFOBSTACLE) movetype = MOVE_JUMP;
	//else
	movetype = MOVE_WALK;
	// if there's an obstacle at the bot's feet and head then
	// the bot might be able to crouch through
	VectorCopy(bs->origin, start);
	start[2] += 18;
	VectorMA(start, 5, hordir, end);
	VectorSet(mins, -16, -16, -24);
	VectorSet(maxs, 16, 16, 4);
	//
	//bsptrace = AAS_Trace(start, mins, maxs, end, bs->entitynum, MASK_PLAYERSOLID);
	//if (bsptrace.fraction >= 1) movetype = MOVE_CROUCH;
	// get the sideward vector
	CrossProduct(hordir, up, sideward);
	//
	if (bs->flags & BFL_AVOIDRIGHT) VectorNegate(sideward, sideward);
	// try to crouch straight forward?
	if (movetype != MOVE_CROUCH || !trap_BotMoveInDirection(bs->ms, hordir, 400, movetype)) {
		// perform the movement
		if (!trap_BotMoveInDirection(bs->ms, sideward, 400, movetype)) {
			// flip the avoid direction flag
			bs->flags ^= BFL_AVOIDRIGHT;
			// flip the direction
			// VectorNegate(sideward, sideward);
			VectorMA(sideward, -1, hordir, sideward);
			// move in the other direction
			trap_BotMoveInDirection(bs->ms, sideward, 400, movetype);
		}
	}
}

/*
==============
NumBots
==============
*/
int NumBots(void) {
  return numbots;
}

void BotBeginIntermission( void )
{
  int i;
  for (i = 0; i < MAX_CLIENTS; i++) {
    if (botstates[i] && botstates[i]->inuse) {
      if (botstates[i]->team == TEAM_HUMANS)
      {
        BotChat_EndLevel(botstates[i]);
        trap_BotEnterChat(botstates[i]->cs, 0, botstates[i]->chatto);
        ClientDisconnect( i );
        //this needs to be done to free up the client slot - I think - Ender Feb 18 2008
        trap_DropClient( i, "disconnected");
      }
    }
  }
}
