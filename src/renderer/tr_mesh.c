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
// tr_mesh.c -- triangle model functions
#include "tr_local.h"

/*
=============
R_CullMDX
=============
*/
static void R_CullMDX(mdxModel_t * model, trRefEntity_t * ent)
{
	mdxFrame_t     *oldFrame, *newFrame;
	int             i;
	vec3_t          v;
	vec3_t          transformed;

	// compute frame pointers
	newFrame = model->frames + ent->e.frame;
	oldFrame = model->frames + ent->e.oldframe;

	// calculate a bounding box in the current coordinate system
	for(i = 0; i < 3; i++)
	{
		ent->localBounds[0][i] =
			oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
		ent->localBounds[1][i] =
			oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
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

	// cull bounding sphere ONLY if this is not an upscaled entity
	if(!ent->e.nonNormalizedAxes)
	{
		if(ent->e.frame == ent->e.oldframe)
		{
			switch (R_CullLocalPointAndRadius(newFrame->localOrigin, newFrame->radius))
			{
				case CULL_OUT:
					tr.pc.c_sphere_cull_mdx_out++;
					ent->cull = CULL_OUT;
					return;

				case CULL_IN:
					tr.pc.c_sphere_cull_mdx_in++;
					ent->cull = CULL_IN;
					return;

				case CULL_CLIP:
					tr.pc.c_sphere_cull_mdx_clip++;
					break;
			}
		}
		else
		{
			int             sphereCull, sphereCullB;

			sphereCull = R_CullLocalPointAndRadius(newFrame->localOrigin, newFrame->radius);
			if(newFrame == oldFrame)
			{
				sphereCullB = sphereCull;
			}
			else
			{
				sphereCullB = R_CullLocalPointAndRadius(oldFrame->localOrigin, oldFrame->radius);
			}

			if(sphereCull == sphereCullB)
			{
				if(sphereCull == CULL_OUT)
				{
					tr.pc.c_sphere_cull_mdx_out++;
					ent->cull = CULL_OUT;
					return;
				}
				else if(sphereCull == CULL_IN)
				{
					tr.pc.c_sphere_cull_mdx_in++;
					ent->cull = CULL_IN;
					return;
				}
				else
				{
					tr.pc.c_sphere_cull_mdx_clip++;
				}
			}
		}
	}

	switch (R_CullLocalBox(ent->localBounds))
	{
		case CULL_IN:
			tr.pc.c_box_cull_mdx_in++;
			ent->cull = CULL_IN;
			return;

		case CULL_CLIP:
			tr.pc.c_box_cull_mdx_clip++;
			ent->cull = CULL_CLIP;
			return;

		case CULL_OUT:
		default:
			tr.pc.c_box_cull_mdx_out++;
			ent->cull = CULL_OUT;
			return;
	}
}



/*
=================
R_ComputeLOD
=================
*/
int R_ComputeLOD(trRefEntity_t * ent)
{
	float           radius;
	float           flod, lodscale;
	float           projectedRadius;
	mdxFrame_t     *frame;
	int             lod;

	if(tr.currentModel->numLods < 2)
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		// multiple LODs exist, so compute projected bounding sphere
		// and use that as a criteria for selecting LOD

		frame = tr.currentModel->mdx[0]->frames;
		frame += ent->e.frame;

		radius = RadiusFromBounds(frame->bounds[0], frame->bounds[1]);

		if((projectedRadius = R_ProjectRadius(radius, ent->e.origin)) != 0)
		{
			lodscale = r_lodscale->value;
			if(lodscale > 20)
				lodscale = 20;
			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.currentModel->numLods;
		lod = Q_ftol(flod);

		if(lod < 0)
		{
			lod = 0;
		}
		else if(lod >= tr.currentModel->numLods)
		{
			lod = tr.currentModel->numLods - 1;
		}
	}

	lod += r_lodbias->integer;

	if(lod >= tr.currentModel->numLods)
		lod = tr.currentModel->numLods - 1;
	if(lod < 0)
		lod = 0;

	return lod;
}

/*
=================
R_AddMDXSurfaces
=================
*/
void R_AddMDXSurfaces(trRefEntity_t * ent)
{
	int             i;
	mdxModel_t     *model = 0;
	mdxSurface_t   *surface = 0;
	shader_t       *shader = 0;
	int             lod;
	qboolean        personalModel;

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;

	if(ent->e.renderfx & RF_WRAP_FRAMES)
	{
		ent->e.frame %= tr.currentModel->mdx[0]->numFrames;
		ent->e.oldframe %= tr.currentModel->mdx[0]->numFrames;
	}

	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	if((ent->e.frame >= tr.currentModel->mdx[0]->numFrames)
	   || (ent->e.frame < 0) || (ent->e.oldframe >= tr.currentModel->mdx[0]->numFrames) || (ent->e.oldframe < 0))
	{
		ri.Printf(PRINT_DEVELOPER, "R_AddMDXSurfaces: no such frame %d to %d for '%s'\n",
				  ent->e.oldframe, ent->e.frame, tr.currentModel->name);
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	// compute LOD
	lod = R_ComputeLOD(ent);

	model = tr.currentModel->mdx[lod];

	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	R_CullMDX(model, ent);
	if(ent->cull == CULL_OUT)
	{
		return;
	}

	// set up lighting now that we know we aren't culled
	if(!personalModel || r_shadows->integer > 1)
	{
		R_SetupEntityLighting(&tr.refdef, ent);
	}

	// draw all surfaces
	if(r_vboModels->integer && model->numVBOSurfaces)
	{
		int             i;
		srfVBOMesh_t   *vboSurface;
		shader_t       *shader;

		for(i = 0; i < model->numVBOSurfaces; i++)
		{
			vboSurface = model->vboSurfaces[i];
			shader = vboSurface->shader;

			// don't add third_person objects if not viewing through a portal
			if(!personalModel)
			{
				R_AddDrawSurf((void *)vboSurface, shader, -1);
			}
		}
	}
	else
	{
		for(i = 0, surface = model->surfaces; i < model->numSurfaces; i++, surface++)
		{
			if(ent->e.customShader)
			{
				shader = R_GetShaderByHandle(ent->e.customShader);
			}
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
			else
			{
				shader = surface->shader;
			}
			/*
			   else if(surface->numShaders <= 0)
			   {
			   shader = tr.defaultShader;
			   }
			   else
			   {
			   mdxShader = surface->shaders;
			   mdxShader += ent->e.skinNum % surface->numShaders;
			   shader = tr.shaders[mdxShader->shaderIndex];
			   }
			 */

			// we will add shadows even if the main object isn't visible in the view

			// projection shadows work fine with personal models
			if(r_shadows->integer == 2 && (ent->e.renderfx & RF_SHADOW_PLANE) && shader->sort == SS_OPAQUE)
			{
				R_AddDrawSurf((void *)surface, tr.projectionShadowShader, -1);
			}

			// don't add third_person objects if not viewing through a portal
			if(!personalModel)
			{
				R_AddDrawSurf((void *)surface, shader, -1);
			}
		}
	}
}

/*
=================
R_AddMDXInteractions
=================
*/
void R_AddMDXInteractions(trRefEntity_t * ent, trRefLight_t * light)
{
	int             i;
	mdxModel_t     *model = 0;
	mdxSurface_t   *surface = 0;
	shader_t       *shader = 0;
	int             lod;
	qboolean        personalModel;
	byte            cubeSideBits;
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

	// avoid drawing of certain objects
	if(light->l.inverseShadows)
	{
		if(iaType != IA_LIGHTONLY && (light->l.noShadowID && (light->l.noShadowID != ent->e.noShadowID)))
			return;
	}
	else
	{
		if(iaType != IA_LIGHTONLY && (light->l.noShadowID && (light->l.noShadowID == ent->e.noShadowID)))
			return;
	}

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;

	// compute LOD
	lod = R_ComputeLOD(ent);

	model = tr.currentModel->mdx[lod];

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

	cubeSideBits = R_CalcLightCubeSideBits(light, ent->worldBounds);

	// generate interactions with all surfaces
	if(r_vboModels->integer && model->numVBOSurfaces)
	{
		// new brute force method: just render everthing with static VBOs
		int             i;
		srfVBOMesh_t   *vboSurface;
		shader_t       *shader;

		if(r_shadows->integer == 3)
		{
			// add shadow interactions because we cannot use shadow volumes with static VBOs ..
			for(i = 0, surface = model->surfaces; i < model->numSurfaces; i++, surface++)
			{
				if(ent->e.customShader)
				{
					shader = R_GetShaderByHandle(ent->e.customShader);
				}
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
				else
				{
					shader = surface->shader;
				}

				// skip all surfaces that don't matter for lighting only pass
				if(shader->isSky || !shader->interactLight || shader->noShadows)
					continue;

				// we will add shadows even if the main object isn't visible in the view

				// don't add third_person objects if not viewing through a portal
				if(!personalModel)
				{
					R_AddLightInteraction(light, (void *)surface, shader, cubeSideBits, IA_SHADOWONLY);
					tr.pc.c_dlightSurfaces++;
				}
			}

			// use static VBOs for lighting only
			for(i = 0; i < model->numVBOSurfaces; i++)
			{
				vboSurface = model->vboSurfaces[i];
				shader = vboSurface->shader;

				// skip all surfaces that don't matter for lighting only pass
				if(shader->isSky || (!shader->interactLight && shader->noShadows))
					continue;

				// don't add third_person objects if not viewing through a portal
				if(!personalModel)
				{
					R_AddLightInteraction(light, (void *)vboSurface, shader, cubeSideBits, IA_LIGHTONLY);
					tr.pc.c_dlightSurfaces++;
				}
			}
		}
		else
		{
			// static VBOs are fine for lighting and shadow mapping
			for(i = 0; i < model->numVBOSurfaces; i++)
			{
				vboSurface = model->vboSurfaces[i];
				shader = vboSurface->shader;

				// skip all surfaces that don't matter for lighting only pass
				if(shader->isSky || (!shader->interactLight && shader->noShadows))
					continue;

				// we will add shadows even if the main object isn't visible in the view

				// don't add third_person objects if not viewing through a portal
				if(!personalModel)
				{
					R_AddLightInteraction(light, (void *)vboSurface, shader, cubeSideBits, iaType);
					tr.pc.c_dlightSurfaces++;
				}
			}
		}
	}
	else
	{
		for(i = 0, surface = model->surfaces; i < model->numSurfaces; i++, surface++)
		{
			if(ent->e.customShader)
			{
				shader = R_GetShaderByHandle(ent->e.customShader);
			}
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
			else
			{
				shader = surface->shader;
			}
			/*
			   else if(surface->numShaders <= 0)
			   {
			   shader = tr.defaultShader;
			   }
			   else
			   {
			   mdxShader = surface->shaders;
			   mdxShader += ent->e.skinNum % surface->numShaders;
			   shader = tr.shaders[mdxShader->shaderIndex];
			   }
			 */

			// skip all surfaces that don't matter for lighting only pass
			if(shader->isSky || (!shader->interactLight && shader->noShadows))
				continue;

			// we will add shadows even if the main object isn't visible in the view

			// don't add third_person objects if not viewing through a portal
			if(!personalModel)
			{
				R_AddLightInteraction(light, (void *)surface, shader, cubeSideBits, iaType);
				tr.pc.c_dlightSurfaces++;
			}
		}
	}
}
