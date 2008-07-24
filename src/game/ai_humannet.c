/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//

/*****************************************************************************
 * name:    ai_dmq3.c
 *
 * desc:    Quake3 bot AI
 *
 * $Archive: /MissionPack/code/game/ai_dmq3.c $
 *
 *****************************************************************************/

#include "g_local.h"
//#include "../botlib/botlib.h"
//#include "../botlib/be_aas.h"
//#include "../botlib/be_ea.h"
//#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
//#include "../botlib/be_ai_gen.h"
//#include "../botlib/be_ai_goal.h"
//#include "../botlib/be_ai_move.h"
//#include "../botlib/be_ai_weap.h"
//
#include "ai_main.h"
//#include "ai_dmq3.h"
#include "ai_chat.h"
//#include "ai_cmd.h"
#include "ai_net.h"
//#include "ai_team.h"
//
#include "chars.h"        //characteristics
#include "inv.h"        //indexes into the inventory
#include "syn.h"        //synonyms
#include "match.h"        //string matching types and vars

// for the voice chats
#include "../../ui/menudef.h" // sos001205 - for q3_ui also

// from aasfile.h
#define AREACONTENTS_MOVER        1024
#define AREACONTENTS_MODELNUMSHIFT    24
#define AREACONTENTS_MAXMODELNUM    0xFF
#define AREACONTENTS_MODELNUM     (AREACONTENTS_MAXMODELNUM << AREACONTENTS_MODELNUMSHIFT)

#define IDEAL_ATTACKDIST      140


//

//NOTE: not using a cvars which can be updated because the game should be reloaded anyway
int gametype;   //game type
int maxclients;   //maximum number of clients

vmCvar_t bot_grapple;
vmCvar_t bot_rocketjump;
vmCvar_t bot_fastchat;
vmCvar_t bot_nochat;
vmCvar_t bot_testrchat;
vmCvar_t bot_challenge;
vmCvar_t bot_predictobstacles;
//vmCvar_t g_spSkill;

extern vmCvar_t bot_developer;

vec3_t lastteleport_origin;   //last teleport event origin
float lastteleport_time;    //last teleport event time
int max_bspmodelindex;      //maximum BSP model index

#define MAX_ALTROUTEGOALS   32

int altroutegoals_setup;
aas_altroutegoal_t alien_altroutegoals[MAX_ALTROUTEGOALS];
int alien_numaltroutegoals;
aas_altroutegoal_t human_altroutegoals[MAX_ALTROUTEGOALS];
int human_numaltroutegoals;


/*
==================
BotSetUserInfo
==================
*/
void BotSetUserInfo(bot_state_t *bs, char *key, char *value) {
  char userinfo[MAX_INFO_STRING];

  trap_GetUserinfo(bs->client, userinfo, sizeof(userinfo));
  Info_SetValueForKey(userinfo, key, value);
  trap_SetUserinfo(bs->client, userinfo);
  ClientUserinfoChanged( bs->client );
}


/*
==================
BotTeam
==================
*/
int BotTeam(bot_state_t *bs) {
  char info[1024];

  if (bs->client < 0 || bs->client >= MAX_CLIENTS) {
    //BotAI_Print(PRT_ERROR, "BotCTFTeam: client out of range\n");
    return qfalse;
  }
  trap_GetConfigstring(CS_PLAYERS+bs->client, info, sizeof(info));
  //
  if (atoi(Info_ValueForKey(info, "t")) == TEAM_HUMANS) return TEAM_HUMANS;
  else if (atoi(Info_ValueForKey(info, "t")) == TEAM_ALIENS) return TEAM_ALIENS;
  return TEAM_NONE;
}

/*
==================
BotOppositeTeam
==================
*/
int BotOppositeTeam(bot_state_t *bs) {
  switch(BotTeam(bs)) {
    case TEAM_HUMANS: return TEAM_ALIENS;
    case TEAM_ALIENS: return TEAM_HUMANS;
    default: return TEAM_NONE;
  }
}

/*
==================
EntityHasQuad
==================
*/
qboolean EntityHasQuad(aas_entityinfo_t *entinfo) {
//  if (entinfo->powerups & (1 << PW_QUAD)) {
//    return qtrue;
//  }
  return qfalse;
}

/*
==================
BotRememberLastOrderedTask
==================
*/
void BotRememberLastOrderedTask(bot_state_t *bs) {
  if (!bs->ordered) {
    return;
  }
  bs->lastgoal_decisionmaker = bs->decisionmaker;
  bs->lastgoal_ltgtype = bs->ltgtype;
  memcpy(&bs->lastgoal_teamgoal, &bs->teamgoal, sizeof(bot_goal_t));
  bs->lastgoal_teammate = bs->teammate;
}


/*
==================
BotLongTermGoal
==================
*/
int BotLongTermGoal(bot_state_t *bs, int tfl, int retreat, bot_goal_t *goal) {
  aas_entityinfo_t entinfo;
  char teammate[MAX_MESSAGE_SIZE];
  float squaredist;
  int areanum;
  vec3_t dir;

  //FIXME: also have air long term goals?
  //
  //if the bot is leading someone and not retreating
  if (bs->lead_time > 0 && !retreat) {
    if (bs->lead_time < FloatTime()) {
      BotAI_BotInitialChat(bs, "lead_stop", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
      trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
      bs->lead_time = 0;
      return BotGetLongTermGoal(bs, tfl, retreat, goal);
    }
    //
    if (bs->leadmessage_time < 0 && -bs->leadmessage_time < FloatTime()) {
      BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
      trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
      bs->leadmessage_time = FloatTime();
    }
    //get entity information of the companion
    BotEntityInfo(bs->lead_teammate, &entinfo);
    //
    if (entinfo.valid) {
      areanum = BotPointAreaNum(entinfo.origin);
      if (areanum && trap_AAS_AreaReachability(areanum)) {
        //update team goal
        bs->lead_teamgoal.entitynum = bs->lead_teammate;
        bs->lead_teamgoal.areanum = areanum;
        VectorCopy(entinfo.origin, bs->lead_teamgoal.origin);
        VectorSet(bs->lead_teamgoal.mins, -8, -8, -8);
        VectorSet(bs->lead_teamgoal.maxs, 8, 8, 8);
      }
    }
    //if the team mate is visible
    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->lead_teammate)) {
      bs->leadvisible_time = FloatTime();
    }
    //if the team mate is not visible for 1 seconds
    if (bs->leadvisible_time < FloatTime() - 1) {
      bs->leadbackup_time = FloatTime() + 2;
    }
    //distance towards the team mate
    VectorSubtract(bs->origin, bs->lead_teamgoal.origin, dir);
    squaredist = VectorLengthSquared(dir);
    //if backing up towards the team mate
    if (bs->leadbackup_time > FloatTime()) {
      if (bs->leadmessage_time < FloatTime() - 20) {
        BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
        trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
        bs->leadmessage_time = FloatTime();
      }
      //if very close to the team mate
      if (squaredist < Square(100)) {
        bs->leadbackup_time = 0;
      }
      //the bot should go back to the team mate
      memcpy(goal, &bs->lead_teamgoal, sizeof(bot_goal_t));
      return qtrue;
    }
    else {
      //if quite distant from the team mate
      if (squaredist > Square(500)) {
        if (bs->leadmessage_time < FloatTime() - 20) {
          BotAI_BotInitialChat(bs, "followme", EasyClientName(bs->lead_teammate, teammate, sizeof(teammate)), NULL);
          trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
          bs->leadmessage_time = FloatTime();
        }
        //look at the team mate
        VectorSubtract(entinfo.origin, bs->origin, dir);
        vectoangles(dir, bs->ideal_viewangles);
        bs->ideal_viewangles[2] *= 0.5;
        //just wait for the team mate
        return qfalse;
      }
    }
  }
  return BotGetLongTermGoal(bs, tfl, retreat, goal);
}


/*
==================
BotGetItemLongTermGoal
==================
*/
int BotGetItemLongTermGoal(bot_state_t *bs, int tfl, bot_goal_t *goal) {
  //if the bot has no goal
  if (!trap_BotGetTopGoal(bs->gs, goal)) {
    //BotAI_Print(PRT_MESSAGE, "no ltg on stack\n");
    bs->ltg_time = 0;
  }
  //if the bot touches the current goal
  else if (BotReachedGoal(bs, goal)) {
    BotChooseWeapon(bs);
    bs->ltg_time = 0;
  }
  //if it is time to find a new long term goal
  if (bs->ltg_time < FloatTime()) {
    //pop the current goal from the stack
    trap_BotPopGoal(bs->gs);
    //BotAI_Print(PRT_MESSAGE, "%s: choosing new ltg\n", ClientName(bs->client, netname, sizeof(netname)));
    //choose a new goal
    //BotAI_Print(PRT_MESSAGE, "%6.1f client %d: BotChooseLTGItem\n", FloatTime(), bs->client);
    if (trap_BotChooseLTGItem(bs->gs, bs->origin, bs->inventory, tfl)) {
      /*
      char buf[128];
      //get the goal at the top of the stack
      trap_BotGetTopGoal(bs->gs, goal);
      trap_BotGoalName(goal->number, buf, sizeof(buf));
      BotAI_Print(PRT_MESSAGE, "%1.1f: new long term goal %s\n", FloatTime(), buf);
            */
      bs->ltg_time = FloatTime() + 20;
    }
    else {//the bot gets sorta stuck with all the avoid timings, shouldn't happen though
      //
#ifdef DEBUG
      char netname[128];

      BotAI_Print(PRT_MESSAGE, "%s: no valid ltg (probably stuck)\n", ClientName(bs->client, netname, sizeof(netname)));
#endif
      //trap_BotDumpAvoidGoals(bs->gs);
      //reset the avoid goals and the avoid reach
      trap_BotResetAvoidGoals(bs->gs);
      trap_BotResetAvoidReach(bs->ms);
    }
    //get the goal at the top of the stack
    return trap_BotGetTopGoal(bs->gs, goal);
  }
  return qtrue;
}

/*
==================
BotGetLongTermGoal

we could also create a seperate AI node for every long term goal type
however this saves us a lot of code
==================
*/
int BotGetLongTermGoal(bot_state_t *bs, int tfl, int retreat, bot_goal_t *goal) {
  vec3_t target, dir, dir2;
  char netname[MAX_NETNAME];
  char buf[MAX_MESSAGE_SIZE];
  int areanum;
  float croucher;
  aas_entityinfo_t entinfo, botinfo;
  bot_waypoint_t *wp;

//  if (bs->ltgtype == LTG_TEAMHELP && !retreat) {
//    //check for bot typing status message
//    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
//      BotAI_BotInitialChat(bs, "help_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
//      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
//      BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
//      trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
//      bs->teammessage_time = 0;
//    }
//    //if trying to help the team mate for more than a minute
//    if (bs->teamgoal_time < FloatTime())
//      bs->ltgtype = 0;
//    //if the team mate IS visible for quite some time
//    if (bs->teammatevisible_time < FloatTime() - 10) bs->ltgtype = 0;
//    //get entity information of the companion
//    BotEntityInfo(bs->teammate, &entinfo);
//    //if the team mate is visible
//    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->teammate)) {
//      //if close just stand still there
//      VectorSubtract(entinfo.origin, bs->origin, dir);
//      if (VectorLengthSquared(dir) < Square(100)) {
//        trap_BotResetAvoidReach(bs->ms);
//        return qfalse;
//      }
//    }
//    else {
//      //last time the bot was NOT visible
//      bs->teammatevisible_time = FloatTime();
//    }
//    //if the entity information is valid (entity in PVS)
//    if (entinfo.valid) {
//      areanum = BotPointAreaNum(entinfo.origin);
//      if (areanum && trap_AAS_AreaReachability(areanum)) {
//        //update team goal
//        bs->teamgoal.entitynum = bs->teammate;
//        bs->teamgoal.areanum = areanum;
//        VectorCopy(entinfo.origin, bs->teamgoal.origin);
//        VectorSet(bs->teamgoal.mins, -8, -8, -8);
//        VectorSet(bs->teamgoal.maxs, 8, 8, 8);
//      }
//    }
//    memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
//    return qtrue;
//  }
  //if the bot accompanies someone
//  if (bs->ltgtype == LTG_TEAMACCOMPANY && !retreat) {
//    //check for bot typing status message
//    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
//      BotAI_BotInitialChat(bs, "accompany_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
//      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
//      BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
//      trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
//      bs->teammessage_time = 0;
//    }
//    //if accompanying the companion for 3 minutes
//    if (bs->teamgoal_time < FloatTime()) {
//      BotAI_BotInitialChat(bs, "accompany_stop", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
//      trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
//      bs->ltgtype = 0;
//    }
//    //get entity information of the companion
//    BotEntityInfo(bs->teammate, &entinfo);
//    //if the companion is visible
//    if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->teammate)) {
//      //update visible time
//      bs->teammatevisible_time = FloatTime();
//      VectorSubtract(entinfo.origin, bs->origin, dir);
//      if (VectorLengthSquared(dir) < Square(bs->formation_dist)) {
//        //
//        // if the client being followed bumps into this bot then
//        // the bot should back up
//        BotEntityInfo(bs->entitynum, &botinfo);
//        // if the followed client is not standing ontop of the bot
//        if (botinfo.origin[2] + botinfo.maxs[2] > entinfo.origin[2] + entinfo.mins[2]) {
//          // if the bounding boxes touch each other
//          if (botinfo.origin[0] + botinfo.maxs[0] > entinfo.origin[0] + entinfo.mins[0] - 4&&
//            botinfo.origin[0] + botinfo.mins[0] < entinfo.origin[0] + entinfo.maxs[0] + 4) {
//            if (botinfo.origin[1] + botinfo.maxs[1] > entinfo.origin[1] + entinfo.mins[1] - 4 &&
//              botinfo.origin[1] + botinfo.mins[1] < entinfo.origin[1] + entinfo.maxs[1] + 4) {
//              if (botinfo.origin[2] + botinfo.maxs[2] > entinfo.origin[2] + entinfo.mins[2] - 4 &&
//                botinfo.origin[2] + botinfo.mins[2] < entinfo.origin[2] + entinfo.maxs[2] + 4) {
//                // if the followed client looks in the direction of this bot
//                AngleVectors(entinfo.angles, dir, NULL, NULL);
//                dir[2] = 0;
//                VectorNormalize(dir);
//                //VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
//                VectorSubtract(bs->origin, entinfo.origin, dir2);
//                VectorNormalize(dir2);
//                if (DotProduct(dir, dir2) > 0.7) {
//                  // back up
//                  BotSetupForMovement(bs);
//                  trap_BotMoveInDirection(bs->ms, dir2, 400, MOVE_WALK);
//                }
//              }
//            }
//          }
//        }
//        //check if the bot wants to crouch
//        //don't crouch if crouched less than 5 seconds ago
//        if (bs->attackcrouch_time < FloatTime() - 5) {
//          croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
//          if (random() < bs->thinktime * croucher) {
//            bs->attackcrouch_time = FloatTime() + 5 + croucher * 15;
//          }
//        }
//        //don't crouch when swimming
//        if (trap_AAS_Swimming(bs->origin)) bs->attackcrouch_time = FloatTime() - 1;
//        //if not arrived yet or arived some time ago
//        if (bs->arrive_time < FloatTime() - 2) {
//          //if not arrived yet
//          if (!bs->arrive_time) {
//            trap_EA_Gesture(bs->client);
//            BotAI_BotInitialChat(bs, "accompany_arrive", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
//            trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
//            bs->arrive_time = FloatTime();
//          }
//          //if the bot wants to crouch
//          else if (bs->attackcrouch_time > FloatTime()) {
//            trap_EA_Crouch(bs->client);
//          }
//          //else do some model taunts
//          else if (random() < bs->thinktime * 0.05) {
//            //do a gesture :)
//            trap_EA_Gesture(bs->client);
//          }
//        }
//        //if just arrived look at the companion
//        if (bs->arrive_time > FloatTime() - 2) {
//          VectorSubtract(entinfo.origin, bs->origin, dir);
//          vectoangles(dir, bs->ideal_viewangles);
//          bs->ideal_viewangles[2] *= 0.5;
//        }
//        //else look strategically around for enemies
//        else if (random() < bs->thinktime * 0.8) {
//          BotRoamGoal(bs, target);
//          VectorSubtract(target, bs->origin, dir);
//          vectoangles(dir, bs->ideal_viewangles);
//          bs->ideal_viewangles[2] *= 0.5;
//        }
//        //check if the bot wants to go for air
//        if (BotGoForAir(bs, bs->tfl, &bs->teamgoal, 400)) {
//          trap_BotResetLastAvoidReach(bs->ms);
//          //get the goal at the top of the stack
//          //trap_BotGetTopGoal(bs->gs, &tmpgoal);
//          //trap_BotGoalName(tmpgoal.number, buf, 144);
//          //BotAI_Print(PRT_MESSAGE, "new nearby goal %s\n", buf);
//          //time the bot gets to pick up the nearby goal item
//          bs->nbg_time = FloatTime() + 8;
//          AIEnter_Seek_NBG(bs, "BotLongTermGoal: go for air");
//          return qfalse;
//        }
//        //
//        trap_BotResetAvoidReach(bs->ms);
//        return qfalse;
//      }
//    }
//    //if the entity information is valid (entity in PVS)
//    if (entinfo.valid) {
//      areanum = BotPointAreaNum(entinfo.origin);
//      if (areanum && trap_AAS_AreaReachability(areanum)) {
//        //update team goal
//        bs->teamgoal.entitynum = bs->teammate;
//        bs->teamgoal.areanum = areanum;
//        VectorCopy(entinfo.origin, bs->teamgoal.origin);
//        VectorSet(bs->teamgoal.mins, -8, -8, -8);
//        VectorSet(bs->teamgoal.maxs, 8, 8, 8);
//      }
//    }
//    //the goal the bot should go for
//    memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
//    //if the companion is NOT visible for too long
//    if (bs->teammatevisible_time < FloatTime() - 60) {
//      BotAI_BotInitialChat(bs, "accompany_cannotfind", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
//      trap_BotEnterChat(bs->cs, bs->teammate, CHAT_TELL);
//      bs->ltgtype = 0;
//      // just to make sure the bot won't spam this message
//      bs->teammatevisible_time = FloatTime();
//    }
//    return qtrue;
//  }
  //
  if (bs->ltgtype == LTG_DEFENDKEYAREA) {
    if (trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin,
        bs->teamgoal.areanum, TFL_DEFAULT) > bs->defendaway_range) {
      bs->defendaway_time = 0;
    }
  }
  //if defending a key area
  if (bs->ltgtype == LTG_DEFENDKEYAREA && !retreat &&
        bs->defendaway_time < FloatTime()) {
    //check for bot typing status message
    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
      trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "defend_start", buf, NULL);
      trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//      BotVoiceChatOnly(bs, -1, VOICECHAT_ONDEFENSE);
      bs->teammessage_time = 0;
    }
    //set the bot goal
    memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
    //stop after 2 minutes
    if (bs->teamgoal_time < FloatTime()) {
      trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "defend_stop", buf, NULL);
      trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
      bs->ltgtype = 0;
    }
    //if very close... go away for some time
    VectorSubtract(goal->origin, bs->origin, dir);
    if (VectorLengthSquared(dir) < Square(70)) {
      trap_BotResetAvoidReach(bs->ms);
      bs->defendaway_time = FloatTime() + 3 + 3 * random();
      if (BotHasPersistantPowerupAndWeapon(bs)) {
        bs->defendaway_range = 100;
      }
      else {
        bs->defendaway_range = 350;
      }
    }
    return qtrue;
  }
  //going to kill someone
  if (bs->ltgtype == LTG_KILL && !retreat) {
    //check for bot typing status message
    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
      EasyClientName(bs->teamgoal.entitynum, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "kill_start", buf, NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      bs->teammessage_time = 0;
    }
    //
    if (bs->lastkilledplayer == bs->teamgoal.entitynum) {
      EasyClientName(bs->teamgoal.entitynum, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "kill_done", buf, NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      bs->lastkilledplayer = -1;
      bs->ltgtype = 0;
    }
    //
    if (bs->teamgoal_time < FloatTime()) {
      bs->ltgtype = 0;
    }
    //just roam around
    return BotGetItemLongTermGoal(bs, tfl, goal);
  }
  //get an item
  if (bs->ltgtype == LTG_GETITEM && !retreat) {
    //check for bot typing status message
    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
      trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "getitem_start", buf, NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
//      BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
      trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
      bs->teammessage_time = 0;
    }
    //set the bot goal
    memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
    //stop after some time
    if (bs->teamgoal_time < FloatTime()) {
      bs->ltgtype = 0;
    }
    //
    if (trap_BotItemGoalInVisButNotVisible(bs->entitynum, bs->eye, bs->viewangles, goal)) {
      trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "getitem_notthere", buf, NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      bs->ltgtype = 0;
    }
    else if (BotReachedGoal(bs, goal)) {
      trap_BotGoalName(bs->teamgoal.number, buf, sizeof(buf));
      BotAI_BotInitialChat(bs, "getitem_gotit", buf, NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      bs->ltgtype = 0;
    }
    return qtrue;
  }
  //if camping somewhere
  if ((bs->ltgtype == LTG_CAMP || bs->ltgtype == LTG_CAMPORDER) && !retreat) {
    //check for bot typing status message
    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
      if (bs->ltgtype == LTG_CAMPORDER) {
        BotAI_BotInitialChat(bs, "camp_start", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
        trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
//        BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
        trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
      }
      bs->teammessage_time = 0;
    }
    //set the bot goal
    memcpy(goal, &bs->teamgoal, sizeof(bot_goal_t));
    //
    if (bs->teamgoal_time < FloatTime()) {
      if (bs->ltgtype == LTG_CAMPORDER) {
        BotAI_BotInitialChat(bs, "camp_stop", NULL);
        trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      }
      bs->ltgtype = 0;
    }
    //if really near the camp spot
    VectorSubtract(goal->origin, bs->origin, dir);
    if (VectorLengthSquared(dir) < Square(60))
    {
      //if not arrived yet
      if (!bs->arrive_time) {
        if (bs->ltgtype == LTG_CAMPORDER) {
          BotAI_BotInitialChat(bs, "camp_arrive", EasyClientName(bs->teammate, netname, sizeof(netname)), NULL);
          trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
          BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_INPOSITION);
        }
        bs->arrive_time = FloatTime();
      }
      //look strategically around for enemies
      if (random() < bs->thinktime * 0.8) {
        BotRoamGoal(bs, target);
        VectorSubtract(target, bs->origin, dir);
        vectoangles(dir, bs->ideal_viewangles);
        bs->ideal_viewangles[2] *= 0.5;
      }
      //check if the bot wants to crouch
      //don't crouch if crouched less than 5 seconds ago
      if (bs->attackcrouch_time < FloatTime() - 5) {
        croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
        if (random() < bs->thinktime * croucher) {
          bs->attackcrouch_time = FloatTime() + 5 + croucher * 15;
        }
      }
      //if the bot wants to crouch
      if (bs->attackcrouch_time > FloatTime()) {
        trap_EA_Crouch(bs->client);
      }
      //don't crouch when swimming
      if (trap_AAS_Swimming(bs->origin)) bs->attackcrouch_time = FloatTime() - 1;
      //make sure the bot is not gonna drown
      if (trap_PointContents(bs->eye,bs->entitynum) & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA)) {
        if (bs->ltgtype == LTG_CAMPORDER) {
          BotAI_BotInitialChat(bs, "camp_stop", NULL);
          trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
          //
          if (bs->lastgoal_ltgtype == LTG_CAMPORDER) {
            bs->lastgoal_ltgtype = 0;
          }
        }
        bs->ltgtype = 0;
      }
      //
      if (bs->camp_range > 0) {
        //FIXME: move around a bit
      }
      //
      trap_BotResetAvoidReach(bs->ms);
      return qfalse;
    }
    return qtrue;
  }
  //patrolling along several waypoints
  if (bs->ltgtype == LTG_PATROL && !retreat) {
    //check for bot typing status message
    if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
      strcpy(buf, "");
      for (wp = bs->patrolpoints; wp; wp = wp->next) {
        strcat(buf, wp->name);
        if (wp->next) strcat(buf, " to ");
      }
      BotAI_BotInitialChat(bs, "patrol_start", buf, NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      BotVoiceChatOnly(bs, bs->decisionmaker, VOICECHAT_YES);
      trap_EA_Action(bs->client, ACTION_AFFIRMATIVE);
      bs->teammessage_time = 0;
    }
    //
    if (!bs->curpatrolpoint) {
      bs->ltgtype = 0;
      return qfalse;
    }
    //if the bot touches the current goal
    if (trap_BotTouchingGoal(bs->origin, &bs->curpatrolpoint->goal)) {
      if (bs->patrolflags & PATROL_BACK) {
        if (bs->curpatrolpoint->prev) {
          bs->curpatrolpoint = bs->curpatrolpoint->prev;
        }
        else {
          bs->curpatrolpoint = bs->curpatrolpoint->next;
          bs->patrolflags &= ~PATROL_BACK;
        }
      }
      else {
        if (bs->curpatrolpoint->next) {
          bs->curpatrolpoint = bs->curpatrolpoint->next;
        }
        else {
          bs->curpatrolpoint = bs->curpatrolpoint->prev;
          bs->patrolflags |= PATROL_BACK;
        }
      }
    }
    //stop after 5 minutes
    if (bs->teamgoal_time < FloatTime()) {
      BotAI_BotInitialChat(bs, "patrol_stop", NULL);
      trap_BotEnterChat(bs->cs, bs->decisionmaker, CHAT_TELL);
      bs->ltgtype = 0;
    }
    if (!bs->curpatrolpoint) {
      bs->ltgtype = 0;
      return qfalse;
    }
    memcpy(goal, &bs->curpatrolpoint->goal, sizeof(bot_goal_t));
    return qtrue;
  }
#ifdef CTF
  if (gametype == GT_CTF) {
    //if going for enemy flag
    if (bs->ltgtype == LTG_GETFLAG) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "captureflag_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONGETFLAG);
        bs->teammessage_time = 0;
      }
      //
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &ctf_redflag, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //if touching the flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        // make sure the bot knows the flag isn't there anymore
        switch(BotTeam(bs)) {
          case TEAM_RED: bs->blueflagstatus = 1; break;
          case TEAM_BLUE: bs->redflagstatus = 1; break;
        }
        bs->ltgtype = 0;
      }
      //stop after 3 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      BotAlternateRoute(bs, goal);
      return qtrue;
    }
    //if rushing to the base
    if (bs->ltgtype == LTG_RUSHBASE && bs->rushbaseaway_time < FloatTime()) {
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &ctf_redflag, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //if not carrying the flag anymore
      if (!BotCTFCarryingFlag(bs)) bs->ltgtype = 0;
      //quit rushing after 2 minutes
      if (bs->teamgoal_time < FloatTime()) bs->ltgtype = 0;
      //if touching the base flag the bot should loose the enemy flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        //if the bot is still carrying the enemy flag then the
        //base flag is gone, now just walk near the base a bit
        if (BotCTFCarryingFlag(bs)) {
          trap_BotResetAvoidReach(bs->ms);
          bs->rushbaseaway_time = FloatTime() + 5 + 10 * random();
          //FIXME: add chat to tell the others to get back the flag
        }
        else {
          bs->ltgtype = 0;
        }
      }
      BotAlternateRoute(bs, goal);
      return qtrue;
    }
    //returning flag
    if (bs->ltgtype == LTG_RETURNFLAG) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "returnflag_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONRETURNFLAG);
        bs->teammessage_time = 0;
      }
      //
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &ctf_redflag, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //if touching the flag
      if (trap_BotTouchingGoal(bs->origin, goal)) bs->ltgtype = 0;
      //stop after 3 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      BotAlternateRoute(bs, goal);
      return qtrue;
    }
  }
#endif //CTF
#ifdef MISSIONPACK
  else if (gametype == GT_1FCTF) {
    if (bs->ltgtype == LTG_GETFLAG) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "captureflag_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONGETFLAG);
        bs->teammessage_time = 0;
      }
      memcpy(goal, &ctf_neutralflag, sizeof(bot_goal_t));
      //if touching the flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        bs->ltgtype = 0;
      }
      //stop after 3 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      return qtrue;
    }
    //if rushing to the base
    if (bs->ltgtype == LTG_RUSHBASE) {
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &ctf_redflag, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //if not carrying the flag anymore
      if (!Bot1FCTFCarryingFlag(bs)) {
        bs->ltgtype = 0;
      }
      //quit rushing after 2 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      //if touching the base flag the bot should loose the enemy flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        bs->ltgtype = 0;
      }
      BotAlternateRoute(bs, goal);
      return qtrue;
    }
    //attack the enemy base
    if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
        bs->attackaway_time < FloatTime()) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "attackenemybase_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
        bs->teammessage_time = 0;
      }
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &ctf_blueflag, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &ctf_redflag, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //quit rushing after 2 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      //if touching the base flag the bot should loose the enemy flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        bs->attackaway_time = FloatTime() + 2 + 5 * random();
      }
      return qtrue;
    }
    //returning flag
    if (bs->ltgtype == LTG_RETURNFLAG) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "returnflag_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONRETURNFLAG);
        bs->teammessage_time = 0;
      }
      //
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      //just roam around
      return BotGetItemLongTermGoal(bs, tfl, goal);
    }
  }
  else if (gametype == GT_OBELISK) {
    if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
        bs->attackaway_time < FloatTime()) {

      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "attackenemybase_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
        bs->teammessage_time = 0;
      }
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &blueobelisk, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &redobelisk, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //if the bot no longer wants to attack the obelisk
      if (BotFeelingBad(bs) > 50) {
        return BotGetItemLongTermGoal(bs, tfl, goal);
      }
      //if touching the obelisk
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        bs->attackaway_time = FloatTime() + 3 + 5 * random();
      }
      // or very close to the obelisk
      VectorSubtract(bs->origin, goal->origin, dir);
      if (VectorLengthSquared(dir) < Square(60)) {
        bs->attackaway_time = FloatTime() + 3 + 5 * random();
      }
      //quit rushing after 2 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      BotAlternateRoute(bs, goal);
      //just move towards the obelisk
      return qtrue;
    }
  }
  else if (gametype == GT_HARVESTER) {
    //if rushing to the base
    if (bs->ltgtype == LTG_RUSHBASE) {
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &blueobelisk, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &redobelisk, sizeof(bot_goal_t)); break;
        default: BotGoHarvest(bs); return qfalse;
      }
      //if not carrying any cubes
      if (!BotHarvesterCarryingCubes(bs)) {
        BotGoHarvest(bs);
        return qfalse;
      }
      //quit rushing after 2 minutes
      if (bs->teamgoal_time < FloatTime()) {
        BotGoHarvest(bs);
        return qfalse;
      }
      //if touching the base flag the bot should loose the enemy flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        BotGoHarvest(bs);
        return qfalse;
      }
      BotAlternateRoute(bs, goal);
      return qtrue;
    }
    //attack the enemy base
    if (bs->ltgtype == LTG_ATTACKENEMYBASE &&
        bs->attackaway_time < FloatTime()) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "attackenemybase_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
        BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
        bs->teammessage_time = 0;
      }
      switch(BotTeam(bs)) {
        case TEAM_RED: memcpy(goal, &blueobelisk, sizeof(bot_goal_t)); break;
        case TEAM_BLUE: memcpy(goal, &redobelisk, sizeof(bot_goal_t)); break;
        default: bs->ltgtype = 0; return qfalse;
      }
      //quit rushing after 2 minutes
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      //if touching the base flag the bot should loose the enemy flag
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        bs->attackaway_time = FloatTime() + 2 + 5 * random();
      }
      return qtrue;
    }
    //harvest cubes
    if (bs->ltgtype == LTG_HARVEST &&
      bs->harvestaway_time < FloatTime()) {
      //check for bot typing status message
      if (bs->teammessage_time && bs->teammessage_time < FloatTime()) {
        BotAI_BotInitialChat(bs, "harvest_start", NULL);
        trap_BotEnterChat(bs->cs, 0, CHAT_TEAM);
//        BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
        bs->teammessage_time = 0;
      }
      memcpy(goal, &neutralobelisk, sizeof(bot_goal_t));
      //
      if (bs->teamgoal_time < FloatTime()) {
        bs->ltgtype = 0;
      }
      //
      if (trap_BotTouchingGoal(bs->origin, goal)) {
        bs->harvestaway_time = FloatTime() + 4 + 3 * random();
      }
      return qtrue;
    }
  }
#endif
  //normal goal stuff
  return BotGetItemLongTermGoal(bs, tfl, goal);
}

/*
==================
BotSetTeamStatus
==================
*/
void BotSetTeamStatus(bot_state_t *bs) {
//#ifdef MISSIONPACK
//  int teamtask;
//  aas_entityinfo_t entinfo;
//
//  teamtask = TEAMTASK_PATROL;
//
//  switch(bs->ltgtype) {
//    case LTG_TEAMHELP:
//      break;
//    case LTG_TEAMACCOMPANY:
//      BotEntityInfo(bs->teammate, &entinfo);
//      if ( ( (gametype == GT_CTF || gametype == GT_1FCTF) && EntityCarriesFlag(&entinfo))
//        || ( gametype == GT_HARVESTER && EntityCarriesCubes(&entinfo)) ) {
//        teamtask = TEAMTASK_ESCORT;
//      }
//      else {
//        teamtask = TEAMTASK_FOLLOW;
//      }
//      break;
//    case LTG_DEFENDKEYAREA:
//      teamtask = TEAMTASK_DEFENSE;
//      break;
//    case LTG_GETFLAG:
//      teamtask = TEAMTASK_OFFENSE;
//      break;
//    case LTG_RUSHBASE:
//      teamtask = TEAMTASK_DEFENSE;
//      break;
//    case LTG_RETURNFLAG:
//      teamtask = TEAMTASK_RETRIEVE;
//      break;
//    case LTG_CAMP:
//    case LTG_CAMPORDER:
//      teamtask = TEAMTASK_CAMP;
//      break;
//    case LTG_PATROL:
//      teamtask = TEAMTASK_PATROL;
//      break;
//    case LTG_GETITEM:
//      teamtask = TEAMTASK_PATROL;
//      break;
//    case LTG_KILL:
//      teamtask = TEAMTASK_PATROL;
//      break;
//    case LTG_HARVEST:
//      teamtask = TEAMTASK_OFFENSE;
//      break;
//    case LTG_ATTACKENEMYBASE:
//      teamtask = TEAMTASK_OFFENSE;
//      break;
//    default:
//      teamtask = TEAMTASK_PATROL;
//      break;
//  }
//  BotSetUserInfo(bs, "teamtask", va("%d", teamtask));
//#endif
}

/*
==================
BotSetLastOrderedTask
==================
*/
int BotSetLastOrderedTask(bot_state_t *bs) {
  if ( bs->lastgoal_ltgtype ) {
    bs->decisionmaker = bs->lastgoal_decisionmaker;
    bs->ordered = qtrue;
    bs->ltgtype = bs->lastgoal_ltgtype;
    memcpy(&bs->teamgoal, &bs->lastgoal_teamgoal, sizeof(bot_goal_t));
    bs->teammate = bs->lastgoal_teammate;
    bs->teamgoal_time = FloatTime() + 300;
    BotSetTeamStatus(bs);
    return qtrue;
  }
  return qfalse;
}

/*
==================
BotRefuseOrder
==================
*/
void BotRefuseOrder(bot_state_t *bs) {
  if (!bs->ordered)
    return;
  // if the bot was ordered to do something
  if ( bs->order_time && bs->order_time > FloatTime() - 10 ) {
    trap_EA_Action(bs->client, ACTION_NEGATIVE);
//    BotVoiceChat(bs, bs->decisionmaker, VOICECHAT_NO);
    bs->order_time = 0;
  }
}


/*
==================
BotTeamGoals
==================
*/
void BotTeamGoals(bot_state_t *bs, int retreat) {

  if ( retreat ) {
  }
  else {
  }
  // reset the order time which is used to see if
  // we decided to refuse an order
  bs->order_time = 0;
}

/*
==================
ClientFromName
==================
*/
int ClientFromName(char *name) {
  int i;
  char buf[MAX_INFO_STRING];
  static int maxclients;

  if (!maxclients)
    maxclients = trap_Cvar_VariableIntegerValue("sv_maxclients");
  for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
    trap_GetConfigstring(CS_PLAYERS+i, buf, sizeof(buf));
    Q_CleanStr( buf );
    if (!Q_stricmp(Info_ValueForKey(buf, "n"), name)) return i;
  }
  return -1;
}

/*
==================
stristr
==================
*/
char *stristr(char *str, char *charset) {
  int i;

  while(*str) {
    for (i = 0; charset[i] && str[i]; i++) {
      if (toupper(charset[i]) != toupper(str[i])) break;
    }
    if (!charset[i]) return str;
    str++;
  }
  return NULL;
}

/*
==================
BotSynonymContext
==================
*/
int BotSynonymContext(bot_state_t *bs) {
  int context;

//  context = CONTEXT_NORMAL|CONTEXT_NEARBYITEM|CONTEXT_NAMES;
//  return context;
  return 0;
}

/*
==================
BotChooseWeapon
==================
*/
void BotChooseWeapon(bot_state_t *bs) {
  int newweaponnum;

  if (bs->cur_ps.weaponstate == WEAPON_RAISING ||
      bs->cur_ps.weaponstate == WEAPON_DROPPING) {
    trap_EA_SelectWeapon(bs->client, bs->weaponnum);
  }
  else {
    newweaponnum = trap_BotChooseBestFightWeapon(bs->ws, bs->inventory);
    if (bs->weaponnum != newweaponnum) bs->weaponchange_time = FloatTime();
    bs->weaponnum = newweaponnum;
    //BotAI_Print(PRT_MESSAGE, "bs->weaponnum = %d\n", bs->weaponnum);
    trap_EA_SelectWeapon(bs->client, bs->weaponnum);
  }
}

/*
==================
BotUpdateBattleInventory
==================
*/
void BotUpdateBattleInventory(bot_state_t *bs, int enemy) {
//  vec3_t dir;
//  aas_entityinfo_t entinfo;
//
//  BotEntityInfo(enemy, &entinfo);
//  VectorSubtract(entinfo.origin, bs->origin, dir);
//  bs->inventory[ENEMY_HEIGHT] = (int) dir[2];
//  dir[2] = 0;
//  bs->inventory[ENEMY_HORIZONTAL_DIST] = (int) VectorLength(dir);
//  //FIXME: add num visible enemies and num visible team mates to the inventory
}

/*
==================
BotBattleUseItems
==================
*/
void BotBattleUseItems(bot_state_t *bs) {
  if (bs->inventory[BI_HEALTH] < 60) {
    if (bs->inventory[BI_MEDKIT] > 0) {
      trap_EA_Use(bs->client);
    }
  }
}

/*
==================
BotAggression
==================
*/
float BotAggression(bot_state_t *bs) {
  //if the enemy is located way higher than the bot
//  if (bs->inventory[ENEMY_HEIGHT] > 200) return 0;
  //if the bot is very low on health
  if (bs->inventory[BI_HEALTH] < 60) return 0;
//  //if the bot is low on health
//  if (bs->inventory[BI_HEALTH] < 80) {
//    //if the bot has insufficient armor
//    if (bs->inventory[BI_ARMOR] < 40) return 0;
//  }
//  //if the bot can use the bfg
//  if (bs->inventory[BI_BFG10K] > 0 &&
//      bs->inventory[BI_BFGAMMO] > 7) return 100;
//  //if the bot can use the railgun
//  if (bs->inventory[BI_RAILGUN] > 0 &&
//      bs->inventory[BI_SLUGS] > 5) return 95;
//  //if the bot can use the lightning gun
//  if (bs->inventory[BI_LIGHTNING] > 0 &&
//      bs->inventory[BI_LIGHTNINGAMMO] > 50) return 90;
//  //if the bot can use the rocketlauncher
//  if (bs->inventory[BI_ROCKETLAUNCHER] > 0 &&
//      bs->inventory[BI_ROCKETS] > 5) return 90;
//  //if the bot can use the plasmagun
//  if (bs->inventory[BI_PLASMAGUN] > 0 &&
//      bs->inventory[BI_CELLS] > 40) return 85;
//  //if the bot can use the grenade launcher
//  if (bs->inventory[BI_GRENADELAUNCHER] > 0 &&
//      bs->inventory[BI_GRENADES] > 10) return 80;
//  //if the bot can use the shotgun
//  if (bs->inventory[BI_SHOTGUN] > 0 &&
//      bs->inventory[BI_SHELLS] > 10) return 50;
//  //otherwise the bot is not feeling too good
  return 0;
}

/*
==================
BotFeelingBad
==================
*/
float BotFeelingBad(bot_state_t *bs) {
  if (bs->inventory[BI_HEALTH] < 40) {
    return 100;
  }
  if (bs->weaponnum == WP_MACHINEGUN) {
    return 90;
  }
  if (bs->inventory[BI_HEALTH] < 60) {
    return 80;
  }
  return 0;
}

/*
==================
HBotWantsToRetreat
==================
*/
int HBotWantsToRetreat(bot_state_t *bs) {
  aas_entityinfo_t entinfo;
  if (BotAggression(bs) < 50)
    return qtrue;
  return qfalse;
}

/*
==================
HBotWantsToChase
==================
*/
int HBotWantsToChase(bot_state_t *bs) {
  aas_entityinfo_t entinfo;

//  if (gametype == GT_CTF) {
//    //never chase when carrying a CTF flag
//    if (BotCTFCarryingFlag(bs))
//      return qfalse;
//    //always chase if the enemy is carrying a flag
//    BotEntityInfo(bs->enemy, &entinfo);
//    if (EntityCarriesFlag(&entinfo))
//      return qtrue;
//  }
//#ifdef MISSIONPACK
//  else if (gametype == GT_1FCTF) {
//    //never chase if carrying the flag
//    if (Bot1FCTFCarryingFlag(bs))
//      return qfalse;
//    //always chase if the enemy is carrying a flag
//    BotEntityInfo(bs->enemy, &entinfo);
//    if (EntityCarriesFlag(&entinfo))
//      return qtrue;
//  }
//  else if (gametype == GT_OBELISK) {
//    //the bots should be dedicated to attacking the enemy obelisk
//    if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
//      if (bs->enemy != redobelisk.entitynum ||
//            bs->enemy != blueobelisk.entitynum) {
//        return qfalse;
//      }
//    }
//  }
//  else if (gametype == GT_HARVESTER) {
//    //never chase if carrying cubes
//    if (BotHarvesterCarryingCubes(bs))
//      return qfalse;
//  }
//#endif
//  //if the bot is getting the flag
//  if (bs->ltgtype == LTG_GETFLAG)
//    return qfalse;
//  //
  if (BotAggression(bs) > 50)
    return qtrue;
  return qfalse;
}

/*
==================
BotWantsToHelp
==================
*/
int BotWantsToHelp(bot_state_t *bs) {
  return qtrue;
}

/*
==================
BotCanAndWantsToRocketJump
==================
*/
int BotCanAndWantsToRocketJump(bot_state_t *bs) {
  float rocketjumper;
//
//  //if rocket jumping is disabled
//  if (!bot_rocketjump.integer) return qfalse;
//  //if no rocket launcher
//  if (bs->inventory[BI_ROCKETLAUNCHER] <= 0) return qfalse;
//  //if low on rockets
//  if (bs->inventory[BI_ROCKETS] < 3) return qfalse;
//  //never rocket jump with the Quad
//  if (bs->inventory[BI_QUAD]) return qfalse;
  //if low on health
  if (bs->inventory[BI_HEALTH] < 60) return qfalse;
  //if not full health
//  if (bs->inventory[BI_HEALTH] < 90) {
//    //if the bot has insufficient armor
//    if (bs->inventory[BI_ARMOR] < 40) return qfalse;
//  }
  rocketjumper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_WEAPONJUMPING, 0, 1);
  if (rocketjumper < 0.5) return qfalse;
  return qtrue;
}

/*
==================
BotHasPersistantPowerupAndWeapon
==================
*/
int BotHasPersistantPowerupAndWeapon(bot_state_t *bs) {
//#ifdef MISSIONPACK
//  // if the bot does not have a persistant powerup
//  if (!bs->inventory[BI_SCOUT] &&
//    !bs->inventory[BI_GUARD] &&
//    !bs->inventory[BI_DOUBLER] &&
//    !bs->inventory[BI_AMMOREGEN] ) {
//    return qfalse;
//  }
//#endif
  //if the bot is very low on health
  if (bs->inventory[BI_HEALTH] < 60) return qfalse;
  //if the bot is low on health
//  if (bs->inventory[BI_HEALTH] < 80) {
//    //if the bot has insufficient armor
//    if (bs->inventory[BI_ARMOR] < 40) return qfalse;
//  }
//  //if the bot can use the bfg
//  if (bs->inventory[BI_BFG10K] > 0 &&
//      bs->inventory[BI_BFGAMMO] > 7) return qtrue;
//  //if the bot can use the railgun
//  if (bs->inventory[BI_RAILGUN] > 0 &&
//      bs->inventory[BI_SLUGS] > 5) return qtrue;
//  //if the bot can use the lightning gun
//  if (bs->inventory[BI_LIGHTNING] > 0 &&
//      bs->inventory[BI_LIGHTNINGAMMO] > 50) return qtrue;
//  //if the bot can use the rocketlauncher
//  if (bs->inventory[BI_ROCKETLAUNCHER] > 0 &&
//      bs->inventory[BI_ROCKETS] > 5) return qtrue;
//  //
//  if (bs->inventory[BI_NAILGUN] > 0 &&
//      bs->inventory[BI_NAILS] > 5) return qtrue;
//  //
//  if (bs->inventory[BI_PROXLAUNCHER] > 0 &&
//      bs->inventory[BI_MINES] > 5) return qtrue;
//  //
//  if (bs->inventory[BI_CHAINGUN] > 0 &&
//      bs->inventory[BI_BELT] > 40) return qtrue;
//  //if the bot can use the plasmagun
//  if (bs->inventory[BI_PLASMAGUN] > 0 &&
//      bs->inventory[BI_CELLS] > 20) return qtrue;
  return qfalse;
}

/*
==================
BotGoCamp
==================
*/
void BotGoCamp(bot_state_t *bs, bot_goal_t *goal) {
  float camper;

  bs->decisionmaker = bs->client;
  //set message time to zero so bot will NOT show any message
  bs->teammessage_time = 0;
  //set the ltg type
  bs->ltgtype = LTG_CAMP;
  //set the team goal
  memcpy(&bs->teamgoal, goal, sizeof(bot_goal_t));
  //get the team goal time
  camper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
  if (camper > 0.99) bs->teamgoal_time = FloatTime() + 99999;
  else bs->teamgoal_time = FloatTime() + 120 + 180 * camper + random() * 15;
  //set the last time the bot started camping
  bs->camp_time = FloatTime();
  //the teammate that requested the camping
  bs->teammate = 0;
  //do NOT type arrive message
  bs->arrive_time = 1;
}

/*
==================
BotWantsToCamp
==================
*/
int BotWantsToCamp(bot_state_t *bs) {
  float camper;
  int cs, traveltime, besttraveltime;
  bot_goal_t goal, bestgoal;

  camper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
  if (camper < 0.1) return qfalse;
  //if the bot has a team goal
  if (bs->ltgtype == LTG_TEAMHELP ||
      bs->ltgtype == LTG_TEAMACCOMPANY ||
      bs->ltgtype == LTG_DEFENDKEYAREA ||
//      bs->ltgtype == LTG_GETFLAG ||
      bs->ltgtype == LTG_RUSHBASE ||
      bs->ltgtype == LTG_CAMP ||
      bs->ltgtype == LTG_CAMPORDER ||
      bs->ltgtype == LTG_PATROL) {
    return qfalse;
  }
  //if camped recently
  if (bs->camp_time > FloatTime() - 60 + 300 * (1-camper)) return qfalse;
  //
  if (random() > camper) {
    bs->camp_time = FloatTime();
    return qfalse;
  }
  //if the bot isn't healthy anough
  if (BotAggression(bs) < 50) return qfalse;
  //the bot should have at least have the rocket launcher, the railgun or the bfg10k with some ammo
//  if ((bs->inventory[BI_ROCKETLAUNCHER] <= 0 || bs->inventory[BI_ROCKETS < 10]) &&
//    (bs->inventory[BI_RAILGUN] <= 0 || bs->inventory[BI_SLUGS] < 10) &&
//    (bs->inventory[BI_BFG10K] <= 0 || bs->inventory[BI_BFGAMMO] < 10)) {
//    return qfalse;
//  }
  //find the closest camp spot
  besttraveltime = 99999;
  for (cs = trap_BotGetNextCampSpotGoal(0, &goal); cs; cs = trap_BotGetNextCampSpotGoal(cs, &goal)) {
    traveltime = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, goal.areanum, TFL_DEFAULT);
    if (traveltime && traveltime < besttraveltime) {
      besttraveltime = traveltime;
      memcpy(&bestgoal, &goal, sizeof(bot_goal_t));
    }
  }
  if (besttraveltime > 150) return qfalse;
  //ok found a camp spot, go camp there
  BotGoCamp(bs, &bestgoal);
  bs->ordered = qfalse;
  //
  return qtrue;
}

/*
==================
BotDontAvoid
==================
*/
void BotDontAvoid(bot_state_t *bs, char *itemname) {
  bot_goal_t goal;
  int num;

  num = trap_BotGetLevelItemGoal(-1, itemname, &goal);
  while(num >= 0) {
    trap_BotRemoveFromAvoidGoals(bs->gs, goal.number);
    num = trap_BotGetLevelItemGoal(num, itemname, &goal);
  }
}

/*
==================
BotGoForPowerups
==================
*/
void BotGoForPowerups(bot_state_t *bs) {

  //don't avoid any of the powerups anymore
//  BotDontAvoid(bs, "Quad Damage");
//  BotDontAvoid(bs, "Regeneration");
//  BotDontAvoid(bs, "Battle Suit");
//  BotDontAvoid(bs, "Speed");
//  BotDontAvoid(bs, "Invisibility");
  //BotDontAvoid(bs, "Flight");
  //reset the long term goal time so the bot will go for the powerup
  //NOTE: the long term goal type doesn't change
  bs->ltg_time = 0;
}

/*
==================
BotRoamGoal
==================
*/
void BotRoamGoal(bot_state_t *bs, vec3_t goal) {
  int pc, i;
  float len, rnd;
  vec3_t dir, bestorg, belowbestorg;
  bsp_trace_t trace;

  for (i = 0; i < 10; i++) {
    //start at the bot origin
    VectorCopy(bs->origin, bestorg);
    rnd = random();
    if (rnd > 0.25) {
      //add a random value to the x-coordinate
      if (random() < 0.5) bestorg[0] -= 800 * random() + 100;
      else bestorg[0] += 800 * random() + 100;
    }
    if (rnd < 0.75) {
      //add a random value to the y-coordinate
      if (random() < 0.5) bestorg[1] -= 800 * random() + 100;
      else bestorg[1] += 800 * random() + 100;
    }
    //add a random value to the z-coordinate (NOTE: 48 = maxjump?)
    bestorg[2] += 2 * 48 * crandom();
    //trace a line from the origin to the roam target
    BotAI_Trace(&trace, bs->origin, NULL, NULL, bestorg, bs->entitynum, MASK_SOLID);
    //direction and length towards the roam target
    VectorSubtract(trace.endpos, bs->origin, dir);
    len = VectorNormalize(dir);
    //if the roam target is far away anough
    if (len > 200) {
      //the roam target is in the given direction before walls
      VectorScale(dir, len * trace.fraction - 40, dir);
      VectorAdd(bs->origin, dir, bestorg);
      //get the coordinates of the floor below the roam target
      belowbestorg[0] = bestorg[0];
      belowbestorg[1] = bestorg[1];
      belowbestorg[2] = bestorg[2] - 800;
      BotAI_Trace(&trace, bestorg, NULL, NULL, belowbestorg, bs->entitynum, MASK_SOLID);
      //
      if (!trace.startsolid) {
        trace.endpos[2]++;
        pc = trap_PointContents(trace.endpos, bs->entitynum);
        if (!(pc & (CONTENTS_LAVA | CONTENTS_SLIME))) {
          VectorCopy(bestorg, goal);
          return;
        }
      }
    }
  }
  VectorCopy(bestorg, goal);
}

/*
==================
BotAttackMove
==================
*/
bot_moveresult_t BotAttackMove(bot_state_t *bs, int tfl) {
  int movetype, i, attackentity;
  float attack_skill, jumper, croucher, dist, strafechange_time;
  float attack_dist, attack_range;
  vec3_t forward, backward, sideward, hordir, up = {0, 0, 1};
  aas_entityinfo_t entinfo;
  bot_moveresult_t moveresult;
  bot_goal_t goal;

  attackentity = bs->enemy;
  //
  if (bs->attackchase_time > FloatTime()) {
    //create the chase goal
    goal.entitynum = attackentity;
    goal.areanum = bs->lastenemyareanum;
    VectorCopy(bs->lastenemyorigin, goal.origin);
    VectorSet(goal.mins, -8, -8, -8);
    VectorSet(goal.maxs, 8, 8, 8);
    //initialize the movement state
    BotSetupForMovement(bs);
    //move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, &goal, tfl);
    return moveresult;
  }
  //
  memset(&moveresult, 0, sizeof(bot_moveresult_t));
  //
  attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
  jumper = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_JUMPER, 0, 1);
  croucher = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
  //if the bot is really stupid
  if (attack_skill < 0.2) return moveresult;
  //initialize the movement state
  BotSetupForMovement(bs);
  //get the enemy entity info
  BotEntityInfo(attackentity, &entinfo);
  //direction towards the enemy
  VectorSubtract(entinfo.origin, bs->origin, forward);
  //the distance towards the enemy
  dist = VectorNormalize(forward);
  VectorNegate(forward, backward);
  //walk, crouch or jump
  movetype = MOVE_WALK;
  //
  if (bs->attackcrouch_time < FloatTime() - 1) {
    if (random() < jumper) {
      movetype = MOVE_JUMP;
    }
    //wait at least one second before crouching again
    else if (bs->attackcrouch_time < FloatTime() - 1 && random() < croucher) {
      bs->attackcrouch_time = FloatTime() + croucher * 5;
    }
  }
  if (bs->attackcrouch_time > FloatTime()) movetype = MOVE_CROUCH;
  //if the bot should jump
  if (movetype == MOVE_JUMP) {
    //if jumped last frame
    if (bs->attackjump_time > FloatTime()) {
      movetype = MOVE_WALK;
    }
    else {
      bs->attackjump_time = FloatTime() + 1;
    }
  }
//  if (bs->cur_ps.weapon == WP_GAUNTLET) {
//    attack_dist = 0;
//    attack_range = 0;
//  }
  else {
    attack_dist = IDEAL_ATTACKDIST;
    attack_range = 40;
  }
  //if the bot is stupid
  if (attack_skill <= 0.4) {
    //just walk to or away from the enemy
    if (dist > attack_dist + attack_range) {
      if (trap_BotMoveInDirection(bs->ms, forward, 400, movetype)) return moveresult;
    }
    if (dist < attack_dist - attack_range) {
      if (trap_BotMoveInDirection(bs->ms, backward, 400, movetype)) return moveresult;
    }
    return moveresult;
  }
  //increase the strafe time
  bs->attackstrafe_time += bs->thinktime;
  //get the strafe change time
  strafechange_time = 0.4 + (1 - attack_skill) * 0.2;
  if (attack_skill > 0.7) strafechange_time += crandom() * 0.2;
  //if the strafe direction should be changed
  if (bs->attackstrafe_time > strafechange_time) {
    //some magic number :)
    if (random() > 0.935) {
      //flip the strafe direction
      bs->flags ^= BFL_STRAFERIGHT;
      bs->attackstrafe_time = 0;
    }
  }
  //
  for (i = 0; i < 2; i++) {
    hordir[0] = forward[0];
    hordir[1] = forward[1];
    hordir[2] = 0;
    VectorNormalize(hordir);
    //get the sideward vector
    CrossProduct(hordir, up, sideward);
    //reverse the vector depending on the strafe direction
    if (bs->flags & BFL_STRAFERIGHT) VectorNegate(sideward, sideward);
    //randomly go back a little
    if (random() > 0.9) {
      VectorAdd(sideward, backward, sideward);
    }
    else {
      //walk forward or backward to get at the ideal attack distance
      if (dist > attack_dist + attack_range) {
        VectorAdd(sideward, forward, sideward);
      }
      else if (dist < attack_dist - attack_range) {
        VectorAdd(sideward, backward, sideward);
      }
    }
    //perform the movement
    if (trap_BotMoveInDirection(bs->ms, sideward, 400, movetype))
      return moveresult;
    //movement failed, flip the strafe direction
    bs->flags ^= BFL_STRAFERIGHT;
    bs->attackstrafe_time = 0;
  }
  //bot couldn't do any usefull movement
//  bs->attackchase_time = AAS_Time() + 6;
  return moveresult;
}

/*
==================
HBotNetFindEnemy
==================
*/
int HBotNetFindEnemy(bot_state_t *bs, int curenemy) {
  int i, healthdecrease;
  float f, alertness, easyfragger, vis;
  float squaredist, cursquaredist;
  aas_entityinfo_t entinfo, curenemyinfo;
  vec3_t dir, angles;

  alertness = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ALERTNESS, 0, 1);
  easyfragger = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_EASY_FRAGGER, 0, 1);
  //check if the health decreased
  healthdecrease = bs->lasthealth > bs->inventory[BI_HEALTH];
  //remember the current health value
  bs->lasthealth = bs->inventory[BI_HEALTH];
  //
  if (curenemy >= 0) {
    BotEntityInfo(curenemy, &curenemyinfo);
    VectorSubtract(curenemyinfo.origin, bs->origin, dir);
    cursquaredist = VectorLengthSquared(dir);
  }
  else {
    cursquaredist = 0;
  }
//#ifdef MISSIONPACK
//  if (gametype == GT_OBELISK) {
//    vec3_t target;
//    bot_goal_t *goal;
//    bsp_trace_t trace;
//
//    if (BotTeam(bs) == TEAM_RED)
//      goal = &blueobelisk;
//    else
//      goal = &redobelisk;
//    //if the obelisk is visible
//    VectorCopy(goal->origin, target);
//    target[2] += 1;
//    BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
//    if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
//      if (goal->entitynum == bs->enemy) {
//        return qfalse;
//      }
//      bs->enemy = goal->entitynum;
//      bs->enemysight_time = FloatTime();
//      bs->enemysuicide = qfalse;
//      bs->enemydeath_time = 0;
//      bs->enemyvisible_time = FloatTime();
//      return qtrue;
//    }
//  }
//#endif
  //
  for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {

    if (i == bs->client) continue;
    //if it's the current enemy
    if (i == curenemy) continue;
    //
    BotEntityInfo(i, &entinfo);
    //
    if (!entinfo.valid) continue;
    //if the enemy isn't dead and the enemy isn't the bot self
    if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum) continue;
    //if the enemy is invisible and not shooting
    if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
      continue;
    }
    //if not an easy fragger don't shoot at chatting players
    if (easyfragger < 0.5 && EntityIsChatting(&entinfo)) continue;
    //
    if (lastteleport_time > FloatTime() - 3) {
      VectorSubtract(entinfo.origin, lastteleport_origin, dir);
      if (VectorLengthSquared(dir) < Square(70)) continue;
    }
    //calculate the distance towards the enemy
    VectorSubtract(entinfo.origin, bs->origin, dir);
    squaredist = VectorLengthSquared(dir);
    //if this entity is not carrying a flag
    if (!EntityCarriesFlag(&entinfo))
    {
      //if this enemy is further away than the current one
      if (curenemy >= 0 && squaredist > cursquaredist) continue;
    } //end if
    //if the bot has no
    if (squaredist > Square(900.0 + alertness * 4000.0)) continue;
    //if on the same team
    if (BotSameTeam(bs, i)) continue;
    //if the bot's health decreased or the enemy is shooting
    if (curenemy < 0 && (healthdecrease || EntityIsShooting(&entinfo)))
      f = 360;
    else
      f = 90 + 90 - (90 - (squaredist > Square(810) ? Square(810) : squaredist) / (810 * 9));
    //check if the enemy is visible
    vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, f, i);
    if (vis <= 0) continue;
    //if the enemy is quite far away, not shooting and the bot is not damaged
    if (curenemy < 0 && squaredist > Square(100) && !healthdecrease && !EntityIsShooting(&entinfo))
    {
      //check if we can avoid this enemy
      VectorSubtract(bs->origin, entinfo.origin, dir);
      vectoangles(dir, angles);
      //if the bot isn't in the fov of the enemy
      if (!BotInFieldOfVision(entinfo.angles, 90, angles)) {
        //update some stuff for this enemy
        BotUpdateBattleInventory(bs, i);
        //if the bot doesn't really want to fight
        if (HBotWantsToRetreat(bs)) continue;
      }
    }
    //found an enemy
    bs->enemy = entinfo.number;
    if (curenemy >= 0) bs->enemysight_time = FloatTime() - 2;
    else bs->enemysight_time = FloatTime();
    bs->enemysuicide = qfalse;
    bs->enemydeath_time = 0;
    bs->enemyvisible_time = FloatTime();
    return qtrue;
  }
  return qfalse;
}

/*
==================
BotVisibleTeamMatesAndEnemies
==================
*/
void BotVisibleTeamMatesAndEnemies(bot_state_t *bs, int *teammates, int *enemies, float range) {
  int i;
  float vis;
  aas_entityinfo_t entinfo;
  vec3_t dir;

  if (teammates)
    *teammates = 0;
  if (enemies)
    *enemies = 0;
  for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
    if (i == bs->client)
      continue;
    //
    BotEntityInfo(i, &entinfo);
    //if this player is active
    if (!entinfo.valid)
      continue;
//    //if this player is carrying a flag
//    if (!EntityCarriesFlag(&entinfo))
//      continue;
    //if not within range
    VectorSubtract(entinfo.origin, bs->origin, dir);
    if (VectorLengthSquared(dir) > Square(range))
      continue;
    //if the flag carrier is not visible
    vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
    if (vis <= 0)
      continue;
    //if the flag carrier is on the same team
    if (BotSameTeam(bs, i)) {
      if (teammates)
        (*teammates)++;
    }
    else {
      if (enemies)
        (*enemies)++;
    }
  }
}

/*
==================
HNetBotAimAtEnemy
==================
*/
void HNetBotAimAtEnemy(bot_state_t *bs) {
  int i, enemyvisible;
  float dist, f, aim_skill, aim_accuracy, speed, reactiontime;
  vec3_t dir, bestorigin, end, start, groundtarget, cmdmove, enemyvelocity;
  vec3_t mins = {-4,-4,-4}, maxs = {4, 4, 4};
  weaponinfo_t wi;
  aas_entityinfo_t entinfo;
  bot_goal_t goal;
  bsp_trace_t trace;
  vec3_t target;

  //if the bot has no enemy
  if (bs->enemy < 0) {
    return;
  }
  //get the enemy entity information
  BotEntityInfo(bs->enemy, &entinfo);
  //if this is not a player (should be an building)
  if (bs->enemy >= MAX_CLIENTS) {
    //if the obelisk is visible
    VectorCopy(entinfo.origin, target);
    //aim at the obelisk
    VectorSubtract(target, bs->eye, dir);
    vectoangles(dir, bs->ideal_viewangles);
    //set the aim target before trying to attack
    VectorCopy(target, bs->aimtarget);
    return;
  }
  //
  //BotAI_Print(PRT_MESSAGE, "client %d: aiming at client %d\n", bs->entitynum, bs->enemy);
  //
  aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL, 0, 1);
  aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
  //
  if (aim_skill > 0.95) {
    //don't aim too early
    reactiontime = 0.5 * trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
    if (bs->enemysight_time > FloatTime() - reactiontime) return;
    if (bs->teleport_time > FloatTime() - reactiontime) return;
  }

  //get the weapon information
  trap_BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
  //get the weapon specific aim accuracy and or aim skill
  if (wi.number == WP_MACHINEGUN) {
    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_MACHINEGUN, 0, 1);
  }
  else if (wi.number == WP_SHOTGUN) {
    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_SHOTGUN, 0, 1);
  }
//  else if (wi.number == WP_GRENADE_LAUNCHER) {
//    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_GRENADELAUNCHER, 0, 1);
//    aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_GRENADELAUNCHER, 0, 1);
//  }
//  else if (wi.number == WP_ROCKET_LAUNCHER) {
//    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_ROCKETLAUNCHER, 0, 1);
//    aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_ROCKETLAUNCHER, 0, 1);
//  }
//  else if (wi.number == WP_LIGHTNING) {
//    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_LIGHTNING, 0, 1);
//  }
//  else if (wi.number == WP_RAILGUN) {
//    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_RAILGUN, 0, 1);
//  }
//  else if (wi.number == WP_PLASMAGUN) {
//    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_PLASMAGUN, 0, 1);
//    aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_PLASMAGUN, 0, 1);
//  }
//  else if (wi.number == WP_BFG) {
//    aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY_BFG10K, 0, 1);
//    aim_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL_BFG10K, 0, 1);
//  }
  //
  if (aim_accuracy <= 0) aim_accuracy = 0.0001f;
  //get the enemy entity information
  BotEntityInfo(bs->enemy, &entinfo);
  //if the enemy is invisible then shoot crappy most of the time
  if (EntityIsInvisible(&entinfo)) {
    if (random() > 0.1) aim_accuracy *= 0.4f;
  }
  //
  VectorSubtract(entinfo.origin, entinfo.lastvisorigin, enemyvelocity);
  VectorScale(enemyvelocity, 1 / entinfo.update_time, enemyvelocity);
  //enemy origin and velocity is remembered every 0.5 seconds
  if (bs->enemyposition_time < FloatTime()) {
    //
    bs->enemyposition_time = FloatTime() + 0.5;
    VectorCopy(enemyvelocity, bs->enemyvelocity);
    VectorCopy(entinfo.origin, bs->enemyorigin);
  }
  //if not extremely skilled
  if (aim_skill < 0.9) {
    VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
    //if the enemy moved a bit
    if (VectorLengthSquared(dir) > Square(48)) {
      //if the enemy changed direction
      if (DotProduct(bs->enemyvelocity, enemyvelocity) < 0) {
        //aim accuracy should be worse now
        aim_accuracy *= 0.7f;
      }
    }
  }
  //check visibility of enemy
  enemyvisible = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy);
  //if the enemy is visible
  if (enemyvisible) {
    //
    VectorCopy(entinfo.origin, bestorigin);
    bestorigin[2] += 8;
    //get the start point shooting from
    //NOTE: the x and y projectile start offsets are ignored
    VectorCopy(bs->origin, start);
    start[2] += bs->cur_ps.viewheight;
    start[2] += wi.offset[2];
    //
    BotAI_Trace(&trace, start, mins, maxs, bestorigin, bs->entitynum, MASK_SHOT);
    //if the enemy is NOT hit
    if (trace.fraction <= 1 && trace.ent != entinfo.number) {
      bestorigin[2] += 16;
    }
    //if it is not an instant hit weapon the bot might want to predict the enemy
    if (wi.speed) {
      //
      VectorSubtract(bestorigin, bs->origin, dir);
      dist = VectorLength(dir);
      VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
      //if the enemy is NOT pretty far away and strafing just small steps left and right
      if (!(dist > 100 && VectorLengthSquared(dir) < Square(32))) {
        //if skilled anough do exact prediction
        if (aim_skill > 0.8 &&
            //if the weapon is ready to fire
            bs->cur_ps.weaponstate == WEAPON_READY) {
          aas_clientmove_t move;
          vec3_t origin;

          VectorSubtract(entinfo.origin, bs->origin, dir);
          //distance towards the enemy
          dist = VectorLength(dir);
          //direction the enemy is moving in
          VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
          //
          VectorScale(dir, 1 / entinfo.update_time, dir);
          //
          VectorCopy(entinfo.origin, origin);
          origin[2] += 1;
          //
          VectorClear(cmdmove);
          //AAS_ClearShownDebugLines();
          trap_AAS_PredictClientMovement(&move, bs->enemy, origin,
                            PRESENCE_CROUCH, qfalse,
                            dir, cmdmove, 0,
                            dist * 10 / wi.speed, 0.1f, 0, 0, qfalse);
          VectorCopy(move.endpos, bestorigin);
          //BotAI_Print(PRT_MESSAGE, "%1.1f predicted speed = %f, frames = %f\n", FloatTime(), VectorLength(dir), dist * 10 / wi.speed);
        }
        //if not that skilled do linear prediction
        else if (aim_skill > 0.4) {
          VectorSubtract(entinfo.origin, bs->origin, dir);
          //distance towards the enemy
          dist = VectorLength(dir);
          //direction the enemy is moving in
          VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
          dir[2] = 0;
          //
          speed = VectorNormalize(dir) / entinfo.update_time;
          //botimport.Print(PRT_MESSAGE, "speed = %f, wi->speed = %f\n", speed, wi->speed);
          //best spot to aim at
          VectorMA(entinfo.origin, (dist / wi.speed) * speed, dir, bestorigin);
        }
      }
    }
    //if the projectile does radial damage
    if (aim_skill > 0.6 && wi.proj.damagetype & DAMAGETYPE_RADIAL) {
      //if the enemy isn't standing significantly higher than the bot
      if (entinfo.origin[2] < bs->origin[2] + 16) {
        //try to aim at the ground in front of the enemy
        VectorCopy(entinfo.origin, end);
        end[2] -= 64;
        BotAI_Trace(&trace, entinfo.origin, NULL, NULL, end, entinfo.number, MASK_SHOT);
        //
        VectorCopy(bestorigin, groundtarget);
        if (trace.startsolid) groundtarget[2] = entinfo.origin[2] - 16;
        else groundtarget[2] = trace.endpos[2] - 8;
        //trace a line from projectile start to ground target
        BotAI_Trace(&trace, start, NULL, NULL, groundtarget, bs->entitynum, MASK_SHOT);
        //if hitpoint is not vertically too far from the ground target
        if (fabs(trace.endpos[2] - groundtarget[2]) < 50) {
          VectorSubtract(trace.endpos, groundtarget, dir);
          //if the hitpoint is near anough the ground target
          if (VectorLengthSquared(dir) < Square(60)) {
            VectorSubtract(trace.endpos, start, dir);
            //if the hitpoint is far anough from the bot
            if (VectorLengthSquared(dir) > Square(100)) {
              //check if the bot is visible from the ground target
              trace.endpos[2] += 1;
              BotAI_Trace(&trace, trace.endpos, NULL, NULL, entinfo.origin, entinfo.number, MASK_SHOT);
              if (trace.fraction >= 1) {
                //botimport.Print(PRT_MESSAGE, "%1.1f aiming at ground\n", AAS_Time());
                VectorCopy(groundtarget, bestorigin);
              }
            }
          }
        }
      }
    }
    bestorigin[0] += 20 * crandom() * (1 - aim_accuracy);
    bestorigin[1] += 20 * crandom() * (1 - aim_accuracy);
    bestorigin[2] += 10 * crandom() * (1 - aim_accuracy);
  }
  else {
    //
    VectorCopy(bs->lastenemyorigin, bestorigin);
    bestorigin[2] += 8;
    //if the bot is skilled anough
    if (aim_skill > 0.5) {
      //do prediction shots around corners
      if (wi.number == WP_LUCIFER_CANNON) {
        //create the chase goal
        goal.entitynum = bs->client;
        goal.areanum = bs->areanum;
        VectorCopy(bs->eye, goal.origin);
        VectorSet(goal.mins, -8, -8, -8);
        VectorSet(goal.maxs, 8, 8, 8);
        //
        if (trap_BotPredictVisiblePosition(bs->lastenemyorigin, bs->lastenemyareanum, &goal, TFL_DEFAULT, target)) {
          VectorSubtract(target, bs->eye, dir);
          if (VectorLengthSquared(dir) > Square(80)) {
            VectorCopy(target, bestorigin);
            bestorigin[2] -= 20;
          }
        }
        aim_accuracy = 1;
      }
    }
  }
  //
  if (enemyvisible) {
    BotAI_Trace(&trace, bs->eye, NULL, NULL, bestorigin, bs->entitynum, MASK_SHOT);
    VectorCopy(trace.endpos, bs->aimtarget);
  }
  else {
    VectorCopy(bestorigin, bs->aimtarget);
  }
  //get aim direction
  VectorSubtract(bestorigin, bs->eye, dir);
  //
  if (wi.number == WP_MACHINEGUN /*||
    wi.number == WP_SHOTGUN ||
    wi.number == WP_LIGHTNING ||
    wi.number == WP_RAILGUN*/) {
    //distance towards the enemy
    dist = VectorLength(dir);
    if (dist > 150) dist = 150;
    f = 0.6 + dist / 150 * 0.4;
    aim_accuracy *= f;
  }
  //add some random stuff to the aim direction depending on the aim accuracy
  if (aim_accuracy < 0.8) {
    VectorNormalize(dir);
    for (i = 0; i < 3; i++) dir[i] += 0.3 * crandom() * (1 - aim_accuracy);
  }
  //set the ideal view angles
  vectoangles(dir, bs->ideal_viewangles);
  //take the weapon spread into account for lower skilled bots
  bs->ideal_viewangles[PITCH] += 6 * wi.vspread * crandom() * (1 - aim_accuracy);
  bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
  bs->ideal_viewangles[YAW] += 6 * wi.hspread * crandom() * (1 - aim_accuracy);
  bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
  //if the bots should be really challenging
  if (bot_challenge.integer) {
    //if the bot is really accurate and has the enemy in view for some time
    if (aim_accuracy > 0.9 && bs->enemysight_time < FloatTime() - 1) {
      //set the view angles directly
      if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
      VectorCopy(bs->ideal_viewangles, bs->viewangles);
      trap_EA_View(bs->client, bs->viewangles);
    }
  }
}

/*
==================
BotCheckAttack
==================
*/
void BotCheckAttack(bot_state_t *bs) {
  float points, reactiontime, fov, firethrottle;
  int attackentity;
  bsp_trace_t bsptrace;
  //float selfpreservation;
  vec3_t forward, right, start, end, dir, angles;
  weaponinfo_t wi;
  bsp_trace_t trace;
  aas_entityinfo_t entinfo;
  vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

  attackentity = bs->enemy;
  //
  BotEntityInfo(attackentity, &entinfo);
  // if not attacking a player
  if (attackentity >= MAX_CLIENTS) {
#ifdef MISSIONPACK
    // if attacking an obelisk
    if ( entinfo.number == redobelisk.entitynum ||
      entinfo.number == blueobelisk.entitynum ) {
      // if obelisk is respawning return
      if ( g_entities[entinfo.number].activator &&
        g_entities[entinfo.number].activator->s.frame == 2 ) {
        return;
      }
    }
#endif
  }
  //
  reactiontime = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
  if (bs->enemysight_time > FloatTime() - reactiontime) return;
  if (bs->teleport_time > FloatTime() - reactiontime) return;
  //if changing weapons
  if (bs->weaponchange_time > FloatTime() - 0.1) return;
  //check fire throttle characteristic
  if (bs->firethrottlewait_time > FloatTime()) return;
  firethrottle = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_FIRETHROTTLE, 0, 1);
  if (bs->firethrottleshoot_time < FloatTime()) {
    if (random() > firethrottle) {
      bs->firethrottlewait_time = FloatTime() + firethrottle;
      bs->firethrottleshoot_time = 0;
    }
    else {
      bs->firethrottleshoot_time = FloatTime() + 1 - firethrottle;
      bs->firethrottlewait_time = 0;
    }
  }
  //
  //
  VectorSubtract(bs->aimtarget, bs->eye, dir);
  //
  if (bs->weaponnum == WP_PAIN_SAW) {
    if (VectorLengthSquared(dir) > Square(60)) {
      return;
    }
  }
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

  //get the weapon info
  trap_BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
  //get the start point shooting from
  VectorCopy(bs->origin, start);
  start[2] += bs->cur_ps.viewheight;
  AngleVectors(bs->viewangles, forward, right, NULL);
  start[0] += forward[0] * wi.offset[0] + right[0] * wi.offset[1];
  start[1] += forward[1] * wi.offset[0] + right[1] * wi.offset[1];
  start[2] += forward[2] * wi.offset[0] + right[2] * wi.offset[1] + wi.offset[2];
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
  //if won't hit the enemy or not attacking a player (obelisk)
  if (trace.ent != attackentity || attackentity >= MAX_CLIENTS) {
    //if the projectile does radial damage
    if (wi.proj.damagetype & DAMAGETYPE_RADIAL) {
      if (trace.fraction * 1000 < wi.proj.radius) {
        points = (wi.proj.damage - 0.5 * trace.fraction * 1000) * 0.5;
        if (points > 0) {
          return;
        }
      }
      //FIXME: check if a teammate gets radial damage
    }
  }
  //if fire has to be release to activate weapon
  if (wi.flags & WFL_FIRERELEASED) {
    if (bs->flags & BFL_ATTACKED) {
      trap_EA_Attack(bs->client);
    }
  }
  else {
    trap_EA_Attack(bs->client);
  }
  bs->flags ^= BFL_ATTACKED;
}

/*
==================
BotMapScripts
==================
*/
void BotMapScripts(bot_state_t *bs) {
  char info[1024];
  char mapname[128];
  int i, shootbutton;
  float aim_accuracy;
  aas_entityinfo_t entinfo;
  vec3_t dir;

  trap_GetServerinfo(info, sizeof(info));

  strncpy(mapname, Info_ValueForKey( info, "mapname" ), sizeof(mapname)-1);
  mapname[sizeof(mapname)-1] = '\0';

  if (!Q_stricmp(mapname, "q3tourney6")) {
    vec3_t mins = {700, 204, 672}, maxs = {964, 468, 680};
    vec3_t buttonorg = {304, 352, 920};
    //NOTE: NEVER use the func_bobbing in q3tourney6
    bs->tfl &= ~TFL_FUNCBOB;
    //if the bot is below the bounding box
    if (bs->origin[0] > mins[0] && bs->origin[0] < maxs[0]) {
      if (bs->origin[1] > mins[1] && bs->origin[1] < maxs[1]) {
        if (bs->origin[2] < mins[2]) {
          return;
        }
      }
    }
    shootbutton = qfalse;
    //if an enemy is below this bounding box then shoot the button
    for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {

      if (i == bs->client) continue;
      //
      BotEntityInfo(i, &entinfo);
      //
      if (!entinfo.valid) continue;
      //if the enemy isn't dead and the enemy isn't the bot self
      if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum) continue;
      //
      if (entinfo.origin[0] > mins[0] && entinfo.origin[0] < maxs[0]) {
        if (entinfo.origin[1] > mins[1] && entinfo.origin[1] < maxs[1]) {
          if (entinfo.origin[2] < mins[2]) {
            //if there's a team mate below the crusher
            if (BotSameTeam(bs, i)) {
              shootbutton = qfalse;
              break;
            }
            else {
              shootbutton = qtrue;
            }
          }
        }
      }
    }
    if (shootbutton) {
      bs->flags |= BFL_IDEALVIEWSET;
      VectorSubtract(buttonorg, bs->eye, dir);
      vectoangles(dir, bs->ideal_viewangles);
      aim_accuracy = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
      bs->ideal_viewangles[PITCH] += 8 * crandom() * (1 - aim_accuracy);
      bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
      bs->ideal_viewangles[YAW] += 8 * crandom() * (1 - aim_accuracy);
      bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
      //
      if (BotInFieldOfVision(bs->viewangles, 20, bs->ideal_viewangles)) {
        trap_EA_Attack(bs->client);
      }
    }
  }
  else if (!Q_stricmp(mapname, "mpq3tourney6")) {
    //NOTE: NEVER use the func_bobbing in mpq3tourney6
    bs->tfl &= ~TFL_FUNCBOB;
  }
}

/*
==================
BotSetMovedir
==================
*/
// bk001205 - made these static
static vec3_t VEC_UP    = {0, -1,  0};
static vec3_t MOVEDIR_UP  = {0,  0,  1};
static vec3_t VEC_DOWN    = {0, -2,  0};
static vec3_t MOVEDIR_DOWN  = {0,  0, -1};

void BotSetMovedir(vec3_t angles, vec3_t movedir) {
  if (VectorCompare(angles, VEC_UP)) {
    VectorCopy(MOVEDIR_UP, movedir);
  }
  else if (VectorCompare(angles, VEC_DOWN)) {
    VectorCopy(MOVEDIR_DOWN, movedir);
  }
  else {
    AngleVectors(angles, movedir, NULL, NULL);
  }
}

/*
==================
BotModelMinsMaxs

this is ugly
==================
*/
int BotModelMinsMaxs(int modelindex, int eType, int contents, vec3_t mins, vec3_t maxs) {
  gentity_t *ent;
  int i;

  ent = &g_entities[0];
  for (i = 0; i < level.num_entities; i++, ent++) {
    if ( !ent->inuse ) {
      continue;
    }
    if ( eType && ent->s.eType != eType) {
      continue;
    }
    if ( contents && ent->r.contents != contents) {
      continue;
    }
    if (ent->s.modelindex == modelindex) {
      if (mins)
        VectorAdd(ent->r.currentOrigin, ent->r.mins, mins);
      if (maxs)
        VectorAdd(ent->r.currentOrigin, ent->r.maxs, maxs);
      return i;
    }
  }
  if (mins)
    VectorClear(mins);
  if (maxs)
    VectorClear(maxs);
  return 0;
}

/*
==================
BotFuncButtonGoal
==================
*/
int BotFuncButtonActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
  int i, areas[10], numareas, modelindex, entitynum;
  char model[128];
  float lip, dist, health, angle;
  vec3_t size, start, end, mins, maxs, angles, points[10];
  vec3_t movedir, origin, goalorigin, bboxmins, bboxmaxs;
  vec3_t extramins = {1, 1, 1}, extramaxs = {-1, -1, -1};
  bsp_trace_t bsptrace;

  activategoal->shoot = qfalse;
  VectorClear(activategoal->target);
  //create a bot goal towards the button
  trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
  if (!*model)
    return qfalse;
  modelindex = atoi(model+1);
  if (!modelindex)
    return qfalse;
  VectorClear(angles);
  entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
  //get the lip of the button
  trap_AAS_FloatForBSPEpairKey(bspent, "lip", &lip);
  if (!lip) lip = 4;
  //get the move direction from the angle
  trap_AAS_FloatForBSPEpairKey(bspent, "angle", &angle);
  VectorSet(angles, 0, angle, 0);
  BotSetMovedir(angles, movedir);
  //button size
  VectorSubtract(maxs, mins, size);
  //button origin
  VectorAdd(mins, maxs, origin);
  VectorScale(origin, 0.5, origin);
  //touch distance of the button
  dist = fabs(movedir[0]) * size[0] + fabs(movedir[1]) * size[1] + fabs(movedir[2]) * size[2];
  dist *= 0.5;
  //
  trap_AAS_FloatForBSPEpairKey(bspent, "health", &health);
  //if the button is shootable
  if (health) {
    //calculate the shoot target
    VectorMA(origin, -dist, movedir, goalorigin);
    //
    VectorCopy(goalorigin, activategoal->target);
    activategoal->shoot = qtrue;
    //
    BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, goalorigin, bs->entitynum, MASK_SHOT);
    // if the button is visible from the current position
    if (bsptrace.fraction >= 1.0 || bsptrace.ent == entitynum) {
      //
      activategoal->goal.entitynum = entitynum; //NOTE: this is the entity number of the shootable button
      activategoal->goal.number = 0;
      activategoal->goal.flags = 0;
      VectorCopy(bs->origin, activategoal->goal.origin);
      activategoal->goal.areanum = bs->areanum;
      VectorSet(activategoal->goal.mins, -8, -8, -8);
      VectorSet(activategoal->goal.maxs, 8, 8, 8);
      //
      return qtrue;
    }
    else {
      //create a goal from where the button is visible and shoot at the button from there
      //add bounding box size to the dist
      trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
      for (i = 0; i < 3; i++) {
        if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
        else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
      }
      //calculate the goal origin
      VectorMA(origin, -dist, movedir, goalorigin);
      //
      VectorCopy(goalorigin, start);
      start[2] += 24;
      VectorCopy(start, end);
      end[2] -= 512;
      numareas = trap_AAS_TraceAreas(start, end, areas, points, 10);
      //
      for (i = numareas-1; i >= 0; i--) {
        if (trap_AAS_AreaReachability(areas[i])) {
          break;
        }
      }
      if (i < 0) {
        // FIXME: trace forward and maybe in other directions to find a valid area
      }
      if (i >= 0) {
        //
        VectorCopy(points[i], activategoal->goal.origin);
        activategoal->goal.areanum = areas[i];
        VectorSet(activategoal->goal.mins, 8, 8, 8);
        VectorSet(activategoal->goal.maxs, -8, -8, -8);
        //
        for (i = 0; i < 3; i++)
        {
          if (movedir[i] < 0) activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
          else activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
        } //end for
        //
        activategoal->goal.entitynum = entitynum;
        activategoal->goal.number = 0;
        activategoal->goal.flags = 0;
        return qtrue;
      }
    }
    return qfalse;
  }
  else {
    //add bounding box size to the dist
    trap_AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
    for (i = 0; i < 3; i++) {
      if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
      else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
    }
    //calculate the goal origin
    VectorMA(origin, -dist, movedir, goalorigin);
    //
    VectorCopy(goalorigin, start);
    start[2] += 24;
    VectorCopy(start, end);
    end[2] -= 100;
    numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
    //
    for (i = 0; i < numareas; i++) {
      if (trap_AAS_AreaReachability(areas[i])) {
        break;
      }
    }
    if (i < numareas) {
      //
      VectorCopy(origin, activategoal->goal.origin);
      activategoal->goal.areanum = areas[i];
      VectorSubtract(mins, origin, activategoal->goal.mins);
      VectorSubtract(maxs, origin, activategoal->goal.maxs);
      //
      for (i = 0; i < 3; i++)
      {
        if (movedir[i] < 0) activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
        else activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
      } //end for
      //
      activategoal->goal.entitynum = entitynum;
      activategoal->goal.number = 0;
      activategoal->goal.flags = 0;
      return qtrue;
    }
  }
  return qfalse;
}

/*
==================
BotFuncDoorGoal
==================
*/
int BotFuncDoorActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
  int modelindex, entitynum;
  char model[MAX_INFO_STRING];
  vec3_t mins, maxs, origin, angles;

  //shoot at the shootable door
  trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
  if (!*model)
    return qfalse;
  modelindex = atoi(model+1);
  if (!modelindex)
    return qfalse;
  VectorClear(angles);
  entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
  //door origin
  VectorAdd(mins, maxs, origin);
  VectorScale(origin, 0.5, origin);
  VectorCopy(origin, activategoal->target);
  activategoal->shoot = qtrue;
  //
  activategoal->goal.entitynum = entitynum; //NOTE: this is the entity number of the shootable door
  activategoal->goal.number = 0;
  activategoal->goal.flags = 0;
  VectorCopy(bs->origin, activategoal->goal.origin);
  activategoal->goal.areanum = bs->areanum;
  VectorSet(activategoal->goal.mins, -8, -8, -8);
  VectorSet(activategoal->goal.maxs, 8, 8, 8);
  return qtrue;
}

/*
==================
BotTriggerMultipleGoal
==================
*/
int BotTriggerMultipleActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
  int i, areas[10], numareas, modelindex, entitynum;
  char model[128];
  vec3_t start, end, mins, maxs, angles;
  vec3_t origin, goalorigin;

  activategoal->shoot = qfalse;
  VectorClear(activategoal->target);
  //create a bot goal towards the trigger
  trap_AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
  if (!*model)
    return qfalse;
  modelindex = atoi(model+1);
  if (!modelindex)
    return qfalse;
  VectorClear(angles);
  entitynum = BotModelMinsMaxs(modelindex, 0, CONTENTS_TRIGGER, mins, maxs);
  //trigger origin
  VectorAdd(mins, maxs, origin);
  VectorScale(origin, 0.5, origin);
  VectorCopy(origin, goalorigin);
  //
  VectorCopy(goalorigin, start);
  start[2] += 24;
  VectorCopy(start, end);
  end[2] -= 100;
  numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
  //
  for (i = 0; i < numareas; i++) {
    if (trap_AAS_AreaReachability(areas[i])) {
      break;
    }
  }
  if (i < numareas) {
    VectorCopy(origin, activategoal->goal.origin);
    activategoal->goal.areanum = areas[i];
    VectorSubtract(mins, origin, activategoal->goal.mins);
    VectorSubtract(maxs, origin, activategoal->goal.maxs);
    //
    activategoal->goal.entitynum = entitynum;
    activategoal->goal.number = 0;
    activategoal->goal.flags = 0;
    return qtrue;
  }
  return qfalse;
}

/*
==================
BotPopFromActivateGoalStack
==================
*/
int BotPopFromActivateGoalStack(bot_state_t *bs) {
  if (!bs->activatestack)
    return qfalse;
  BotEnableActivateGoalAreas(bs->activatestack, qtrue);
  bs->activatestack->inuse = qfalse;
  bs->activatestack->justused_time = FloatTime();
  bs->activatestack = bs->activatestack->next;
  return qtrue;
}

/*
==================
BotPushOntoActivateGoalStack
==================
*/
int BotPushOntoActivateGoalStack(bot_state_t *bs, bot_activategoal_t *activategoal) {
  int i, best;
  float besttime;

  best = -1;
  besttime = FloatTime() + 9999;
  //
  for (i = 0; i < MAX_ACTIVATESTACK; i++) {
    if (!bs->activategoalheap[i].inuse) {
      if (bs->activategoalheap[i].justused_time < besttime) {
        besttime = bs->activategoalheap[i].justused_time;
        best = i;
      }
    }
  }
  if (best != -1) {
    memcpy(&bs->activategoalheap[best], activategoal, sizeof(bot_activategoal_t));
    bs->activategoalheap[best].inuse = qtrue;
    bs->activategoalheap[best].next = bs->activatestack;
    bs->activatestack = &bs->activategoalheap[best];
    return qtrue;
  }
  return qfalse;
}

/*
==================
BotClearActivateGoalStack
==================
*/
void BotClearActivateGoalStack(bot_state_t *bs) {
  while(bs->activatestack)
    BotPopFromActivateGoalStack(bs);
}

/*
==================
BotEnableActivateGoalAreas
==================
*/
void BotEnableActivateGoalAreas(bot_activategoal_t *activategoal, int enable) {
  int i;

  if (activategoal->areasdisabled == !enable)
    return;
  for (i = 0; i < activategoal->numareas; i++)
    trap_AAS_EnableRoutingArea( activategoal->areas[i], enable );
  activategoal->areasdisabled = !enable;
}

/*
==================
BotIsGoingToActivateEntity
==================
*/
int BotIsGoingToActivateEntity(bot_state_t *bs, int entitynum) {
  bot_activategoal_t *a;
  int i;

  for (a = bs->activatestack; a; a = a->next) {
    if (a->time < FloatTime())
      continue;
    if (a->goal.entitynum == entitynum)
      return qtrue;
  }
  for (i = 0; i < MAX_ACTIVATESTACK; i++) {
    if (bs->activategoalheap[i].inuse)
      continue;
    //
    if (bs->activategoalheap[i].goal.entitynum == entitynum) {
      // if the bot went for this goal less than 2 seconds ago
      if (bs->activategoalheap[i].justused_time > FloatTime() - 2)
        return qtrue;
    }
  }
  return qfalse;
}

/*
==================
BotGetActivateGoal

  returns the number of the bsp entity to activate
  goal->entitynum will be set to the game entity to activate
==================
*/
//#define OBSTACLEDEBUG

int BotGetActivateGoal(bot_state_t *bs, int entitynum, bot_activategoal_t *activategoal) {
  int i, ent, cur_entities[10], spawnflags, modelindex, areas[MAX_ACTIVATEAREAS*2], numareas, t;
  char model[MAX_INFO_STRING], tmpmodel[128];
  char target[128], classname[128];
  float health;
  char targetname[10][128];
  aas_entityinfo_t entinfo;
  aas_areainfo_t areainfo;
  vec3_t origin, angles, absmins, absmaxs;

  memset(activategoal, 0, sizeof(bot_activategoal_t));
  BotEntityInfo(entitynum, &entinfo);
  Com_sprintf(model, sizeof( model ), "*%d", entinfo.modelindex);
  for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
    if (!trap_AAS_ValueForBSPEpairKey(ent, "model", tmpmodel, sizeof(tmpmodel))) continue;
    if (!strcmp(model, tmpmodel)) break;
  }
  if (!ent) {
    BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity found with model %s\n", model);
    return 0;
  }
  trap_AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname));
  if (!classname) {
    BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model %s has no classname\n", model);
    return 0;
  }
  //if it is a door
  if (!strcmp(classname, "func_door")) {
    if (trap_AAS_FloatForBSPEpairKey(ent, "health", &health)) {
      //if the door has health then the door must be shot to open
      if (health) {
        BotFuncDoorActivateGoal(bs, ent, activategoal);
        return ent;
      }
    }
    //
    trap_AAS_IntForBSPEpairKey(ent, "spawnflags", &spawnflags);
    // if the door starts open then just wait for the door to return
    if ( spawnflags & 1 )
      return 0;
    //get the door origin
    if (!trap_AAS_VectorForBSPEpairKey(ent, "origin", origin)) {
      VectorClear(origin);
    }
    //if the door is open or opening already
    if (!VectorCompare(origin, entinfo.origin))
      return 0;
    // store all the areas the door is in
    trap_AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model));
    if (*model) {
      modelindex = atoi(model+1);
      if (modelindex) {
        VectorClear(angles);
        BotModelMinsMaxs(modelindex, ET_MOVER, 0, absmins, absmaxs);
        //
        numareas = trap_AAS_BBoxAreas(absmins, absmaxs, areas, MAX_ACTIVATEAREAS*2);
        // store the areas with reachabilities first
        for (i = 0; i < numareas; i++) {
          if (activategoal->numareas >= MAX_ACTIVATEAREAS)
            break;
          if ( !trap_AAS_AreaReachability(areas[i]) ) {
            continue;
          }
          trap_AAS_AreaInfo(areas[i], &areainfo);
          if (areainfo.contents & AREACONTENTS_MOVER) {
            activategoal->areas[activategoal->numareas++] = areas[i];
          }
        }
        // store any remaining areas
        for (i = 0; i < numareas; i++) {
          if (activategoal->numareas >= MAX_ACTIVATEAREAS)
            break;
          if ( trap_AAS_AreaReachability(areas[i]) ) {
            continue;
          }
          trap_AAS_AreaInfo(areas[i], &areainfo);
          if (areainfo.contents & AREACONTENTS_MOVER) {
            activategoal->areas[activategoal->numareas++] = areas[i];
          }
        }
      }
    }
  }
  // if the bot is blocked by or standing on top of a button
  if (!strcmp(classname, "func_button")) {
    return 0;
  }
  // get the targetname so we can find an entity with a matching target
  if (!trap_AAS_ValueForBSPEpairKey(ent, "targetname", targetname[0], sizeof(targetname[0]))) {
    if (bot_developer.integer) {
      BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model \"%s\" has no targetname\n", model);
    }
    return 0;
  }
  // allow tree-like activation
  cur_entities[0] = trap_AAS_NextBSPEntity(0);
  for (i = 0; i >= 0 && i < 10;) {
    for (ent = cur_entities[i]; ent; ent = trap_AAS_NextBSPEntity(ent)) {
      if (!trap_AAS_ValueForBSPEpairKey(ent, "target", target, sizeof(target))) continue;
      if (!strcmp(targetname[i], target)) {
        cur_entities[i] = trap_AAS_NextBSPEntity(ent);
        break;
      }
    }
    if (!ent) {
      if (bot_developer.integer) {
        BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity with target \"%s\"\n", targetname[i]);
      }
      i--;
      continue;
    }
    if (!trap_AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname))) {
      if (bot_developer.integer) {
        BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with target \"%s\" has no classname\n", targetname[i]);
      }
      continue;
    }
    // BSP button model
    if (!strcmp(classname, "func_button")) {
      //
      if (!BotFuncButtonActivateGoal(bs, ent, activategoal))
        continue;
      // if the bot tries to activate this button already
      if ( bs->activatestack && bs->activatestack->inuse &&
         bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
         bs->activatestack->time > FloatTime() &&
         bs->activatestack->start_time < FloatTime() - 2)
        continue;
      // if the bot is in a reachability area
      if ( trap_AAS_AreaReachability(bs->areanum) ) {
        // disable all areas the blocking entity is in
        BotEnableActivateGoalAreas( activategoal, qfalse );
        //
        t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
        // if the button is not reachable
        if (!t) {
          continue;
        }
        activategoal->time = FloatTime() + t * 0.01 + 5;
      }
      return ent;
    }
    // invisible trigger multiple box
    else if (!strcmp(classname, "trigger_multiple")) {
      //
      if (!BotTriggerMultipleActivateGoal(bs, ent, activategoal))
        continue;
      // if the bot tries to activate this trigger already
      if ( bs->activatestack && bs->activatestack->inuse &&
         bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
         bs->activatestack->time > FloatTime() &&
         bs->activatestack->start_time < FloatTime() - 2)
        continue;
      // if the bot is in a reachability area
      if ( trap_AAS_AreaReachability(bs->areanum) ) {
        // disable all areas the blocking entity is in
        BotEnableActivateGoalAreas( activategoal, qfalse );
        //
        t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
        // if the trigger is not reachable
        if (!t) {
          continue;
        }
        activategoal->time = FloatTime() + t * 0.01 + 5;
      }
      return ent;
    }
    else if (!strcmp(classname, "func_timer")) {
      // just skip the func_timer
      continue;
    }
    // the actual button or trigger might be linked through a target_relay or target_delay
    else if (!strcmp(classname, "target_relay") || !strcmp(classname, "target_delay")) {
      if (trap_AAS_ValueForBSPEpairKey(ent, "targetname", targetname[i+1], sizeof(targetname[0]))) {
        i++;
        cur_entities[i] = trap_AAS_NextBSPEntity(0);
      }
    }
  }
#ifdef OBSTACLEDEBUG
  BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no valid activator for entity with target \"%s\"\n", targetname[0]);
#endif
  return 0;
}

/*
==================
BotGoForActivateGoal
==================
*/
int BotGoForActivateGoal(bot_state_t *bs, bot_activategoal_t *activategoal) {
  aas_entityinfo_t activateinfo;

  activategoal->inuse = qtrue;
  if (!activategoal->time)
    activategoal->time = FloatTime() + 10;
  activategoal->start_time = FloatTime();
  BotEntityInfo(activategoal->goal.entitynum, &activateinfo);
  VectorCopy(activateinfo.origin, activategoal->origin);
  //
  if (BotPushOntoActivateGoalStack(bs, activategoal)) {
    // enter the activate entity AI node
    AIEnter_Seek_ActivateEntity(bs, "BotGoForActivateGoal");
    return qtrue;
  }
  else {
    // enable any routing areas that were disabled
    BotEnableActivateGoalAreas(activategoal, qtrue);
    return qfalse;
  }
}

/*
==================
BotPrintActivateGoalInfo
==================
*/
void BotPrintActivateGoalInfo(bot_state_t *bs, bot_activategoal_t *activategoal, int bspent) {
  char netname[MAX_NETNAME];
  char classname[128];
  char buf[128];

  ClientName(bs->client, netname, sizeof(netname));
  trap_AAS_ValueForBSPEpairKey(bspent, "classname", classname, sizeof(classname));
  if (activategoal->shoot) {
    Com_sprintf(buf, sizeof(buf), "%s: I have to shoot at a %s from %1.1f %1.1f %1.1f in area %d\n",
            netname, classname,
            activategoal->goal.origin[0],
            activategoal->goal.origin[1],
            activategoal->goal.origin[2],
            activategoal->goal.areanum);
  }
  else {
    Com_sprintf(buf, sizeof(buf), "%s: I have to activate a %s at %1.1f %1.1f %1.1f in area %d\n",
            netname, classname,
            activategoal->goal.origin[0],
            activategoal->goal.origin[1],
            activategoal->goal.origin[2],
            activategoal->goal.areanum);
  }
  trap_EA_Say(bs->client, buf);
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
void HBotAINetBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate) {
  int movetype, bspent;
  vec3_t hordir, start, end, mins, maxs, sideward, angles, up = {0, 0, 1};
  aas_entityinfo_t entinfo;
  bot_activategoal_t activategoal;

  // if the bot is not blocked by anything
  if (!moveresult->blocked) {
    bs->notblocked_time = FloatTime();
    return;
  }
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
  // if blocked by a bsp model and the bot wants to activate it
  if (activate && entinfo.modelindex > 0 && entinfo.modelindex <= max_bspmodelindex) {
    // find the bsp entity which should be activated in order to get the blocking entity out of the way
    bspent = BotGetActivateGoal(bs, entinfo.number, &activategoal);
    if (bspent) {
      //
      if (bs->activatestack && !bs->activatestack->inuse)
        bs->activatestack = NULL;
      // if not already trying to activate this entity
      if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
        //
        BotGoForActivateGoal(bs, &activategoal);
      }
      // if ontop of an obstacle or
      // if the bot is not in a reachability area it'll still
      // need some dynamic obstacle avoidance, otherwise return
      if (!(moveresult->flags & MOVERESULT_ONTOPOFOBSTACLE) &&
        trap_AAS_AreaReachability(bs->areanum))
        return;
    }
    else {
      // enable any routing areas that were disabled
      BotEnableActivateGoalAreas(&activategoal, qtrue);
    }
  }
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
  //
  if (bs->notblocked_time < FloatTime() - 0.4) {
    // just reset goals and hope the bot will go into another direction?
    // is this still needed??
    if (bs->ainode == AINode_Seek_NBG) bs->nbg_time = 0;
    else if (bs->ainode == AINode_Seek_LTG) bs->ltg_time = 0;
  }
}

/*
==================
BotAIPredictObstacles

Predict the route towards the goal and check if the bot
will be blocked by certain obstacles. When the bot has obstacles
on it's path the bot should figure out if they can be removed
by activating certain entities.
==================
*/
int BotAIPredictObstacles(bot_state_t *bs, bot_goal_t *goal) {
  int modelnum, entitynum, bspent;
  bot_activategoal_t activategoal;
  aas_predictroute_t route;

  if (!bot_predictobstacles.integer)
    return qfalse;

  // always predict when the goal change or at regular intervals
  if (bs->predictobstacles_goalareanum == goal->areanum &&
    bs->predictobstacles_time > FloatTime() - 6) {
    return qfalse;
  }
  bs->predictobstacles_goalareanum = goal->areanum;
  bs->predictobstacles_time = FloatTime();

  // predict at most 100 areas or 10 seconds ahead
  trap_AAS_PredictRoute(&route, bs->areanum, bs->origin,
              goal->areanum, bs->tfl, 100, 1000,
              RSE_USETRAVELTYPE|RSE_ENTERCONTENTS,
              AREACONTENTS_MOVER, TFL_BRIDGE, 0);
  // if bot has to travel through an area with a mover
  if (route.stopevent & RSE_ENTERCONTENTS) {
    // if the bot will run into a mover
    if (route.endcontents & AREACONTENTS_MOVER) {
      //NOTE: this only works with bspc 2.1 or higher
      modelnum = (route.endcontents & AREACONTENTS_MODELNUM) >> AREACONTENTS_MODELNUMSHIFT;
      if (modelnum) {
        //
        entitynum = BotModelMinsMaxs(modelnum, ET_MOVER, 0, NULL, NULL);
        if (entitynum) {
          //NOTE: BotGetActivateGoal already checks if the door is open or not
          bspent = BotGetActivateGoal(bs, entitynum, &activategoal);
          if (bspent) {
            //
            if (bs->activatestack && !bs->activatestack->inuse)
              bs->activatestack = NULL;
            // if not already trying to activate this entity
            if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
              //
              //BotAI_Print(PRT_MESSAGE, "blocked by mover model %d, entity %d ?\n", modelnum, entitynum);
              //
              BotGoForActivateGoal(bs, &activategoal);
              return qtrue;
            }
            else {
              // enable any routing areas that were disabled
              BotEnableActivateGoalAreas(&activategoal, qtrue);
            }
          }
        }
      }
    }
  }
  else if (route.stopevent & RSE_USETRAVELTYPE) {
    if (route.endtravelflags & TFL_BRIDGE) {
      //FIXME: check if the bridge is available to travel over
    }
  }
  return qfalse;
}

/*
==================
BotCheckConsoleMessages
==================
*/
void BotCheckConsoleMessages(bot_state_t *bs) {
  char botname[MAX_NETNAME], message[MAX_MESSAGE_SIZE], netname[MAX_NETNAME], *ptr;
  float chat_reply;
  int context, handle;
  bot_consolemessage_t m;
  bot_match_t match;

  //the name of this bot
  ClientName(bs->client, botname, sizeof(botname));
  //
  while((handle = trap_BotNextConsoleMessage(bs->cs, &m)) != 0) {
    //if the chat state is flooded with messages the bot will read them quickly
    if (trap_BotNumConsoleMessages(bs->cs) < 10) {
      //if it is a chat message the bot needs some time to read it
      if (m.type == CMS_CHAT && m.time > FloatTime() - (1 + random())) break;
    }
    //
    ptr = m.message;
    //if it is a chat message then don't unify white spaces and don't
    //replace synonyms in the netname
    if (m.type == CMS_CHAT) {
      //
      if (trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
        ptr = m.message + match.variables[MESSAGE].offset;
      }
    }
    //unify the white spaces in the message
    trap_UnifyWhiteSpaces(ptr);
    //replace synonyms in the right context
    context = BotSynonymContext(bs);
    trap_BotReplaceSynonyms(ptr, context);
    //if there's no match
    if (!BotMatchMessage(bs, m.message)) {
      //if it is a chat message
      if (m.type == CMS_CHAT && !bot_nochat.integer) {
        //
        if (!trap_BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
          trap_BotRemoveConsoleMessage(bs->cs, handle);
          continue;
        }
        //don't use eliza chats with team messages
        if (match.subtype & ST_TEAM) {
          trap_BotRemoveConsoleMessage(bs->cs, handle);
          continue;
        }
        //
        trap_BotMatchVariable(&match, NETNAME, netname, sizeof(netname));
        trap_BotMatchVariable(&match, MESSAGE, message, sizeof(message));
        //if this is a message from the bot self
        if (bs->client == ClientFromName(netname)) {
          trap_BotRemoveConsoleMessage(bs->cs, handle);
          continue;
        }
        //unify the message
        trap_UnifyWhiteSpaces(message);
        //
        trap_Cvar_Update(&bot_testrchat);
        if (bot_testrchat.integer) {
          //
          trap_BotLibVarSet("bot_testrchat", "1");
          //if bot replies with a chat message
          if (trap_BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
                              NULL, NULL,
                              NULL, NULL,
                              NULL, NULL,
                              botname, netname)) {
            BotAI_Print(PRT_MESSAGE, "------------------------\n");
          }
          else {
            BotAI_Print(PRT_MESSAGE, "**** no valid reply ****\n");
          }
        }
        //if at a valid chat position and not chatting already and not in teamplay
        else if (bs->ainode != AINode_Stand && BotValidChatPosition(bs) && !TeamPlayIsOn()) {
          chat_reply = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_REPLY, 0, 1);
          if (random() < 1.5 / (NumBots()+1) && random() < chat_reply) {
            //if bot replies with a chat message
            if (trap_BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
                                NULL, NULL,
                                NULL, NULL,
                                NULL, NULL,
                                botname, netname)) {
              //remove the console message
              trap_BotRemoveConsoleMessage(bs->cs, handle);
              bs->stand_time = FloatTime() + BotChatTime(bs);
              AIEnter_Stand(bs, "BotCheckConsoleMessages: reply chat");
              //EA_Say(bs->client, bs->cs.chatmessage);
              break;
            }
          }
        }
      }
    }
    //remove the console message
    trap_BotRemoveConsoleMessage(bs->cs, handle);
  }
}

/*
==================
HBotNetCheckEvents
==================
*/
void BotCheckForGrenades(bot_state_t *bs, entityState_t *state) {
  // if this is not a grenade
  if (state->eType != ET_MISSILE || state->weapon != WP_GRENADE)
    return;
  // try to avoid the grenade
  trap_BotAddAvoidSpot(bs->ms, state->pos.trBase, 160, AVOID_ALWAYS);
}

/*
==================
HBotNetCheckEvents
==================
*/
void HBotNetCheckEvents(bot_state_t *bs, entityState_t *state) {
  int event;
  char buf[128];
#ifdef MISSIONPACK
  aas_entityinfo_t entinfo;
#endif

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
      mod = state->eventParm;
      //
      if (target == bs->client) {
        bs->botdeathtype = mod;
        bs->lastkilledby = attacker;
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
      if (state->eventParm < 0 || state->eventParm > MAX_SOUNDS) {
        BotAI_Print(PRT_ERROR, "EV_GLOBAL_SOUND: eventParm (%d) out of range\n", state->eventParm);
        break;
      }
      trap_GetConfigstring(CS_SOUNDS + state->eventParm, buf, sizeof(buf));
      /*
      if (!strcmp(buf, "sound/teamplay/flagret_red.wav")) {
        //red flag is returned
        bs->redflagstatus = 0;
        bs->flagstatuschanged = qtrue;
      }
      else if (!strcmp(buf, "sound/teamplay/flagret_blu.wav")) {
        //blue flag is returned
        bs->blueflagstatus = 0;
        bs->flagstatuschanged = qtrue;
      }
      else*/
      if (!strcmp(buf, "sound/items/poweruprespawn.wav")) {
      //powerup respawned... go get it
      BotGoForPowerups(bs);
      }
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
      //if this sound is played on the bot
      if (state->number == bs->client) {
        if (state->eventParm < 0 || state->eventParm > MAX_SOUNDS) {
          BotAI_Print(PRT_ERROR, "EV_GENERAL_SOUND: eventParm (%d) out of range\n", state->eventParm);
          break;
        }
        //check out the sound
        trap_GetConfigstring(CS_SOUNDS + state->eventParm, buf, sizeof(buf));
        //if falling into a death pit
        if (!strcmp(buf, "*falling1.wav")) {
          //if the bot has a personal teleporter
//          if (bs->inventory[BI_TELEPORTER] > 0) {
//            //use the holdable item
//            trap_EA_Use(bs->client);
//          }
        }
      }
      break;
    }
    case EV_FOOTSTEP:
    case EV_FOOTSTEP_METAL:
    case EV_FOOTSPLASH:
    case EV_FOOTWADE:
    case EV_SWIM:
    case EV_FALL_SHORT:
    case EV_FALL_MEDIUM:
    case EV_FALL_FAR:
    case EV_STEP_4:
    case EV_STEP_8:
    case EV_STEP_12:
    case EV_STEP_16:
//    case EV_JUMP_PAD:
    case EV_JUMP:
    case EV_TAUNT:
    case EV_WATER_TOUCH:
    case EV_WATER_LEAVE:
    case EV_WATER_UNDER:
    case EV_WATER_CLEAR:
    case EV_NOAMMO:
    case EV_CHANGE_WEAPON:
    case EV_FIRE_WEAPON:
      //FIXME: either add to sound queue or mark player as someone making noise
      break;
//    case EV_USE_ITEM0:
//    case EV_USE_ITEM1:
//    case EV_USE_ITEM2:
//    case EV_USE_ITEM3:
//    case EV_USE_ITEM4:
//    case EV_USE_ITEM5:
//    case EV_USE_ITEM6:
//    case EV_USE_ITEM7:
//    case EV_USE_ITEM8:
//    case EV_USE_ITEM9:
//    case EV_USE_ITEM10:
//    case EV_USE_ITEM11:
//    case EV_USE_ITEM12:
//    case EV_USE_ITEM13:
//    case EV_USE_ITEM14:
      break;
  }
}

/*
==================
HBotNetCheckSnapshot
==================
*/
void HBotNetCheckSnapshot(bot_state_t *bs) {
  int ent;
  entityState_t state;

  //remove all avoid spots
  trap_BotAddAvoidSpot(bs->ms, vec3_origin, 0, AVOID_CLEAR);
  //reset kamikaze body
  bs->kamikazebody = 0;
  //reset number of proxmines
  bs->numproxmines = 0;
  //
  ent = 0;
  while( ( ent = BotAI_GetSnapshotEntity( bs->client, ent, &state ) ) != -1 ) {
    //check the entity state for events
    HBotNetCheckEvents(bs, &state);
    //check for grenades the bot should avoid
    BotCheckForGrenades(bs, &state);
  }
  //check the player state for events
  BotAI_GetEntityState(bs->client, &state);
  //copy the player state events to the entity state
  state.event = bs->cur_ps.externalEvent;
  state.eventParm = bs->cur_ps.externalEventParm;
  //
  HBotNetCheckEvents(bs, &state);
}

/*
==================
BotCheckAir
==================
*/
void BotCheckAir(bot_state_t *bs) {
//  if (bs->inventory[BI_ENVIRONMENTSUIT] <= 0) {
//    if (trap_AAS_PointContents(bs->eye) & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA)) {
//      return;
//    }
//  }
  bs->lastair_time = FloatTime();
}

/*
==================
BotAlternateRoute
==================
*/
bot_goal_t *BotAlternateRoute(bot_state_t *bs, bot_goal_t *goal) {
  int t;

  // if the bot has an alternative route goal
  if (bs->altroutegoal.areanum) {
    //
    if (bs->reachedaltroutegoal_time)
      return goal;
    // travel time towards alternative route goal
    t = trap_AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, bs->altroutegoal.areanum, bs->tfl);
    if (t && t < 20) {
      //BotAI_Print(PRT_MESSAGE, "reached alternate route goal\n");
      bs->reachedaltroutegoal_time = FloatTime();
    }
    memcpy(goal, &bs->altroutegoal, sizeof(bot_goal_t));
    return &bs->altroutegoal;
  }
  return goal;
}

/*
==================
BotGetAlternateRouteGoal
==================
*/
int BotGetAlternateRouteGoal(bot_state_t *bs, int base) {
  aas_altroutegoal_t *altroutegoals;
  bot_goal_t *goal;
  int numaltroutegoals, rnd;

  if (base == TEAM_ALIENS) {
    altroutegoals = alien_altroutegoals;
    numaltroutegoals = alien_numaltroutegoals;
  }
  else {
    altroutegoals = human_altroutegoals;
    numaltroutegoals = human_numaltroutegoals;
  }
  if (!numaltroutegoals)
    return qfalse;
  rnd = (float) random() * numaltroutegoals;
  if (rnd >= numaltroutegoals)
    rnd = numaltroutegoals-1;
  goal = &bs->altroutegoal;
  goal->areanum = altroutegoals[rnd].areanum;
  VectorCopy(altroutegoals[rnd].origin, goal->origin);
  VectorSet(goal->mins, -8, -8, -8);
  VectorSet(goal->maxs, 8, 8, 8);
  goal->entitynum = 0;
  goal->iteminfo = 0;
  goal->number = 0;
  goal->flags = 0;
  //
  bs->reachedaltroutegoal_time = 0;
  return qtrue;
}

/*
==================
BotSetupAlternateRouteGoals
==================
*/
void BotSetupAlternativeRouteGoals(void) {

  if (altroutegoals_setup)
    return;
#ifdef MISSIONPACK
  if (gametype == GT_CTF) {
    if (trap_BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
      BotAI_Print(PRT_WARNING, "no alt routes without Neutral Flag\n");
    if (ctf_neutralflag.areanum) {
      //
      alien_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                    ctf_neutralflag.origin, ctf_neutralflag.areanum,
                    ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
                    alien_altroutegoals, MAX_ALTROUTEGOALS,
                    ALTROUTEGOAL_CLUSTERPORTALS|
                    ALTROUTEGOAL_VIEWPORTALS);
      human_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                    ctf_neutralflag.origin, ctf_neutralflag.areanum,
                    ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
                    human_altroutegoals, MAX_ALTROUTEGOALS,
                    ALTROUTEGOAL_CLUSTERPORTALS|
                    ALTROUTEGOAL_VIEWPORTALS);
    }
  }
  else if (gametype == GT_1FCTF) {
    //
    alien_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                  ctf_neutralflag.origin, ctf_neutralflag.areanum,
                  ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
                  alien_altroutegoals, MAX_ALTROUTEGOALS,
                  ALTROUTEGOAL_CLUSTERPORTALS|
                  ALTROUTEGOAL_VIEWPORTALS);
    human_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                  ctf_neutralflag.origin, ctf_neutralflag.areanum,
                  ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
                  human_altroutegoals, MAX_ALTROUTEGOALS,
                  ALTROUTEGOAL_CLUSTERPORTALS|
                  ALTROUTEGOAL_VIEWPORTALS);
  }
  else if (gametype == GT_OBELISK) {
    if (trap_BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
      BotAI_Print(PRT_WARNING, "Harvester without neutral obelisk\n");
    //
    alien_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                  neutralobelisk.origin, neutralobelisk.areanum,
                  redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
                  alien_altroutegoals, MAX_ALTROUTEGOALS,
                  ALTROUTEGOAL_CLUSTERPORTALS|
                  ALTROUTEGOAL_VIEWPORTALS);
    human_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                  neutralobelisk.origin, neutralobelisk.areanum,
                  blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
                  human_altroutegoals, MAX_ALTROUTEGOALS,
                  ALTROUTEGOAL_CLUSTERPORTALS|
                  ALTROUTEGOAL_VIEWPORTALS);
  }
  else if (gametype == GT_HARVESTER) {
    //
    alien_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                  neutralobelisk.origin, neutralobelisk.areanum,
                  redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
                  alien_altroutegoals, MAX_ALTROUTEGOALS,
                  ALTROUTEGOAL_CLUSTERPORTALS|
                  ALTROUTEGOAL_VIEWPORTALS);
    human_numaltroutegoals = trap_AAS_AlternativeRouteGoals(
                  neutralobelisk.origin, neutralobelisk.areanum,
                  blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
                  human_altroutegoals, MAX_ALTROUTEGOALS,
                  ALTROUTEGOAL_CLUSTERPORTALS|
                  ALTROUTEGOAL_VIEWPORTALS);
  }
#endif
  altroutegoals_setup = qtrue;
}

/*
==================
HBotNeuralNetAI
==================
*/
void HBotNeuralNetAI(bot_state_t *bs, float thinktime) {
  char gender[144], name[144], buf[144];
  char userinfo[MAX_INFO_STRING];
  int i;

  //if the bot has just been setup
  if (bs->setupcount > 0) {
    bs->setupcount--;
    if (bs->setupcount > 0) return;
    //get the gender characteristic
    trap_Characteristic_String(bs->character, CHARACTERISTIC_GENDER, gender, sizeof(gender));
    //set the bot gender
    trap_GetUserinfo(bs->client, userinfo, sizeof(userinfo));
    Info_SetValueForKey(userinfo, "sex", gender);
    trap_SetUserinfo(bs->client, userinfo);
    //set the team
    if ( !bs->map_restart ) {
      Com_sprintf(buf, sizeof(buf), "team %s", bs->settings.team);
      trap_EA_Command(bs->client, buf);
    }
    //set the chat gender
    if (gender[0] == 'm') trap_BotSetChatGender(bs->cs, CHAT_GENDERMALE);
    else if (gender[0] == 'f')  trap_BotSetChatGender(bs->cs, CHAT_GENDERFEMALE);
    else  trap_BotSetChatGender(bs->cs, CHAT_GENDERLESS);
    //set the chat name
    ClientName(bs->client, name, sizeof(name));
    trap_BotSetChatName(bs->cs, name, bs->client);
    //
    bs->lastframe_health = bs->inventory[BI_HEALTH];
    bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
    //
    bs->setupcount = 0;
    //
    BotSetupAlternativeRouteGoals();
  }
  //no ideal view set
  bs->flags &= ~BFL_IDEALVIEWSET;
  //
  if (!BotIntermission(bs)) {
    //set the teleport time
    BotSetTeleportTime(bs);
    //update some inventory values
    BotUpdateInventory(bs);
    //check out the snapshot
    HBotNetCheckSnapshot(bs);
    //check for air
    BotCheckAir(bs);
  }
  //check the console messages
  BotCheckConsoleMessages(bs);
  //if not in the intermission and not in observer mode
  if (!BotIntermission(bs) && !BotIsObserver(bs)) {
    //do team AI
    //TODO: Copy team ai stuff over
//    BotTeamAI(bs);
  }
  //if the bot has no ai node
  if (!bs->ainode) {
    AIEnter_Seek_LTG(bs, "BotDeathmatchAI: no ai node");
  }
  //if the bot entered the game less than 8 seconds ago
  if (!bs->entergamechat && bs->entergame_time > FloatTime() - 8) {
    if (BotChat_EnterGame(bs)) {
      bs->stand_time = FloatTime() + BotChatTime(bs);
      AIEnter_Stand(bs, "BotDeathmatchAI: chat enter game");
    }
    bs->entergamechat = qtrue;
  }
  //reset the node switches from the previous frame
  BotResetNodeSwitches();
  //execute AI nodes
  for (i = 0; i < MAX_NODESWITCHES; i++) {
    if (bs->ainode(bs)) break;
  }
  //if the bot removed itself :)
  if (!bs->inuse) return;
  //if the bot executed too many AI nodes
  if (i >= MAX_NODESWITCHES) {
    trap_BotDumpGoalStack(bs->gs);
    trap_BotDumpAvoidGoals(bs->gs);
    BotDumpNodeSwitches(bs);
    ClientName(bs->client, name, sizeof(name));
    BotAI_Print(PRT_ERROR, "%s at %1.1f switched more than %d AI nodes\n", name, FloatTime(), MAX_NODESWITCHES);
  }
  //
  bs->lastframe_health = bs->inventory[BI_HEALTH];
  bs->lasthitcount = bs->cur_ps.persistant[PERS_HITS];
}

/*
==================
BotSetEntityNumForGoal
==================
*/
void BotSetEntityNumForGoal(bot_goal_t *goal, char *classname) {
  gentity_t *ent;
  int i;
  vec3_t dir;

  ent = &g_entities[0];
  for (i = 0; i < level.num_entities; i++, ent++) {
    if ( !ent->inuse ) {
      continue;
    }
    if ( !Q_stricmp(ent->classname, classname) ) {
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
BotGoalForBSPEntity
==================
*/
int BotGoalForBSPEntity( char *classname, bot_goal_t *goal ) {
  char value[MAX_INFO_STRING];
  vec3_t origin, start, end;
  int ent, numareas, areas[10];

  memset(goal, 0, sizeof(bot_goal_t));
  for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
    if (!trap_AAS_ValueForBSPEpairKey(ent, "classname", value, sizeof(value)))
      continue;
    if (!strcmp(value, classname)) {
      if (!trap_AAS_VectorForBSPEpairKey(ent, "origin", origin))
        return qfalse;
      VectorCopy(origin, goal->origin);
      VectorCopy(origin, start);
      start[2] -= 32;
      VectorCopy(origin, end);
      end[2] += 32;
      numareas = trap_AAS_TraceAreas(start, end, areas, NULL, 10);
      if (!numareas)
        return qfalse;
      goal->areanum = areas[0];
      return qtrue;
    }
  }
  return qfalse;
}

/*
==================
BotSetupDeathmatchAI
==================
*/
void BotSetupHumanAI(void) {
  int ent, modelnum;
  char model[128];

//  gametype = trap_Cvar_VariableIntegerValue("g_gametype");
//  maxclients = trap_Cvar_VariableIntegerValue("sv_maxclients");

//  trap_Cvar_Register(&bot_rocketjump, "bot_rocketjump", "1", 0);
//  trap_Cvar_Register(&bot_grapple, "bot_grapple", "0", 0);
//  trap_Cvar_Register(&bot_fastchat, "bot_fastchat", "0", 0);
//  trap_Cvar_Register(&bot_nochat, "bot_nochat", "0", 0);
//  trap_Cvar_Register(&bot_testrchat, "bot_testrchat", "0", 0);
//  trap_Cvar_Register(&bot_challenge, "bot_challenge", "0", 0);
//  trap_Cvar_Register(&bot_predictobstacles, "bot_predictobstacles", "1", 0);
//  trap_Cvar_Register(&g_spSkill, "g_spSkill", "2", 0);
  //

  max_bspmodelindex = 0;
  for (ent = trap_AAS_NextBSPEntity(0); ent; ent = trap_AAS_NextBSPEntity(ent)) {
    if (!trap_AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model))) continue;
    if (model[0] == '*') {
      modelnum = atoi(model+1);
      if (modelnum > max_bspmodelindex)
        max_bspmodelindex = modelnum;
    }
  }
  //initialize the waypoint heap
  BotInitWaypoints();
}

/*
==================
BotShutdownDeathmatchAI
==================
*/
void BotShutdownHumanAI(void) {
  altroutegoals_setup = qfalse;
}

/*
==================
AIEnter_Intermission
==================
*/
void AIEnter_Intermission(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "intermission", "", s);
  //reset the bot state
  BotResetState(bs);
  //check for end level chat
  if (BotChat_EndLevel(bs)) {
    trap_BotEnterChat(bs->cs, 0, bs->chatto);
  }
  bs->ainode = AINode_Intermission;
}

/*
==================
AINode_Intermission
==================
*/
int AINode_Intermission(bot_state_t *bs) {
  //if the intermission ended
  if (!BotIntermission(bs)) {
    if (BotChat_StartLevel(bs)) {
      bs->stand_time = FloatTime() + BotChatTime(bs);
    }
    else {
      bs->stand_time = FloatTime() + 2;
    }
    AIEnter_Stand(bs, "intermission: chat");
  }
  return qtrue;
}

/*
==================
AIEnter_Observer
==================
*/
void AIEnter_Observer(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "observer", "", s);
  //reset the bot state
  BotResetState(bs);
  bs->ainode = AINode_Observer;
}

/*
==================
AINode_Observer
==================
*/
int AINode_Observer(bot_state_t *bs) {
  //if the bot left observer mode
  if (!BotIsObserver(bs)) {
    AIEnter_Stand(bs, "observer: left observer");
  }
  return qtrue;
}

/*
==================
AIEnter_Stand
==================
*/
void AIEnter_Stand(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "stand", "", s);
  bs->standfindenemy_time = FloatTime() + 1;
  bs->ainode = AINode_Stand;
}

/*
==================
AINode_Stand
==================
*/
int AINode_Stand(bot_state_t *bs) {

  //if the bot's health decreased
  if (bs->lastframe_health > bs->inventory[BI_HEALTH]) {
    if (BotChat_HitTalking(bs)) {
      bs->standfindenemy_time = FloatTime() + BotChatTime(bs) + 0.1;
      bs->stand_time = FloatTime() + BotChatTime(bs) + 0.1;
    }
  }
  if (bs->standfindenemy_time < FloatTime()) {
    if (HBotNetFindEnemy(bs, -1)) {
      AIEnter_Battle_Fight(bs, "stand: found enemy");
      return qfalse;
    }
    bs->standfindenemy_time = FloatTime() + 1;
  }
  // put up chat icon
  trap_EA_Talk(bs->client);
  // when done standing
  if (bs->stand_time < FloatTime()) {
    trap_BotEnterChat(bs->cs, 0, bs->chatto);
    AIEnter_Seek_LTG(bs, "stand: time out");
    return qfalse;
  }
  //
  return qtrue;
}

/*
==================
AIEnter_Respawn
==================
*/
void AIEnter_Respawn(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "respawn", "", s);
  //reset some states
  trap_BotResetMoveState(bs->ms);
  trap_BotResetGoalState(bs->gs);
  trap_BotResetAvoidGoals(bs->gs);
  trap_BotResetAvoidReach(bs->ms);
  //if the bot wants to chat
  if (BotChat_Death(bs)) {
    bs->respawn_time = FloatTime() + BotChatTime(bs);
    bs->respawnchat_time = FloatTime();
  }
  else {
    bs->respawn_time = FloatTime() + 1 + random();
    bs->respawnchat_time = 0;
  }
  //set respawn state
  bs->respawn_wait = qfalse;
  bs->ainode = AINode_Respawn;
}

/*
==================
AINode_Respawn
==================
*/
int AINode_Respawn(bot_state_t *bs) {
  // if waiting for the actual respawn
  if (bs->respawn_wait) {
    if (!BotIsDead(bs)) {
      AIEnter_Seek_LTG(bs, "respawn: respawned");
    }
    else {
      trap_EA_Respawn(bs->client);
    }
  }
  else if (bs->respawn_time < FloatTime()) {
    // wait until respawned
    bs->respawn_wait = qtrue;
    // elementary action respawn
    trap_EA_Respawn(bs->client);
    //
    if (bs->respawnchat_time) {
      trap_BotEnterChat(bs->cs, 0, bs->chatto);
      bs->enemy = -1;
    }
  }
  if (bs->respawnchat_time && bs->respawnchat_time < FloatTime() - 0.5) {
    trap_EA_Talk(bs->client);
  }
  //
  return qtrue;
}

/*
==================
BotSelectActivateWeapon
==================
*/
int BotSelectActivateWeapon(bot_state_t *bs) {
  //
//  TODO: Tremifiy
//  if (bs->inventory[WP_MACHINEGUN] > 0 && bs->inventory[INVENTORY_BULLETS] > 0)
//    return WEAPONINDEX_MACHINEGUN;
//  else if (bs->inventory[INVENTORY_SHOTGUN] > 0 && bs->inventory[INVENTORY_SHELLS] > 0)
//    return WEAPONINDEX_SHOTGUN;
//  else if (bs->inventory[INVENTORY_PLASMAGUN] > 0 && bs->inventory[INVENTORY_CELLS] > 0)
//    return WEAPONINDEX_PLASMAGUN;
//  else if (bs->inventory[INVENTORY_LIGHTNING] > 0 && bs->inventory[INVENTORY_LIGHTNINGAMMO] > 0)
//    return WEAPONINDEX_LIGHTNING;
//#ifdef MISSIONPACK
//  else if (bs->inventory[INVENTORY_CHAINGUN] > 0 && bs->inventory[INVENTORY_BELT] > 0)
//    return WEAPONINDEX_CHAINGUN;
//  else if (bs->inventory[INVENTORY_NAILGUN] > 0 && bs->inventory[INVENTORY_NAILS] > 0)
//    return WEAPONINDEX_NAILGUN;
//#endif
//  else if (bs->inventory[INVENTORY_RAILGUN] > 0 && bs->inventory[INVENTORY_SLUGS] > 0)
//    return WEAPONINDEX_RAILGUN;
//  else if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 && bs->inventory[INVENTORY_ROCKETS] > 0)
//    return WEAPONINDEX_ROCKET_LAUNCHER;
//  else if (bs->inventory[INVENTORY_BFG10K] > 0 && bs->inventory[INVENTORY_BFGAMMO] > 0)
//    return WEAPONINDEX_BFG;
//  else {
//    return -1;
//  }
}


/*
==================
AIEnter_Seek_ActivateEntity
==================
*/
void AIEnter_Seek_ActivateEntity(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "activate entity", "", s);
  bs->ainode = AINode_Seek_ActivateEntity;
}

/*
==================
AINode_Seek_Activate_Entity
==================
*/
int AINode_Seek_ActivateEntity(bot_state_t *bs) {
  bot_goal_t *goal;
  vec3_t target, dir, ideal_viewangles;
  bot_moveresult_t moveresult;
  int targetvisible;
  bsp_trace_t bsptrace;
  aas_entityinfo_t entinfo;

  if (BotIsObserver(bs)) {
    BotClearActivateGoalStack(bs);
    AIEnter_Observer(bs, "active entity: observer");
    return qfalse;
  }
  //if in the intermission
  if (BotIntermission(bs)) {
    BotClearActivateGoalStack(bs);
    AIEnter_Intermission(bs, "activate entity: intermission");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    BotClearActivateGoalStack(bs);
    AIEnter_Respawn(bs, "activate entity: bot dead");
    return qfalse;
  }
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  // if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  // map specific code
  BotMapScripts(bs);
  // no enemy
  bs->enemy = -1;
  // if the bot has no activate goal
  if (!bs->activatestack) {
    BotClearActivateGoalStack(bs);
    AIEnter_Seek_NBG(bs, "activate entity: no goal");
    return qfalse;
  }
  //
  goal = &bs->activatestack->goal;
  // initialize target being visible to false
  targetvisible = qfalse;
  // if the bot has to shoot at a target to activate something
  if (bs->activatestack->shoot) {
    //
    BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->activatestack->target, bs->entitynum, MASK_SHOT);
    // if the shootable entity is visible from the current position
    if (bsptrace.fraction >= 1.0 || bsptrace.ent == goal->entitynum) {
      targetvisible = qtrue;
      // if holding the right weapon
      if (bs->cur_ps.weapon == bs->activatestack->weapon) {
        VectorSubtract(bs->activatestack->target, bs->eye, dir);
        vectoangles(dir, ideal_viewangles);
        // if the bot is pretty close with it's aim
        if (BotInFieldOfVision(bs->viewangles, 20, ideal_viewangles)) {
          trap_EA_Attack(bs->client);
        }
      }
    }
  }
  // if the shoot target is visible
  if (targetvisible) {
    // get the entity info of the entity the bot is shooting at
    BotEntityInfo(goal->entitynum, &entinfo);
    // if the entity the bot shoots at moved
    if (!VectorCompare(bs->activatestack->origin, entinfo.origin)) {
#ifdef DEBUG
      BotAI_Print(PRT_MESSAGE, "hit shootable button or trigger\n");
#endif //DEBUG
      bs->activatestack->time = 0;
    }
    // if the activate goal has been activated or the bot takes too long
    if (bs->activatestack->time < FloatTime()) {
      BotPopFromActivateGoalStack(bs);
      // if there are more activate goals on the stack
      if (bs->activatestack) {
        bs->activatestack->time = FloatTime() + 10;
        return qfalse;
      }
      AIEnter_Seek_NBG(bs, "activate entity: time out");
      return qfalse;
    }
    memset(&moveresult, 0, sizeof(bot_moveresult_t));
  }
  else {
    // if the bot has no goal
    if (!goal) {
      bs->activatestack->time = 0;
    }
    // if the bot does not have a shoot goal
    else if (!bs->activatestack->shoot) {
      //if the bot touches the current goal
      if (trap_BotTouchingGoal(bs->origin, goal)) {
#ifdef DEBUG
        BotAI_Print(PRT_MESSAGE, "touched button or trigger\n");
#endif //DEBUG
        bs->activatestack->time = 0;
      }
    }
    // if the activate goal has been activated or the bot takes too long
    if (bs->activatestack->time < FloatTime()) {
      BotPopFromActivateGoalStack(bs);
      // if there are more activate goals on the stack
      if (bs->activatestack) {
        bs->activatestack->time = FloatTime() + 10;
        return qfalse;
      }
      AIEnter_Seek_NBG(bs, "activate entity: activated");
      return qfalse;
    }
    //predict obstacles
    if (BotAIPredictObstacles(bs, goal))
      return qfalse;
    //initialize the movement state
    BotSetupForMovement(bs);
    //move towards the goal
    trap_BotMoveToGoal(&moveresult, bs->ms, goal, bs->tfl);
    //if the movement failed
    if (moveresult.failure) {
      //reset the avoid reach, otherwise bot is stuck in current area
      trap_BotResetAvoidReach(bs->ms);
      //
      bs->activatestack->time = 0;
    }
    //check if the bot is blocked
    BotAIBlocked(bs, &moveresult, qtrue);
  }
  //
  BotClearPath(bs, &moveresult);
  // if the bot has to shoot to activate
  if (bs->activatestack->shoot) {
    // if the view angles aren't yet used for the movement
    if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEW)) {
      VectorSubtract(bs->activatestack->target, bs->eye, dir);
      vectoangles(dir, moveresult.ideal_viewangles);
      moveresult.flags |= MOVERESULT_MOVEMENTVIEW;
    }
    // if there's no weapon yet used for the movement
    if (!(moveresult.flags & MOVERESULT_MOVEMENTWEAPON)) {
      moveresult.flags |= MOVERESULT_MOVEMENTWEAPON;
      //
      bs->activatestack->weapon = BotSelectActivateWeapon(bs);
      if (bs->activatestack->weapon == -1) {
        //FIXME: find a decent weapon first
        bs->activatestack->weapon = 0;
      }
      moveresult.weapon = bs->activatestack->weapon;
    }
  }
  // if the ideal view angles are set for movement
  if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET|MOVERESULT_MOVEMENTVIEW|MOVERESULT_SWIMVIEW)) {
    VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
  }
  // if waiting for something
  else if (moveresult.flags & MOVERESULT_WAITING) {
    if (random() < bs->thinktime * 0.8) {
      BotRoamGoal(bs, target);
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
      bs->ideal_viewangles[2] *= 0.5;
    }
  }
  else if (!(bs->flags & BFL_IDEALVIEWSET)) {
    if (trap_BotMovementViewTarget(bs->ms, goal, bs->tfl, 300, target)) {
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
    }
    else {
      vectoangles(moveresult.movedir, bs->ideal_viewangles);
    }
    bs->ideal_viewangles[2] *= 0.5;
  }
  // if the weapon is used for the bot movement
  if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON)
    bs->weaponnum = moveresult.weapon;
  // if there is an enemy
  if (HBotNetFindEnemy(bs, -1)) {
    if (HBotWantsToRetreat(bs)) {
      //keep the current long term goal and retreat
      AIEnter_Battle_NBG(bs, "activate entity: found enemy");
    }
    else {
      trap_BotResetLastAvoidReach(bs->ms);
      //empty the goal stack
      trap_BotEmptyGoalStack(bs->gs);
      //go fight
      AIEnter_Battle_Fight(bs, "activate entity: found enemy");
    }
    BotClearActivateGoalStack(bs);
  }
  return qtrue;
}

/*
==================
AIEnter_Seek_NBG
==================
*/
void AIEnter_Seek_NBG(bot_state_t *bs, char *s) {
  bot_goal_t goal;
  char buf[144];

  if (trap_BotGetTopGoal(bs->gs, &goal)) {
    trap_BotGoalName(goal.number, buf, 144);
    BotRecordNodeSwitch(bs, "seek NBG", buf, s);
  }
  else {
    BotRecordNodeSwitch(bs, "seek NBG", "no goal", s);
  }
  bs->ainode = AINode_Seek_NBG;
}

/*
==================
AINode_Seek_NBG
==================
*/
int AINode_Seek_NBG(bot_state_t *bs) {
  bot_goal_t goal;
  vec3_t target, dir;
  bot_moveresult_t moveresult;

  if (BotIsObserver(bs)) {
    AIEnter_Observer(bs, "seek nbg: observer");
    return qfalse;
  }
  //if in the intermission
  if (BotIntermission(bs)) {
    AIEnter_Intermission(bs, "seek nbg: intermision");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    AIEnter_Respawn(bs, "seek nbg: bot dead");
    return qfalse;
  }
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  //if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  //
  if (BotCanAndWantsToRocketJump(bs)) {
    bs->tfl |= TFL_ROCKETJUMP;
  }
  //map specific code
  BotMapScripts(bs);
  //no enemy
  bs->enemy = -1;
  //if the bot has no goal
  if (!trap_BotGetTopGoal(bs->gs, &goal)) bs->nbg_time = 0;
  //if the bot touches the current goal
  else if (BotReachedGoal(bs, &goal)) {
    BotChooseWeapon(bs);
    bs->nbg_time = 0;
  }
  //
  if (bs->nbg_time < FloatTime()) {
    //pop the current goal from the stack
    trap_BotPopGoal(bs->gs);
    //check for new nearby items right away
    //NOTE: we canNOT reset the check_time to zero because it would create an endless loop of node switches
    bs->check_time = FloatTime() + 0.05;
    //go back to seek ltg
    AIEnter_Seek_LTG(bs, "seek nbg: time out");
    return qfalse;
  }
  //predict obstacles
  if (BotAIPredictObstacles(bs, &goal))
    return qfalse;
  //initialize the movement state
  BotSetupForMovement(bs);
  //move towards the goal
  trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
  //if the movement failed
  if (moveresult.failure) {
    //reset the avoid reach, otherwise bot is stuck in current area
    trap_BotResetAvoidReach(bs->ms);
    bs->nbg_time = 0;
  }
  //check if the bot is blocked
  BotAIBlocked(bs, &moveresult, qtrue);
  //
  BotClearPath(bs, &moveresult);
  //if the viewangles are used for the movement
  if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET|MOVERESULT_MOVEMENTVIEW|MOVERESULT_SWIMVIEW)) {
    VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
  }
  //if waiting for something
  else if (moveresult.flags & MOVERESULT_WAITING) {
    if (random() < bs->thinktime * 0.8) {
      BotRoamGoal(bs, target);
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
      bs->ideal_viewangles[2] *= 0.5;
    }
  }
  else if (!(bs->flags & BFL_IDEALVIEWSET)) {
    if (!trap_BotGetSecondGoal(bs->gs, &goal)) trap_BotGetTopGoal(bs->gs, &goal);
    if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
    }
    //FIXME: look at cluster portals?
    else vectoangles(moveresult.movedir, bs->ideal_viewangles);
    bs->ideal_viewangles[2] *= 0.5;
  }
  //if the weapon is used for the bot movement
  if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) bs->weaponnum = moveresult.weapon;
  //if there is an enemy
  if (HBotNetFindEnemy(bs, -1)) {
    if (HBotWantsToRetreat(bs)) {
      //keep the current long term goal and retreat
      AIEnter_Battle_NBG(bs, "seek nbg: found enemy");
    }
    else {
      trap_BotResetLastAvoidReach(bs->ms);
      //empty the goal stack
      trap_BotEmptyGoalStack(bs->gs);
      //go fight
      AIEnter_Battle_Fight(bs, "seek nbg: found enemy");
    }
  }
  return qtrue;
}

/*
==================
AIEnter_Seek_LTG
==================
*/
void AIEnter_Seek_LTG(bot_state_t *bs, char *s) {
  bot_goal_t goal;
  char buf[144];

  if (trap_BotGetTopGoal(bs->gs, &goal)) {
    trap_BotGoalName(goal.number, buf, 144);
    BotRecordNodeSwitch(bs, "seek LTG", buf, s);
  }
  else {
    BotRecordNodeSwitch(bs, "seek LTG", "no goal", s);
  }
  bs->ainode = AINode_Seek_LTG;
}

/*
==================
AINode_Seek_LTG
==================
*/
int AINode_Seek_LTG(bot_state_t *bs)
{
  bot_goal_t goal;
  vec3_t target, dir;
  bot_moveresult_t moveresult;
  int range;
  //char buf[128];
  //bot_goal_t tmpgoal;

  if (BotIsObserver(bs)) {
    AIEnter_Observer(bs, "seek ltg: observer");
    return qfalse;
  }
  //if in the intermission
  if (BotIntermission(bs)) {
    AIEnter_Intermission(bs, "seek ltg: intermission");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    AIEnter_Respawn(bs, "seek ltg: bot dead");
    return qfalse;
  }
  //
  if (BotChat_Random(bs)) {
    bs->stand_time = FloatTime() + BotChatTime(bs);
    AIEnter_Stand(bs, "seek ltg: random chat");
    return qfalse;
  }
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  //if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  //
  if (BotCanAndWantsToRocketJump(bs)) {
    bs->tfl |= TFL_ROCKETJUMP;
  }
  //map specific code
  BotMapScripts(bs);
  //no enemy
  bs->enemy = -1;
  //
  if (bs->killedenemy_time > FloatTime() - 2) {
    if (random() < bs->thinktime * 1) {
      trap_EA_Gesture(bs->client);
    }
  }
  //if there is an enemy
  if (HBotNetFindEnemy(bs, -1)) {
    if (HBotWantsToRetreat(bs)) {
      //keep the current long term goal and retreat
      AIEnter_Battle_Retreat(bs, "seek ltg: found enemy");
      return qfalse;
    }
    else {
      trap_BotResetLastAvoidReach(bs->ms);
      //empty the goal stack
      trap_BotEmptyGoalStack(bs->gs);
      //go fight
      AIEnter_Battle_Fight(bs, "seek ltg: found enemy");
      return qfalse;
    }
  }
  //
  BotTeamGoals(bs, qfalse);
  //get the current long term goal
  if (!BotLongTermGoal(bs, bs->tfl, qfalse, &goal)) {
    return qtrue;
  }
  //check for nearby goals periodicly
  if (bs->check_time < FloatTime()) {
    bs->check_time = FloatTime() + 0.5;
    //check if the bot wants to camp
    BotWantsToCamp(bs);
    //
    if (bs->ltgtype == LTG_DEFENDKEYAREA) range = 400;
    else range = 150;
    //
#ifdef CTF
    if (gametype == GT_CTF) {
      //if carrying a flag the bot shouldn't be distracted too much
      if (BotCTFCarryingFlag(bs))
        range = 50;
    }
#endif //CTF
#ifdef MISSIONPACK
    else if (gametype == GT_1FCTF) {
      if (Bot1FCTFCarryingFlag(bs))
        range = 50;
    }
    else if (gametype == GT_HARVESTER) {
      if (BotHarvesterCarryingCubes(bs))
        range = 80;
    }
#endif
    //
    if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
      trap_BotResetLastAvoidReach(bs->ms);
      //get the goal at the top of the stack
      //trap_BotGetTopGoal(bs->gs, &tmpgoal);
      //trap_BotGoalName(tmpgoal.number, buf, 144);
      //BotAI_Print(PRT_MESSAGE, "new nearby goal %s\n", buf);
      //time the bot gets to pick up the nearby goal item
      bs->nbg_time = FloatTime() + 4 + range * 0.01;
      AIEnter_Seek_NBG(bs, "ltg seek: nbg");
      return qfalse;
    }
  }
  //predict obstacles
  if (BotAIPredictObstacles(bs, &goal))
    return qfalse;
  //initialize the movement state
  BotSetupForMovement(bs);
  //move towards the goal
  trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
  //if the movement failed
  if (moveresult.failure) {
    //reset the avoid reach, otherwise bot is stuck in current area
    trap_BotResetAvoidReach(bs->ms);
    //BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
    bs->ltg_time = 0;
  }
  //
  BotAIBlocked(bs, &moveresult, qtrue);
  //
  BotClearPath(bs, &moveresult);
  //if the viewangles are used for the movement
  if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET|MOVERESULT_MOVEMENTVIEW|MOVERESULT_SWIMVIEW)) {
    VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
  }
  //if waiting for something
  else if (moveresult.flags & MOVERESULT_WAITING) {
    if (random() < bs->thinktime * 0.8) {
      BotRoamGoal(bs, target);
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
      bs->ideal_viewangles[2] *= 0.5;
    }
  }
  else if (!(bs->flags & BFL_IDEALVIEWSET)) {
    if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
    }
    //FIXME: look at cluster portals?
    else if (VectorLengthSquared(moveresult.movedir)) {
      vectoangles(moveresult.movedir, bs->ideal_viewangles);
    }
    else if (random() < bs->thinktime * 0.8) {
      BotRoamGoal(bs, target);
      VectorSubtract(target, bs->origin, dir);
      vectoangles(dir, bs->ideal_viewangles);
      bs->ideal_viewangles[2] *= 0.5;
    }
    bs->ideal_viewangles[2] *= 0.5;
  }
  //if the weapon is used for the bot movement
  if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) bs->weaponnum = moveresult.weapon;
  //
  return qtrue;
}

/*
==================
AIEnter_Battle_Fight
==================
*/
void AIEnter_Battle_Fight(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "battle fight", "", s);
  trap_BotResetLastAvoidReach(bs->ms);
  bs->ainode = AINode_Battle_Fight;
}

/*
==================
AIEnter_Battle_Fight
==================
*/
void AIEnter_Battle_SuicidalFight(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "battle fight", "", s);
  trap_BotResetLastAvoidReach(bs->ms);
  bs->ainode = AINode_Battle_Fight;
  bs->flags |= BFL_FIGHTSUICIDAL;
}

/*
==================
AINode_Battle_Fight
==================
*/
int AINode_Battle_Fight(bot_state_t *bs) {
  int areanum;
  vec3_t target;
  aas_entityinfo_t entinfo;
  bot_moveresult_t moveresult;

  if (BotIsObserver(bs)) {
    AIEnter_Observer(bs, "battle fight: observer");
    return qfalse;
  }

  //if in the intermission
  if (BotIntermission(bs)) {
    AIEnter_Intermission(bs, "battle fight: intermission");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    AIEnter_Respawn(bs, "battle fight: bot dead");
    return qfalse;
  }
  //if there is another better enemy
  if (HBotNetFindEnemy(bs, bs->enemy)) {
#ifdef DEBUG
    BotAI_Print(PRT_MESSAGE, "found new better enemy\n");
#endif
  }
  //if no enemy
  if (bs->enemy < 0) {
    AIEnter_Seek_LTG(bs, "battle fight: no enemy");
    return qfalse;
  }
  //
  BotEntityInfo(bs->enemy, &entinfo);
  //if the enemy is dead
  if (bs->enemydeath_time) {
    if (bs->enemydeath_time < FloatTime() - 1.0) {
      bs->enemydeath_time = 0;
      if (bs->enemysuicide) {
        BotChat_EnemySuicide(bs);
      }
      if (bs->lastkilledplayer == bs->enemy && BotChat_Kill(bs)) {
        bs->stand_time = FloatTime() + BotChatTime(bs);
        AIEnter_Stand(bs, "battle fight: enemy dead");
      }
      else {
        bs->ltg_time = 0;
        AIEnter_Seek_LTG(bs, "battle fight: enemy dead");
      }
      return qfalse;
    }
  }
  else {
    if (EntityIsDead(&entinfo)) {
      bs->enemydeath_time = FloatTime();
    }
  }
  //if the enemy is invisible and not shooting the bot looses track easily
  if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
    if (random() < 0.2) {
      AIEnter_Seek_LTG(bs, "battle fight: invisible");
      return qfalse;
    }
  }
  //
  VectorCopy(entinfo.origin, target);
  // if not a player enemy
  if (bs->enemy >= MAX_CLIENTS) {
#ifdef MISSIONPACK
    // if attacking an obelisk
    if ( bs->enemy == redobelisk.entitynum ||
      bs->enemy == blueobelisk.entitynum ) {
      target[2] += 16;
    }
#endif
  }
  //update the reachability area and origin if possible
  areanum = BotPointAreaNum(target);
  if (areanum && trap_AAS_AreaReachability(areanum)) {
    VectorCopy(target, bs->lastenemyorigin);
    bs->lastenemyareanum = areanum;
  }
  //update the attack inventory values
  BotUpdateBattleInventory(bs, bs->enemy);
  //if the bot's health decreased
  if (bs->lastframe_health > bs->inventory[BI_HEALTH]) {
    if (BotChat_HitNoDeath(bs)) {
      bs->stand_time = FloatTime() + BotChatTime(bs);
      AIEnter_Stand(bs, "battle fight: chat health decreased");
      return qfalse;
    }
  }
  //if the bot hit someone
  if (bs->cur_ps.persistant[PERS_HITS] > bs->lasthitcount) {
    if (BotChat_HitNoKill(bs)) {
      bs->stand_time = FloatTime() + BotChatTime(bs);
      AIEnter_Stand(bs, "battle fight: chat hit someone");
      return qfalse;
    }
  }
  //if the enemy is not visible
  if (!BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
    if (HBotWantsToChase(bs)) {
      AIEnter_Battle_Chase(bs, "battle fight: enemy out of sight");
      return qfalse;
    }
    else {
      AIEnter_Seek_LTG(bs, "battle fight: enemy out of sight");
      return qfalse;
    }
  }
  //use holdable items
  BotBattleUseItems(bs);
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  //if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  //
  if (BotCanAndWantsToRocketJump(bs)) {
    bs->tfl |= TFL_ROCKETJUMP;
  }
  //choose the best weapon to fight with
  BotChooseWeapon(bs);
  //do attack movements
  moveresult = BotAttackMove(bs, bs->tfl);
  //if the movement failed
  if (moveresult.failure) {
    //reset the avoid reach, otherwise bot is stuck in current area
    trap_BotResetAvoidReach(bs->ms);
    //BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
    bs->ltg_time = 0;
  }
  //
  BotAIBlocked(bs, &moveresult, qfalse);
  //aim at the enemy
  BotAimAtEnemy(bs);
  //attack the enemy if possible
  BotCheckAttack(bs);
  //if the bot wants to retreat
  if (!(bs->flags & BFL_FIGHTSUICIDAL)) {
    if (HBotWantsToRetreat(bs)) {
      AIEnter_Battle_Retreat(bs, "battle fight: wants to retreat");
      return qtrue;
    }
  }
  return qtrue;
}

/*
==================
AIEnter_Battle_Chase
==================
*/
void AIEnter_Battle_Chase(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "battle chase", "", s);
  bs->chase_time = FloatTime();
  bs->ainode = AINode_Battle_Chase;
}

/*
==================
AINode_Battle_Chase
==================
*/
int AINode_Battle_Chase(bot_state_t *bs)
{
  bot_goal_t goal;
  vec3_t target, dir;
  bot_moveresult_t moveresult;
  float range;

  if (BotIsObserver(bs)) {
    AIEnter_Observer(bs, "battle chase: observer");
    return qfalse;
  }
  //if in the intermission
  if (BotIntermission(bs)) {
    AIEnter_Intermission(bs, "battle chase: intermission");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    AIEnter_Respawn(bs, "battle chase: bot dead");
    return qfalse;
  }
  //if no enemy
  if (bs->enemy < 0) {
    AIEnter_Seek_LTG(bs, "battle chase: no enemy");
    return qfalse;
  }
  //if the enemy is visible
  if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
    AIEnter_Battle_Fight(bs, "battle chase");
    return qfalse;
  }
  //if there is another enemy
  if (HBotNetFindEnemy(bs, -1)) {
    AIEnter_Battle_Fight(bs, "battle chase: better enemy");
    return qfalse;
  }
  //there is no last enemy area
  if (!bs->lastenemyareanum) {
    AIEnter_Seek_LTG(bs, "battle chase: no enemy area");
    return qfalse;
  }
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  //if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  //
  if (BotCanAndWantsToRocketJump(bs)) {
    bs->tfl |= TFL_ROCKETJUMP;
  }
  //map specific code
  BotMapScripts(bs);
  //create the chase goal
  goal.entitynum = bs->enemy;
  goal.areanum = bs->lastenemyareanum;
  VectorCopy(bs->lastenemyorigin, goal.origin);
  VectorSet(goal.mins, -8, -8, -8);
  VectorSet(goal.maxs, 8, 8, 8);
  //if the last seen enemy spot is reached the enemy could not be found
  if (trap_BotTouchingGoal(bs->origin, &goal)) bs->chase_time = 0;
  //if there's no chase time left
  if (!bs->chase_time || bs->chase_time < FloatTime() - 10) {
    AIEnter_Seek_LTG(bs, "battle chase: time out");
    return qfalse;
  }
  //check for nearby goals periodicly
  if (bs->check_time < FloatTime()) {
    bs->check_time = FloatTime() + 1;
    range = 150;
    //
    if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
      //the bot gets 5 seconds to pick up the nearby goal item
      bs->nbg_time = FloatTime() + 0.1 * range + 1;
      trap_BotResetLastAvoidReach(bs->ms);
      AIEnter_Battle_NBG(bs, "battle chase: nbg");
      return qfalse;
    }
  }
  //
  BotUpdateBattleInventory(bs, bs->enemy);
  //initialize the movement state
  BotSetupForMovement(bs);
  //move towards the goal
  trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
  //if the movement failed
  if (moveresult.failure) {
    //reset the avoid reach, otherwise bot is stuck in current area
    trap_BotResetAvoidReach(bs->ms);
    //BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
    bs->ltg_time = 0;
  }
  //
  BotAIBlocked(bs, &moveresult, qfalse);
  //
  if (moveresult.flags & (MOVERESULT_MOVEMENTVIEWSET|MOVERESULT_MOVEMENTVIEW|MOVERESULT_SWIMVIEW)) {
    VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
  }
  else if (!(bs->flags & BFL_IDEALVIEWSET)) {
    if (bs->chase_time > FloatTime() - 2) {
      BotAimAtEnemy(bs);
    }
    else {
      if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
        VectorSubtract(target, bs->origin, dir);
        vectoangles(dir, bs->ideal_viewangles);
      }
      else {
        vectoangles(moveresult.movedir, bs->ideal_viewangles);
      }
    }
    bs->ideal_viewangles[2] *= 0.5;
  }
  //if the weapon is used for the bot movement
  if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) bs->weaponnum = moveresult.weapon;
  //if the bot is in the area the enemy was last seen in
  if (bs->areanum == bs->lastenemyareanum) bs->chase_time = 0;
  //if the bot wants to retreat (the bot could have been damage during the chase)
  if (HBotWantsToRetreat(bs)) {
    AIEnter_Battle_Retreat(bs, "battle chase: wants to retreat");
    return qtrue;
  }
  return qtrue;
}

/*
==================
AIEnter_Battle_Retreat
==================
*/
void AIEnter_Battle_Retreat(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "battle retreat", "", s);
  bs->ainode = AINode_Battle_Retreat;
}

/*
==================
AINode_Battle_Retreat
==================
*/
int AINode_Battle_Retreat(bot_state_t *bs) {
  bot_goal_t goal;
  aas_entityinfo_t entinfo;
  bot_moveresult_t moveresult;
  vec3_t target, dir;
  float attack_skill, range;
  int areanum;

  if (BotIsObserver(bs)) {
    AIEnter_Observer(bs, "battle retreat: observer");
    return qfalse;
  }
  //if in the intermission
  if (BotIntermission(bs)) {
    AIEnter_Intermission(bs, "battle retreat: intermission");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    AIEnter_Respawn(bs, "battle retreat: bot dead");
    return qfalse;
  }
  //if no enemy
  if (bs->enemy < 0) {
    AIEnter_Seek_LTG(bs, "battle retreat: no enemy");
    return qfalse;
  }
  //
  BotEntityInfo(bs->enemy, &entinfo);
  if (EntityIsDead(&entinfo)) {
    AIEnter_Seek_LTG(bs, "battle retreat: enemy dead");
    return qfalse;
  }
  //if there is another better enemy
  if (HBotNetFindEnemy(bs, bs->enemy)) {
#ifdef DEBUG
    BotAI_Print(PRT_MESSAGE, "found new better enemy\n");
#endif
  }
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  //if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  //map specific code
  BotMapScripts(bs);
  //update the attack inventory values
  BotUpdateBattleInventory(bs, bs->enemy);
  //if the bot doesn't want to retreat anymore... probably picked up some nice items
  if (HBotWantsToChase(bs)) {
    //empty the goal stack, when chasing, only the enemy is the goal
    trap_BotEmptyGoalStack(bs->gs);
    //go chase the enemy
    AIEnter_Battle_Chase(bs, "battle retreat: wants to chase");
    return qfalse;
  }
  //update the last time the enemy was visible
  if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
    bs->enemyvisible_time = FloatTime();
    VectorCopy(entinfo.origin, target);
    // if not a player enemy
    if (bs->enemy >= MAX_CLIENTS) {
#ifdef MISSIONPACK
      // if attacking an obelisk
      if ( bs->enemy == redobelisk.entitynum ||
        bs->enemy == blueobelisk.entitynum ) {
        target[2] += 16;
      }
#endif
    }
    //update the reachability area and origin if possible
    areanum = BotPointAreaNum(target);
    if (areanum && trap_AAS_AreaReachability(areanum)) {
      VectorCopy(target, bs->lastenemyorigin);
      bs->lastenemyareanum = areanum;
    }
  }
  //if the enemy is NOT visible for 4 seconds
  if (bs->enemyvisible_time < FloatTime() - 4) {
    AIEnter_Seek_LTG(bs, "battle retreat: lost enemy");
    return qfalse;
  }
  //else if the enemy is NOT visible
  else if (bs->enemyvisible_time < FloatTime()) {
    //if there is another enemy
    if (HBotNetFindEnemy(bs, -1)) {
      AIEnter_Battle_Fight(bs, "battle retreat: another enemy");
      return qfalse;
    }
  }
  //
  BotTeamGoals(bs, qtrue);
  //use holdable items
  BotBattleUseItems(bs);
  //get the current long term goal while retreating
  if (!BotLongTermGoal(bs, bs->tfl, qtrue, &goal)) {
    AIEnter_Battle_SuicidalFight(bs, "battle retreat: no way out");
    return qfalse;
  }
  //check for nearby goals periodicly
  if (bs->check_time < FloatTime()) {
    bs->check_time = FloatTime() + 1;
    range = 150;
#ifdef CTF
    if (gametype == GT_CTF) {
      //if carrying a flag the bot shouldn't be distracted too much
      if (BotCTFCarryingFlag(bs))
        range = 50;
    }
#endif //CTF
#ifdef MISSIONPACK
    else if (gametype == GT_1FCTF) {
      if (Bot1FCTFCarryingFlag(bs))
        range = 50;
    }
    else if (gametype == GT_HARVESTER) {
      if (BotHarvesterCarryingCubes(bs))
        range = 80;
    }
#endif
    //
    if (BotNearbyGoal(bs, bs->tfl, &goal, range)) {
      trap_BotResetLastAvoidReach(bs->ms);
      //time the bot gets to pick up the nearby goal item
      bs->nbg_time = FloatTime() + range / 100 + 1;
      AIEnter_Battle_NBG(bs, "battle retreat: nbg");
      return qfalse;
    }
  }
  //initialize the movement state
  BotSetupForMovement(bs);
  //move towards the goal
  trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
  //if the movement failed
  if (moveresult.failure) {
    //reset the avoid reach, otherwise bot is stuck in current area
    trap_BotResetAvoidReach(bs->ms);
    //BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
    bs->ltg_time = 0;
  }
  //
  BotAIBlocked(bs, &moveresult, qfalse);
  //choose the best weapon to fight with
  BotChooseWeapon(bs);
  //if the view is fixed for the movement
  if (moveresult.flags & (MOVERESULT_MOVEMENTVIEW|MOVERESULT_SWIMVIEW)) {
    VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
  }
  else if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEWSET)
        && !(bs->flags & BFL_IDEALVIEWSET) ) {
    attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
    //if the bot is skilled anough
    if (attack_skill > 0.3) {
      BotAimAtEnemy(bs);
    }
    else {
      if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
        VectorSubtract(target, bs->origin, dir);
        vectoangles(dir, bs->ideal_viewangles);
      }
      else {
        vectoangles(moveresult.movedir, bs->ideal_viewangles);
      }
      bs->ideal_viewangles[2] *= 0.5;
    }
  }
  //if the weapon is used for the bot movement
  if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) bs->weaponnum = moveresult.weapon;
  //attack the enemy if possible
  BotCheckAttack(bs);
  //
  return qtrue;
}

/*
==================
AIEnter_Battle_NBG
==================
*/
void AIEnter_Battle_NBG(bot_state_t *bs, char *s) {
  BotRecordNodeSwitch(bs, "battle NBG", "", s);
  bs->ainode = AINode_Battle_NBG;
}

/*
==================
AINode_Battle_NBG
==================
*/
int AINode_Battle_NBG(bot_state_t *bs) {
  int areanum;
  bot_goal_t goal;
  aas_entityinfo_t entinfo;
  bot_moveresult_t moveresult;
  float attack_skill;
  vec3_t target, dir;

  if (BotIsObserver(bs)) {
    AIEnter_Observer(bs, "battle nbg: observer");
    return qfalse;
  }
  //if in the intermission
  if (BotIntermission(bs)) {
    AIEnter_Intermission(bs, "battle nbg: intermission");
    return qfalse;
  }
  //respawn if dead
  if (BotIsDead(bs)) {
    AIEnter_Respawn(bs, "battle nbg: bot dead");
    return qfalse;
  }
  //if no enemy
  if (bs->enemy < 0) {
    AIEnter_Seek_NBG(bs, "battle nbg: no enemy");
    return qfalse;
  }
  //
  BotEntityInfo(bs->enemy, &entinfo);
  if (EntityIsDead(&entinfo)) {
    AIEnter_Seek_NBG(bs, "battle nbg: enemy dead");
    return qfalse;
  }
  //
  bs->tfl = TFL_DEFAULT;
  if (bot_grapple.integer) bs->tfl |= TFL_GRAPPLEHOOK;
  //if in lava or slime the bot should be able to get out
  if (BotInLavaOrSlime(bs)) bs->tfl |= TFL_LAVA|TFL_SLIME;
  //
  if (BotCanAndWantsToRocketJump(bs)) {
    bs->tfl |= TFL_ROCKETJUMP;
  }
  //map specific code
  BotMapScripts(bs);
  //update the last time the enemy was visible
  if (BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)) {
    bs->enemyvisible_time = FloatTime();
    VectorCopy(entinfo.origin, target);
    // if not a player enemy
    if (bs->enemy >= MAX_CLIENTS) {
#ifdef MISSIONPACK
      // if attacking an obelisk
      if ( bs->enemy == redobelisk.entitynum ||
        bs->enemy == blueobelisk.entitynum ) {
        target[2] += 16;
      }
#endif
    }
    //update the reachability area and origin if possible
    areanum = BotPointAreaNum(target);
    if (areanum && trap_AAS_AreaReachability(areanum)) {
      VectorCopy(target, bs->lastenemyorigin);
      bs->lastenemyareanum = areanum;
    }
  }
  //if the bot has no goal or touches the current goal
  if (!trap_BotGetTopGoal(bs->gs, &goal)) {
    bs->nbg_time = 0;
  }
  else if (BotReachedGoal(bs, &goal)) {
    bs->nbg_time = 0;
  }
  //
  if (bs->nbg_time < FloatTime()) {
    //pop the current goal from the stack
    trap_BotPopGoal(bs->gs);
    //if the bot still has a goal
    if (trap_BotGetTopGoal(bs->gs, &goal))
      AIEnter_Battle_Retreat(bs, "battle nbg: time out");
    else
      AIEnter_Battle_Fight(bs, "battle nbg: time out");
    //
    return qfalse;
  }
  //initialize the movement state
  BotSetupForMovement(bs);
  //move towards the goal
  trap_BotMoveToGoal(&moveresult, bs->ms, &goal, bs->tfl);
  //if the movement failed
  if (moveresult.failure) {
    //reset the avoid reach, otherwise bot is stuck in current area
    trap_BotResetAvoidReach(bs->ms);
    //BotAI_Print(PRT_MESSAGE, "movement failure %d\n", moveresult.traveltype);
    bs->nbg_time = 0;
  }
  //
  BotAIBlocked(bs, &moveresult, qfalse);
  //update the attack inventory values
  BotUpdateBattleInventory(bs, bs->enemy);
  //choose the best weapon to fight with
  BotChooseWeapon(bs);
  //if the view is fixed for the movement
  if (moveresult.flags & (MOVERESULT_MOVEMENTVIEW|MOVERESULT_SWIMVIEW)) {
    VectorCopy(moveresult.ideal_viewangles, bs->ideal_viewangles);
  }
  else if (!(moveresult.flags & MOVERESULT_MOVEMENTVIEWSET)
        && !(bs->flags & BFL_IDEALVIEWSET)) {
    attack_skill = trap_Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
    //if the bot is skilled anough and the enemy is visible
    if (attack_skill > 0.3) {
      //&& BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy)
      BotAimAtEnemy(bs);
    }
    else {
      if (trap_BotMovementViewTarget(bs->ms, &goal, bs->tfl, 300, target)) {
        VectorSubtract(target, bs->origin, dir);
        vectoangles(dir, bs->ideal_viewangles);
      }
      else {
        vectoangles(moveresult.movedir, bs->ideal_viewangles);
      }
      bs->ideal_viewangles[2] *= 0.5;
    }
  }
  //if the weapon is used for the bot movement
  if (moveresult.flags & MOVERESULT_MOVEMENTWEAPON) bs->weaponnum = moveresult.weapon;
  //attack the enemy if possible
  BotCheckAttack(bs);
  //
  return qtrue;
}
