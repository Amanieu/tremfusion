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
// tr_scene.c
#include "tr_local.h"

static int      r_firstSceneDrawSurf;
static int      r_firstSceneInteraction;

static int      r_numLights;
static int      r_firstSceneLight;

static int      r_numEntities;
static int      r_firstSceneEntity;

static int      r_numPolys;
static int      r_firstScenePoly;

static int      r_numPolyVerts;


/*
====================
R_ToggleSmpFrame
====================
*/
void R_ToggleSmpFrame(void)
{
	if(r_smp->integer)
	{
		// use the other buffers next frame, because another CPU
		// may still be rendering into the current ones
		tr.smpFrame ^= 1;
	}
	else
	{
		tr.smpFrame = 0;
	}

	backEndData[tr.smpFrame]->commands.used = 0;

	r_firstSceneDrawSurf = 0;
	r_firstSceneInteraction = 0;

	r_numLights = 0;
	r_firstSceneLight = 0;

	r_numEntities = 0;
	r_firstSceneEntity = 0;

	r_numPolys = 0;
	r_firstScenePoly = 0;

	r_numPolyVerts = 0;
}


/*
====================
RE_ClearScene
====================
*/
void RE_ClearScene(void)
{
	r_firstSceneLight = r_numLights;
	r_firstSceneEntity = r_numEntities;
	r_firstScenePoly = r_numPolys;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces(void)
{
	int             i;
	shader_t       *sh;
	srfPoly_t      *poly;

	tr.currentEntity = &tr.worldEntity;

	for(i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys; i++, poly++)
	{
		sh = R_GetShaderByHandle(poly->hShader);
		R_AddDrawSurf((void *)poly, sh, -1);
	}
}

/*
=====================
RE_AddPolyToScene
=====================
*/
void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t * verts, int numPolys)
{
	srfPoly_t      *poly;
	int             j;

	if(!tr.registered)
	{
		return;
	}

	if(!hShader)
	{
		ri.Printf(PRINT_DEVELOPER, "WARNING: RE_AddPolyToScene: NULL poly shader\n");
		return;
	}

	for(j = 0; j < numPolys; j++)
	{
		if(r_numPolyVerts + numVerts >= r_maxPolyVerts->integer || r_numPolys >= r_maxPolys->integer)
		{
			/*
			   NOTE TTimo this was initially a PRINT_WARNING
			   but it happens a lot with high fighting scenes and particles
			   since we don't plan on changing the const and making for room for those effects
			   simply cut this message to developer only
			 */
			ri.Printf(PRINT_DEVELOPER, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData[tr.smpFrame]->polys[r_numPolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData[tr.smpFrame]->polyVerts[r_numPolyVerts];

		Com_Memcpy(poly->verts, &verts[numVerts * j], numVerts * sizeof(*verts));

		// done.
		r_numPolys++;
		r_numPolyVerts += numVerts;
	}
}


//=================================================================================


/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene(const refEntity_t * ent)
{
	if(!tr.registered)
	{
		return;
	}

	// Tr3B: fixed was ENTITYNUM_WORLD
	if(r_numEntities >= MAX_REF_ENTITIES)
	{
		return;
	}

	if(ent->reType < 0 || ent->reType >= RT_MAX_REF_ENTITY_TYPE)
	{
		ri.Error(ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType);
	}

	Com_Memcpy(&backEndData[tr.smpFrame]->entities[r_numEntities].e, ent, sizeof(refEntity_t));
	//backEndData[tr.smpFrame]->entities[r_numentities].e = *ent;
	backEndData[tr.smpFrame]->entities[r_numEntities].lightingCalculated = qfalse;

	r_numEntities++;
}

/*
=====================
RE_AddRefLightToScene
=====================
*/
void RE_AddRefLightToScene(const refLight_t * l)
{
	trRefLight_t   *light;

	if(!tr.registered)
	{
		return;
	}

	if(r_numLights >= MAX_REF_LIGHTS)
	{
		return;
	}

	if(l->radius[0] <= 0 && !VectorLength(l->radius) && l->distFar <= 0)
	{
		return;
	}

	if(l->rlType < 0 || l->rlType >= RL_MAX_REF_LIGHT_TYPE)
	{
		ri.Error(ERR_DROP, "RE_AddRefLightToScene: bad rlType %i", l->rlType);
	}

	light = &backEndData[tr.smpFrame]->lights[r_numLights++];
	Com_Memcpy(&light->l, l, sizeof(light->l));

	light->isStatic = qfalse;
	light->additive = qtrue;

	if(light->l.scale <= 0)
	{
		light->l.scale = r_lightScale->value;
	}

	if(!r_hdrRendering->integer || !glConfig.textureFloatAvailable || !glConfig.framebufferObjectAvailable || !glConfig.framebufferBlitAvailable)
	{
		if(light->l.scale >= r_lightScale->value)
		{
			light->l.scale = r_lightScale->value;
		}
	}

	if(!r_dynamicLightsCastShadows->integer && !light->l.inverseShadows)
	{
		light->l.noShadows = qtrue;
	}
}

/*
=====================
R_AddWorldLightsToScene
=====================
*/
static void R_AddWorldLightsToScene()
{
	int             i;
	trRefLight_t   *light;

	if(!tr.registered)
	{
		return;
	}

	if(tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	for(i = 0; i < tr.world->numLights; i++)
	{
		light = tr.currentLight = &tr.world->lights[i];

		if(r_numLights >= MAX_REF_LIGHTS)
		{
			return;
		}

		/*
		   if(light->radius[0] <= 0 && !VectorLength(light->radius) && light->distance <= 0)
		   {
		   continue;
		   }
		 */

		if(!light->firstInteractionCache)
		{
			// this light has no interactions precached
			continue;
		}

		Com_Memcpy(&backEndData[tr.smpFrame]->lights[r_numLights], light, sizeof(trRefLight_t));
		r_numLights++;
	}
}

/*
=====================
RE_AddDynamicLightToScene
=====================
*/
static void RE_AddDynamicLightToScene(const vec3_t org, float intensity, float r, float g, float b, int additive)
{
	trRefLight_t   *light;

	if(!tr.registered)
	{
		return;
	}

	if(r_numLights >= MAX_REF_LIGHTS)
	{
		return;
	}

	if(intensity <= 0)
	{
		return;
	}

	light = &backEndData[tr.smpFrame]->lights[r_numLights++];

	light->l.rlType = RL_OMNI;
//  light->l.lightfx = 0;
	VectorCopy(org, light->l.origin);

	QuatClear(light->l.rotation);
	VectorClear(light->l.center);

	// HACK: this will tell the renderer backend to use tr.defaultLightShader
	light->l.attenuationShader = 0;

	light->l.radius[0] = intensity;
	light->l.radius[1] = intensity;
	light->l.radius[2] = intensity;

	light->l.color[0] = r;
	light->l.color[1] = g;
	light->l.color[2] = b;

	light->l.noShadows = r_dynamicLightsCastShadows->integer ? qfalse : qtrue;
	light->l.inverseShadows = qfalse;

	light->isStatic = qfalse;
	light->additive = additive;

	if(light->l.scale <= 0)
	{
		light->l.scale = r_lightScale->value;
	}
}

/*
=====================
RE_AddLightToScene
=====================
*/
void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	RE_AddDynamicLightToScene(org, intensity, r, g, b, qfalse);
}

/*
=====================
RE_AddAdditiveLightToScene
=====================
*/
void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	RE_AddDynamicLightToScene(org, intensity, r, g, b, qtrue);
}

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene(const refdef_t * fd)
{
	viewParms_t     parms;
	int             startTime;

	if(!tr.registered)
	{
		return;
	}
	GLimp_LogComment("====== RE_RenderScene =====\n");

	if(r_norefresh->integer)
	{
		return;
	}

	startTime = ri.Milliseconds();

	if(!tr.world && !(fd->rdflags & RDF_NOWORLDMODEL))
	{
		ri.Error(ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	Com_Memcpy(tr.refdef.text, fd->text, sizeof(tr.refdef.text));

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy(fd->vieworg, tr.refdef.vieworg);
	VectorCopy(fd->viewaxis[0], tr.refdef.viewaxis[0]);
	VectorCopy(fd->viewaxis[1], tr.refdef.viewaxis[1]);
	VectorCopy(fd->viewaxis[2], tr.refdef.viewaxis[2]);

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if(!(tr.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		int             areaDiff;
		int             i;

		// compare the area bits
		areaDiff = 0;
		for(i = 0; i < MAX_MAP_AREA_BYTES / 4; i++)
		{
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if(areaDiff)
		{
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}

	R_AddWorldLightsToScene();

	// derived info
	tr.refdef.floatTime = tr.refdef.time * 0.001f;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData[tr.smpFrame]->drawSurfs;

	tr.refdef.numInteractions = r_firstSceneInteraction;
	tr.refdef.interactions = backEndData[tr.smpFrame]->interactions;

	tr.refdef.numEntities = r_numEntities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData[tr.smpFrame]->entities[r_firstSceneEntity];

	tr.refdef.numLights = r_numLights - r_firstSceneLight;
	tr.refdef.lights = &backEndData[tr.smpFrame]->lights[r_firstSceneLight];

	tr.refdef.numPolys = r_numPolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData[tr.smpFrame]->polys[r_firstScenePoly];

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	Com_Memset(&parms, 0, sizeof(parms));

#if 1
	if(tr.refdef.pixelTarget == NULL)
	{
		parms.viewportX = tr.refdef.x;
		parms.viewportY = glConfig.vidHeight - (tr.refdef.y + tr.refdef.height);
	}
	else
	{
		//Driver bug, if we try and do pixel target work along the top edge of a window
		//we can end up capturing part of the status bar. (see screenshot corruption..)
		//Soooo.. use the middle.
		parms.viewportX = glConfig.vidWidth / 2;
		parms.viewportY = glConfig.vidHeight / 2;
	}
#else
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - (tr.refdef.y + tr.refdef.height);
#endif

	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	VectorSet4(parms.viewportVerts[0], parms.viewportX, parms.viewportY, 0, 1);
	VectorSet4(parms.viewportVerts[1], parms.viewportX + parms.viewportWidth, parms.viewportY, 0, 1);
	VectorSet4(parms.viewportVerts[2], parms.viewportX + parms.viewportWidth, parms.viewportY + parms.viewportHeight, 0, 1);
	VectorSet4(parms.viewportVerts[3], parms.viewportX, parms.viewportY + parms.viewportHeight, 0, 1);

	parms.isPortal = qfalse;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	VectorCopy(fd->vieworg, parms.orientation.origin);
	VectorCopy(fd->viewaxis[0], parms.orientation.axis[0]);
	VectorCopy(fd->viewaxis[1], parms.orientation.axis[1]);
	VectorCopy(fd->viewaxis[2], parms.orientation.axis[2]);

	VectorCopy(fd->vieworg, parms.pvsOrigin);

	R_RenderView(&parms);

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	r_firstSceneInteraction = tr.refdef.numInteractions;
	r_firstSceneEntity = r_numEntities;
	r_firstSceneLight = r_numLights;
	r_firstScenePoly = r_numPolys;

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
