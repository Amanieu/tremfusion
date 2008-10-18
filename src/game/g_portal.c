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

#define PORTAL_SHORT_DELAY 200
#define PORTAL_LONG_DELAY 500

/*
===============
G_Portal_Touch
Send someone over to the other portal.
===============
*/
void G_Portal_Touch(gentity_t *self, gentity_t *other, trace_t *trace)
{
	gentity_t *portal;
	vec3_t origin, dir, end, angles;
	trace_t tr;
	int speed;

	if (!other->client)
		return;

	if (self->parent->client->pers.portals[0] == self)
		portal = self->parent->client->pers.portals[1];
	else
		portal = self->parent->client->pers.portals[0];
	if (!portal)
		return;

	if (self->parent->client->portaltime + PORTAL_SHORT_DELAY > level.time)
		return;
	if (self->parent->client->portaltime + PORTAL_LONG_DELAY > level.time && self == self->parent->client->lastportal)
		return;
	self->parent->client->lastportal = self;

	// Check if there is room to spawn
	VectorCopy(portal->r.currentOrigin, origin);
	VectorCopy(portal->portaldir, dir);
	VectorMA(origin, (other->r.maxs[2] + 10) * M_ROOT3, dir, end);
	trap_Trace(&tr, origin, NULL, NULL, end, portal->s.number, MASK_SHOT);
	if (tr.entityNum != ENTITYNUM_NONE)
		return;
	trap_Trace(&tr, end, other->r.mins, other->r.maxs, end, -1, MASK_PLAYERSOLID);
	if (tr.entityNum != ENTITYNUM_NONE)
		return;

	// Teleport!
	self->parent->client->portaltime = level.time;
	trap_UnlinkEntity(other);
	VectorCopy(end, other->client->ps.origin);
	speed = VectorLength(other->client->ps.velocity);
	VectorScale(portal->portaldir, speed, other->client->ps.velocity);
	other->client->ps.eFlags ^= EF_TELEPORT_BIT;
	G_UnlaggedClear(other);
	if (!dir[2]) {
		vectoangles(dir, angles);
		G_SetClientViewAngle(other, angles);
	}
	BG_PlayerStateToEntityState(&other->client->ps, &other->s, qtrue);
	VectorCopy(other->client->ps.origin, other->r.currentOrigin);
	trap_LinkEntity(other);
}

/*
===============
G_Portal_Create
This is used to spawn a portal.
===============
*/
void G_Portal_Create(gentity_t *ent, vec3_t origin, vec3_t normal)
{
	gentity_t *portal;
	vec3_t range = {15.0f, 15.0f, 15.0f};

	// Create the portal
	portal = G_Spawn();
	portal->r.contents = CONTENTS_TRIGGER;
	portal->s.eType = ET_TELEPORT_TRIGGER;
	portal->touch = G_Portal_Touch;
	portal->s.modelindex = BA_H_REPEATER;
	VectorCopy(range, portal->r.maxs);
	VectorSubtract(vec3_origin, range, portal->r.mins);
	G_SetOrigin(portal, origin);
	VectorCopy(normal, portal->portaldir);
	trap_LinkEntity(portal);

	// Attach it to the client
	portal->parent = ent;
	portal->r.ownerNum = ent->s.clientNum;
	if (!ent->client->pers.portals[0]) {
		ent->client->pers.portals[0] = portal;
		ent->client->pers.lastportal = 0;
	} else if (!ent->client->pers.portals[1]) {
		ent->client->pers.portals[1] = portal;
		ent->client->pers.lastportal = 1;
	} else {
		ent->client->pers.lastportal = !ent->client->pers.lastportal;
		G_FreeEntity(ent->client->pers.portals[ent->client->pers.lastportal]);
		ent->client->pers.portals[ent->client->pers.lastportal] = portal;
	}
}
