/*
===========================================================================
Copyright (C) 2009 Amanieu d'Antras

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

extern "C" {
#include "cm_local.h"
}

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btGeometryUtil.h>

// Maximum number of physics objects that can be in the world at once
#define MAX_PHYSICS_OBJECTS 0x8000

btAxisSweep3 *broadphase;
btDefaultCollisionConfiguration *collisionConfiguration;
btCollisionDispatcher *dispatcher;
btSequentialImpulseConstraintSolver *solver;
btDiscreteDynamicsWorld *dynamicsWorld;
btAlignedObjectArray<btCollisionShape *> collisionShapes;

bool bulletInit;

/*
================
CM_LoadWorldShape

TODO: Build an optimized triangle mesh for the world, and save it to disk for faster loading
TODO: Store surfaceflags and brush contents somewhere
TODO: Add patches and trisoups
TODO: Load submodels
TODO: Maybe share this data with the renderer?
================
*/
static void CM_LoadWorldShape(void)
{
	for (int i = 0; i < cm.numLeafBrushes; i++) {
		btAlignedObjectArray<btVector3> planeEquations;
		btAlignedObjectArray<btVector3> vertices;

		// Build list of plane equations
		cbrush_t *brush = cm.brushes + cm.leafbrushes[i];
		if (!(brush->contents & CONTENTS_SOLID))
			continue;
		for (int j = 0; j < brush->numsides; j++) {
			btVector3 planeEq = btVector4(brush->sides[j].plane->normal[0], brush->sides[j].plane->normal[1],
			                              brush->sides[j].plane->normal[2], -brush->sides[j].plane->dist);
			planeEquations.push_back(planeEq);
		}

		// Convert plane equations to vertices and make a convex shape
		btGeometryUtil::getVerticesFromPlaneEquations(planeEquations, vertices);
		btCollisionShape *shape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());
		collisionShapes.push_back(shape);

		// Add brush to world
		btDefaultMotionState *myMotionState = new btDefaultMotionState();
		btRigidBody::btRigidBodyConstructionInfo cInfo(0, myMotionState, shape);
		btRigidBody *body = new btRigidBody(cInfo);
		dynamicsWorld->addRigidBody(body);
	}
}

/*
================
CM_InitBullet
================
*/
void CM_InitBullet(void)
{
	// Using world model AABB bounds
	btVector3 worldAabbMin(cm.cmodels[0].mins[0], cm.cmodels[0].mins[1], cm.cmodels[0].mins[2]);
	btVector3 worldAabbMax(cm.cmodels[0].maxs[0], cm.cmodels[0].maxs[1], cm.cmodels[0].maxs[2]);

	broadphase = new btAxisSweep3(worldAabbMin, worldAabbMax, MAX_PHYSICS_OBJECTS);
	collisionConfiguration = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collisionConfiguration);
	solver = new btSequentialImpulseConstraintSolver;
	dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);

	CM_LoadWorldShape();

	bulletInit = true;
}

/*
================
CM_ShutdownBullet
================
*/
void CM_ShutdownBullet(void)
{
	if (!bulletInit)
		return;

	// Delete all rigid bodies in the world
	for (int i = 0; i < dynamicsWorld->getNumCollisionObjects(); i++) {
		btCollisionObject *obj = dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody *body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
			delete body->getMotionState();
		dynamicsWorld->removeCollisionObject(obj);
		delete obj;
	}

	// Delete all collision shapes
	for (int i = 0; i < collisionShapes.size(); i++)
		delete collisionShapes[i];

	// Delete the collision world
    delete dynamicsWorld;
    delete solver;
    delete dispatcher;
    delete collisionConfiguration;
    delete broadphase;

	bulletInit = false;
}
