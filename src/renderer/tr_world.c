/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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

	// now it must be a SF_FACE
	sface = (srfSurfaceFace_t *) surface;

	if(shader->isPortal)
	{
		if(R_CullLocalBox(sface->bounds) == CULL_OUT)
		{
			return qtrue;
		}
	}

	if(shader->cullType == CT_TWO_SIDED)
	{
		return qfalse;
	}

	// face culling
	if(!r_facePlaneCull->integer)
	{
		return qfalse;
	}


	d = DotProduct(tr.orientation.viewOrigin, sface->plane.normal);

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

// *INDENT-OFF*
static qboolean R_LightFace(srfSurfaceFace_t * face, trRefLight_t  * light, byte * cubeSideBits)
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

	if(r_cullShadowPyramidFaces->integer)
	{
		*cubeSideBits = R_CalcLightCubeSideBits(light, face->bounds);
	}
	return qtrue;
}
// *INDENT-ON*

static int R_LightGrid(srfGridMesh_t * grid, trRefLight_t * light, byte * cubeSideBits)
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

	if(r_cullShadowPyramidCurves->integer)
	{
		*cubeSideBits = R_CalcLightCubeSideBits(light, grid->meshBounds);
	}
	return qtrue;
}


static int R_LightTrisurf(srfTriangles_t * tri, trRefLight_t * light, byte * cubeSideBits)
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

	if(r_cullShadowPyramidTriangles->integer)
	{
		*cubeSideBits = R_CalcLightCubeSideBits(light, tri->bounds);
	}
	return qtrue;
}


/*
======================
R_AddInteractionSurface
======================
*/
static void R_AddInteractionSurface(bspSurface_t * surf, trRefLight_t * light)
{
	qboolean        intersects;
	interactionType_t iaType = IA_DEFAULT;
	byte            cubeSideBits = CUBESIDE_CLIPALL;

	// Tr3B - this surface is maybe not in this view but it may still cast a shadow
	// into this view
	if(surf->viewCount != tr.viewCount)
	{
		if(r_shadows->integer <= 2 || light->l.noShadows)
			return;
		else
			iaType = IA_SHADOWONLY;
	}

	if(surf->lightCount == tr.lightCount)
	{
		return;					// already checked this surface
	}
	surf->lightCount = tr.lightCount;

	if(r_vboDynamicLighting->integer && !surf->shader->isSky && !surf->shader->isPortal && !surf->shader->numDeforms)
		return;

	//  skip all surfaces that don't matter for lighting only pass
	if(surf->shader->isSky || (!surf->shader->interactLight && surf->shader->noShadows))
		return;

	if(*surf->data == SF_FACE)
	{
		intersects = R_LightFace((srfSurfaceFace_t *) surf->data, light, &cubeSideBits);
	}
	else if(*surf->data == SF_GRID)
	{
		intersects = R_LightGrid((srfGridMesh_t *) surf->data, light, &cubeSideBits);
	}
	else if(*surf->data == SF_TRIANGLES)
	{
		intersects = R_LightTrisurf((srfTriangles_t *) surf->data, light, &cubeSideBits);
	}
	else
	{
		intersects = qfalse;
	}

	if(intersects)
	{
		R_AddLightInteraction(light, surf->data, surf->shader, cubeSideBits, iaType);

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
static void R_AddWorldSurface(bspSurface_t * surf)
{
	shader_t       *shader;

	if(surf->viewCount == tr.viewCount)
		return;
	surf->viewCount = tr.viewCount;

	shader = surf->shader;

	if(r_vboWorld->integer && !shader->isSky && !shader->isPortal && !shader->numDeforms)
		return;

	// try to cull before lighting or adding
	if(R_CullSurface(surf->data, surf->shader))
	{
		return;
	}

	R_AddDrawSurf(surf->data, surf->shader, surf->lightmapNum);
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
static void R_AddBrushModelSurface(bspSurface_t * surf)
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

	R_AddDrawSurf(surf->data, surf->shader, -1);//surf->lightmapNum);
}

/*
=================
R_AddBSPModelSurfaces
=================
*/
void R_AddBSPModelSurfaces(trRefEntity_t * ent)
{
	bspModel_t     *bspModel;
	model_t        *pModel;
	int             i;
	vec3_t          v;
	vec3_t          transformed;

	pModel = R_GetModelByHandle(ent->e.hModel);
	bspModel = pModel->bsp;

	// copy local bounds
	for(i = 0; i < 3; i++)
	{
		ent->localBounds[0][i] = bspModel->bounds[0][i];
		ent->localBounds[1][i] = bspModel->bounds[1][i];
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

	ent->cull = R_CullLocalBox(bspModel->bounds);
	if(ent->cull == CULL_OUT)
	{
		return;
	}

	// Tr3B: BSP inline models should always use vertex lighting
	R_SetupEntityLighting(&tr.refdef, ent);

	if(r_vboModels->integer && bspModel->numVBOSurfaces)
	{
		int             i;
		srfVBOMesh_t   *vboSurface;

		for(i = 0; i < bspModel->numVBOSurfaces; i++)
		{
			vboSurface = bspModel->vboSurfaces[i];

			R_AddDrawSurf((void *)vboSurface, vboSurface->shader, -1);//vboSurface->lightmapNum);
		}
	}
	else
	{
		for(i = 0; i < bspModel->numSurfaces; i++)
		{
			R_AddBrushModelSurface(bspModel->firstSurface + i);
		}
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
static void R_RecursiveWorldNode(bspNode_t * node, int planeBits)
{
	do
	{
		// if the node wasn't marked as potentially visible, exit
		if(node->visCounts[tr.visIndex] != tr.visCounts[tr.visIndex])
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
						return;	// culled
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
		bspSurface_t   *surf, **mark;

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
		mark = node->markSurfaces;
		c = node->numMarkSurfaces;
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
static void R_RecursiveInteractionNode(bspNode_t * node, trRefLight_t * light, int planeBits)
{
	int             i;
	int             r;

	// if the node wasn't marked as potentially visible, exit
	if(node->visCounts[tr.visIndex] != tr.visCounts[tr.visIndex])
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
		bspSurface_t   *surf, **mark;

		// add the individual surfaces
		mark = node->markSurfaces;
		c = node->numMarkSurfaces;
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
static bspNode_t *R_PointInLeaf(const vec3_t p)
{
	bspNode_t      *node;
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
	bspNode_t      *leaf;
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
=================
BSPSurfaceCompare
compare function for qsort()
=================
*/
static int BSPSurfaceCompare(const void *a, const void *b)
{
	bspSurface_t   *aa, *bb;

	aa = *(bspSurface_t **) a;
	bb = *(bspSurface_t **) b;

	// shader first
	if(aa->shader < bb->shader)
		return -1;

	else if(aa->shader > bb->shader)
		return 1;

	// by lightmap
	if(aa->lightmapNum < bb->lightmapNum)
		return -1;

	else if(aa->lightmapNum > bb->lightmapNum)
		return 1;

	return 0;
}

/*
===============
R_UpdateClusterSurfaces()
===============
*/
static void R_UpdateClusterSurfaces()
{
	int             i, k, l;

	int             numVerts;
	int             numTriangles;

//  static glIndex_t indexes[MAX_MAP_DRAW_INDEXES];
//  static byte     indexes[MAX_MAP_DRAW_INDEXES * sizeof(glIndex_t)];
	glIndex_t      *indexes;
	int             indexesSize;

	shader_t       *shader, *oldShader;
	int             lightmapNum, oldLightmapNum;

	int             numSurfaces;
	bspSurface_t   *surface, *surface2;
	bspSurface_t  **surfacesSorted;

	bspCluster_t   *cluster;

	srfVBOMesh_t   *vboSurf;
	IBO_t          *ibo;

	vec3_t          bounds[2];

	if(tr.visClusters[tr.visIndex] < 0 || tr.visClusters[tr.visIndex] >= tr.world->numClusters)
	{
		// Tr3B: this is not a bug, the super cluster is the last one in the array
		cluster = &tr.world->clusters[tr.world->numClusters];
	}
	else
	{
		cluster = &tr.world->clusters[tr.visClusters[tr.visIndex]];
	}

	tr.world->numClusterVBOSurfaces[tr.visIndex] = 0;

	// count number of static cluster surfaces
	numSurfaces = 0;
	for(k = 0; k < cluster->numMarkSurfaces; k++)
	{
		surface = cluster->markSurfaces[k];
		shader = surface->shader;

		if(shader->isSky)
			continue;

		if(shader->isPortal)
			continue;

		if(shader->numDeforms)
			continue;

		numSurfaces++;
	}

	if(!numSurfaces)
		return;

	// build interaction caches list
	surfacesSorted = ri.Hunk_AllocateTempMemory(numSurfaces * sizeof(surfacesSorted[0]));

	numSurfaces = 0;
	for(k = 0; k < cluster->numMarkSurfaces; k++)
	{
		surface = cluster->markSurfaces[k];
		shader = surface->shader;

		if(shader->isSky)
			continue;

		if(shader->isPortal)
			continue;

		if(shader->numDeforms)
			continue;

		surfacesSorted[numSurfaces] = surface;
		numSurfaces++;
	}

	// sort surfaces by shader
	qsort(surfacesSorted, numSurfaces, sizeof(surfacesSorted), BSPSurfaceCompare);

	shader = oldShader = NULL;
	lightmapNum = oldLightmapNum = -1;

	for(k = 0; k < numSurfaces; k++)
	{
		surface = surfacesSorted[k];
		shader = surface->shader;
		lightmapNum = surface->lightmapNum;

		if(shader != oldShader || (r_precomputedLighting->integer ? lightmapNum != oldLightmapNum : 0))
		{
			oldShader = shader;
			oldLightmapNum = lightmapNum;

			// count vertices and indices
			numVerts = 0;
			numTriangles = 0;

			for(l = k; l < numSurfaces; l++)
			{
				surface2 = surfacesSorted[l];

				if(surface2->shader != shader || surface2->lightmapNum != lightmapNum)
					continue;

				if(*surface2->data == SF_FACE)
				{
					srfSurfaceFace_t *face = (srfSurfaceFace_t *) surface2->data;

					if(face->numVerts)
						numVerts += face->numVerts;

					if(face->numTriangles)
						numTriangles += face->numTriangles;
				}
				else if(*surface2->data == SF_GRID)
				{
					srfGridMesh_t  *grid = (srfGridMesh_t *) surface2->data;

					if(grid->numVerts)
						numVerts += grid->numVerts;

					if(grid->numTriangles)
						numTriangles += grid->numTriangles;
				}
				else if(*surface2->data == SF_TRIANGLES)
				{
					srfTriangles_t *tri = (srfTriangles_t *) surface2->data;

					if(tri->numVerts)
						numVerts += tri->numVerts;

					if(tri->numTriangles)
						numTriangles += tri->numTriangles;
				}
			}

			if(!numVerts || !numTriangles)
				continue;

			ClearBounds(bounds[0], bounds[1]);

			// build triangle indices
			indexesSize = numTriangles * 3 * sizeof(glIndex_t);
			indexes = ri.Hunk_AllocateTempMemory(indexesSize);

			numTriangles = 0;
			for(l = k; l < numSurfaces; l++)
			{
				surface2 = surfacesSorted[l];

				if(surface2->shader != shader || surface2->lightmapNum != lightmapNum)
					continue;

				// set up triangle indices
				if(*surface2->data == SF_FACE)
				{
					srfSurfaceFace_t *srf = (srfSurfaceFace_t *) surface2->data;

					if(srf->numTriangles)
					{
						srfTriangle_t  *tri;

						for(i = 0, tri = tr.world->triangles + srf->firstTriangle; i < srf->numTriangles; i++, tri++)
						{
							indexes[numTriangles * 3 + i * 3 + 0] = tri->indexes[0];
							indexes[numTriangles * 3 + i * 3 + 1] = tri->indexes[1];
							indexes[numTriangles * 3 + i * 3 + 2] = tri->indexes[2];
						}

						numTriangles += srf->numTriangles;
						BoundsAdd(bounds[0], bounds[1], srf->bounds[0], srf->bounds[1]);
					}
				}
				else if(*surface2->data == SF_GRID)
				{
					srfGridMesh_t  *srf = (srfGridMesh_t *) surface2->data;

					if(srf->numTriangles)
					{
						srfTriangle_t  *tri;

						for(i = 0, tri = tr.world->triangles + srf->firstTriangle; i < srf->numTriangles; i++, tri++)
						{
							indexes[numTriangles * 3 + i * 3 + 0] = tri->indexes[0];
							indexes[numTriangles * 3 + i * 3 + 1] = tri->indexes[1];
							indexes[numTriangles * 3 + i * 3 + 2] = tri->indexes[2];
						}

						numTriangles += srf->numTriangles;
						BoundsAdd(bounds[0], bounds[1], srf->meshBounds[0], srf->meshBounds[1]);
					}
				}
				else if(*surface2->data == SF_TRIANGLES)
				{
					srfTriangles_t *srf = (srfTriangles_t *) surface2->data;

					if(srf->numTriangles)
					{
						srfTriangle_t  *tri;

						for(i = 0, tri = tr.world->triangles + srf->firstTriangle; i < srf->numTriangles; i++, tri++)
						{
							indexes[numTriangles * 3 + i * 3 + 0] = tri->indexes[0];
							indexes[numTriangles * 3 + i * 3 + 1] = tri->indexes[1];
							indexes[numTriangles * 3 + i * 3 + 2] = tri->indexes[2];
						}

						numTriangles += srf->numTriangles;
						BoundsAdd(bounds[0], bounds[1], srf->bounds[0], srf->bounds[1]);
					}
				}
			}

			if(tr.world->numClusterVBOSurfaces[tr.visIndex] < tr.world->clusterVBOSurfaces[tr.visIndex].currentElements)
			{
				vboSurf =
					(srfVBOMesh_t *) Com_GrowListElement(&tr.world->clusterVBOSurfaces[tr.visIndex],
														 tr.world->numClusterVBOSurfaces[tr.visIndex]);
				ibo = vboSurf->ibo;

				/*
				   if(ibo->indexesVBO)
				   {
				   qglDeleteBuffersARB(1, &ibo->indexesVBO);
				   ibo->indexesVBO = 0;
				   }
				 */

				//Com_Dealloc(ibo);
				//Com_Dealloc(vboSurf);
			}
			else
			{
				vboSurf = ri.Hunk_Alloc(sizeof(*vboSurf), h_low);
				vboSurf->surfaceType = SF_VBO_MESH;

				vboSurf->vbo = tr.world->vbo;
				vboSurf->ibo = ibo = ri.Hunk_Alloc(sizeof(*ibo), h_low);

				qglGenBuffersARB(1, &ibo->indexesVBO);

				Com_AddToGrowList(&tr.world->clusterVBOSurfaces[tr.visIndex], vboSurf);
			}

			// update surface properties
			vboSurf->numIndexes = numTriangles * 3;
			vboSurf->numVerts = numVerts;

			vboSurf->shader = shader;
			vboSurf->lightmapNum = lightmapNum;

			VectorCopy(bounds[0], vboSurf->bounds[0]);
			VectorCopy(bounds[1], vboSurf->bounds[1]);

			// make sure the render thread is stopped
			R_SyncRenderThread();

			// update IBO
			Q_strncpyz(ibo->name,
					   va("staticWorldMesh_IBO_visIndex%i_surface%i", tr.visIndex, tr.world->numClusterVBOSurfaces[tr.visIndex]),
					   sizeof(ibo->name));
			ibo->indexesSize = indexesSize;

			R_BindIBO(ibo);
			qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexesSize, indexes, GL_DYNAMIC_DRAW_ARB);
			R_BindNullIBO();

			GL_CheckErrors();

			ri.Hunk_FreeTempMemory(indexes);

			tr.world->numClusterVBOSurfaces[tr.visIndex]++;
		}
	}

	ri.Hunk_FreeTempMemory(surfacesSorted);

#if 0
	if(r_showcluster->integer)
	{
		ri.Printf(PRINT_ALL, "%i VBO surfaces created for cluster %i and vis index %i\n",
				  tr.world->numClusterVBOSurfaces[tr.visIndex], cluster - tr.world->clusters, tr.visIndex);
	}
#endif
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
	bspNode_t      *leaf, *parent;
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

	for(i = 0; i < MAX_VISCOUNTS; i++)
	{
		if(tr.visClusters[i] == cluster)
		{
			//tr.visIndex = i;
			break;
		}
	}
	// if r_showcluster was just turned on, remark everything
	if(i != MAX_VISCOUNTS && !tr.refdef.areamaskModified && !r_showcluster->modified)
	{
		if(tr.visClusters[i] != tr.visClusters[tr.visIndex] && r_showcluster->integer)
		{
			ri.Printf(PRINT_ALL, "found cluster:%i  area:%i  index:%i\n", cluster, leaf->area, i);
		}
		tr.visIndex = i;
		return;
	}

	tr.visIndex = (tr.visIndex + 1) % MAX_VISCOUNTS;
	tr.visCounts[tr.visIndex]++;
	tr.visClusters[tr.visIndex] = cluster;

	if(r_showcluster->modified || r_showcluster->integer)
	{
		r_showcluster->modified = qfalse;
		if(r_showcluster->integer)
		{
			ri.Printf(PRINT_ALL, "update cluster:%i  area:%i  index:%i\n", cluster, leaf->area, tr.visIndex);
		}
	}

	R_UpdateClusterSurfaces();

	if(r_novis->integer || tr.visClusters[tr.visIndex] == -1)
	{
		for(i = 0; i < tr.world->numnodes; i++)
		{
			if(tr.world->nodes[i].contents != CONTENTS_SOLID)
			{
				tr.world->nodes[i].visCounts[tr.visIndex] = tr.visCounts[tr.visIndex];
			}
		}
		return;
	}

	vis = R_ClusterPVS(tr.visClusters[tr.visIndex]);

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
			if(parent->visCounts[tr.visIndex] == tr.visCounts[tr.visIndex])
				break;
			parent->visCounts[tr.visIndex] = tr.visCounts[tr.visIndex];
			parent = parent->parent;
		} while(parent);
	}
}


/*
=============
R_SetFarClip
=============
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

		VectorSubtract(v, tr.viewParms.orientation.origin, vecTo);

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

	// update visbounds and add surfaces that weren't cached with VBOs
	R_RecursiveWorldNode(tr.world->nodes, FRUSTUM_CLIPALL);

	// dynamically compute far clip plane distance for sky
	R_SetFarClip();

	if(r_vboWorld->integer)
	{
		int             j, i;
		srfVBOMesh_t   *srf;
		shader_t       *shader;
		cplane_t       *frust;
		int             r;

		for(j = 0; j < tr.world->numClusterVBOSurfaces[tr.visIndex]; j++)
		{
			srf = (srfVBOMesh_t *) Com_GrowListElement(&tr.world->clusterVBOSurfaces[tr.visIndex], j);
			shader = srf->shader;

			for(i = 0; i < FRUSTUM_PLANES; i++)
			{
				frust = &tr.viewParms.frustum[i];

				r = BoxOnPlaneSide(srf->bounds[0], srf->bounds[1], frust);

				if(r == 2)
				{
					// completely outside frustum
					continue;
				}
			}

			R_AddDrawSurf((void *)srf, shader, srf->lightmapNum);
		}
	}
}

/*
=============
R_AddWorldInteractions
=============
*/
void R_AddWorldInteractions(trRefLight_t * light)
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

	if(r_vboDynamicLighting->integer)
	{
		int             j;
		srfVBOMesh_t   *srf;
		shader_t       *shader;
		qboolean        intersects;
		interactionType_t iaType = IA_DEFAULT;
		byte            cubeSideBits = CUBESIDE_CLIPALL;

		for(j = 0; j < tr.world->numClusterVBOSurfaces[tr.visIndex]; j++)
		{
			srf = (srfVBOMesh_t *) Com_GrowListElement(&tr.world->clusterVBOSurfaces[tr.visIndex], j);
			shader = srf->shader;

			//  skip all surfaces that don't matter for lighting only pass
			if(shader->isSky || (!shader->interactLight && shader->noShadows))
				continue;

			intersects = qtrue;

			// do a quick AABB cull
			if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], srf->bounds[0], srf->bounds[1]))
				intersects = qfalse;

			// FIXME? do a more expensive and precise light frustum cull
			if(!r_noLightFrustums->integer)
			{
				if(R_CullLightWorldBounds(light, srf->bounds) == CULL_OUT)
				{
					intersects = qfalse;
				}
			}

			// FIXME?
			if(r_cullShadowPyramidFaces->integer)
			{
				cubeSideBits = R_CalcLightCubeSideBits(light, srf->bounds);
			}

			if(intersects)
			{
				R_AddLightInteraction(light, (void *)srf, srf->shader, cubeSideBits, iaType);

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
	}
}

/*
=============
R_AddPrecachedWorldInteractions
=============
*/
void R_AddPrecachedWorldInteractions(trRefLight_t * light)
{
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

	if(r_vboShadows->integer || r_vboLighting->integer)
	{
		interactionVBO_t *iaVBO;
		srfVBOMesh_t   *srf;
		srfVBOShadowVolume_t *shadowSrf;
		shader_t       *shader;

		if(r_shadows->integer == 3)
		{
			for(iaVBO = light->firstInteractionVBO; iaVBO; iaVBO = iaVBO->next)
			{
				if(!iaVBO->vboShadowVolume)
					continue;

				shadowSrf = iaVBO->vboShadowVolume;

				R_AddLightInteraction(light, (void *)shadowSrf, tr.defaultShader, CUBESIDE_CLIPALL, IA_SHADOWONLY);
			}

			for(iaVBO = light->firstInteractionVBO; iaVBO; iaVBO = iaVBO->next)
			{
				if(!iaVBO->vboLightMesh)
					continue;

				srf = iaVBO->vboLightMesh;
				shader = iaVBO->shader;

				R_AddLightInteraction(light, (void *)srf, shader, CUBESIDE_CLIPALL, IA_LIGHTONLY);
			}
		}
		else
		{
			// this can be shadow mapping or shadowless lighting
			for(iaVBO = light->firstInteractionVBO; iaVBO; iaVBO = iaVBO->next)
			{
				if(!iaVBO->vboLightMesh)
					continue;

				srf = iaVBO->vboLightMesh;
				shader = iaVBO->shader;

				switch (light->l.rlType)
				{
					case RL_OMNI:
						R_AddLightInteraction(light, (void *)srf, shader, CUBESIDE_CLIPALL, IA_LIGHTONLY);
						break;

					default:
					case RL_PROJ:
						R_AddLightInteraction(light, (void *)srf, shader, CUBESIDE_CLIPALL, IA_DEFAULT);
						break;
				}
			}

			// add meshes for shadowmap generation if any
			for(iaVBO = light->firstInteractionVBO; iaVBO; iaVBO = iaVBO->next)
			{
				if(!iaVBO->vboShadowMesh)
					continue;

				srf = iaVBO->vboShadowMesh;
				shader = iaVBO->shader;

				R_AddLightInteraction(light, (void *)srf, shader, iaVBO->cubeSideBits, IA_SHADOWONLY);
			}
		}
	}
	else
	{
		interactionCache_t *iaCache;
		bspSurface_t   *surface;

		for(iaCache = light->firstInteractionCache; iaCache; iaCache = iaCache->next)
		{
			if(iaCache->redundant)
				continue;

			surface = iaCache->surface;

			// Tr3B - this surface is maybe not in this view but it may still cast a shadow
			// into this view
			if(surface->viewCount != tr.viewCount)
			{
				if(r_shadows->integer <= 3 || light->l.noShadows)
					continue;
				else
					iaType = IA_SHADOWONLY;
			}
			else
			{
				iaType = iaCache->type;
			}

			R_AddLightInteraction(light, surface->data, surface->shader, iaCache->cubeSideBits, iaType);
		}
	}
}
