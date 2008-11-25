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
// tr_animation.c
#include "tr_local.h"

/*

All bones should be an identity orientation to display the mesh exactly
as it is specified.

For all other frames, the bones represent the transformation from the 
orientation of the bone in the base frame to the orientation in this
frame.

*/

static md5Animation_t        *R_AllocAnimation(void)
{
	md5Animation_t *anim;

	if(tr.numAnimations == MAX_ANIMATIONFILES)
	{
		return NULL;
	}

	anim = ri.Hunk_Alloc(sizeof(*anim), h_low);
	anim->index = tr.numAnimations;
	tr.animations[tr.numAnimations] = anim;
	tr.numAnimations++;

	return anim;
}


/*
===============
R_InitAnimations
===============
*/
void R_InitAnimations(void)
{
	md5Animation_t *anim;

	// leave a space for NULL animation
	tr.numAnimations = 0;

	anim = R_AllocAnimation();
	anim->type = AT_BAD;
	strcpy(anim->name, "<default animation>");
}

/*
===============
RE_RegisterAnimation
===============
*/
qhandle_t RE_RegisterAnimation(const char *name)
{
	int             i, j;
	qhandle_t       hAnim;
	md5Animation_t *anim;
	md5Channel_t   *channel;
	md5Frame_t     *frame;
	char           *buffer, *buf_p;
	char           *token;
	int             version;
	
	if(!name || !name[0])
	{
		Com_Printf("Empty name passed to RE_RegisterAnimation\n");
		return 0;
	}

	if(strlen(name) >= MAX_QPATH)
	{
		Com_Printf("Animation name exceeds MAX_QPATH\n");
		return 0;
	}
	
	Com_DPrintf( "Registering animation '%s' in RE_RegisterAnimation\n", name );
	// search the currently loaded animations
	for(hAnim = 1; hAnim < tr.numAnimations; hAnim++)
	{
		anim = tr.animations[hAnim];
		if(!Q_stricmp(anim->name, name))
		{
			if(anim->type == AT_BAD)
			{
				return 0;
			}
			return hAnim;
		}
	}

	// allocate a new model_t
	if((anim = R_AllocAnimation()) == NULL)
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: R_AllocAnimation() failed for '%s'\n", name);
		return 0;
	}
	
	// only set the name after the animation has been successfully allocated
	Q_strncpyz(anim->name, name, sizeof(anim->name));

	// make sure the render thread is stopped
	R_SyncRenderThread();
	
	// load and parse the .md5anim file
	ri.FS_ReadFile(name, (void **)&buffer);
	if(!buffer)
	{
		return 0;
	}

	buf_p = buffer;
	
	// skip MD5Version indent string
	COM_ParseExt(&buf_p, qfalse);
	
	// check version
	token = COM_ParseExt(&buf_p, qfalse);
	version = atoi(token);
	if(version != MD5_VERSION)
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: '%s' has wrong version (%i should be %i)\n", name, version, MD5_VERSION);
		return qfalse;
	}
	
	// skip commandline <arguments string>
	token = COM_ParseExt(&buf_p, qtrue);
	token = COM_ParseExt(&buf_p, qtrue);

	// parse numFrames <number>
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numFrames"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'numFrames' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	anim->numFrames = atoi(token);
	
	// parse numJoints <number>
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numJoints"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'numJoints' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	anim->numChannels = atoi(token);
	
	// parse frameRate <number>
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "frameRate"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'frameRate' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	anim->frameRate = atoi(token);
	
	// parse numAnimatedComponents <number>
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numAnimatedComponents"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'numAnimatedComponents' found '%s' in model '%s'\n", token,
				  name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	anim->numAnimatedComponents = atoi(token);
	
	// parse hierarchy {
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "hierarchy"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'hierarchy' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	if(Q_stricmp(token, "{"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '{' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	
	// parse all the channels
	anim->channels = ri.Hunk_Alloc(sizeof(md5Channel_t) * anim->numChannels, h_low);
	
	for(i = 0, channel = anim->channels; i < anim->numChannels; i++, channel++)
	{
		token = COM_ParseExt(&buf_p, qtrue);
		Q_strncpyz(channel->name, token, sizeof(channel->name));
		
		//ri.Printf(PRINT_ALL, "RE_RegisterAnimation: '%s' has channel '%s'\n", name, channel->name);
		
		token = COM_ParseExt(&buf_p, qfalse);
		channel->parentIndex = atoi(token);
		
		if(channel->parentIndex >= anim->numChannels)
		{
			ri.Error(ERR_DROP, "RE_RegisterAnimation: '%s' has channel '%s' with bad parent index %i while numBones is %i\n",
					 name, channel->name, channel->parentIndex, anim->numChannels);
		}
		
		token = COM_ParseExt(&buf_p, qfalse);
		channel->componentsBits = atoi(token);
		
		token = COM_ParseExt(&buf_p, qfalse);
		channel->componentsOffset = atoi(token);
	}
	
	// parse }
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "}"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '}' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	
	
	// parse bounds {
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "bounds"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'bounds' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	if(Q_stricmp(token, "{"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '{' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	
	anim->frames = ri.Hunk_Alloc(sizeof(md5Frame_t) * anim->numFrames, h_low);
	
	for(i = 0, frame = anim->frames; i < anim->numFrames; i++, frame++)
	{
		// skip (
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '(' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		for(j = 0; j < 3; j++)
		{
			token = COM_ParseExt(&buf_p, qfalse);
			frame->bounds[0][j] = atof(token);
		}
		
		// skip )
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected ')' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		// skip (
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '(' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		for(j = 0; j < 3; j++)
		{
			token = COM_ParseExt(&buf_p, qfalse);
			frame->bounds[1][j] = atof(token);
		}
		
		// skip )
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected ')' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
	}
	
	// parse }
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "}"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '}' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	
	
	// parse baseframe {
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "baseframe"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'baseframe' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	if(Q_stricmp(token, "{"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '{' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	
	for(i = 0, channel = anim->channels; i < anim->numChannels; i++, channel++)
	{
		// skip (
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '(' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		for(j = 0; j < 3; j++)
		{
			token = COM_ParseExt(&buf_p, qfalse);
			channel->baseOrigin[j] = atof(token);
		}
		
		// skip )
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected ')' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		// skip (
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '(' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		for(j = 0; j < 3; j++)
		{
			token = COM_ParseExt(&buf_p, qfalse);
			channel->baseQuat[j] = atof(token);
		}
		QuatCalcW(channel->baseQuat);
		
		// skip )
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected ')' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
	}
	
	// parse }
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "}"))
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '}' found '%s' in model '%s'\n", token, name);
		return qfalse;
	}
	
	
	for(i = 0, frame = anim->frames; i < anim->numFrames; i++, frame++)
	{
		// parse frame <number> {
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "frame"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected 'baseframe' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, va("%i", i)))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '%i' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "{"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '{' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
		
		frame->components = ri.Hunk_Alloc(sizeof(float) * anim->numAnimatedComponents, h_low);
		for(j = 0; j < anim->numAnimatedComponents; j++)
		{
			token = COM_ParseExt(&buf_p, qtrue);
			frame->components[j] = atof(token);
		}
		
		// parse }
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "}"))
		{
			ri.Printf(PRINT_WARNING, "RE_RegisterAnimation: expected '}' found '%s' in model '%s'\n", token, name);
			return qfalse;
		}
	}
	
	// everything went ok
	Com_DPrintf( "Registration succeeded\n" );
	anim->type = AT_MD5;

	ri.FS_FreeFile(buffer);

	return anim->index;
}


/*
================
R_GetAnimationByHandle
================
*/
md5Animation_t        *R_GetAnimationByHandle(qhandle_t index)
{
	md5Animation_t *anim;

	// out of range gets the default animation
	if(index < 1 || index >= tr.numAnimations)
	{
		return tr.animations[0];
	}

	anim = tr.animations[index];

	return anim;
}

/*
================
R_AnimationList_f
================
*/
void R_AnimationList_f(void)
{
	int             i;
	md5Animation_t *anim;

	for(i = 0; i < tr.numAnimations; i++)
	{
		anim = tr.animations[i];
		
		ri.Printf(PRINT_ALL, "%s\n", anim->name);
	}
	ri.Printf(PRINT_ALL, "%8i : Total animations\n", tr.numAnimations);
}

/*
=============
R_CullMD5
=============
*/
static void R_CullMD5(trRefEntity_t * ent)
{
	int             i;
	
	if(ent->e.skeleton.type == SK_INVALID)
	{
		// we have a bad configuration
		ClearBounds(ent->localBounds[0], ent->localBounds[1]);
		tr.pc.c_box_cull_md5_in++;
		ent->cull = CULL_IN;
		return;	
	}
	
	// copy a bounding box in the current coordinate system provided by skeleton
	for(i = 0; i < 3; i++)
	{
		ent->localBounds[0][i] = ent->e.skeleton.bounds[0][i];
		ent->localBounds[1][i] = ent->e.skeleton.bounds[1][i];
	}
	
	switch (R_CullLocalBox(ent->localBounds))
	{
		case CULL_IN:
			tr.pc.c_box_cull_md5_in++;
			ent->cull = CULL_IN;
			return;
			
		case CULL_CLIP:
			tr.pc.c_box_cull_md5_clip++;
			ent->cull = CULL_CLIP;
			return;
			
		case CULL_OUT:
		default:
			tr.pc.c_box_cull_md5_out++;
			ent->cull = CULL_OUT;
			return;
	}
}

/*
==============
R_AddMD5Surfaces
==============
*/
void R_AddMD5Surfaces(trRefEntity_t * ent)
{
	md5Model_t     *model;
	md5Surface_t   *surface;
	shader_t       *shader;
	int             i;
	int             fogNum = 0;
	qboolean        personalModel;
	
	model = tr.currentModel->md5;
	
	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;

	/*
	if(ent->e.renderfx & RF_WRAP_FRAMES)
	{
		ent->e.frame %= header->numFrames;
		ent->e.oldframe %= header->numFrames;
	}
	
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	if((ent->e.frame >= header->numFrames) || (ent->e.frame < 0) || (ent->e.oldframe >= header->numFrames) || (ent->e.oldframe < 0))
	{
		ri.Printf(PRINT_DEVELOPER, "R_AddMDSSurfaces: no such frame %d to %d for '%s'\n",
				  ent->e.oldframe, ent->e.frame, tr.currentModel->name);
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}
	*/
	
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum
	R_CullMD5(ent);
	if(ent->cull == CULL_OUT)
	{
		return;
	}
	
	// set up world bounds for light intersection tests
	R_SetupEntityWorldBounds(ent);
	
	// set up lighting now that we know we aren't culled
	if(!personalModel || r_shadows->integer > 1)
	{
		R_SetupEntityLighting(&tr.refdef, ent);
	}

	// FIXME: see if we are in a fog volume
	//fogNum = R_ComputeFogNumForMDS(header, ent);

	// finally add surfaces
	for(i = 0, surface = model->surfaces; i < model->numSurfaces; i++, surface++)
	{
		if(ent->e.customShader)
		{
			shader = R_GetShaderByHandle(ent->e.customShader);
		}
		/*
		else if(ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins)
		{
			skin_t         *skin;
			int             j;

			skin = R_GetSkinByHandle(ent->e.customSkin);

			// match the surface name to something in the skin file
			shader = tr.defaultShader;
			for(j = 0; j < skin->numSurfaces; j++)
			{
				// the names have both been lowercased
				if(!strcmp(skin->surfaces[j]->name, surface->name))
				{
					shader = skin->surfaces[j]->shader;
					break;
				}
			}
			if(shader == tr.defaultShader)
			{
				ri.Printf(PRINT_DEVELOPER, "WARNING: no shader for surface %s in skin %s\n", surface->name, skin->name);
			}
			else if(shader->defaultShader)
			{
				ri.Printf(PRINT_DEVELOPER, "WARNING: shader %s in skin %s not found\n", shader->name, skin->name);
			}
		}
		*/
		else
		{
			shader = R_GetShaderByHandle(surface->shaderIndex);
		}
		
		// we will add shadows even if the main object isn't visible in the view

		// projection shadows work fine with personal models 	 
		if(r_shadows->integer == 2 && fogNum == 0 && (ent->e.renderfx & RF_SHADOW_PLANE) && shader->sort == SS_OPAQUE) 	 
		{ 	 
			R_AddDrawSurf((void *)surface, tr.projectionShadowShader, -1, 0); 	 
		}

		// don't add third_person objects if not viewing through a portal
		if(!personalModel)
		{
			R_AddDrawSurf((void *)surface, shader, -1, fogNum);
		}
	}
}

/*
=================
R_AddMD5Interactions
=================
*/
void R_AddMD5Interactions(trRefEntity_t * ent, trRefDlight_t * light)
{
	int             i;
	md5Model_t     *model;
	md5Surface_t   *surface;
	shader_t       *shader = 0;
	qboolean        personalModel;
	interactionType_t iaType = IA_DEFAULT;
	
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum and we don't care about proper shadowing
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

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;

	model = tr.currentModel->md5;

	// do a quick AABB cull
	if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], ent->worldBounds[0], ent->worldBounds[1]))
	{
		tr.pc.c_dlightSurfacesCulled += model->numSurfaces;
		return;
	}

	// do a more expensive and precise light frustum cull
	if(!r_noLightFrustums->integer)
	{
		if(R_CullLightWorldBounds(light, ent->worldBounds) == CULL_OUT)
		{
			tr.pc.c_dlightSurfacesCulled += model->numSurfaces;
			return;
		}
	}

	// generate interactions with all surfaces
	for(i = 0, surface = model->surfaces; i < model->numSurfaces; i++, surface++)
	{
		if(ent->e.customShader)
		{
			shader = R_GetShaderByHandle(ent->e.customShader);
		}
		/*
		else if(ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins)
		{
			skin_t         *skin;
			int             j;

			skin = R_GetSkinByHandle(ent->e.customSkin);

			// match the surface name to something in the skin file
			shader = tr.defaultShader;
			for(j = 0; j < skin->numSurfaces; j++)
			{
				// the names have both been lowercased
				if(!strcmp(skin->surfaces[j]->name, surface->name))
				{
					shader = skin->surfaces[j]->shader;
					break;
				}
			}
			if(shader == tr.defaultShader)
			{
				ri.Printf(PRINT_DEVELOPER, "WARNING: no shader for surface %s in skin %s\n", surface->name,
						  skin->name);
			}
			else if(shader->defaultShader)
			{
				ri.Printf(PRINT_DEVELOPER, "WARNING: shader %s in skin %s not found\n", shader->name,
						  skin->name);
			}
		}
		*/
		else
		{
			shader = R_GetShaderByHandle(surface->shaderIndex);
		}
		
		// skip all surfaces that don't matter for lighting only pass
		if(shader->isSky || (!shader->interactLight && shader->noShadows))
			continue;
		
		// we will add shadows even if the main object isn't visible in the view

		// don't add third_person objects if not viewing through a portal
		if(!personalModel)
		{
			R_AddDlightInteraction(light, (void *)surface, shader, 0, NULL, 0, NULL, iaType);
			tr.pc.c_dlightSurfaces++;
		}
	}
}

/*
==============
RE_BuildSkeleton
==============
*/
int RE_BuildSkeleton(refSkeleton_t * skel, qhandle_t hAnim, int startFrame, int endFrame, float frac, qboolean clearOrigin)
{
	int             i;
	md5Animation_t *anim;
	md5Channel_t   *channel;
	md5Frame_t     *newFrame, *oldFrame;
	vec3_t          newOrigin, oldOrigin, lerpedOrigin;
	quat_t          newQuat, oldQuat, lerpedQuat;
	int             componentsApplied;

	anim = R_GetAnimationByHandle(hAnim);
	
	if(anim->type == AT_MD5)
	{
		// Validate the frames so there is no chance of a crash.
		// This will write directly into the entity structure, so
		// when the surfaces are rendered, they don't need to be
		// range checked again.
		/*
		if((startFrame >= anim->numFrames) || (startFrame < 0) || (endFrame >= anim->numFrames) || (endFrame < 0))
		{
		   ri.Printf(PRINT_DEVELOPER, "RE_BuildSkeleton: no such frame %d to %d for '%s'\n", startFrame, endFrame, anim->name);
		   //startFrame = 0;
		   //endFrame = 0;
		}
		 */
		
		Q_clamp(startFrame, 0, anim->numFrames - 1);
		Q_clamp(endFrame, 0, anim->numFrames - 1);
		
		// compute frame pointers
		oldFrame = &anim->frames[startFrame];
		newFrame = &anim->frames[endFrame];
	
		// calculate a bounding box in the current coordinate system
		for(i = 0; i < 3; i++)
		{
			skel->bounds[0][i] =
				oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
			skel->bounds[1][i] =
				oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
		}
		
		for(i = 0, channel = anim->channels; i < anim->numChannels; i++, channel++)
		{
			// set baseframe values
			VectorCopy(channel->baseOrigin, newOrigin);
			VectorCopy(channel->baseOrigin, oldOrigin);
			
			QuatCopy(channel->baseQuat, newQuat);
			QuatCopy(channel->baseQuat, oldQuat);
			
			componentsApplied = 0;
			
			// update tranlation bits
			if(channel->componentsBits & COMPONENT_BIT_TX)
			{
				oldOrigin[0] = oldFrame->components[channel->componentsOffset + componentsApplied];
				newOrigin[0] = newFrame->components[channel->componentsOffset + componentsApplied];
				componentsApplied++;
			}
			
			if(channel->componentsBits & COMPONENT_BIT_TY)
			{
				oldOrigin[1] = oldFrame->components[channel->componentsOffset + componentsApplied];
				newOrigin[1] = newFrame->components[channel->componentsOffset + componentsApplied];
				componentsApplied++;
			}
			
			if(channel->componentsBits & COMPONENT_BIT_TZ)
			{
				oldOrigin[2] = oldFrame->components[channel->componentsOffset + componentsApplied];
				newOrigin[2] = newFrame->components[channel->componentsOffset + componentsApplied];
				componentsApplied++;
			}
			
			// update quaternion rotation bits
			if(channel->componentsBits & COMPONENT_BIT_QX)
			{
				((vec_t*)oldQuat)[0] = oldFrame->components[channel->componentsOffset + componentsApplied];
				((vec_t*)newQuat)[0] = newFrame->components[channel->componentsOffset + componentsApplied];
				componentsApplied++;
			}
			
			if(channel->componentsBits & COMPONENT_BIT_QY)
			{
				((vec_t*)oldQuat)[1] = oldFrame->components[channel->componentsOffset + componentsApplied];
				((vec_t*)newQuat)[1] = newFrame->components[channel->componentsOffset + componentsApplied];
				componentsApplied++;
			}
			
			if(channel->componentsBits & COMPONENT_BIT_QZ)
			{
				((vec_t*)oldQuat)[2] = oldFrame->components[channel->componentsOffset + componentsApplied];
				((vec_t*)newQuat)[2] = newFrame->components[channel->componentsOffset + componentsApplied];
			}
			
			QuatCalcW(oldQuat);
			QuatNormalize(oldQuat);

			QuatCalcW(newQuat);
			QuatNormalize(newQuat);
			
#if 1
			TR_VectorLerp(oldOrigin, newOrigin, frac, lerpedOrigin);
			QuatSlerp(oldQuat, newQuat, frac, lerpedQuat);
#else
			VectorCopy(newOrigin, lerpedOrigin);
			QuatCopy(newQuat, lerpedQuat);
#endif
					
			// copy lerped information to the bone + extra data
			skel->bones[i].parentIndex = channel->parentIndex;
			
			if(channel->parentIndex < 0 && clearOrigin)
			{
				VectorClear(skel->bones[i].origin);
				QuatClear(skel->bones[i].rotation);

				// move bounding box back
				VectorSubtract(skel->bounds[0], lerpedOrigin, skel->bounds[0]);
				VectorSubtract(skel->bounds[1], lerpedOrigin, skel->bounds[1]);
			}
			else
			{
				VectorCopy(lerpedOrigin, skel->bones[i].origin);
			}
			
			QuatCopy(lerpedQuat, skel->bones[i].rotation);
		}
		
		skel->numBones = anim->numChannels;
		skel->type = SK_RELATIVE;
		return qtrue;
	}
	
	ri.Printf(PRINT_WARNING, "RE_BuildSkeleton: bad animation '%s' with handle %i\n", anim->name, hAnim);
	
	// FIXME: clear existing bones and bounds?
	return qfalse;
}


/*
==============

==============
*/
int RE_BlendSkeleton(refSkeleton_t * skel, const refSkeleton_t * blend, float frac)
{
	int             i;
	vec3_t			lerpedOrigin;
	quat_t          lerpedQuat;
	vec3_t          bounds[2];
	
	if(skel->numBones != blend->numBones)
	{
		ri.Printf(PRINT_WARNING, "RE_BlendSkeleton: different number of bones %d != %d\n", skel->numBones, blend->numBones);
		return qfalse;
	}
	
	// lerp between the 2 bone poses
	for(i = 0; i < skel->numBones; i++)
	{
		TR_VectorLerp(skel->bones[i].origin, blend->bones[i].origin, frac, lerpedOrigin);
		QuatSlerp(skel->bones[i].rotation, blend->bones[i].rotation, frac, lerpedQuat);
		
		VectorCopy(lerpedOrigin, skel->bones[i].origin);
		QuatCopy(lerpedQuat, skel->bones[i].rotation);
	}
	
	// calculate a bounding box in the current coordinate system
	for(i = 0; i < 3; i++)
	{
		bounds[0][i] = skel->bounds[0][i] < blend->bounds[0][i] ? skel->bounds[0][i] : blend->bounds[0][i];
		bounds[1][i] = skel->bounds[1][i] > blend->bounds[1][i] ? skel->bounds[1][i] : blend->bounds[1][i];
	}
	VectorCopy(bounds[0], skel->bounds[0]);
	VectorCopy(bounds[1], skel->bounds[1]);
	
	return qtrue;
}


/*
==============
RE_AnimNumFrames
==============
*/
int RE_AnimNumFrames(qhandle_t hAnim)
{
	md5Animation_t *anim;

	anim = R_GetAnimationByHandle(hAnim);

	if(anim->type == AT_MD5)
	{
		return anim->numFrames;
	}

	return 0;
}


/*
==============
RE_AnimFrameRate
==============
*/
int RE_AnimFrameRate(qhandle_t hAnim)
{
	md5Animation_t *anim;

	anim = R_GetAnimationByHandle(hAnim);

	if(anim->type == AT_MD5)
	{
		return anim->frameRate;
	}

	return 0;
}
