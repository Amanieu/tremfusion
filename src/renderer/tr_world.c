/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_world.c
#include "tr_local.h"

/*
=================
R_CullTriSurf

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static qboolean R_CullTriSurf(srfTriangles_t * cv)
{
	int             boxCull;

	boxCull = R_CullLocalBox(cv->bounds);

	if(boxCull == CULL_OUT)
	{
		return qtrue;
	}
	return qfalse;
}

/*
=================
R_CullGrid

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static qboolean R_CullGrid(srfGridMesh_t * cv)
{
	int             boxCull;
	int             sphereCull;

	if(r_nocurves->integer)
	{
		return qtrue;
	}

	if(tr.currentEntity != &tr.worldEntity)
	{
		sphereCull = R_CullLocalPointAndRadius(cv->localOrigin, cv->meshRadius);
	}
	else
	{
		sphereCull = R_CullPointAndRadius(cv->localOrigin, cv->meshRadius);
	}
	boxCull = CULL_OUT;

	// check for trivial reject
	if(sphereCull == CULL_OUT)
	{
		tr.pc.c_sphere_cull_patch_out++;
		return qtrue;
	}
	// check bounding box if necessary
	else if(sphereCull == CULL_CLIP)
	{
		tr.pc.c_sphere_cull_patch_clip++;

		boxCull = R_CullLocalBox(cv->meshBounds);

		if(boxCull == CULL_OUT)
		{
			tr.pc.c_box_cull_patch_out++;
			return qtrue;
		}
		else if(boxCull == CULL_IN)
		{
			tr.pc.c_box_cull_patch_in++;
		}
		else
		{
			tr.pc.c_box_cull_patch_clip++;
		}
	}
	else
	{
		tr.pc.c_sphere_cull_patch_in++;
	}

	return qfalse;
}


/*
================
R_CullSurface

Tries to back face cull surfaces before they are lighted or
added to the sorting list.

This will also allow mirrors on both sides of a model without recursion.
================
*/
static qboolean R_CullSurface(surfaceType_t * surface, shader_t * shader)
{
	srfSurfaceFace_t *sface;
	float           d;

	if(r_nocull->integer)
	{
		return qfalse;
	}

	if(*surface == SF_GRID)
	{
		return R_CullGrid((srfGridMesh_t *) surface);
	}

	if(*surface == SF_TRIANGLES)
	{
		return R_CullTriSurf((srfTriangles_t *) surface);
	}

	if(*surface != SF_FACE)
	{
		return qfalse;
	}

	if(shader->cullType == CT_TWO_SIDED)
	{
		return qfalse;
	}

	// face culling
	sface = (srfSurfaceFace_t *) surface;
	d = DotProduct(tr.or.viewOrigin, sface->plane.normal);

	// don't cull exactly on the plane, because there are levels of rounding
	// through the BSP, ICD, and hardware that may cause pixel gaps if an
	// epsilon isn't allowed here 
	if(shader->cullType == CT_FRONT_SIDED)
	{
		if(d < sface->plane.dist - 8)
		{
			return qtrue;
		}
	}
	else
	{
		if(d > sface->plane.dist + 8)
		{
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean R_DlightFace(srfSurfaceFace_t * face, trRefDlight_t  * light)
{
	// do a quick AABB cull
	if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], face->bounds[0], face->bounds[1]))
	{
		return qfalse;
	}

	// do a more expensive and precise light frustum cull
	if(!r_noLightFrustums->integer)
	{
		if(R_CullLightWorldBounds(light, face->bounds) == CULL_OUT)
		{
			return qfalse;
		}
	}
	
	return qtrue;
}

static int R_DlightGrid(srfGridMesh_t * grid, trRefDlight_t * light)
{
	// do a quick AABB cull
	if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], grid->meshBounds[0], grid->meshBounds[1]))
	{
		return qfalse;
	}

	// do a more expensive and precise light frustum cull
	if(!r_noLightFrustums->integer)
	{
		if(R_CullLightWorldBounds(light, grid->meshBounds) == CULL_OUT)
		{
			return qfalse;
		}
	}
	return qtrue;
}


static int R_DlightTrisurf(srfTriangles_t * tri, trRefDlight_t * light)
{
	// do a quick AABB cull
	if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], tri->bounds[0], tri->bounds[1]))
	{
		return qfalse;
	}

	// do a more expensive and precise light frustum cull
	if(!r_noLightFrustums->integer)
	{
		if(R_CullLightWorldBounds(light, tri->bounds) == CULL_OUT)
		{
			return qfalse;
		}
	}
	return qtrue;
}


/*
======================
R_AddInteractionSurface
======================
*/
static void R_AddInteractionSurface(msurface_t * surf, trRefDlight_t * light)
{
	qboolean        intersects;
	interactionType_t iaType = IA_DEFAULT;
	
	// Tr3B - this surface is maybe not in this view but it may still cast a shadow
	// into this view
	if(surf->viewCount != tr.viewCount)
	{
		if(r_shadows->integer <= 2 || light->l.noShadows)
			return;
		else
			iaType = IA_SHADOWONLY;
	}
	else
	{
		if(r_shadows->integer <= 2)
			iaType = IA_LIGHTONLY;	
	}
	
	if(surf->lightCount == tr.lightCount)
	{
		return;					// already checked this surface
	}
	surf->lightCount = tr.lightCount;
	
	//  skip all surfaces that don't matter for lighting only pass
	if(surf->shader->isSky || (!surf->shader->interactLight && surf->shader->noShadows))
		return;

	if(*surf->data == SF_FACE)
	{
		intersects = R_DlightFace((srfSurfaceFace_t *) surf->data, light);
	}
	else if(*surf->data == SF_GRID)
	{
		intersects = R_DlightGrid((srfGridMesh_t *) surf->data, light);
	}
	else if(*surf->data == SF_TRIANGLES)
	{
		intersects = R_DlightTrisurf((srfTriangles_t *) surf->data, light);
	}
	else
	{
		intersects = qfalse;	
	}
	
	if(intersects)
	{
		R_AddDlightInteraction(light, surf->data, surf->shader, 0, NULL, 0, NULL, iaType);
		
		if(light->isStatic)
			tr.pc.c_slightSurfaces++;
		else
			tr.pc.c_dlightSurfaces++;
	}
	else
	{
		if(!light->isStatic)
			tr.pc.c_dlightSurfacesCulled++;	
	}
}

/*
======================
R_AddWorldSurface
======================
*/
static void R_AddWorldSurface(msurface_t * surf)
{
	if(surf->viewCount == tr.viewCount)
	{
		return;					// already in this view
	}
	surf->viewCount = tr.viewCount;
	
	// FIXME: bmodel fog?

	// try to cull before dlighting or adding
	if(R_CullSurface(surf->data, surf->shader))
	{
		return;
	}

	R_AddDrawSurf(surf->data, surf->shader, surf->lightmapNum, surf->fogIndex);
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
======================
R_AddBrushModelSurface
======================
*/
static void R_AddBrushModelSurface(msurface_t * surf, int fogIndex)
{
	if(surf->viewCount == tr.viewCount)
	{
		return;					// already in this view
	}
	surf->viewCount = tr.viewCount;

	// try to cull before lighting or adding
	if(R_CullSurface(surf->data, surf->shader))
	{
		return;
	}

	R_AddDrawSurf(surf->data, surf->shader, surf->lightmapNum, fogIndex);
}

/*
=================
R_AddBrushModelSurfaces
=================
*/
void R_AddBrushModelSurfaces(trRefEntity_t * ent)
{
	bmodel_t       *bModel;
	model_t        *pModel;
	int             i;
	vec3_t          v;
	vec3_t          transformed;
	int             fogNum = 0;

	pModel = R_GetModelByHandle(ent->e.hModel);
	bModel = pModel->bmodel;
	
	// copy local bounds
	for(i = 0; i < 3; i++)
	{
		ent->localBounds[0][i] = bModel->bounds[0][i];
		ent->localBounds[1][i] = bModel->bounds[1][i];
	}
	
	// setup world bounds for intersection tests
	ClearBounds(ent->worldBounds[0], ent->worldBounds[1]);
		
	for(i = 0; i < 8; i++)
	{
		v[0] = ent->localBounds[i & 1][0];
		v[1] = ent->localBounds[(i >> 1) & 1][1];
		v[2] = ent->localBounds[(i >> 2) & 1][2];
	
		// transform local bounds vertices into world space
		R_LocalPointToWorld(v, transformed);
			
		AddPointToBounds(transformed, ent->worldBounds[0], ent->worldBounds[1]);
	}

	ent->cull = R_CullLocalBox(bModel->bounds);
	if(ent->cull == CULL_OUT)
	{
		return;
	}
	
	fogNum = R_FogWorldBox(ent->worldBounds);

	for(i = 0; i < bModel->numSurfaces; i++)
	{
		R_AddBrushModelSurface(bModel->firstSurface + i, fogNum);
	}
}


/*
=============================================================

	WORLD MODEL

=============================================================
*/


/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode(mnode_t * node, int planeBits)
{
	do
	{
		// if the node wasn't marked as potentially visible, exit
		if(node->visCount != tr.visCount)
		{
			return;
		}

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?
		if(!r_nocull->integer)
		{
			int             i;
			int             r;

			for(i = 0; i < FRUSTUM_PLANES; i++)
			{
				if(planeBits & (1 << i))
				{
					r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[i]);
					if(r == 2)
					{
						return;		// culled
					}
					if(r == 1)
					{
						planeBits &= ~(1 << i);	// all descendants will also be in front
					}
				}
			}
		}

		if(node->contents != -1)
		{
			break;
		}

		// recurse down the children, front side first
		R_RecursiveWorldNode(node->children[0], planeBits);

		// tail recurse
		node = node->children[1];
	} while(1);

	{
		// leaf node, so add mark surfaces
		int             c;
		msurface_t     *surf, **mark;

		tr.pc.c_leafs++;

		// add to z buffer bounds
		if(node->mins[0] < tr.viewParms.visBounds[0][0])
		{
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if(node->mins[1] < tr.viewParms.visBounds[0][1])
		{
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if(node->mins[2] < tr.viewParms.visBounds[0][2])
		{
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if(node->maxs[0] > tr.viewParms.visBounds[1][0])
		{
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if(node->maxs[1] > tr.viewParms.visBounds[1][1])
		{
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if(node->maxs[2] > tr.viewParms.visBounds[1][2])
		{
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while(c--)
		{
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddWorldSurface(surf);
			mark++;
		}
	}
}

/*
================
R_RecursiveInteractionNode
================
*/
static void R_RecursiveInteractionNode(mnode_t * node, trRefDlight_t * light, int planeBits)
{
	int             i;
	int             r;
	
	// if the node wasn't marked as potentially visible, exit
	if(node->visCount != tr.visCount)
	{
		return;
	}
	
	// light already hit node
	if(node->lightCount == tr.lightCount)
	{
		return;
	}
	node->lightCount = tr.lightCount;

	// if the bounding volume is outside the frustum, nothing
	// inside can be visible OPTIMIZE: don't do this all the way to leafs?

	// Tr3B - even surfaces that belong to nodes that are outside of the view frustum
	// can cast shadows into the view frustum
	if(!r_nocull->integer && r_shadows->integer <= 2)
	{
		for(i = 0; i < FRUSTUM_PLANES; i++)
		{
			if(planeBits & (1 << i))
			{
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[i]);
				
				if(r == 2)
				{
					return;		// culled
				}
				
				if(r == 1)
				{
					planeBits &= ~(1 << i);	// all descendants will also be in front
				}
			}
		}
	}

	if(node->contents != -1)
	{
		// leaf node, so add mark surfaces
		int             c;
		msurface_t     *surf, **mark;

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while(c--)
		{
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddInteractionSurface(surf, light);
			mark++;
		}
		return;
	}

	// node is just a decision point, so go down both sides
	// since we don't care about sort orders, just go positive to negative
	r = BoxOnPlaneSide(light->worldBounds[0], light->worldBounds[1], node->plane);
	
	switch (r)
	{
		case 1:
			R_RecursiveInteractionNode(node->children[0], light, planeBits);
			break;
			
		case 2:
			R_RecursiveInteractionNode(node->children[1], light, planeBits);
			break;
		
		case 3:
		default:
			// recurse down the children, front side first
			R_RecursiveInteractionNode(node->children[0], light, planeBits);
			R_RecursiveInteractionNode(node->children[1], light, planeBits);
			break;
	}
}


/*
===============
R_PointInLeaf
===============
*/
static mnode_t *R_PointInLeaf(const vec3_t p)
{
	mnode_t        *node;
	float           d;
	cplane_t       *plane;

	if(!tr.world)
	{
		ri.Error(ERR_DROP, "R_PointInLeaf: bad model");
	}

	node = tr.world->nodes;
	while(1)
	{
		if(node->contents != -1)
		{
			break;
		}
		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;
		if(d > 0)
		{
			node = node->children[0];
		}
		else
		{
			node = node->children[1];
		}
	}

	return node;
}

/*
==============
R_ClusterPVS
==============
*/
static const byte *R_ClusterPVS(int cluster)
{
	if(!tr.world || !tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters)
	{
		return tr.world->novis;
	}

	return tr.world->vis + cluster * tr.world->clusterBytes;
}

/*
=================
R_inPVS
=================
*/
qboolean R_inPVS(const vec3_t p1, const vec3_t p2)
{
	mnode_t        *leaf;
	byte           *vis;

	leaf = R_PointInLeaf(p1);
	vis = CM_ClusterPVS(leaf->cluster);
	leaf = R_PointInLeaf(p2);

	if(!(vis[leaf->cluster >> 3] & (1 << (leaf->cluster & 7))))
	{
		return qfalse;
	}
	return qtrue;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void R_MarkLeaves(void)
{
	const byte     *vis;
	mnode_t        *leaf, *parent;
	int             i;
	int             cluster;

	// lockpvs lets designers walk around to determine the
	// extent of the current pvs
	if(r_lockpvs->integer)
	{
		return;
	}

	// current viewcluster
	leaf = R_PointInLeaf(tr.viewParms.pvsOrigin);
	cluster = leaf->cluster;

	// if the cluster is the same and the area visibility matrix
	// hasn't changed, we don't need to mark everything again

	// if r_showcluster was just turned on, remark everything
	if(tr.viewCluster == cluster && !tr.refdef.areamaskModified && !r_showcluster->modified)
	{
		return;
	}

	if(r_showcluster->modified || r_showcluster->integer)
	{
		r_showcluster->modified = qfalse;
		if(r_showcluster->integer)
		{
			ri.Printf(PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area);
		}
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if(r_novis->integer || tr.viewCluster == -1)
	{
		for(i = 0; i < tr.world->numnodes; i++)
		{
			if(tr.world->nodes[i].contents != CONTENTS_SOLID)
			{
				tr.world->nodes[i].visCount = tr.visCount;
			}
		}
		return;
	}

	vis = R_ClusterPVS(tr.viewCluster);

	for(i = 0, leaf = tr.world->nodes; i < tr.world->numnodes; i++, leaf++)
	{
		if(tr.world->vis)
		{
			cluster = leaf->cluster;

			if(cluster >= 0 && cluster < tr.world->numClusters)
			{
				// check general pvs
				if(!(vis[cluster >> 3] & (1 << (cluster & 7))))
				{
					continue;
				}
			}
		}

		// check for door connection
		if((tr.refdef.areamask[leaf->area >> 3] & (1 << (leaf->area & 7))))
		{
			continue;			// not visible
		}

		parent = leaf;
		do
		{
			if(parent->visCount == tr.visCount)
				break;
			parent->visCount = tr.visCount;
			parent = parent->parent;
		} while(parent);
	}
}


/*
** SetFarClip
*/
static void R_SetFarClip(void)
{
	float           farthestCornerDistance = 0;
	int             i;

	// if not rendering the world (icons, menus, etc)
	// set a 2k far clip plane
	if(tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		tr.viewParms.skyFar = 2048;
		return;
	}

	// set far clipping planes dynamically
	farthestCornerDistance = 0;
	for(i = 0; i < 8; i++)
	{
		vec3_t          v;
		vec3_t          vecTo;
		float           distance;

		if(i & 1)
		{
			v[0] = tr.viewParms.visBounds[0][0];
		}
		else
		{
			v[0] = tr.viewParms.visBounds[1][0];
		}

		if(i & 2)
		{
			v[1] = tr.viewParms.visBounds[0][1];
		}
		else
		{
			v[1] = tr.viewParms.visBounds[1][1];
		}

		if(i & 4)
		{
			v[2] = tr.viewParms.visBounds[0][2];
		}
		else
		{
			v[2] = tr.viewParms.visBounds[1][2];
		}

		VectorSubtract(v, tr.viewParms.or.origin, vecTo);

		distance = vecTo[0] * vecTo[0] + vecTo[1] * vecTo[1] + vecTo[2] * vecTo[2];

		if(distance > farthestCornerDistance)
		{
			farthestCornerDistance = distance;
		}
	}
	tr.viewParms.skyFar = sqrt(farthestCornerDistance);
}

/*
=============
R_AddWorldSurfaces
=============
*/
void R_AddWorldSurfaces(void)
{
	if(!r_drawworld->integer)
	{
		return;
	}

	if(tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	tr.currentEntity = &tr.worldEntity;

	// determine which leaves are in the PVS / areamask
	R_MarkLeaves();

	// clear out the visible min/max
	ClearBounds(tr.viewParms.visBounds[0], tr.viewParms.visBounds[1]);

	// perform frustum culling and add all the potentially visible surfaces
	R_RecursiveWorldNode(tr.world->nodes, FRUSTUM_CLIPALL);
	
	// dynamically compute far clip plane distance for sky
	R_SetFarClip();
}

/*
=============
R_AddWorldInteractions
=============
*/
void R_AddWorldInteractions(trRefDlight_t * light)
{
	if(!r_drawworld->integer)
	{
		return;
	}

	if(tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	tr.currentEntity = &tr.worldEntity;

	// perform frustum culling and add all the potentially visible surfaces
	tr.lightCount++;
	R_RecursiveInteractionNode(tr.world->nodes, light, FRUSTUM_CLIPALL);
}

/*
=============
R_AddPrecachedWorldInteractions
=============
*/
void R_AddPrecachedWorldInteractions(trRefDlight_t * light)
{
	interactionCache_t  *iaCache;
	msurface_t     *surface;
	interactionType_t iaType = IA_DEFAULT;
	
	if(!r_drawworld->integer)
	{
		return;
	}

	if(tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}
	
	if(!light->firstInteractionCache)
	{
		// this light has no interactions precached
		return;
	}

	tr.currentEntity = &tr.worldEntity;
	
	for(iaCache = light->firstInteractionCache; iaCache; iaCache = iaCache->next)
	{
		surface = iaCache->surface;
		
		// Tr3B - this surface is maybe not in this view but it may still cast a shadow
		// into this view
		if(surface->viewCount != tr.viewCount)
		{
			if(r_shadows->integer <= 2 || light->l.noShadows)
				continue;
			else
				iaType = IA_SHADOWONLY;
		}
		else
		{
			if(r_shadows->integer <= 2)
				iaType = IA_LIGHTONLY;
			else
				iaType = IA_DEFAULT;
		}
		
		R_AddDlightInteraction(light, surface->data, surface->shader, iaCache->numLightIndexes, iaCache->lightIndexes, iaCache->numShadowIndexes, iaCache->shadowIndexes, iaType);
	}
}


/*
===============
R_ShutdownVBOs
===============
*/
void R_ShutdownVBOs()
{
	int             i;
	msurface_t     *surface;
	
	if(!tr.world || (tr.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		return;
	}
	
	if(!glConfig2.vertexBufferObjectAvailable)
	{
		return;
	}

	for(i = 0, surface = &tr.world->surfaces[0]; i < tr.world->numsurfaces; i++, surface++)
	{
		if(*surface->data == SF_FACE)
		{
			srfSurfaceFace_t *face = (srfSurfaceFace_t *) surface->data;
			
			if(face->indexesVBO)
			{
				qglDeleteBuffersARB(1, &face->indexesVBO);
			}
			
			if(face->vertsVBO)
			{
				qglDeleteBuffersARB(1, &face->vertsVBO);
			}
		}
		else if(*surface->data == SF_GRID)
		{
			srfGridMesh_t  *grid = (srfGridMesh_t *) surface->data;
			
			if(grid->indexesVBO)
			{
				qglDeleteBuffersARB(1, &grid->indexesVBO);
			}
			
			if(grid->vertsVBO)
			{
				qglDeleteBuffersARB(1, &grid->vertsVBO);
			}
		}
		else if(*surface->data == SF_TRIANGLES)
		{
			srfTriangles_t  *tri = (srfTriangles_t *) surface->data;
			
			if(tri->indexesVBO)
			{
				qglDeleteBuffersARB(1, &tri->indexesVBO);
			}
			
			if(tri->vertsVBO)
			{
				qglDeleteBuffersARB(1, &tri->vertsVBO);
			}
		}
	}
}
