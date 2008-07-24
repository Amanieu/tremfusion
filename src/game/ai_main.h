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

#ifndef BLIB_HEADERS
#define BLIB_HEADERS
#include "../botlib/botlib.h"

#include "../botlib/be_aas.h"		// TFL_*
#include "../botlib/be_ai_goal.h"	// bot_goal_t
#include "../botlib/be_ai_move.h"	// bot_initmove_t
#include "../botlib/be_ai_weap.h"	// weaponinfo_t
#include "../botlib/be_ai_chat.h" // weaponinfo_t

#include "chars.h"				//characteristics
#endif

#define MAX_ITEMS					256
extern float floattime;
#define FloatTime() floattime
//bot flags
#define BFL_STRAFERIGHT				1	//strafe to the right
#define BFL_ATTACKED				2	//bot has attacked last ai frame
#define BFL_ATTACKJUMPED			4	//bot jumped during attack last frame
#define BFL_AIMATENEMY				8	//bot aimed at the enemy this frame
#define BFL_AVOIDRIGHT				16	//avoid obstacles by going to the right
#define BFL_IDEALVIEWSET			32	//bot has ideal view angles set
#define BFL_FIGHTSUICIDAL			64	//bot is in a suicidal fight

#define IDEAL_ATTACKDIST			140

//copied from the aas file header
#define PRESENCE_NONE				1
#define PRESENCE_NORMAL				2
#define PRESENCE_CROUCH				4

//long term goal types
#define LTG_TEAMHELP        1 //help a team mate
#define LTG_TEAMACCOMPANY     2 //accompany a team mate
#define LTG_DEFENDKEYAREA     3 //defend a key area
#define LTG_GETFLAG         4 //get the enemy flag
#define LTG_RUSHBASE        5 //rush to the base
#define LTG_RETURNFLAG        6 //return the flag
#define LTG_CAMP          7 //camp somewhere
#define LTG_CAMPORDER       8 //ordered to camp somewhere
#define LTG_PATROL          9 //patrol
#define LTG_GETITEM         10  //get an item
#define LTG_KILL          11  //kill someone
#define LTG_HARVEST         12  //harvest skulls
#define LTG_ATTACKENEMYBASE     13  //attack the enemy base
#define LTG_MAKELOVE_UNDER      14
#define LTG_MAKELOVE_ONTOP      15

//goal flag, see ../botlib/be_ai_goal.h for the other GFL_*
#define GFL_AIR     128

// Neural network
#define MAX_NODESWITCHES  50
#define MAX_WAYPOINTS   128

//check points
typedef struct bot_waypoint_s
{
  int     inuse;
  char    name[32];
  bot_goal_t  goal;
  struct    bot_waypoint_s *next, *prev;
} bot_waypoint_t;

// print types
typedef enum{
	BPMSG,
	BPERROR,
	BPDEBUG
}botprint_t;

// inventory
typedef enum{
	BI_HEALTH,
	BI_CREDITS,		// evos for alien bots
	BI_WEAPON,
	BI_AMMO,
	BI_CLIPS,		
	BI_STAMINA,		// == boost time for aliens?
	BI_GRENADE,
	BI_MEDKIT,
	BI_JETPACK,
	BI_BATTPACK,
	BI_LARMOR,
	BI_HELMET,
	BI_BSUIT,
	BI_SIZE,
	BI_CLASS
} bot_inventory_t;

typedef enum
{
	NEXT_STATE_NONE, 
  NEXT_STATE_LOS, // Go to next state when u can see the goal
  NEXT_STATE_DISTANCE //Go to the next state when u are close enough to the goal.
} nextstatetype_t;

#define MAX_ACTIVATESTACK   8
#define MAX_ACTIVATEAREAS   32

typedef struct bot_activategoal_s
{
  int inuse;
  bot_goal_t goal;            //goal to activate (buttons etc.)
  float time;               //time to activate something
  float start_time;           //time starting to activate something
  float justused_time;          //time the goal was used
  int shoot;                //true if bot has to shoot to activate
  int weapon;               //weapon to be used for activation
  vec3_t target;              //target to shoot at to activate something
  vec3_t origin;              //origin of the blocking entity to activate
  int areas[MAX_ACTIVATEAREAS];     //routing areas disabled by blocking entity
  int numareas;             //number of disabled routing areas
  int areasdisabled;            //true if the areas are disabled for the routing
  struct bot_activategoal_s *next;    //next activate goal on stack
} bot_activategoal_t;

//bot state
typedef struct bot_state_s
{
	int inuse;										//true if this state is used by a bot client
	int botthink_residual;							//residual for the bot thinks
	int client;										//client number of the bot
	int entitynum;									//entity number of the bot
	playerState_t cur_ps;							//current player state
	int last_eFlags;								//last ps flags
	usercmd_t lastucmd;								//usercmd from last frame
	int entityeventTime[1024];						//last entity event time
	//
	bot_settings_t settings;           //several bot settings
	int (*ainode)(struct bot_state_s *bs);     //current AI node
	float thinktime;								//time the bot thinks this frame
	vec3_t origin;									//origin of the bot
	vec3_t velocity;								//velocity of the bot
	int presencetype;								//presence type of the bot
	vec3_t eye;										//eye coordinates of the bot
	 int areanum;                  //the number of the area the bot is in
  int inventory[MAX_ITEMS];           //string with items amounts the bot has
  int tfl;                    //the travel flags the bot uses
  int flags;                    //several flags
  int respawn_wait;               //wait until respawned
  int lasthealth;                 //health value previous frame
  int lastkilledplayer;             //last killed player
  int lastkilledby;               //player that last killed this bot
  int botdeathtype;               //the death type of the bot
  int enemydeathtype;               //the death type of the enemy
  int botsuicide;                 //true when the bot suicides
  int enemysuicide;               //true when the enemy of the bot suicides
  int setupcount;                 //true when the bot has just been setup
  int map_restart;                  //true when the map is being restarted
  int entergamechat;                //true when the bot used an enter game chat
  int num_deaths;                 //number of time this bot died
  int num_kills;                  //number of kills of this bot
  int revenge_enemy;                //the revenge enemy
  int revenge_kills;                //number of kills the enemy made
  int lastframe_health;             //health value the last frame
  int lasthitcount;               //number of hits last frame
  int chatto;                   //chat to all or team
  float walker;                 //walker charactertic
  float ltime;                  //local bot time
  float entergame_time;             //time the bot entered the game
  float ltg_time;                 //long term goal time
  float nbg_time;                 //nearby goal time
  float respawn_time;               //time the bot takes to respawn
  float respawnchat_time;             //time the bot started a chat during respawn
  float chase_time;               //time the bot will chase the enemy
  float enemyvisible_time;            //time the enemy was last visible
  float check_time;               //time to check for nearby items
  float stand_time;               //time the bot is standing still
  float lastchat_time;              //time the bot last selected a chat
  float kamikaze_time;              //time to check for kamikaze usage
  float invulnerability_time;           //time to check for invulnerability usage
  float standfindenemy_time;            //time to find enemy while standing
  float attackstrafe_time;            //time the bot is strafing in one dir
  float attackcrouch_time;            //time the bot will stop crouching
  float attackchase_time;             //time the bot chases during actual attack
  float attackjump_time;              //time the bot jumped during attack
  float enemysight_time;              //time before reacting to enemy
  float enemydeath_time;              //time the enemy died
  float enemyposition_time;           //time the position and velocity of the enemy were stored
  float defendaway_time;              //time away while defending
  float defendaway_range;             //max travel time away from defend area
  float rushbaseaway_time;            //time away from rushing to the base
  float attackaway_time;              //time away from attacking the enemy base
  float harvestaway_time;             //time away from harvesting
  float ctfroam_time;               //time the bot is roaming in ctf
  float killedenemy_time;             //time the bot killed the enemy
  float arrive_time;                //time arrived (at companion)
  float lastair_time;               //last time the bot had air
  float teleport_time;              //last time the bot teleported
  float camp_time;                //last time camped
  float camp_range;               //camp range
  float weaponchange_time;            //time the bot started changing weapons
  float firethrottlewait_time;          //amount of time to wait
  float firethrottleshoot_time;         //amount of time to shoot
  float notblocked_time;              //last time the bot was not blocked
  float blockedbyavoidspot_time;          //time blocked by an avoid spot
  float predictobstacles_time;          //last time the bot predicted obstacles
  int predictobstacles_goalareanum;       //last goal areanum the bot predicted obstacles for
  vec3_t aimtarget;
  vec3_t enemyvelocity;             //enemy velocity 0.5 secs ago during battle
  vec3_t enemyorigin;               //enemy origin 0.5 secs ago during battle
  //
  int kamikazebody;               //kamikaze body
//  int proxmines[MAX_PROXMINES];
  int numproxmines;
  //
  int character;                  //the bot character
  int ms;                     //move state of the bot
  int gs;                     //goal state of the bot
  int cs;                     //chat state of the bot
  int ws;                     //weapon state of the bot
  //
  int enemy;                    //enemy entity number
  int lastenemyareanum;             //last reachability area the enemy was in
  vec3_t lastenemyorigin;             //last origin of the enemy in the reachability area
  int weaponnum;                  //current weapon number
  vec3_t viewangles;                //current view angles
  vec3_t ideal_viewangles;            //ideal view angles
  vec3_t viewanglespeed;
  //
  int ltgtype;                  //long term goal type
  // team goals
  int teammate;                 //team mate involved in this team goal
  int decisionmaker;                //player who decided to go for this goal
  int ordered;                  //true if ordered to do something
  float order_time;               //time ordered to do something
  int owndecision_time;             //time the bot made it's own decision
  bot_goal_t teamgoal;              //the team goal
  bot_goal_t altroutegoal;            //alternative route goal
  float reachedaltroutegoal_time;         //time the bot reached the alt route goal
  float teammessage_time;             //time to message team mates what the bot is doing
  float teamgoal_time;              //time to stop helping team mate
  float teammatevisible_time;           //last time the team mate was NOT visible
  int teamtaskpreference;             //team task preference
  // last ordered team goal
  int lastgoal_decisionmaker;
  int lastgoal_ltgtype;
  int lastgoal_teammate;
  bot_goal_t lastgoal_teamgoal;
  // for leading team mates
  int lead_teammate;                //team mate the bot is leading
  bot_goal_t lead_teamgoal;           //team goal while leading
  float lead_time;                //time leading someone
  float leadvisible_time;             //last time the team mate was visible
  float leadmessage_time;             //last time a messaged was sent to the team mate
  float leadbackup_time;              //time backing up towards team mate
  //
  char teamleader[32];              //netname of the team leader
  float askteamleader_time;           //time asked for team leader
  float becometeamleader_time;          //time the bot will become the team leader
  float teamgiveorders_time;            //time to give team orders
  float lastflagcapture_time;           //last time a flag was captured
  int numteammates;               //number of team mates
  int redflagstatus;                //0 = at base, 1 = not at base
  int blueflagstatus;               //0 = at base, 1 = not at base
  int neutralflagstatus;              //0 = at base, 1 = our team has flag, 2 = enemy team has flag, 3 = enemy team dropped the flag
  int flagstatuschanged;              //flag status changed
  int forceorders;                //true if forced to give orders
  int flagcarrier;                //team mate carrying the enemy flag
  int ctfstrategy;                //ctf strategy
  char subteam[32];               //sub team name
  float formation_dist;             //formation team mate intervening space
  char formation_teammate[16];          //netname of the team mate the bot uses for relative positioning
  float formation_angle;              //angle relative to the formation team mate
  vec3_t formation_dir;             //the direction the formation is moving in
  vec3_t formation_origin;            //origin the bot uses for relative positioning
  bot_goal_t formation_goal;            //formation goal

  bot_activategoal_t *activatestack;        //first activate goal on the stack
  bot_activategoal_t activategoalheap[MAX_ACTIVATESTACK]; //activate goal heap
//
  bot_waypoint_t *checkpoints;          //check points
  bot_waypoint_t *patrolpoints;         //patrol points
  bot_waypoint_t *curpatrolpoint;         //current patrol point the bot is going for
  int patrolflags;                //patrol flags
  
  float lastheardtime; // Time we last ''heard'' something
  vec3_t lastheardorigin; // Origin of it
  
  int team;
  gentity_t *enemyent;
  gentity_t *ent;
  char hudinfo[MAX_INFO_STRING];  // for bot_developer hud debug text
  float buyammo_time;
  float findenemy_time;  //time the bot last looked for a new enemy
  int state;
  int prevstate;
  int statecycles;
  bot_goal_t goal;
} bot_state_t;

// ai_utils.c
void BotChangeViewAngles(bot_state_t *bs, float thinktime);
int BotPointAreaNum(vec3_t origin);
void QDECL Bot_Print(botprint_t type, char *fmt, ...);
void BotShowViewAngles(bot_state_t* bs);
void BotTestAAS(vec3_t origin);
int BotAI_GetClientState( int clientNum, playerState_t *state );
void BotSetupForMovement(bot_state_t *bs);
qboolean BotIsAlive(bot_state_t* bs);
qboolean BotIntermission(bot_state_t *bs);
qboolean BotGoalForBuildable(bot_state_t* bs, bot_goal_t* goal, int bclass);
qboolean Nullcheckfuct( bot_state_t* bs, int entnum, float dist );
qboolean BotGoalForClosestBuildable(bot_state_t* bs, bot_goal_t* goal, int bclass, qboolean(*check)(bot_state_t* bs, int entnum, float dist ));
qboolean BotGoalForNearestEnemy(bot_state_t* bs, bot_goal_t* goal);
int BotFindEnemy(bot_state_t *bs, int curenemy);
void BotAddInfo(bot_state_t* bs, char* key, char* value );
char *ClientName(int client, char *name, int size);
char *ClientSkin(int client, char *skin, int size);
qboolean AI_BuildableWalkingRange( bot_state_t* bs, int r, buildable_t buildable );
qboolean BotGoalForEnemy(bot_state_t *bs, bot_goal_t* goal);

//int CheckAreaForGoal(vec3_t origin);
qboolean BotInFieldOfVision(vec3_t viewangles, float fov, vec3_t angles);
float BotEntityVisible(int viewer, vec3_t eye, vec3_t viewangles, float fov, int ent);
int BotSameTeam(bot_state_t *bs, int entnum);
float BotEntityDistance(bot_state_t* bs, int ent);
void	BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
qboolean EntityIsInvisible(aas_entityinfo_t *entinfo);
qboolean EntityIsDead(aas_entityinfo_t *entinfo);
qboolean EntityIsShooting(aas_entityinfo_t *entinfo);
qboolean EntityIsChatting(aas_entityinfo_t *entinfo);
int ClientOnSameTeamFromName(bot_state_t *bs, char *name);
void BotSetTeleportTime(bot_state_t *bs);
qboolean BotIsDead(bot_state_t *bs);
qboolean BotIsObserver(bot_state_t *bs);
qboolean BotIntermission(bot_state_t *bs);
qboolean BotInLavaOrSlime(bot_state_t *bs);
bot_waypoint_t *BotCreateWayPoint(char *name, vec3_t origin, int areanum);
bot_waypoint_t *BotFindWayPoint(bot_waypoint_t *waypoints, char *name);
void BotFreeWaypoints(bot_waypoint_t *wp);
void BotInitWaypoints(void);
qboolean TeamPlayIsOn( void );
void BotSetEntityNumForGoalWithModel(bot_goal_t *goal, int eType, char *modelname);
int BotReachedGoal(bot_state_t *bs, bot_goal_t *goal);

extern bot_waypoint_t botai_waypoints[MAX_WAYPOINTS];
extern bot_waypoint_t *botai_freewaypoints;

// ai_main.c
void BotEntityInfo(int entnum, aas_entityinfo_t *info);
void BotCheckDeath(int target, int attacker, int mod);
void BotAIBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate);
void BotRandomMove(bot_state_t *bs, bot_moveresult_t *moveresult);
int BotAILoadMap( int restart );
void  QDECL BotAI_Print(int type, char *fmt, ...);

//returns the number of bots in the game
int NumBots(void);
extern int max_bspmodelindex;      //maximum BSP model index

// ai_human.c
void BotHumanAI(bot_state_t* bs, float thinktime);
void HBotEnterChat(bot_state_t *bs);
// ai_alien.c
void BotAlienAI(bot_state_t* bs, float thinktime);

//g_bot.c functions
void G_BotAdd( gentity_t *ent, char *name, int team, float *skill, char *botinfo );
void G_BotDel( gentity_t *ent, int clientNum );

void G_BotThink( gentity_t *self );
void G_BotSpectatorThink( gentity_t *self );
void G_BotClearCommands( gentity_t *self );
qboolean botAimAtTarget( gentity_t *self, gentity_t *target );
qboolean botTargetInRange( gentity_t *self, gentity_t *target );
int botFindClosestEnemy( gentity_t *self, qboolean includeTeam );
int botGetDistanceBetweenPlayer( gentity_t *self, gentity_t *player );
void G_RemoveQueuedBotBegin( int clientNum );
void G_AddRandomBot(gentity_t *ent, int team , char *name, float *skill);
int G_RemoveRandomBot( int team );
int G_CountHumanPlayers( int team );
int G_CountBotPlayers( int team );
void G_CheckMinimumPlayers( void );
void G_CheckBotSpawn( void );
qboolean G_BotConnect( int clientNum, qboolean restart );
char *G_GetBotInfoByName( const char *name );
void G_BotRemoveAll( gentity_t *ent );
qboolean G_ListChars(gentity_t *ent, int skiparg);
