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

// g_weapon.c
// perform the server side effects of a weapon firing

#include "g_local.h"

static  vec3_t  forward, right, up;
static  vec3_t  muzzle;

/*
================
G_ForceWeaponChange
================
*/
void G_ForceWeaponChange( gentity_t *ent, weapon_t weapon )
{
  playerState_t *ps = &ent->client->ps;

  if( !ent )
    return;

  // stop a reload in progress
  if( ps->weaponstate == WEAPON_RELOADING )
  {
    ps->torsoAnim = ( ( ps->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | TORSO_RAISE;
    ps->weaponTime = 250;
    ps->weaponstate = WEAPON_READY;
  }

  if( weapon == WP_NONE ||
      !BG_InventoryContainsWeapon( weapon, ps->stats ) )
  {
    ps->persistant[ PERS_NEWWEAPON ] =
      BG_PrimaryWeapon( ent->client->ps.stats );
  }
  else
    ps->persistant[ PERS_NEWWEAPON ] = weapon;

  // force this here to prevent flamer effect from continuing
  ps->generic1 = WPM_NOTFIRING;

  // The PMove will do an animated drop, raise, and set the new weapon
  ps->pm_flags |= PMF_WEAPON_SWITCH;
}

/*
=================
G_GiveClientMaxAmmo
=================
*/
void G_GiveClientMaxAmmo( gentity_t *ent, qboolean buyingEnergyAmmo )
{
  int i, maxAmmo, maxClips;
  qboolean restoredAmmo = qfalse, restoredEnergy = qfalse;

  for( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ )
  {
    qboolean energyWeapon;
  
    energyWeapon = BG_Weapon( i )->usesEnergy;
    if( !BG_InventoryContainsWeapon( i, ent->client->ps.stats ) ||
        BG_Weapon( i )->infiniteAmmo ||
        BG_WeaponIsFull( i, ent->client->ps.stats,
                         ent->client->ps.ammo, ent->client->ps.clips ) ||
        ( buyingEnergyAmmo && !energyWeapon ) )
      continue;

    maxAmmo = BG_Weapon( i )->maxAmmo;
    maxClips = BG_Weapon( i )->maxClips;

    // Apply battery pack modifier
    if( energyWeapon &&
        BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) )
    {
      maxAmmo *= BATTPACK_MODIFIER;
      restoredEnergy = qtrue;
    }

    ent->client->ps.ammo = maxAmmo;
    ent->client->ps.clips = maxClips;
    restoredAmmo = qtrue;
  } 

  if( restoredAmmo )
    G_ForceWeaponChange( ent, ent->client->ps.weapon );

  if( restoredEnergy )
    G_AddEvent( ent, EV_RPTUSE_SOUND, 0 );
}

/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout )
{
  vec3_t v, newv;
  float dot;

  VectorSubtract( impact, start, v );
  dot = DotProduct( v, dir );
  VectorMA( v, -2 * dot, dir, newv );

  VectorNormalize(newv);
  VectorMA(impact, 8192, newv, endout);
}

/*
================
G_WideTrace

Trace a bounding box against entities, but not the world
Also check there is a line of sight between the start and end point
================
*/
static void G_WideTrace( trace_t *tr, gentity_t *ent, float range,
                         float width, float height, gentity_t **target )
{
  vec3_t    mins, maxs;
  vec3_t    end;

  VectorSet( mins, -width, -width, -width );
  VectorSet( maxs, width, width, width );
  mins[ 2 ] -= height - width;

  *target = NULL;

  if( !ent->client )
    return;

  // Set aiming directions
  VectorMA( muzzle, range, forward, end );

  G_UnlaggedOn( ent, muzzle, range );
  //prefer the target in the crosshairs
  trap_Trace( tr, muzzle, NULL, NULL, end, ent->s.number, CONTENTS_BODY );

  if( tr->entityNum == ENTITYNUM_NONE )
    // Trace against entities
    trap_Trace( tr, muzzle, mins, maxs, end, ent->s.number, CONTENTS_BODY );

  // If we started in a solid that means someone is within our muzzle box,
  // depending on which tremded is used the trace may not give us the entity 
  // number though. We can do a trace from the entity origin to the muzzle
  // to get it.
  if( tr->startsolid && tr->entityNum == ENTITYNUM_NONE )
  {
    trap_Trace( tr, ent->client->ps.origin, mins, maxs, muzzle,
                ent->s.number, CONTENTS_BODY );
    if( tr->entityNum != ENTITYNUM_NONE )
      *target = &g_entities[ tr->entityNum ];
  }

  else if( tr->entityNum != ENTITYNUM_NONE )
  {
    *target = &g_entities[ tr->entityNum ];

    // Set range to the trace length plus the width, so that the end of the
    // LOS trace is close to the exterior of the target's bounding box
    range = Distance( muzzle, tr->endpos ) + width;
    VectorMA( muzzle, range, forward, end );

    // Trace for line of sight against the world
    trap_Trace( tr, muzzle, NULL, NULL, end, 0, CONTENTS_SOLID );
    if( tr->fraction < 1.0f )
      *target = NULL;
  }

  G_UnlaggedOff( );
}

/*
======================
SnapVectorTowards
SnapVectorNormal

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to )
{
  int   i;

  for( i = 0 ; i < 3 ; i++ )
  {
    if( v[ i ] >= 0 )
      v[ i ] = (int)( v[ i ] + ( to[ i ] <= v[ i ] ? 0 : 1 ) );
    else
      v[ i ] = (int)( v[ i ] + ( to[ i ] <= v[ i ] ? -1 : 0 ) );
  }
}

void SnapVectorNormal( vec3_t v, vec3_t normal )
{
  int i;

  for( i = 0 ; i < 3 ; i++ )
  {
    if( v[ i ] >= 0 )
      v[ i ] = (int)( v[ i ] + ( normal[ i ] <= 0 ? 0 : 1 ) );
    else
      v[ i ] = (int)( v[ i ] + ( normal[ i ] <= 0 ? -1 : 0 ) );
  }
}

/*
===============
BloodSpurt

Generates a blood spurt event for traces with accurate end points
===============
*/
static void BloodSpurt( gentity_t *attacker, gentity_t *victim, trace_t *tr )
{
  gentity_t *tent;

  if( !attacker->client )
    return;
  tent = G_TempEntity( tr->endpos, EV_MISSILE_HIT );
  tent->s.otherEntityNum = victim->s.number;
  tent->s.eventParm = DirToByte( tr->plane.normal );
  tent->s.weapon = attacker->s.weapon;
  tent->s.generic1 = attacker->s.generic1; // weaponMode
  }

/*
===============
WideBloodSpurt

Calculates the position of a blood spurt for wide traces and generates an event
===============
*/
static void WideBloodSpurt( gentity_t *attacker, gentity_t *victim, trace_t *tr )
{
  gentity_t *tent;
  vec3_t normal, origin;
  float mag, radius;

  if( !attacker->client )
    return;

  if( tr )
    VectorSubtract( tr->endpos, victim->client->ps.origin, normal );
  else
    VectorSubtract( attacker->client->ps.origin,
                    victim->client->ps.origin, normal );

  // Normalize the horizontal components of the vector difference to the
  // "radius" of the bounding box
  mag = sqrt( normal[ 0 ] * normal[ 0 ] + normal[ 1 ] * normal[ 1 ] );
  radius = victim->r.maxs[ 0 ] * 1.21f;
  if( mag > radius )
  {
    normal[ 0 ] = normal[ 0 ] / mag * radius;
    normal[ 1 ] = normal[ 1 ] / mag * radius;
  }

  // Clamp origin to be within bounding box vertically
  if( normal[ 2 ] > victim->r.maxs[ 2 ] )
    normal[ 2 ] = victim->r.maxs[ 2 ];
  if( normal[ 2 ] < victim->r.mins[ 2 ] )
    normal[ 2 ] = victim->r.mins[ 2 ];

  VectorAdd( victim->client->ps.origin, normal, origin );
  VectorNegate( normal, normal );
  VectorNormalize( normal );

  // Create the blood spurt effect entity
  tent = G_TempEntity( origin, EV_MISSILE_HIT );
  tent->s.eventParm = DirToByte( normal );
  tent->s.otherEntityNum = victim->s.number;
  tent->s.weapon = attacker->s.weapon;
  tent->s.generic1 = attacker->s.generic1; // weaponMode
}

/*
===============
meleeAttack
===============
*/
void meleeAttack( gentity_t *ent, float range, float width, float height,
                  int damage, meansOfDeath_t mod )
{
  trace_t   tr;
  gentity_t *traceEnt;

  G_WideTrace( &tr, ent, range, width, height, &traceEnt );
  if( traceEnt == NULL || !traceEnt->takedamage ) {
    G_AddEvent( ent, EV_ALIEN_MISS, 0 );
    return;
  }
  WideBloodSpurt( ent, traceEnt, &tr );
  G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NO_KNOCKBACK, mod );
}

/*
======================================================================

MACHINEGUN

======================================================================
*/

void bulletFire( gentity_t *ent, float spread, int damage, int mod )
{
  trace_t   tr;
  vec3_t    end;
  float   r;
  float   u;
  gentity_t *tent;
  gentity_t *traceEnt;

  r = random( ) * M_PI * 2.0f;
  u = sin( r ) * crandom( ) * spread * 16;
  r = cos( r ) * crandom( ) * spread * 16;
  VectorMA( muzzle, 8192 * 16, forward, end );
  VectorMA( end, r, right, end );
  VectorMA( end, u, up, end );

  // don't use unlagged if this is not a client (e.g. turret)
  if( ent->client )
  {
    G_UnlaggedOn( ent, muzzle, 8192 * 16 );
    trap_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );
    G_UnlaggedOff( );
  }
  else
    trap_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, muzzle );

  // send bullet impact
  if( traceEnt->takedamage && traceEnt->client )
  {
    tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
    tent->s.eventParm = traceEnt->s.number;
  }
  else
  {
    tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
    tent->s.eventParm = DirToByte( tr.plane.normal );
  }
  tent->s.otherEntityNum = ent->s.number;

  if( traceEnt->takedamage )
  {
    G_Damage( traceEnt, ent, ent, forward, tr.endpos,
      damage, 0, mod );
  }
}

/*
======================================================================

SHOTGUN

======================================================================
*/

// this should match CG_ShotgunPattern
void ShotgunPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent )
{
  int        i;
  float      r, u;
  vec3_t    end;
  vec3_t    forward, right, up;
  trace_t    tr;
  gentity_t  *traceEnt;

  // derive the right and up vectors from the forward vector, because
  // the client won't have any other information
  VectorNormalize2( origin2, forward );
  PerpendicularVector( right, forward );
  CrossProduct( forward, right, up );

  // generate the "random" spread pattern
  for( i = 0; i < SHOTGUN_PELLETS; i++ )
  {
    r = Q_crandom( &seed ) * SHOTGUN_SPREAD * 16;
    u = Q_crandom( &seed ) * SHOTGUN_SPREAD * 16;
    VectorMA( origin, SHOTGUN_RANGE, forward, end );
    VectorMA( end, r, right, end );
    VectorMA( end, u, up, end );

    trap_Trace( &tr, origin, NULL, NULL, end, ent->s.number, MASK_SHOT );
    traceEnt = &g_entities[ tr.entityNum ];

    // send bullet impact
    if( !( tr.surfaceFlags & SURF_NOIMPACT ) )
    {
      if( traceEnt->takedamage )
        G_Damage( traceEnt, ent, ent, forward, tr.endpos,  SHOTGUN_DMG, 0, MOD_SHOTGUN );
    }
  }
}


void shotgunFire( gentity_t *ent )
{
  gentity_t    *tent;

  // send shotgun blast
  tent = G_TempEntity( muzzle, EV_SHOTGUN );
  VectorScale( forward, 4096, tent->s.origin2 );
  SnapVector( tent->s.origin2 );
  tent->s.eventParm = rand() & 255;    // seed for spread pattern
  tent->s.otherEntityNum = ent->s.number;
  //G_UnlaggedOn( ent, muzzle, 8192 * 16 );
  G_UnlaggedOn( ent, muzzle, SHOTGUN_RANGE );
  ShotgunPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent );
  G_UnlaggedOff();
}

/*
======================================================================

MASS DRIVER

======================================================================
*/

void massDriverFire( gentity_t *ent )
{
  trace_t tr;
  vec3_t hitPoints[ MDRIVER_MAX_HITS ], hitNormals[ MDRIVER_MAX_HITS ],
         origin, originToEnd, muzzleToEnd, muzzleToOrigin, end;
  gentity_t *traceEnts[ MDRIVER_MAX_HITS ], *traceEnt, *tent;
  float length_offset;
  int i, hits = 0, skipent;

  // loop through all entities hit by a line trace
  G_UnlaggedOn( ent, muzzle, 8192 * 16 );
  VectorMA( muzzle, 8192 * 16, forward, end );
  VectorCopy( muzzle, tr.endpos );
  skipent = ent->s.number;
  for( i = 0; i < MDRIVER_MAX_HITS && skipent != ENTITYNUM_NONE; i++ )
  {
    trap_Trace( &tr, tr.endpos, NULL, NULL, end, skipent, MASK_SHOT );
    if( tr.surfaceFlags & SURF_NOIMPACT )
      break;
    traceEnt = &g_entities[ tr.entityNum ];
    skipent = tr.entityNum;
    if( traceEnt->s.eType == ET_PLAYER )
    {
      // don't travel through teammates with FF off
      if( OnSameTeam( ent, traceEnt ) &&
          ( !g_friendlyFire.integer || !g_friendlyFireHumans.integer ) )
        skipent = ENTITYNUM_NONE;
    }
    else if( traceEnt->s.eType == ET_BUILDABLE )
    {
      // don't travel through team buildables with FF off
      if( traceEnt->buildableTeam == ent->client->pers.teamSelection &&
          !g_friendlyBuildableFire.integer )
        skipent = ENTITYNUM_NONE;
    }
    else
      skipent = ENTITYNUM_NONE;

    // save the hit entity, position, and normal
    VectorCopy( tr.endpos, hitPoints[ hits ] );
    VectorCopy( tr.plane.normal, hitNormals[ hits ] );
    SnapVectorNormal( hitPoints[ hits ], tr.plane.normal );
    traceEnts[ hits++ ] = traceEnt;
  }

  // originate trail line from the gun tip, not the head!  
  VectorCopy( muzzle, origin );
  VectorMA( origin, -6, up, origin );
  VectorMA( origin, 4, right, origin );
  VectorMA( origin, 24, forward, origin );
  
  // save the final position
  VectorCopy( tr.endpos, end );
  VectorSubtract( end, origin, originToEnd );
  VectorNormalize( originToEnd );
  
  // origin is further in front than muzzle, need to adjust length
  VectorSubtract( origin, muzzle, muzzleToOrigin );
  VectorSubtract( end, muzzle, muzzleToEnd );
  VectorNormalize( muzzleToEnd );
  length_offset = DotProduct( muzzleToEnd, muzzleToOrigin );

  // now that the trace is finished, we know where we stopped and can generate
  // visually correct impact locations
  for( i = 0; i < hits; i++ )
  {
    vec3_t muzzleToPos;
    float length;
    
    // restore saved values
    VectorCopy( hitPoints[ i ], tr.endpos );
    VectorCopy( hitNormals[ i ], tr.plane.normal );
    traceEnt = traceEnts[ i ];
    
    // compute the visually correct impact point
    VectorSubtract( tr.endpos, muzzle, muzzleToPos );
    length = VectorLength( muzzleToPos ) - length_offset;
    VectorMA( origin, length, originToEnd, tr.endpos );

    // send impact
    if( traceEnt->takedamage && 
        ( traceEnt->s.eType == ET_BUILDABLE || 
          traceEnt->s.eType == ET_PLAYER ) )
      BloodSpurt( ent, traceEnt, &tr );
    else if( i < hits - 1 )
    {
      tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
      tent->s.eventParm = DirToByte( tr.plane.normal );
      tent->s.weapon = ent->s.weapon;
      tent->s.generic1 = ent->s.generic1; // weaponMode
    }
    
    if( traceEnt->takedamage )
      G_Damage( traceEnt, ent, ent, forward, tr.endpos,
                MDRIVER_DMG, 0, MOD_MDRIVER );    
  }

  // create an event entity for the trail, doubles as an impact event
  SnapVectorNormal( end, tr.plane.normal );
  tent = G_TempEntity( end, EV_MASS_DRIVER );
  tent->s.eventParm = DirToByte( tr.plane.normal );
  tent->s.weapon = ent->s.weapon;
  tent->s.generic1 = ent->s.generic1; // weaponMode
  VectorCopy( origin, tent->s.origin2 );
  
  G_UnlaggedOff( );
}

/*
======================================================================

LOCKBLOB

======================================================================
*/

void lockBlobLauncherFire( gentity_t *ent )
{
  gentity_t *m;

  m = fire_lockblob( ent, muzzle, forward );

//  VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );  // "real" physics
}

/*
======================================================================

HIVE

======================================================================
*/

void hiveFire( gentity_t *ent )
{
  vec3_t origin;

  // Fire from the hive tip, not the center
  VectorMA( muzzle, ent->r.maxs[ 2 ], ent->s.origin2, origin );

  fire_hive( ent, origin, forward );
}

/*
======================================================================

BLASTER PISTOL

======================================================================
*/

void blasterFire( gentity_t *ent )
{
  gentity_t *m;

  m = fire_blaster( ent, muzzle, forward );

//  VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );  // "real" physics
}

/*
======================================================================

PULSE RIFLE

======================================================================
*/

void pulseRifleFire( gentity_t *ent )
{
  gentity_t *m;

  m = fire_pulseRifle( ent, muzzle, forward );

//  VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );  // "real" physics
}

/*
======================================================================

FLAME THROWER

======================================================================
*/

void flamerFire( gentity_t *ent )
{
  vec3_t origin;

  // Correct muzzle so that the missile does not start in the ceiling 
  VectorMA( muzzle, -7.f, up, origin );

  // Correct muzzle so that the missile fires from the player's hand
  VectorMA( origin, 4.5f, right, origin );

  fire_flamer( ent, origin, forward );
}

/*
======================================================================

GRENADE

======================================================================
*/

void throwGrenade( gentity_t *ent )
{
  gentity_t *m;

  m = launch_grenade( ent, muzzle, forward );
}

/*
======================================================================

LAS GUN

======================================================================
*/

/*
===============
lasGunFire
===============
*/
void lasGunFire( gentity_t *ent )
{
  trace_t   tr;
  vec3_t    end;
  gentity_t *tent;
  gentity_t *traceEnt;

  VectorMA( muzzle, 8192 * 16, forward, end );

  G_UnlaggedOn( ent, muzzle, 8192 * 16 );
  trap_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );
  G_UnlaggedOff( );

  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  // snap the endpos to integers, but nudged towards the line
  SnapVectorTowards( tr.endpos, muzzle );

  // send impact
  if( traceEnt->takedamage && 
      ( traceEnt->s.eType == ET_BUILDABLE || 
        traceEnt->s.eType == ET_PLAYER ) )
  {
    BloodSpurt( ent, traceEnt, &tr );
  }
  else
  {
    tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
    tent->s.eventParm = DirToByte( tr.plane.normal );
    tent->s.weapon = ent->s.weapon;
    tent->s.generic1 = ent->s.generic1; //weaponMode
  }

  if( traceEnt->takedamage )
    G_Damage( traceEnt, ent, ent, forward, tr.endpos, LASGUN_DAMAGE, 0, MOD_LASGUN );
}

/*
======================================================================

PAIN SAW

======================================================================
*/

void painSawFire( gentity_t *ent )
{
  trace_t   tr;
  vec3_t    temp;
  gentity_t *tent, *traceEnt;

  G_WideTrace( &tr, ent, PAINSAW_RANGE, PAINSAW_WIDTH, PAINSAW_HEIGHT,
               &traceEnt );
  if( !traceEnt || !traceEnt->takedamage )
    return;

  // hack to line up particle system with weapon model
  tr.endpos[ 2 ] -= 5.0f;

  // send blood impact
  if( traceEnt->s.eType == ET_PLAYER || traceEnt->s.eType == ET_BUILDABLE )
  {
    BloodSpurt( ent, traceEnt, &tr );
  }
  else
  {
    VectorCopy( tr.endpos, temp );
    tent = G_TempEntity( temp, EV_MISSILE_MISS );
    tent->s.eventParm = DirToByte( tr.plane.normal );
    tent->s.weapon = ent->s.weapon;
    tent->s.generic1 = ent->s.generic1; //weaponMode
  }

  G_Damage( traceEnt, ent, ent, forward, tr.endpos, PAINSAW_DAMAGE, DAMAGE_NO_KNOCKBACK, MOD_PAINSAW );
}

/*
======================================================================

LUCIFER CANNON

======================================================================
*/

/*
===============
LCChargeFire
===============
*/
void LCChargeFire( gentity_t *ent, qboolean secondary )
{
  gentity_t *m;

  if( secondary && ent->client->ps.stats[ STAT_MISC ] <= 0 )
    m = fire_luciferCannon( ent, muzzle, forward, LCANNON_SECONDARY_DAMAGE,
                            LCANNON_SECONDARY_RADIUS, LCANNON_SECONDARY_SPEED );
  else
    m = fire_luciferCannon( ent, muzzle, forward,
                            ent->client->ps.stats[ STAT_MISC ] *
                            LCANNON_DAMAGE / LCANNON_CHARGE_TIME_MAX,
                            LCANNON_RADIUS, LCANNON_SPEED );

  ent->client->ps.stats[ STAT_MISC ] = 0;
}

/*
======================================================================

TESLA GENERATOR

======================================================================
*/


void teslaFire( gentity_t *self )
{
  trace_t   tr;
  vec3_t origin, target;
  gentity_t *tent;

  if( !self->enemy )
    return;

  // Move the muzzle from the entity origin up a bit to fire over turrets
  VectorMA( muzzle, self->r.maxs[ 2 ], self->s.origin2, origin );

  // Don't aim for the center, aim at the top of the bounding box
  VectorCopy( self->enemy->s.origin, target );
  target[ 2 ] += self->enemy->r.maxs[ 2 ];

  // Trace to the target entity
  trap_Trace( &tr, origin, NULL, NULL, target, self->s.number, MASK_SHOT );
  if( tr.entityNum != self->enemy->s.number )
    return;

  // Client side firing effect
  self->s.eFlags |= EF_FIRING;

  // Deal damage
  if( self->enemy->takedamage )
  {
    vec3_t dir;
    
    VectorSubtract( target, origin, dir );
    G_Damage( self->enemy, self, self, dir, tr.endpos,
      TESLAGEN_DMG, 0, MOD_TESLAGEN );
  }

  // Send tesla zap trail
  tent = G_TempEntity( tr.endpos, EV_TESLATRAIL );
  tent->s.generic1 = self->s.number; // src
  tent->s.clientNum = self->enemy->s.number; // dest
}


/*
======================================================================

BUILD GUN

======================================================================
*/

/*
===============
cancelBuildFire
===============
*/
void cancelBuildFire( gentity_t *ent )
{
  vec3_t      forward, end;
  trace_t     tr;
  gentity_t   *traceEnt;
  int         bHealth;

  // Cancel ghost buildable
  if( ent->client->ps.stats[ STAT_BUILDABLE ] != BA_NONE )
  {
    ent->client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
    return;
  }

  // Construction kit repair
  if( ent->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS )
  {
    AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
    VectorMA( ent->client->ps.origin, 100, forward, end );

    trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->s.number,
                MASK_PLAYERSOLID );
    traceEnt = &g_entities[ tr.entityNum ];

    if( tr.fraction < 1.0 && traceEnt->spawned && traceEnt->health > 0 &&
        traceEnt->s.eType == ET_BUILDABLE && traceEnt->buildableTeam == TEAM_HUMANS )
    {
      if( ent->client->ps.stats[ STAT_MISC ] > 0 )
      {
        G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
        return;
      }

      bHealth = BG_Buildable( traceEnt->s.modelindex )->health;
      traceEnt->health += HBUILD_HEALRATE;
      if( traceEnt->health >= bHealth )
      {
        traceEnt->health = bHealth;
        G_AddEvent( ent, EV_BUILD_REPAIRED, 0 );
      }
      else
        G_AddEvent( ent, EV_BUILD_REPAIR, 0 );
    }
  }
  
  // Granger slash
  else if( ent->client->ps.weapon == WP_ABUILD ||
           ent->client->ps.weapon == WP_ABUILD2 )
    meleeAttack( ent, ABUILDER_CLAW_RANGE, ABUILDER_CLAW_WIDTH,
                 ABUILDER_CLAW_WIDTH, ABUILDER_CLAW_DMG, MOD_ABUILDER_CLAW );
}

/*
===============
buildFire
===============
*/
void buildFire( gentity_t *ent, dynMenu_t menu )
{
  buildable_t buildable = ( ent->client->ps.stats[ STAT_BUILDABLE ]
                            & ~SB_VALID_TOGGLEBIT );

  if( buildable > BA_NONE )
  {
    if( ent->client->ps.stats[ STAT_MISC ] > 0 )
    {
      G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
      return;
    }

    if( G_BuildIfValid( ent, buildable ) )
    {
      if( !g_cheats.integer )
        ent->client->ps.stats[ STAT_MISC ] +=
          BG_Buildable( buildable )->buildTime;
      ent->client->ps.stats[ STAT_BUILDABLE ] = BA_NONE;
    }
    return;
  }

  G_TriggerMenu( ent->client->ps.clientNum, menu );
}

void slowBlobFire( gentity_t *ent )
{
  gentity_t *m;

  m = fire_slowBlob( ent, muzzle, forward );

//  VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );  // "real" physics
}


/*
======================================================================

LEVEL0

======================================================================
*/

/*
===============
CheckVenomAttack
===============
*/
qboolean CheckVenomAttack( gentity_t *ent )
{
  trace_t   tr;
  gentity_t *traceEnt;
  int       damage = LEVEL0_BITE_DMG;

  if( ent->client->ps.weaponTime )
	return qfalse;

  // Calculate muzzle point
  AngleVectors( ent->client->ps.viewangles, forward, right, up );
  CalcMuzzlePoint( ent, forward, right, up, muzzle );

  G_WideTrace( &tr, ent, LEVEL0_BITE_RANGE, LEVEL0_BITE_WIDTH,
               LEVEL0_BITE_WIDTH, &traceEnt );

  if( traceEnt == NULL )
    return qfalse;

  if( !traceEnt->takedamage )
    return qfalse;

  if( !traceEnt->client && !traceEnt->s.eType == ET_BUILDABLE )
    return qfalse;

  // only allow bites to work against buildings as they are constructing
  if( traceEnt->s.eType == ET_BUILDABLE )
  {
    if( traceEnt->spawned )
      return qfalse;
    if( traceEnt->buildableTeam == TEAM_ALIENS )
      return qfalse;
  }

  if( traceEnt->client )
  {
    if( traceEnt->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      return qfalse;
    if( traceEnt->client->ps.stats[ STAT_HEALTH ] <= 0 )
      return qfalse;
  }

  // send blood impact
  WideBloodSpurt( ent, traceEnt, &tr );

  G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NO_KNOCKBACK, MOD_LEVEL0_BITE );
  ent->client->ps.weaponTime += LEVEL0_BITE_REPEAT;
  return qtrue;
}

/*
======================================================================

LEVEL1

======================================================================
*/

/*
===============
CheckGrabAttack
===============
*/
void CheckGrabAttack( gentity_t *ent )
{
  trace_t   tr;
  vec3_t    end, dir;
  gentity_t *traceEnt;

  // set aiming directions
  AngleVectors( ent->client->ps.viewangles, forward, right, up );

  CalcMuzzlePoint( ent, forward, right, up, muzzle );

  VectorMA( muzzle, LEVEL1_GRAB_RANGE, forward, end );

  trap_Trace( &tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT );
  if( tr.surfaceFlags & SURF_NOIMPACT )
    return;

  traceEnt = &g_entities[ tr.entityNum ];

  if( !traceEnt->takedamage )
    return;

  if( traceEnt->client )
  {
    if( traceEnt->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS )
      return;

    if( traceEnt->client->ps.stats[ STAT_HEALTH ] <= 0 )
      return;

    if( !( traceEnt->client->ps.stats[ STAT_STATE ] & SS_GRABBED ) )
    {
      AngleVectors( traceEnt->client->ps.viewangles, dir, NULL, NULL );
      traceEnt->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );

      //event for client side grab effect
      G_AddPredictableEvent( ent, EV_LEV1_GRAB, 0 );
    }

    traceEnt->client->ps.stats[ STAT_STATE ] |= SS_GRABBED;

    if( ent->client->ps.weapon == WP_ALEVEL1 )
      traceEnt->client->grabExpiryTime = level.time + LEVEL1_GRAB_TIME;
    else if( ent->client->ps.weapon == WP_ALEVEL1_UPG )
      traceEnt->client->grabExpiryTime = level.time + LEVEL1_GRAB_U_TIME;
  }
  else if( traceEnt->s.eType == ET_BUILDABLE &&
      traceEnt->s.modelindex == BA_H_MGTURRET )
  {
    if( !traceEnt->lev1Grabbed )
      G_AddPredictableEvent( ent, EV_LEV1_GRAB, 0 );

    traceEnt->lev1Grabbed = qtrue;
    traceEnt->lev1GrabTime = level.time;
  }
}

/*
===============
poisonCloud
===============
*/
void poisonCloud( gentity_t *ent )
{
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { LEVEL1_PCLOUD_RANGE, LEVEL1_PCLOUD_RANGE, LEVEL1_PCLOUD_RANGE };
  vec3_t    mins, maxs;
  int       i, num;
  gentity_t *humanPlayer;
  trace_t   tr;

  VectorAdd( ent->client->ps.origin, range, maxs );
  VectorSubtract( ent->client->ps.origin, range, mins );

  G_UnlaggedOn( ent, ent->client->ps.origin, LEVEL1_PCLOUD_RANGE );
  num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
  {
    humanPlayer = &g_entities[ entityList[ i ] ];

    if( humanPlayer->client &&
        humanPlayer->client->pers.teamSelection == TEAM_HUMANS )
    {
      trap_Trace( &tr, muzzle, NULL, NULL, humanPlayer->s.origin,
                  humanPlayer->s.number, CONTENTS_SOLID );

      //can't see target from here
      if( tr.entityNum == ENTITYNUM_WORLD )
        continue;

      humanPlayer->client->ps.eFlags |= EF_POISONCLOUDED;
      humanPlayer->client->lastPoisonCloudedTime = level.time;

      trap_SendServerCommand( humanPlayer->client->ps.clientNum,
                              "poisoncloud" );
    }
  }
  G_UnlaggedOff( );
}


/*
======================================================================

LEVEL2

======================================================================
*/

static vec3_t sortReference;
static int QDECL G_SortDistance( const void *a, const void *b )
{
  gentity_t    *aent, *bent;
  float        adist, bdist;

  aent = &g_entities[ *(int *)a ];
  bent = &g_entities[ *(int *)b ];
  adist = Distance( sortReference, aent->s.origin );
  bdist = Distance( sortReference, bent->s.origin );
  if( adist > bdist )
    return -1;
  else if( adist < bdist )
    return 1;
  else
    return 0;
}

/*
===============
G_UpdateZaps
===============
*/
void G_UpdateZaps( gentity_t *ent )
{
  int       entityList[ MAX_GENTITIES ];
  int       hitList[ MAX_GENTITIES ];
  vec3_t    range = { LEVEL2_AREAZAP_CUTOFF,
                      LEVEL2_AREAZAP_CUTOFF,
                      LEVEL2_AREAZAP_CUTOFF };
  vec3_t    mins, maxs;
  int       i, j;
  int       hit = 0;
  int       num;
  gentity_t *enemy;
  gentity_t *effect;
  trace_t   tr;
  qboolean  alreadyTargeted = qfalse;
  int       damage;


  if( !ent->zapping || ent->health <= 0 )
    return;

  VectorScale( range, 1.0f / M_ROOT3, range );
  VectorAdd( ent->s.origin, range, maxs );
  VectorSubtract( ent->s.origin, range, mins );

  num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    enemy = &g_entities[ entityList[ i ] ];

    if( ( ( enemy->client &&
            enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS ) ||
        ( enemy->s.eType == ET_BUILDABLE &&
          BG_Buildable( enemy->s.modelindex )->team == TEAM_HUMANS ) ) &&
        enemy->health > 0 )
    {

      alreadyTargeted = qfalse;
      for( j = 0; j < LEVEL2_AREAZAP_MAX_TARGETS; j++ )
      {
        if( ent->zapTargets[ j ] == entityList[ i ] )
        {
          alreadyTargeted = qtrue;
            break;
        }
      }

      if( !alreadyTargeted &&
          Distance( ent->s.origin, enemy->s.origin ) > LEVEL2_AREAZAP_RANGE )
      {
        continue;
      }

      trap_Trace( &tr, ent->s.origin, NULL, NULL, enemy->s.origin,
        ent-g_entities, MASK_SHOT );
      if( tr.entityNum == enemy-g_entities )
        hitList[ hit++ ] = tr.entityNum;
    }
  }

  for( i = 0; i < LEVEL2_AREAZAP_MAX_TARGETS; i++ )
    ent->zapTargets[ i ] = -1;

  if( !hit )
    return;

  ent->zapDmg += ( (float)( level.time - level.previousTime ) / 1000.0f )
                     * LEVEL2_AREAZAP_DMG;
  damage = (int)ent->zapDmg;
  // wait until we've accumulated enough damage for bsuit to take at
  // least 1 HP
  if( damage < 5 )
    damage = 0;
  else
    ent->zapDmg -= (int)damage;
/*
  effect->s.eType = ET_LEV2_ZAP_CHAIN;
  effect->classname = "lev2zapchain";
  G_SetOrigin( effect, zap->creator->s.origin );
  effect->s.misc = zap->creator->s.number;
*/

  VectorCopy( ent->s.origin, sortReference );
  qsort( hitList, hit, sizeof( int ), G_SortDistance );

  effect = G_TempEntity( ent->s.origin, EV_LEV2_ZAP );
  effect->s.misc = ent-g_entities;
  effect->s.time = effect->s.time2 = effect->s.constantLight = -1;
  for( i = 0; i < hit; i++ )
  {
    if( i >= LEVEL2_AREAZAP_MAX_TARGETS )
      break;

    ent->zapTargets[ i ] = hitList[ i ];

    enemy = &g_entities[ hitList[ i ] ];


    if( damage > 0 )
    {
      G_Damage( enemy, ent, ent, NULL, enemy->s.origin,
                    damage, DAMAGE_NO_KNOCKBACK | DAMAGE_NO_LOCDAMAGE, MOD_LEVEL2_ZAP );
    }
    switch( i )
    {
      case 0: effect->s.time = hitList[ i ];          break;
      case 1: effect->s.time2 = hitList[ i ];         break;
      case 2: effect->s.constantLight = hitList[ i ]; break;
      default:                                        break;
    }
  }
}

/*
======================================================================

LEVEL3

======================================================================
*/

/*
===============
CheckPounceAttack
===============
*/
qboolean CheckPounceAttack( gentity_t *ent )
{
  trace_t   tr;
  gentity_t *traceEnt;
  int damage, timeMax, payload;

  if( ent->client->pmext.pouncePayload <= 0 )
    return qfalse;

  // In case the goon lands on his target, he get's one shot after landing
  payload = ent->client->pmext.pouncePayload;
  if( !( ent->client->ps.pm_flags & PMF_CHARGE ) )
    ent->client->pmext.pouncePayload = 0;
    
  if( ent->client->ps.weaponTime > 0 )
    return qfalse;

  // Calculate muzzle point
  AngleVectors( ent->client->ps.viewangles, forward, right, up );
  CalcMuzzlePoint( ent, forward, right, up, muzzle );

  // Trace from muzzle to see what we hit
  G_WideTrace( &tr, ent, LEVEL3_POUNCE_RANGE, LEVEL3_POUNCE_WIDTH,
               LEVEL3_POUNCE_WIDTH, &traceEnt );
  if( traceEnt == NULL )
    return qfalse;

  // Send blood impact
  if( traceEnt->takedamage )
    WideBloodSpurt( ent, traceEnt, &tr );

  if( !traceEnt->takedamage )
    return qfalse;

  // Deal damage
  timeMax = ent->client->ps.weapon == WP_ALEVEL3 ? LEVEL3_POUNCE_TIME :
                                                   LEVEL3_POUNCE_TIME_UPG;
  damage = payload * LEVEL3_POUNCE_DMG / timeMax;
  ent->client->pmext.pouncePayload = 0;
  G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage,
            DAMAGE_NO_LOCDAMAGE, MOD_LEVEL3_POUNCE );

  return qtrue;
}

void bounceBallFire( gentity_t *ent )
{
  gentity_t *m;

  m = fire_bounceBall( ent, muzzle, forward );

//  VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );  // "real" physics
}


/*
======================================================================

LEVEL4

======================================================================
*/

/*
===============
G_ChargeAttack
===============
*/
void G_ChargeAttack( gentity_t *ent, gentity_t *victim )
{
  int       damage;
  vec3_t    forward, normal;

  if( ent->client->ps.stats[ STAT_MISC ] <= 0 ||
      !( ent->client->ps.stats[ STAT_STATE ] & SS_CHARGING ) ||
      ent->client->ps.weaponTime )
    return;

  VectorSubtract( victim->s.origin, ent->s.origin, forward );
  VectorNormalize( forward );
  VectorNegate( forward, normal );

  if( !victim->takedamage )
    return;

  WideBloodSpurt( ent, victim, NULL );

  damage = LEVEL4_TRAMPLE_DMG * ent->client->ps.stats[ STAT_MISC ] /
           LEVEL4_TRAMPLE_DURATION;

  G_Damage( victim, ent, ent, forward, victim->s.origin, damage,
            DAMAGE_NO_LOCDAMAGE, MOD_LEVEL4_TRAMPLE );

  ent->client->ps.weaponTime += LEVEL4_TRAMPLE_REPEAT;

  if( !victim->client )
    ent->client->ps.stats[ STAT_MISC ] = 0;
}

/*
===============
G_CrushAttack

Should only be called if there was an impact between a tyrant and another player
===============
*/
void G_CrushAttack( gentity_t *ent, gentity_t *victim )
{
  vec3_t dir;
  float jump;
  int damage;

  if( !victim->takedamage ||
      ent->client->ps.origin[ 2 ] + ent->r.mins[ 2 ] <
      victim->s.origin[ 2 ] + victim->r.maxs[ 2 ] ||
      ( victim->client &&
        victim->client->ps.groundEntityNum == ENTITYNUM_NONE ) )
    return;

  // Deal velocity based damage to target
  jump = BG_Class( ent->client->ps.stats[ STAT_CLASS ] )->jumpMagnitude;
  damage = ( ent->client->pmext.fallVelocity + jump ) *
           -LEVEL4_CRUSH_DAMAGE_PER_V;
  if( damage < 0 )
    damage = 0;

  // Players also get damaged periodically
  if( victim->client &&
      ent->client->lastCrushTime + LEVEL4_CRUSH_REPEAT < level.time )
  {
    ent->client->lastCrushTime = level.time;
    damage += LEVEL4_CRUSH_DAMAGE;
  }
  
  if( damage < 1 )
    return;

  // Crush the victim over a period of time
  VectorSubtract( victim->s.origin, ent->client->ps.origin, dir );
  G_Damage( victim, ent, ent, dir, victim->s.origin, damage,
            DAMAGE_NO_LOCDAMAGE, MOD_LEVEL4_CRUSH );
}

//======================================================================

/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint )
{
  vec3_t normal;

  VectorCopy( ent->client->ps.origin, muzzlePoint );
  BG_GetClientNormal( &ent->client->ps, normal );
  VectorMA( muzzlePoint, ent->client->ps.viewheight, normal, muzzlePoint );
  VectorMA( muzzlePoint, 1, forward, muzzlePoint );
  // snap to integer coordinates for more efficient network bandwidth usage
  SnapVector( muzzlePoint );
}

/*
===============
FireWeapon3
===============
*/
void FireWeapon3( gentity_t *ent )
{
  if( ent->client )
  {
    // set aiming directions
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    CalcMuzzlePoint( ent, forward, right, up, muzzle );
  }
  else
  {
    AngleVectors( ent->s.angles2, forward, right, up );
    VectorCopy( ent->s.pos.trBase, muzzle );
  }

  // fire the specific weapon
  switch( ent->s.weapon )
  {
    case WP_ALEVEL3_UPG:
      bounceBallFire( ent );
      break;

    case WP_ABUILD2:
      slowBlobFire( ent );
      break;

    default:
      break;
  }
}

/*
===============
FireWeapon2
===============
*/
void FireWeapon2( gentity_t *ent )
{
  if( ent->client )
  {
    // set aiming directions
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    CalcMuzzlePoint( ent, forward, right, up, muzzle );
  }
  else
  {
    AngleVectors( ent->s.angles2, forward, right, up );
    VectorCopy( ent->s.pos.trBase, muzzle );
  }

  // fire the specific weapon
  switch( ent->s.weapon )
  {
    case WP_ALEVEL1_UPG:
      poisonCloud( ent );
      break;

    case WP_LUCIFER_CANNON:
      LCChargeFire( ent, qtrue );
      break;

    case WP_ABUILD:
    case WP_ABUILD2:
    case WP_HBUILD:
      cancelBuildFire( ent );
      break;
    default:
      break;
  }
}

/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent )
{
  if( ent->client )
  {
    // set aiming directions
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    CalcMuzzlePoint( ent, forward, right, up, muzzle );
  }
  else
  {
    AngleVectors( ent->turretAim, forward, right, up );
    VectorCopy( ent->s.pos.trBase, muzzle );
  }

  // fire the specific weapon
  switch( ent->s.weapon )
  {
    case WP_ALEVEL1:
    case WP_ALEVEL1_UPG:
      meleeAttack( ent, LEVEL1_CLAW_RANGE, LEVEL1_CLAW_WIDTH, LEVEL1_CLAW_WIDTH,
                   LEVEL1_CLAW_DMG, MOD_LEVEL1_CLAW );
      break;
    case WP_ALEVEL3:
      meleeAttack( ent, LEVEL3_CLAW_RANGE, LEVEL3_CLAW_WIDTH, LEVEL3_CLAW_WIDTH,
                   LEVEL3_CLAW_DMG, MOD_LEVEL3_CLAW );
      break;
    case WP_ALEVEL3_UPG:
      meleeAttack( ent, LEVEL3_CLAW_UPG_RANGE, LEVEL3_CLAW_WIDTH,
                   LEVEL3_CLAW_WIDTH, LEVEL3_CLAW_DMG, MOD_LEVEL3_CLAW );
      break;
    case WP_ALEVEL2:
      meleeAttack( ent, LEVEL2_CLAW_RANGE, LEVEL2_CLAW_WIDTH, LEVEL2_CLAW_WIDTH,
                   LEVEL2_CLAW_DMG, MOD_LEVEL2_CLAW );
      break;
    case WP_ALEVEL2_UPG:
      meleeAttack( ent, LEVEL2_CLAW_RANGE, LEVEL2_CLAW_WIDTH, LEVEL2_CLAW_WIDTH,
                   LEVEL2_CLAW_DMG, MOD_LEVEL2_CLAW );
      break;
    case WP_ALEVEL4:
      meleeAttack( ent, LEVEL4_CLAW_RANGE, LEVEL4_CLAW_WIDTH,
                   LEVEL4_CLAW_HEIGHT, LEVEL4_CLAW_DMG, MOD_LEVEL4_CLAW );
      break;

    case WP_BLASTER:
      blasterFire( ent );
      break;
    case WP_MACHINEGUN:
      bulletFire( ent, RIFLE_SPREAD, RIFLE_DMG, MOD_MACHINEGUN );
      break;
    case WP_SHOTGUN:
      shotgunFire( ent );
      break;
    case WP_CHAINGUN:
      bulletFire( ent, CHAINGUN_SPREAD, CHAINGUN_DMG, MOD_CHAINGUN );
      break;
    case WP_FLAMER:
      flamerFire( ent );
      break;
    case WP_PULSE_RIFLE:
      pulseRifleFire( ent );
      break;
    case WP_MASS_DRIVER:
      massDriverFire( ent );
      break;
    case WP_LUCIFER_CANNON:
      LCChargeFire( ent, qfalse );
      break;
    case WP_LAS_GUN:
      lasGunFire( ent );
      break;
    case WP_PAIN_SAW:
      painSawFire( ent );
      break;
    case WP_GRENADE:
      throwGrenade( ent );
      break;

    case WP_LOCKBLOB_LAUNCHER:
      lockBlobLauncherFire( ent );
      break;
    case WP_HIVE:
      hiveFire( ent );
      break;
    case WP_TESLAGEN:
      teslaFire( ent );
      break;
    case WP_MGTURRET:
      bulletFire( ent, MGTURRET_SPREAD, MGTURRET_DMG, MOD_MGTURRET );
      break;

    case WP_ABUILD:
    case WP_ABUILD2:
      buildFire( ent, MN_A_BUILD );
      break;
    case WP_HBUILD:
      buildFire( ent, MN_H_BUILD );
      break;
    default:
      break;
  }
}

