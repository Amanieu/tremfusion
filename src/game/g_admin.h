/*
===========================================================================
Copyright (C) 2004-2006 Tony J. White

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

#ifndef _G_ADMIN_H
#define _G_ADMIN_H

#define AP(x) trap_SendServerCommand(-1, x)
#define CP(x) trap_SendServerCommand(ent-g_entities, x)
#define CPx(x, y) trap_SendServerCommand(x, y)
#define ADMP(x) G_admin_print(ent, x)
#define ADMBP(x) G_admin_buffer_print(ent, x)
#define ADMBP_begin() G_admin_buffer_begin()
#define ADMBP_end() G_admin_buffer_end(ent)

#define MAX_ADMIN_GROUPS 64
#define MAX_ADMIN_ADMINS 4092
#define MAX_ADMIN_BANS 4092
#define MAX_ADMIN_NAMELOGS 128
#define MAX_ADMIN_NAMELOG_NAMES 5
#define MAX_ADMIN_SCRIPTS 256
#define MAX_ADMIN_CMD_LEN 20
#define MAX_ADMIN_BAN_REASON 50

/*
 * 1 - cannot be vote kicked, vote muted
 * 2 - cannot be censored or flood protected TODO
 * 3 - never loses credits for changing teams
 * 4 - can see team chat as a spectator
 * 5 - can switch teams any time, regardless of balance
 * 6 - does not need to specify a reason for a kick/ban
 * 7 - can call a vote at any time (regardless of a vote being disabled or 
 * voting limitations)
 * 8 - does not need to specify a duration for a ban
 * 9 - can run commands from team chat
 * 0 - inactivity rules do not apply to them
 * ! - admin commands cannot be used on them
 * @ - does not show up as an admin in !listplayers
 * $ - sees all information in !listplayers 
 */
#define ADMF_IMMUNITY "@immunity"
#define ADMF_NOCENSORFLOOD "@noflood" /* TODO */
#define ADMF_TEAMCHANGEFREE "@freeteamchange"
#define ADMF_SPEC_ALLCHAT "@specallchat"
#define ADMF_FORCETEAMCHANGE "@forceteamchange"
#define ADMF_UNACCOUNTABLE "@noreason"
#define ADMF_NO_VOTE_LIMIT "@unlimitedvotes"
#define ADMF_CAN_PERM_BAN "@canpermban"
#define ADMF_TEAMCHAT_CMD "@teamchatcmd"
#define ADMF_ACTIVITY "@caninactive"

#define ADMF_IMMUTABLE "@immutable"
#define ADMF_INCOGNITO "@incognito"
#define ADMF_SEESFULLLISTPLAYERS "@fulllistplayers"

#define MAX_ADMIN_LISTITEMS 20
#define MAX_ADMIN_SHOWBANS 10

typedef struct
{
  char *keyword;
  qboolean ( * handler ) ( gentity_t *ent, int skiparg );
  char *function;  // used for !help
  char *syntax;  // used for !help
}
g_admin_cmd_t;

typedef struct g_admin_group
{
  char name[ MAX_NAME_LENGTH ];
  char longname[ MAX_NAME_LENGTH ];
  char rights[ 8192 ];
  int level;
  struct g_admin_group *inherit[ MAX_ADMIN_GROUPS ];
}
g_admin_group_t;

typedef struct g_admin_admin
{
  char name[ MAX_NAME_LENGTH ];
  g_admin_group_t *group;
  char id[ RSA_STRING_LENGTH ];
}
g_admin_admin_t;

typedef struct g_admin_ban
{
  char name[ MAX_NAME_LENGTH ];
  char id[ RSA_STRING_LENGTH ];
  char ip[ 40 ]; // Enough space for IPv6
  char reason[ MAX_ADMIN_BAN_REASON ];
  int expires;
  g_admin_admin_t *banner;
}
g_admin_ban_t;

typedef struct g_admin_script
{
  char command[ MAX_ADMIN_CMD_LEN ];
  char script[ MAX_QPATH ];
  char desc[ 50 ];
  char syntax[ 50 ];
}
g_admin_script_t;

typedef struct g_admin_namelog
{
  char      name[ MAX_ADMIN_NAMELOG_NAMES ][MAX_NAME_LENGTH ];
  char      ip[ 40 ];
  char      id[ RSA_STRING_LENGTH ];
  int       slot;
  qboolean  banned;
}
g_admin_namelog_t;

qboolean G_admin_ban_check( char *userinfo, char *reason, int rlen );
qboolean G_admin_cmd_check( gentity_t *ent, qboolean say );
qboolean G_admin_readconfig( gentity_t *ent, int skiparg );
void G_admin_writeconfig( void );
qboolean G_admin_permission( gentity_t *ent, char *right );
qboolean G_admin_name_check( gentity_t *ent, char *name, char *err, int len );
void G_admin_namelog_update( gclient_t *ent, qboolean disconnect );
g_admin_admin_t *G_admin_admin( char *id );

// ! command functions
qboolean G_admin_time( gentity_t *ent, int skiparg );
qboolean G_admin_setlevel( gentity_t *ent, int skiparg );
qboolean G_admin_kick( gentity_t *ent, int skiparg );
qboolean G_admin_ban( gentity_t *ent, int skiparg );
qboolean G_admin_unban( gentity_t *ent, int skiparg );
qboolean G_admin_putteam( gentity_t *ent, int skiparg );
qboolean G_admin_listadmins( gentity_t *ent, int skiparg );
qboolean G_admin_listlayouts( gentity_t *ent, int skiparg );
qboolean G_admin_listplayers( gentity_t *ent, int skiparg );
qboolean G_admin_map( gentity_t *ent, int skiparg );
qboolean G_admin_mute( gentity_t *ent, int skiparg );
qboolean G_admin_denybuild( gentity_t *ent, int skiparg );
qboolean G_admin_showbans( gentity_t *ent, int skiparg );
qboolean G_admin_help( gentity_t *ent, int skiparg );
qboolean G_admin_admintest( gentity_t *ent, int skiparg );
qboolean G_admin_allready( gentity_t *ent, int skiparg );
qboolean G_admin_cancelvote( gentity_t *ent, int skiparg );
qboolean G_admin_passvote( gentity_t *ent, int skiparg );
qboolean G_admin_spec999( gentity_t *ent, int skiparg );
qboolean G_admin_rename( gentity_t *ent, int skiparg );
qboolean G_admin_restart( gentity_t *ent, int skiparg );
qboolean G_admin_nextmap( gentity_t *ent, int skiparg );
qboolean G_admin_namelog( gentity_t *ent, int skiparg );
qboolean G_admin_lock( gentity_t *ent, int skiparg );
qboolean G_admin_unlock( gentity_t *ent, int skiparg );

void G_admin_print( gentity_t *ent, char *m );
void G_admin_buffer_print( gentity_t *ent, char *m );
void G_admin_buffer_begin( void );
void G_admin_buffer_end( gentity_t *ent );

void G_admin_duration( int secs, char *duration, int dursize );
void G_admin_cleanup( void );
void G_admin_namelog_cleanup( void );

#endif /* ifndef _G_ADMIN_H */
