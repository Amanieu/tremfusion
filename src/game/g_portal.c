/*
===========================================================================
Copyright (C) 2008-2009 Mercury

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

/*
===============
G_Portal_Clear
This will clear portals defined by the integer "portal".
===============
*/
void G_Portal_Clear( gentity_t *ent, int portal )
{
	if( portal == 0 )
	{
		// If there is a first portal, clear it
		if( ent->portal_1 )
		{
			G_FreeEntity( ent->portal_1 );
			ent->portal_1 = NULL;
		}
	
		// If there is a second portal, clear it
		if( ent->portal_2 )
		{
			G_FreeEntity( ent->portal_2 );
			ent->portal_2 = NULL;
		}
		return;
	}
	else if( portal == 1 )
	{
		// If there is a first portal, clear it
		if( ent->portal_1 )
		{
			G_FreeEntity( ent->portal_1 );
			ent->portal_1 = NULL;
		}
		return;
	}
	else if( portal == 2 )
	{
		// If there is a second portal, clear it
		if( ent->portal_2 )
		{
			G_FreeEntity( ent->portal_2 );
			ent->portal_2 = NULL;
		}
		return;
	}
  // If there is nothing, do nothing?
  return;
}

#define  SCAN_RANGE  100

// The portal one think...
void G_Portal_Think_One( gentity_t *ent )
{
	int		entityList[ MAX_GENTITIES ];
 	int		i, num;
 	trace_t		tr;
 	vec3_t		origin, end, dir, angles, mins, maxs, kvel;
	vec3_t		range = { 35.0f, 35.0f, 35.0f }; // Scanning range!
	gentity_t	*tent, *traceEnt;
	float 		len;
	
	// Set up a scanning area, so we can tell if anyone is around to teleport
	VectorAdd( ent->r.currentOrigin, range, maxs );
	VectorSubtract( ent->r.currentOrigin, range, mins );
  
	// If the portal owner has left the game, or if they are not holding the portal gun, kill self
	if( !ent->parent || !ent->parent->client || ent->parent->client->pers.connected != CON_CONNECTED || ent->parent->client->ps.weapon != WP_MASS_DRIVER )
	{
		G_Portal_Clear( ent->parent, 0 );
		return;
  	}
	
  	if( ent->livetime < level.time )
	{
		G_Portal_Clear( ent->parent, 1 );
		return;
	}
	
	// If we don't have a mate, just wait till we do
	if( !ent->parent->portal_2 )
	{
		ent->nextthink = level.time;
		return;
	}
		
  	num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
	for( i = 0; i < num; i++ )
	{
  		tent = &g_entities[ entityList[ i ] ];
		
		if( !tent->client )
			continue;
		
		// Lets grab this info so we can use it later...
		VectorCopy( ent->parent->portal_2->r.currentOrigin, origin );
		VectorCopy( ent->parent->portal_2->pdir, dir );
	
		// Lets trace to see where we are going to spawn the entity
		VectorMA( origin, SCAN_RANGE, dir, end );
		trap_Trace( &tr, origin, tent->r.mins, tent->r.maxs, end, tent->s.number, MASK_SHOT );
	
		// Set the trace entity so we can work with it
		traceEnt = &g_entities[ tr.entityNum ];
	
		// Lets make sure we can go there first!
		if( tr.fraction < 1.0f )
		{
			ent->nextthink = level.time;
			G_Portal_Clear( ent->parent, 2 );
			return;
		}
		
		// Copy the end trace to the players origin
		VectorCopy( tr.endpos, tent->client->ps.origin );
		
		// Grab how big the velocity is right now...
		len = VectorLength( tent->client->ps.velocity );
		
		// Lets make sure we aren't on the ground when we change it.
		if( !( dir[ 2 ] >= 1.0f || ( dir[ 2 ] <= -1.0f ) ) )
		{
			// Lets convert the direction to a viewangle and copy to the players viewangle
			vectoangles( dir, angles );
			G_SetClientViewAngle( tent, angles );
		}
		else
		{
			// Cut it a bit for anti-glitch reasons...
			len /= 1.5;
		}
		
		// Lets scale the velocity and copy to the players
		VectorScale( dir, len, kvel );
		VectorCopy( kvel, tent->client->ps.velocity );
			
		// Make it look right when the players are teleported to the other portal.
		tent->client->ps.eFlags ^= EF_TELEPORT_BIT;
			
		ent->nextthink = level.time;
		return;
	}
	ent->nextthink = level.time;
	return;
}

// The portal two think...
void G_Portal_Think_Two( gentity_t *ent )
{
	int		entityList[ MAX_GENTITIES ];
 	int		i, num;
 	trace_t		tr;
 	vec3_t		origin, end, dir, angles, mins, maxs, kvel;
	vec3_t		range = { 35.0f, 35.0f, 35.0f }; // Scanning range!
	gentity_t	*tent, *traceEnt;
	float 		len;
	
	// Set up a scanning area, so we can tell if anyone is around to teleport
	VectorAdd( ent->r.currentOrigin, range, maxs );
	VectorSubtract( ent->r.currentOrigin, range, mins );
  
	// If the portal owner has left the game, or if they are not holding the portal gun, kill self
	if( !ent->parent || !ent->parent->client || ent->parent->client->pers.connected != CON_CONNECTED || ent->parent->client->ps.weapon != WP_MASS_DRIVER )
	{
		G_Portal_Clear( ent->parent, 0 );
		return;
  	}
	if( ent->livetime < level.time )
	{
		G_Portal_Clear( ent->parent, 2 );
		return;
	}
	
	// If we don't have a mate, just wait till we do
	if( !ent->parent->portal_1 )
	{
		ent->nextthink = level.time;
		return;
	}
	
  	num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
	for( i = 0; i < num; i++ )
	{
  		tent = &g_entities[ entityList[ i ] ];
		
		if( !tent->client )
			continue;
			
		// Lets grab this info so we can use it later...
		VectorCopy( ent->parent->portal_1->r.currentOrigin, origin );
		VectorCopy( ent->parent->portal_1->pdir, dir );
	
		// Lets trace to see where we are going to spawn the entity
		VectorMA( origin, SCAN_RANGE, dir, end );
		trap_Trace( &tr, origin, tent->r.mins, tent->r.maxs, end, tent->s.number, MASK_SHOT );
	
		// Set the trace entity so we can work with it
		traceEnt = &g_entities[ tr.entityNum ];
	
		// Lets make sure we can go there first!
		if( tr.fraction < 1.0f )
		{
			ent->nextthink = level.time;
			G_Portal_Clear( ent->parent, 1 );
			return;
		}
		
		// Copy the end trace to the players origin
		VectorCopy( tr.endpos, tent->client->ps.origin );
	
		// Grab how big the velocity is right now...
		len = VectorLength( tent->client->ps.velocity );
	
		
		// Lets make sure we aren't on the ground when we change it.
		if( !( dir[ 2 ] >= 1.0f || ( dir[ 2 ] <= -1.0f ) ) )
		{
			// Lets convert the direction to a viewangle and copy to the players viewangle
			vectoangles( dir, angles );
			G_SetClientViewAngle( tent, angles );
		}
		else
		{
			// Cut it a bit for anti-glitch reasons...
			len /= 1.5;
		}
		
		// Lets scale the velocity and copy to the players
		VectorScale( dir, len, kvel );
		VectorCopy( kvel, tent->client->ps.velocity );
			
		// Make it look right when the players are teleported to the other portal.
		tent->client->ps.eFlags ^= EF_TELEPORT_BIT;
			
		ent->nextthink = level.time;
		return;
	}
	ent->nextthink = level.time;
	return;
}

/*
===============
G_Portal_Create_One
This is used to spawn a portal.
===============
*/
void G_Portal_Create_One( gentity_t *ent, vec3_t origin, vec3_t normal )
{
  // Make a temp entity and set it's think...
  ent->portal_1 = G_Spawn( );
  ent->portal_1->s.modelindex = 1;
  ent->portal_1->think = G_Portal_Think_One;
  ent->portal_1->nextthink = level.time + 5;
	
  // Move it to the wall it will be placed on
  VectorCopy( origin, ent->portal_1->s.pos.trBase );
  VectorCopy( origin, ent->portal_1->r.currentOrigin );
   	
  // Set the way it will be tracing
  VectorCopy( normal, ent->portal_1->pdir );
  	
  // Make sure we know its a portal no matter what
  ent->portal_1->portalent_1 = qtrue;
  
  // lets give it a max time to live.
  ent->portal_1->livetime = level.time + 30000; // 30 seconds?
  	
  // Link it to the world!
  trap_LinkEntity( ent->portal_1 );
  	
  // Set its parent for tracking its mate
  ent->portal_1->parent = ent;
  
  return;
}

/*
===============
G_Portal_Create_Two
This is used to spawn a portal.
===============
*/
void G_Portal_Create_Two( gentity_t *ent, vec3_t origin, vec3_t normal )
{
  // Make a temp entity and set it's think...
  ent->portal_2 = G_Spawn( );
  ent->portal_2->s.modelindex = 1;
  ent->portal_2->think = G_Portal_Think_Two;
  ent->portal_2->nextthink = level.time + 5;
	
  // Move it to the wall it will be placed on
  VectorCopy( origin, ent->portal_2->s.pos.trBase );
  VectorCopy( origin, ent->portal_2->r.currentOrigin );
   	
  // Set the way it will be tracing
  VectorCopy( normal, ent->portal_2->pdir );
  	
  // Make sure we know its a portal no matter what
  ent->portal_2->portalent_2 = qtrue;
  
  // lets give it a max time to live.
  ent->portal_2->livetime = level.time + 30000; // 30 seconds?
  	
  // Link it to the world!
  trap_LinkEntity( ent->portal_2 );
  	
  // Set its parent for tracking it's mate
  ent->portal_2->parent = ent;
  
  return;
}
