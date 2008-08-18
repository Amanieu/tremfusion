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
// tr_light.c

#include "tr_local.h"

/*
===============
R_TransformDlights

Transforms the origins of an array of dlights.
Used by both the front end (for DlightBmodel) and
the back end (before doing the lighting calculation)
===============
*/
void R_TransformDlights(int count, trRefDlight_t * dl, orientationr_t * or)
{
	int             i;
	vec3_t          temp;

	for(i = 0; i < count; i++, dl++)
	{
		VectorSubtract(dl->l.origin, or->origin, temp);
		dl->transformed[0] = DotProduct(temp, or->axis[0]);
		dl->transformed[1] = DotProduct(temp, or->axis[1]);
		dl->transformed[2] = DotProduct(temp, or->axis[2]);
	}
}

/*
=============
R_AddBrushModelInteractions

Determine which dynamic lights may effect this bmodel
=============
*/
void R_AddBrushModelInteractions(trRefEntity_t * ent, trRefDlight_t * light)
{
	int             i;
	msurface_t     *surf;
	bmodel_t       *bModel = NULL;
	model_t        *pModel = NULL;
	interactionType_t iaType = IA_DEFAULT;

	// cull the entire model if it is outside the view frustum
	// and we don't care about proper shadowing
	if(ent->cull == CULL_OUT)
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

	pModel = R_GetModelByHandle(ent->e.hModel);
	bModel = pModel->bmodel;

	// do a quick AABB cull
	if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], ent->worldBounds[0], ent->worldBounds[1]))
	{
		tr.pc.c_dlightSurfacesCulled += bModel->numSurfaces;
		return;
	}

	// do a more expensive and precise light frustum cull
	if(!r_noLightFrustums->integer)
	{
		if(R_CullLightWorldBounds(light, ent->worldBounds) == CULL_OUT)
		{
			tr.pc.c_dlightSurfacesCulled += bModel->numSurfaces;
			return;
		}
	}

	// set the dlight bits in all the surfaces
	for(i = 0; i < bModel->numSurfaces; i++)
	{
		surf = bModel->firstSurface + i;

		// FIXME: do more culling?

		/*
		   if(*surf->data == SF_FACE)
		   {
		   ((srfSurfaceFace_t *) surf->data)->dlightBits[tr.smpFrame] = mask;
		   }
		   else if(*surf->data == SF_GRID)
		   {
		   ((srfGridMesh_t *) surf->data)->dlightBits[tr.smpFrame] = mask;
		   }
		   else if(*surf->data == SF_TRIANGLES)
		   {
		   ((srfTriangles_t *) surf->data)->dlightBits[tr.smpFrame] = mask;
		   }
		 */
		
		// skip all surfaces that don't matter for lighting only pass
		if(surf->shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY))
			continue;

		R_AddDlightInteraction(light, surf->data, surf->shader, 0, NULL, 0, NULL, iaType);
		tr.pc.c_dlightSurfaces++;
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/



/*
=================
R_SetupEntityLightingGrid
=================
*/
static void R_SetupEntityLightingGrid(trRefEntity_t * ent)
{
	vec3_t          lightOrigin;
	int             pos[3];
	int             i, j;
	byte           *gridData;
	float           frac[3];
	int             gridStep[3];
	vec3_t          direction;
	float           totalFactor;

	if(ent->e.renderfx & RF_LIGHTING_ORIGIN)
	{
		// seperate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy(ent->e.lightingOrigin, lightOrigin);
	}
	else
	{
		VectorCopy(ent->e.origin, lightOrigin);
	}

	VectorSubtract(lightOrigin, tr.world->lightGridOrigin, lightOrigin);
	for(i = 0; i < 3; i++)
	{
		float           v;

		v = lightOrigin[i] * tr.world->lightGridInverseSize[i];
		pos[i] = floor(v);
		frac[i] = v - pos[i];
		if(pos[i] < 0)
		{
			pos[i] = 0;
		}
		else if(pos[i] >= tr.world->lightGridBounds[i] - 1)
		{
			pos[i] = tr.world->lightGridBounds[i] - 1;
		}
	}

	VectorClear(ent->ambientLight);
	VectorClear(ent->directedLight);
	VectorClear(direction);

	assert(tr.world->lightGridData);	// bk010103 - NULL with -nolight maps

	// trilerp the light value
	gridStep[0] = 8;
	gridStep[1] = 8 * tr.world->lightGridBounds[0];
	gridStep[2] = 8 * tr.world->lightGridBounds[0] * tr.world->lightGridBounds[1];
	gridData = tr.world->lightGridData + pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2];

	totalFactor = 0;
	for(i = 0; i < 8; i++)
	{
		float           factor;
		byte           *data;
		int             lat, lng;
		vec3_t          normal;

#if idppc
		float           d0, d1, d2, d3, d4, d5;
#endif
		factor = 1.0;
		data = gridData;
		for(j = 0; j < 3; j++)
		{
			if(i & (1 << j))
			{
				factor *= frac[j];
				data += gridStep[j];
			}
			else
			{
				factor *= (1.0f - frac[j]);
			}
		}

		if(!(data[0] + data[1] + data[2]))
		{
			continue;			// ignore samples in walls
		}
		totalFactor += factor;
#if idppc
		d0 = data[0];
		d1 = data[1];
		d2 = data[2];
		d3 = data[3];
		d4 = data[4];
		d5 = data[5];

		ent->ambientLight[0] += factor * d0;
		ent->ambientLight[1] += factor * d1;
		ent->ambientLight[2] += factor * d2;

		ent->directedLight[0] += factor * d3;
		ent->directedLight[1] += factor * d4;
		ent->directedLight[2] += factor * d5;
#else
		ent->ambientLight[0] += factor * data[0];
		ent->ambientLight[1] += factor * data[1];
		ent->ambientLight[2] += factor * data[2];

		ent->directedLight[0] += factor * data[3];
		ent->directedLight[1] += factor * data[4];
		ent->directedLight[2] += factor * data[5];
#endif
		lat = data[7];
		lng = data[6];
		lat *= (FUNCTABLE_SIZE / 256);
		lng *= (FUNCTABLE_SIZE / 256);

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		normal[0] = tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng];
		normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		normal[2] = tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK];

		VectorMA(direction, factor, normal, direction);
	}

	if(totalFactor > 0 && totalFactor < 0.99)
	{
		totalFactor = 1.0f / totalFactor;
		VectorScale(ent->ambientLight, totalFactor, ent->ambientLight);
		VectorScale(ent->directedLight, totalFactor, ent->directedLight);
	}

	VectorScale(ent->ambientLight, r_ambientScale->value, ent->ambientLight);
	VectorScale(ent->directedLight, r_directedScale->value, ent->directedLight);

	VectorNormalize2(direction, ent->lightDir);
}


/*
===============
LogLight
===============
*/
static void LogLight(trRefEntity_t * ent)
{
	int             max1, max2;

	if(!(ent->e.renderfx & RF_FIRST_PERSON))
	{
		return;
	}

	max1 = ent->ambientLight[0];
	if(ent->ambientLight[1] > max1)
	{
		max1 = ent->ambientLight[1];
	}
	else if(ent->ambientLight[2] > max1)
	{
		max1 = ent->ambientLight[2];
	}

	max2 = ent->directedLight[0];
	if(ent->directedLight[1] > max2)
	{
		max2 = ent->directedLight[1];
	}
	else if(ent->directedLight[2] > max2)
	{
		max2 = ent->directedLight[2];
	}

	ri.Printf(PRINT_ALL, "amb:%i  dir:%i\n", max1, max2);
}

/*
=================
R_SetupEntityLighting

Calculates all the lighting values that will be used
by the Calc_* functions
=================
*/
void R_SetupEntityLighting(const trRefdef_t * refdef, trRefEntity_t * ent)
{
	int             i;
	vec3_t          lightDir;
	vec3_t          lightOrigin;
	float           d;

	// lighting calculations 
	if(ent->lightingCalculated)
	{
		return;
	}
	ent->lightingCalculated = qtrue;

	// trace a sample point down to find ambient light
	if(ent->e.renderfx & RF_LIGHTING_ORIGIN)
	{
		// seperate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy(ent->e.lightingOrigin, lightOrigin);
	}
	else
	{
		VectorCopy(ent->e.origin, lightOrigin);
	}

	// if NOWORLDMODEL, only use dynamic lights (menu system, etc)
	if(!(refdef->rdflags & RDF_NOWORLDMODEL) && tr.world->lightGridData)
	{
		R_SetupEntityLightingGrid(ent);
	}
	else
	{
		ent->ambientLight[0] = ent->ambientLight[1] = ent->ambientLight[2] = tr.identityLight * 150;
		ent->directedLight[0] = ent->directedLight[1] = ent->directedLight[2] = tr.identityLight * 150;
		VectorCopy(tr.sunDirection, ent->lightDir);
	}

	// bonus items and view weapons have a fixed minimum add
	if(1 /* ent->e.renderfx & RF_MINLIGHT */ )
	{
		// give everything a minimum light add
		ent->ambientLight[0] += tr.identityLight * 32;
		ent->ambientLight[1] += tr.identityLight * 32;
		ent->ambientLight[2] += tr.identityLight * 32;
	}

	// clamp ambient
	for(i = 0; i < 3; i++)
	{
		if(ent->ambientLight[i] > tr.identityLightByte)
		{
			ent->ambientLight[i] = tr.identityLightByte;
		}
	}

	if(r_debugLight->integer)
	{
		LogLight(ent);
	}

	// save out the byte packet version
	((byte *) & ent->ambientLightInt)[0] = myftol(ent->ambientLight[0]);
	((byte *) & ent->ambientLightInt)[1] = myftol(ent->ambientLight[1]);
	((byte *) & ent->ambientLightInt)[2] = myftol(ent->ambientLight[2]);
	((byte *) & ent->ambientLightInt)[3] = 0xff;

	// transform the direction to local space
	d = VectorLength(ent->directedLight);
	VectorScale(ent->lightDir, d, lightDir);
	VectorNormalize(lightDir);
	ent->lightDir[0] = DotProduct(lightDir, ent->e.axis[0]);
	ent->lightDir[1] = DotProduct(lightDir, ent->e.axis[1]);
	ent->lightDir[2] = DotProduct(lightDir, ent->e.axis[2]);
}

/*
=================
R_LightForPoint
=================
*/
int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)
{
	trRefEntity_t   ent;

	// bk010103 - this segfaults with -nolight maps
	if(tr.world->lightGridData == NULL)
		return qfalse;

	Com_Memset(&ent, 0, sizeof(ent));
	VectorCopy(point, ent.e.origin);
	R_SetupEntityLightingGrid(&ent);
	VectorCopy(ent.ambientLight, ambientLight);
	VectorCopy(ent.directedLight, directedLight);
	VectorCopy(ent.lightDir, lightDir);

	return qtrue;
}


/*
=================
R_SetupDlightOrigin
Tr3B - needs finished transformMatrix
=================
*/
void R_SetupDlightOrigin(trRefDlight_t * dl)
{
	vec3_t          transformed;
	
	MatrixTransformNormal(dl->transformMatrix, dl->l.center, transformed);
	VectorAdd(dl->l.origin, transformed, dl->origin);
}

/*
=================
R_SetupDlightLocalBounds
=================
*/
void R_SetupDlightLocalBounds(trRefDlight_t * dl)
{
	switch (dl->l.rlType)
	{
		default:
		case RL_OMNI:
		{
			dl->localBounds[0][0] = dl->l.radius[0];
			dl->localBounds[0][1] = dl->l.radius[1];
			dl->localBounds[0][2] = dl->l.radius[2];
			dl->localBounds[1][0] = -dl->l.radius[0];
			dl->localBounds[1][1] = -dl->l.radius[1];
			dl->localBounds[1][2] = -dl->l.radius[2];
			break;
		}
		
		case RL_PROJ:
		{
			dl->localBounds[0][0] = dl->l.radius[0];
			dl->localBounds[0][1] = dl->l.radius[1];
			dl->localBounds[0][2] = dl->l.radius[2];
			dl->localBounds[1][0] = 0;
			dl->localBounds[1][1] = -dl->l.radius[1];
			dl->localBounds[1][2] = -dl->l.radius[2];
			break;
		}
	}
}

/*
=================
R_SetupDlightWorldBounds
Tr3B - needs finished transformMatrix
=================
*/
void R_SetupDlightWorldBounds(trRefDlight_t * dl)
{
	int             j;
	vec3_t          v, transformed;

	ClearBounds(dl->worldBounds[0], dl->worldBounds[1]);

	for(j = 0; j < 8; j++)
	{
		v[0] = dl->localBounds[j & 1][0];
		v[1] = dl->localBounds[(j >> 1) & 1][1];
		v[2] = dl->localBounds[(j >> 2) & 1][2];

		// transform local bounds vertices into world space
		MatrixTransformPoint(dl->transformMatrix, v, transformed);

		AddPointToBounds(transformed, dl->worldBounds[0], dl->worldBounds[1]);
	}
}

/*
=================
R_SetupDlightFrustum
=================
*/
void R_SetupDlightFrustum(trRefDlight_t * dl)
{
	switch (dl->l.rlType)
	{
		case RL_OMNI:
		{
			int             i;
			vec3_t          planeNormal;
			vec3_t          planeOrigin;

			for(i = 0; i < 3; i++)
			{
				VectorCopy(dl->l.origin, planeOrigin);

				VectorNegate(dl->l.axis[i], planeNormal);
				planeOrigin[i] += dl->l.radius[i];

				VectorCopy(planeNormal, dl->frustum[i].normal);
				dl->frustum[i].type = PlaneTypeForNormal(planeNormal);
				dl->frustum[i].dist = DotProduct(planeOrigin, planeNormal);
				SetPlaneSignbits(&dl->frustum[i]);
			}

			for(i = 0; i < 3; i++)
			{
				VectorCopy(dl->l.origin, planeOrigin);

				VectorCopy(dl->l.axis[i], planeNormal);
				planeOrigin[i] -= dl->l.radius[i];

				VectorCopy(planeNormal, dl->frustum[i + 3].normal);
				dl->frustum[i + 3].type = PlaneTypeForNormal(planeNormal);
				dl->frustum[i + 3].dist = DotProduct(planeOrigin, planeNormal);
				SetPlaneSignbits(&dl->frustum[i + 3]);
			}
			break;
		}
		
		default:
			break;
	}
}


/*
=================
R_SetupDlightProjection
=================
*/
void R_SetupDlightProjection(trRefDlight_t * dl)
{
	switch (dl->l.rlType)
	{
		case RL_OMNI:
		{
			MatrixSetupScale(dl->projectionMatrix, 1.0 / dl->l.radius[0], 1.0 / dl->l.radius[1], 1.0 / dl->l.radius[2]);
			break;
		}

		case RL_PROJ:
		{
#if 1
			float           xMin, xMax, yMin, yMax;
			float           width, height, depth;
			float           zNear, zFar;
			float           fovX, fovY;
			vec3_t          target, right, up;
			float          *proj = dl->projectionMatrix;

			MatrixTransformNormal(dl->transformMatrix, dl->l.target, target);
			MatrixTransformNormal(dl->transformMatrix, dl->l.right, right);
			MatrixTransformNormal(dl->transformMatrix, dl->l.up, up);

			fovX = 30;
			fovY = R_CalcFov(fovX, VectorLength(right) * 2, VectorLength(up) * 2);

			zNear = 1.0;
			zFar = VectorLength(target);

			xMax = zNear * tan(fovX * M_PI / 360.0f);
			xMin = -xMax;

			yMax = zNear * tan(fovY * M_PI / 360.0f);
			yMin = -yMax;

			width = xMax - xMin;
			height = yMax - yMin;
			depth = zFar - zNear;

			// standard OpenGL projection matrix
			proj[0] = 2 * zNear / width;
			proj[4] = 0;
			proj[8] = (xMax + xMin) / width;
			proj[12] = 0;

			proj[1] = 0;
			proj[5] = 2 * zNear / height;
			proj[9] = (yMax + yMin) / height;
			proj[13] = 0;

			proj[2] = 0;
			proj[6] = 0;
			proj[10] = -(zFar + zNear) / depth;
			proj[14] = -2 * zFar * zNear / depth;

			proj[3] = 0;
			proj[7] = 0;
			proj[11] = -1;
			proj[15] = 0;
			
			// HACK: rotate transform into the direction we are facing
			MatrixMultiplyRotation(proj, 90, 90, 0);
#else
			// Tr3B - recoded from GtkRadiant entity plugin source
			int             i;
			vec4_t          lightProject[4];
			vec4_t          frustum[6];
			vec3_t          start, stop;
			vec3_t          right, up;
			vec4_t          targetGlobal;
			float           rLen, uLen, fLen;
			vec3_t          normal;
			vec_t           dist;
			vec3_t          falloff;
			
			float          *proj = dl->projectionMatrix;

			//MatrixTransformNormal(dl->transformMatrix, dl->l.target, target);
			//MatrixTransformNormal(dl->transformMatrix, dl->l.right, right);
			//MatrixTransformNormal(dl->transformMatrix, dl->l.up, up);
			

			VectorNormalize2(dl->l.target, start);
			VectorCopy(dl->l.target, stop);

			rLen = VectorNormalize2(dl->l.right, right);
			uLen = VectorNormalize2(dl->l.up, up);
			
			CrossProduct(up, right, normal);
			dist = DotProduct(dl->l.target, normal);

			if(dist < 0)
			{
				dist = -dist;
				VectorInverse(normal);
			}

			VectorScale(right, (0.5f * dist) / rLen, right);
			VectorScale(up, -(0.5f * dist) / uLen, up);

			VectorCopy(normal, lightProject[2]);
			lightProject[2][3] = 0;
			
			VectorCopy(right, lightProject[0]);
			lightProject[0][3] = 0;
			
			VectorCopy(up, lightProject[1]);
			lightProject[1][3] = 0;

			// now offset to center
			VectorCopy(dl->l.target, targetGlobal);
			targetGlobal[3] = 1;

			{
				float           a, b, ofs;
				a = DotProduct4(targetGlobal, lightProject[0]);
				b = DotProduct4(targetGlobal, lightProject[2]);
				ofs = 0.5f - a / b;

				VectorMA4(lightProject[0], ofs, lightProject[2], lightProject[0]);
			}
			{
				float           a, b, ofs;
				a = DotProduct4(targetGlobal, lightProject[1]);
				b = DotProduct4(targetGlobal, lightProject[2]);
				ofs = 0.5f - a / b;

				VectorMA4(lightProject[1], ofs, lightProject[2], lightProject[1]);
			}

			// set the falloff vector
			VectorSubtract(stop, start, falloff);
			fLen = VectorNormalize(falloff);
			if(fLen <= 0)
			{
				fLen = 1;
			}
			VectorScale(falloff, (1.0f / fLen), falloff);
			
			VectorCopy(falloff, lightProject[3]);
			lightProject[3][3] = -DotProduct(start, falloff);

			// we want the planes of s=0, s=q, t=0, and t=q
			
			// left
			VectorCopy4(lightProject[0], frustum[0]);
			
			// bottom
			VectorCopy4(lightProject[1], frustum[1]);
			
			// right
			VectorSubtract(lightProject[2], lightProject[0], frustum[2]);
			frustum[2][3] = lightProject[2][3] - lightProject[0][3];
			
			// top
			VectorSubtract(lightProject[2], lightProject[1], frustum[3]);
			frustum[3][3] = lightProject[2][3] - lightProject[1][3];
			
			// we want the planes of s=0 and s=1 for front and rear clipping planes
			
			// front
			VectorCopy4(lightProject[3], frustum[4]);
			
			// back
			VectorNegate(lightProject[3], frustum[5]);
			frustum[5][3] = lightProject[3][3] - 1.0f;
			
			MatrixFromPlanes(proj, frustum[0], frustum[1], frustum[2], frustum[3], frustum[4], frustum[5]);
			
			for(i = 0; i < 6; i++)
			{
				PlaneNormalize(frustum[i]);
				VectorNegate(frustum[i], dl->frustum[i].normal);
				dl->frustum[i].type = PlaneTypeForNormal(dl->frustum[i].normal);
				dl->frustum[i].dist = frustum[i][3];
				SetPlaneSignbits(&dl->frustum[i]);
			}
#endif
			break;
		}

		default:
			ri.Error(ERR_DROP, "R_SetupDlightProjection: Bad rlType");
	}
}

/*
=================
R_AddDlightInteraction
=================
*/
void R_AddDlightInteraction(trRefDlight_t * light, surfaceType_t * surface, shader_t * surfaceShader, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes, interactionType_t iaType)
{
	int             iaIndex;
	interaction_t  *ia;
	interaction_t  *iaLast;
	
	// skip all surfaces that don't matter for lighting only pass
	if(surfaceShader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY))
		return;
		
	if(!surfaceShader->interactLight && iaType == IA_LIGHTONLY)
		return;

	// instead of checking for overflow, we just mask the index
	// so it wraps around
	iaIndex = tr.refdef.numInteractions & INTERACTION_MASK;
	ia = &tr.refdef.interactions[iaIndex];
	tr.refdef.numInteractions++;

	// connect to interaction grid
	if(light->firstInteractionIndex == -1)
	{
		light->firstInteractionIndex = iaIndex;
	}
	
	if(light->lastInteractionIndex != -1)
	{
		iaLast = &tr.refdef.interactions[light->lastInteractionIndex];
		
		iaLast->next = ia;
		
		if(light->lastInteractionIndex == INTERACTION_MASK)
		{
			light->noSort = qtrue;
		}
	}
	
	light->lastInteractionIndex = iaIndex;
	
	// update counters
	light->numInteractions++;
	
	switch (iaType)
	{
		case IA_SHADOWONLY:
			light->numShadowOnlyInteractions++;
			break;
			
		case IA_LIGHTONLY:
			light->numLightOnlyInteractions++;
			break;
			
		default:
			break;
	}

	// check what kind of attenuationShader is used
	if(!light->l.attenuationShader)
	{
		if(light->isStatic)
		{
			switch (light->l.rlType)
			{
				default:
				case RL_OMNI:
					ia->dlightShader = tr.defaultPointLightShader;
					break;
				
				case RL_PROJ:
					ia->dlightShader = tr.defaultProjectedLightShader;
					break;
			}
		}
		else
		{
			switch (light->l.rlType)
			{
				default:
				case RL_OMNI:
					ia->dlightShader = tr.defaultDynamicLightShader;
					break;
				
				case RL_PROJ:
					ia->dlightShader = tr.defaultProjectedLightShader;
					break;
			}
		}
	}
	else
	{
		ia->dlightShader = R_GetShaderByHandle(light->l.attenuationShader);
	}

	ia->next = NULL;
	
	ia->type = iaType;
	
	ia->dlight = light;
	ia->entity = tr.currentEntity;
	ia->surface = surface;
	ia->surfaceShader = surfaceShader;
	
	ia->numLightIndexes = numLightIndexes;
	ia->lightIndexes = lightIndexes;
	
	ia->numShadowIndexes = numShadowIndexes;
	ia->shadowIndexes = shadowIndexes;

	ia->scissorX = light->scissor.coords[0];
	ia->scissorY = light->scissor.coords[1];
	ia->scissorWidth = light->scissor.coords[2] - light->scissor.coords[0];
	ia->scissorHeight = light->scissor.coords[3] - light->scissor.coords[1];
	
	if(qglDepthBoundsEXT)
	{
		ia->depthNear = light->depthBounds[0];
		ia->depthFar = light->depthBounds[1];
		ia->noDepthBoundsTest = light->noDepthBoundsTest;
	}

	if(light->isStatic)
	{
		tr.pc.c_slightInteractions++;
	}
	else
	{
		tr.pc.c_dlightInteractions++;
	}
}


/*
=================
InteractionCompare
compare function for qsort()
=================
*/
static int InteractionCompare(const void *a, const void *b)
{
#if 1
	// shader first
	if(((interaction_t *) a)->surfaceShader < ((interaction_t *) b)->surfaceShader)
		return -1;
	
	else if(((interaction_t *) a)->surfaceShader > ((interaction_t *) b)->surfaceShader)
		return 1;
#endif

#if 1
	// then entity
	if(((interaction_t *) a)->entity == &tr.worldEntity && ((interaction_t *) b)->entity != &tr.worldEntity)
		return -1;
	
	else if(((interaction_t *) a)->entity != &tr.worldEntity && ((interaction_t *) b)->entity == &tr.worldEntity)
		return 1;
	
	else if(((interaction_t *) a)->entity < ((interaction_t *) b)->entity)
		return -1;
	
	else if(((interaction_t *) a)->entity > ((interaction_t *) b)->entity)
		return 1;
#endif

	return 0;
}

/*
=================
R_SortInteractions
=================
*/
void R_SortInteractions(trRefDlight_t * light)
{
	int             i;
	interaction_t  *ia;
	interaction_t  *iaLast;
	
	if(r_noInteractionSort->integer)
	{
		return;	
	}

	if(!light->numInteractions || light->noSort)
	{
		return;
	}
	
	ia = &tr.refdef.interactions[light->firstInteractionIndex];
	
	// sort by material etc. for geometry batching in the renderer backend
	qsort(ia, light->numInteractions, sizeof(interaction_t), InteractionCompare);
	
	// fix linked list
	iaLast = NULL;
	for(i = 0; i < light->numInteractions; i++)
	{
		ia = &tr.refdef.interactions[light->firstInteractionIndex + i];
		
		if(iaLast)
		{
			iaLast->next = ia;
		}
		
		ia->next = NULL;
		
		iaLast = ia;
	}
}


/*
=================
R_IntersectRayPlane
=================
*/
static void R_IntersectRayPlane(const vec3_t v1, const vec3_t v2, cplane_t * plane, vec3_t res)
{
	vec3_t          v;
	float           sect;

	VectorSubtract(v1, v2, v);
	sect = -(DotProduct(plane->normal, v1) - plane->dist) / DotProduct(plane->normal, v);
	VectorScale(v, sect, v);
	VectorAdd(v1, v, res);
}


/*
=================
R_AddPointToLightScissor
=================
*/
static void R_AddPointToLightScissor(trRefDlight_t * light, const vec3_t world)
{
	vec4_t          eye, clip, normalized, window;
	
	R_TransformWorldToClip(world, tr.viewParms.world.viewMatrix, tr.viewParms.projectionMatrix, eye, clip);
	R_TransformClipToWindow(clip, &tr.viewParms, normalized, window);
	
	if(window[0] > light->scissor.coords[2])
		light->scissor.coords[2] = (int)window[0];
	
	if(window[0] < light->scissor.coords[0])
		light->scissor.coords[0] = (int)window[0];
	
	if(window[1] > light->scissor.coords[3])
		light->scissor.coords[3] = (int)window[1];
	
	if(window[1] < light->scissor.coords[1])
		light->scissor.coords[1] = (int)window[1];
}

/*
=================
R_AddEdgeToLightScissor
=================
*/
static void R_AddEdgeToLightScissor(trRefDlight_t * light, vec3_t local1, vec3_t local2)
{
	int             i;
	vec3_t          intersect;
	vec3_t          world1, world2;
	qboolean        side1, side2;
	cplane_t       *frust;
	
	for(i = 0; i < FRUSTUM_PLANES; i++)
	{
		R_LocalPointToWorld(local1, world1);
		R_LocalPointToWorld(local2, world2);
		
		frust = &tr.viewParms.frustum[i];
	
		// check edge to frustrum plane
		side1 = ((DotProduct(frust->normal, world1) - frust->dist) >= 0.0);
		side2 = ((DotProduct(frust->normal, world2) - frust->dist) >= 0.0);

		if(!side1 && !side2)
			continue;					// edge behind plane

		if(!side1 || !side2)
			R_IntersectRayPlane(world1, world2, frust, intersect);

		if(!side1)
		{
			VectorCopy(intersect, world1);
		}
		else if(!side2)
		{
			VectorCopy(intersect, world2);
		}
		
		R_AddPointToLightScissor(light, world1);
		R_AddPointToLightScissor(light, world2);
	}
}

/*
=================
R_SetDlightScissor
Recturns the screen space rectangle taken by the box.
	(Clips the box to the near plane to have correct results even if the box intersects the near plane)
Tr3B - recoded from Tenebrae2
=================
*/
void R_SetupDlightScissor(trRefDlight_t * light)
{
	vec3_t          v1, v2;

	light->scissor.coords[0] = tr.viewParms.viewportX;
	light->scissor.coords[1] = tr.viewParms.viewportY;
	light->scissor.coords[2] = tr.viewParms.viewportX + tr.viewParms.viewportWidth;
	light->scissor.coords[3] = tr.viewParms.viewportY + tr.viewParms.viewportHeight;

	if(r_noLightScissors->integer || R_CullLightPoint(light, tr.viewParms.or.origin) == CULL_IN)
	{
		return;
	}

	// transform local light corners to world space -> eye space -> clip space -> window space
	// and extend the light scissor's mins maxs by resulting window coords
	light->scissor.coords[0] = 100000000;
	light->scissor.coords[1] = 100000000;
	light->scissor.coords[2] = -100000000;
	light->scissor.coords[3] = -100000000;
	
	// top plane
	VectorSet(v1, light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
	VectorSet(v2, light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
	VectorSet(v2, light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
	VectorSet(v2, light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
	VectorSet(v2, light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	// bottom plane
	VectorSet(v1, light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	// sides
	VectorSet(v1, light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);

	VectorSet(v1, light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
	VectorSet(v2, light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
	R_AddEdgeToLightScissor(light, v1, v2);
}


/*
=================
R_SetupDlightDepthBounds
=================
*/
void R_SetupDlightDepthBounds(trRefDlight_t * dl)
{
	int             i, j;
	vec3_t          v, world;
	vec4_t          eye, clip, normalized, window;
	float           depthMin, depthMax;
	
	if(qglDepthBoundsEXT)
	{
		tr.pc.c_depthBoundsTestsRejected++;
		
		depthMin = 1.0;
		depthMax = 0.0;
		
		for(j = 0; j < 8; j++)
		{
			v[0] = dl->localBounds[j & 1][0];
			v[1] = dl->localBounds[(j >> 1) & 1][1];
			v[2] = dl->localBounds[(j >> 2) & 1][2];

			// transform local bounds vertices into world space
			MatrixTransformPoint(dl->transformMatrix, v, world);
			
			R_TransformWorldToClip(world, tr.viewParms.world.viewMatrix, tr.viewParms.projectionMatrix, eye, clip);
			
			// check to see if the point is completely off screen
			for(i = 0; i < 3; i++)
			{
				if(clip[i] >= clip[3] || clip[i] <= -clip[3])
				{
					dl->noDepthBoundsTest = qtrue;
					return;
				}
			}
			
			R_TransformClipToWindow(clip, &tr.viewParms, normalized, window);
			
			if(window[0] < 0 || window[0] >= tr.viewParms.viewportWidth
			|| window[1] < 0 || window[1] >= tr.viewParms.viewportHeight)
			{
				// shouldn't happen, since we check the clip[] above, except for FP rounding
				dl->noDepthBoundsTest = qtrue;
				return;
			}

			depthMin = MIN(normalized[2], depthMin);
			depthMax = MAX(normalized[2], depthMax);
		}
		
		if(depthMin > depthMax)
		{
			// light behind near plane or clipped
			dl->noDepthBoundsTest = qtrue;
		}
		else
		{
			dl->noDepthBoundsTest = qfalse;
			dl->depthBounds[0] = depthMin;
			dl->depthBounds[1] = depthMax;
			
			tr.pc.c_depthBoundsTestsRejected--;
			tr.pc.c_depthBoundsTests++;
		}
	}
}

/*
=================
R_CullLightPoint

Returns CULL_IN, CULL_CLIP, or CULL_OUT
=================
*/
int R_CullLightPoint(trRefDlight_t * light, const vec3_t p)
{
	int             i;
	cplane_t       *frust;
	float           dist;

	// check against frustum planes
	for(i = 0; i < 6; i++)
	{
		frust = &light->frustum[i];

		dist = DotProduct(p, frust->normal) - frust->dist;
		if(dist < 0)
		{
			// completely outside frustum
			return CULL_OUT;
		}
	}

	// completely inside frustum
	return CULL_IN;
}

/*
=================
R_CullLightTriangle

Returns CULL_IN, CULL_CLIP, or CULL_OUT
=================
*/
int R_CullLightTriangle(trRefDlight_t * light, vec3_t verts[3])
{
	int             i;
	vec3_t          worldBounds[2];

	if(r_nocull->integer)
	{
		return CULL_CLIP;
	}

	// calc AABB of the triangle
	ClearBounds(worldBounds[0], worldBounds[1]);
	for(i = 0; i < 3; i++)
	{
		AddPointToBounds(verts[i], worldBounds[0], worldBounds[1]);
	}

	return R_CullLightWorldBounds(light, worldBounds);
}

/*
=================
R_CullLightTriangle

Returns CULL_IN, CULL_CLIP, or CULL_OUT
=================
*/
int R_CullLightWorldBounds(trRefDlight_t * light, vec3_t worldBounds[2])
{
	int             i;
	cplane_t       *frust;
	qboolean        anyClip;
	int             r;

	if(r_nocull->integer)
	{
		return CULL_CLIP;
	}

	// check against frustum planes
	anyClip = qfalse;
	for(i = 0; i < 6; i++)
	{
		frust = &light->frustum[i];

		r = BoxOnPlaneSide(worldBounds[0], worldBounds[1], frust);

		if(r == 2)
		{
			// completely outside frustum
			return CULL_OUT;
		}
		if(r == 3)
		{
			anyClip = qtrue;
		}
	}

	if(!anyClip)
	{
		// completely inside frustum
		return CULL_IN;
	}

	// partially clipped
	return CULL_CLIP;
}
