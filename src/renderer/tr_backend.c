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
// tr_backend.c
#include "tr_local.h"

backEndData_t  *backEndData[SMP_FRAMES];
backEndState_t  backEnd;

void GL_Bind(image_t * image)
{
	int             texnum;

	if(!image)
	{
		ri.Printf(PRINT_WARNING, "GL_Bind: NULL image\n");
		texnum = tr.defaultImage->texnum;
	}
	else
	{
		if(r_logFile->integer)
		{
			// don't just call LogComment, or we will get a call to va() every frame!
			GLimp_LogComment(va("--- GL_Bind( %s ) ---\n", image->name));
		}

		texnum = image->texnum;
	}

	if(r_nobind->integer && tr.blackImage)
	{
		// performance evaluation option
		texnum = tr.blackImage->texnum;
	}

	if(glState.currenttextures[glState.currenttmu] != texnum)
	{
		image->frameUsed = tr.frameCount;
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture(image->type, texnum);
	}
}

void GL_Program(GLhandleARB program)
{
	if(glConfig2.shadingLanguage100Available)
	{
		if(glState.currentProgram != program)
		{
			glState.currentProgram = program;
			qglUseProgramObjectARB(program);
		}
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture(int unit)
{
	if(glState.currenttmu == unit)
	{
		return;
	}

	if(unit >= 0 && unit <= 7)
	{
		qglActiveTextureARB(GL_TEXTURE0_ARB + unit);
		qglClientActiveTextureARB(GL_TEXTURE0_ARB + unit);

		if(r_logFile->integer)
		{
			GLimp_LogComment(va("glActiveTextureARB( GL_TEXTURE%i_ARB )\n", unit));
			GLimp_LogComment(va("glClientActiveTextureARB( GL_TEXTURE%i_ARB )\n", unit));
		}
	}
	else
	{
		ri.Error(ERR_DROP, "GL_SelectTexture: unit = %i", unit);
	}

	glState.currenttmu = unit;
}


/*
** GL_BindMultitexture
*/
void GL_BindMultitexture(image_t * image0, GLuint env0, image_t * image1, GLuint env1)
{
	int             texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if(r_nobind->integer && tr.blackImage)
	{
		// performance evaluation option
		texnum0 = texnum1 = tr.blackImage->texnum;
	}

	if(glState.currenttextures[1] != texnum1)
	{
		GL_SelectTexture(1);
		image1->frameUsed = tr.frameCount;
		glState.currenttextures[1] = texnum1;
		qglBindTexture(GL_TEXTURE_2D, texnum1);
	}
	if(glState.currenttextures[0] != texnum0)
	{
		GL_SelectTexture(0);
		image0->frameUsed = tr.frameCount;
		glState.currenttextures[0] = texnum0;
		qglBindTexture(GL_TEXTURE_2D, texnum0);
	}
}


/*
** GL_Cull
*/
void GL_Cull(int cullType)
{
	if(glState.faceCulling == cullType)
	{
		return;
	}

	glState.faceCulling = cullType;

	if(cullType == CT_TWO_SIDED)
	{
		qglDisable(GL_CULL_FACE);
	}
	else
	{
		qglEnable(GL_CULL_FACE);

		if(cullType == CT_BACK_SIDED)
		{
			if(backEnd.viewParms.isMirror)
			{
				qglCullFace(GL_FRONT);
			}
			else
			{
				qglCullFace(GL_BACK);
			}
		}
		else
		{
			if(backEnd.viewParms.isMirror)
			{
				qglCullFace(GL_BACK);
			}
			else
			{
				qglCullFace(GL_FRONT);
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv(int env)
{
	if(env == glState.texEnv[glState.currenttmu])
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch (env)
	{
		case GL_MODULATE:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			break;
		case GL_REPLACE:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			break;
		case GL_DECAL:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			break;
		case GL_ADD:
			qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			break;
		default:
			ri.Error(ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env);
			break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State(unsigned long stateBits)
{
	unsigned long   diff = stateBits ^ glState.glStateBits;

	if(!diff)
	{
		return;
	}

	// check depthFunc bits
	if(diff & GLS_DEPTHFUNC_BITS)
	{
		switch (stateBits & GLS_DEPTHFUNC_BITS)
		{
			case 0:
				qglDepthFunc(GL_LEQUAL);
				break;
			case GLS_DEPTHFUNC_LESS:
				qglDepthFunc(GL_LESS);
				break;
			case GLS_DEPTHFUNC_EQUAL:
				qglDepthFunc(GL_EQUAL);
				break;
			default:
				assert(0);
				break;
		}
	}

	// check blend bits
	if(diff & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
	{
		GLenum          srcFactor, dstFactor;

		if(stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))
		{
			switch (stateBits & GLS_SRCBLEND_BITS)
			{
				case GLS_SRCBLEND_ZERO:
					srcFactor = GL_ZERO;
					break;
				case GLS_SRCBLEND_ONE:
					srcFactor = GL_ONE;
					break;
				case GLS_SRCBLEND_DST_COLOR:
					srcFactor = GL_DST_COLOR;
					break;
				case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
					srcFactor = GL_ONE_MINUS_DST_COLOR;
					break;
				case GLS_SRCBLEND_SRC_ALPHA:
					srcFactor = GL_SRC_ALPHA;
					break;
				case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
					srcFactor = GL_ONE_MINUS_SRC_ALPHA;
					break;
				case GLS_SRCBLEND_DST_ALPHA:
					srcFactor = GL_DST_ALPHA;
					break;
				case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
					srcFactor = GL_ONE_MINUS_DST_ALPHA;
					break;
				case GLS_SRCBLEND_ALPHA_SATURATE:
					srcFactor = GL_SRC_ALPHA_SATURATE;
					break;
				default:
					srcFactor = GL_ONE;	// to get warning to shut up
					ri.Error(ERR_DROP, "GL_State: invalid src blend state bits\n");
					break;
			}

			switch (stateBits & GLS_DSTBLEND_BITS)
			{
				case GLS_DSTBLEND_ZERO:
					dstFactor = GL_ZERO;
					break;
				case GLS_DSTBLEND_ONE:
					dstFactor = GL_ONE;
					break;
				case GLS_DSTBLEND_SRC_COLOR:
					dstFactor = GL_SRC_COLOR;
					break;
				case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
					dstFactor = GL_ONE_MINUS_SRC_COLOR;
					break;
				case GLS_DSTBLEND_SRC_ALPHA:
					dstFactor = GL_SRC_ALPHA;
					break;
				case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
					dstFactor = GL_ONE_MINUS_SRC_ALPHA;
					break;
				case GLS_DSTBLEND_DST_ALPHA:
					dstFactor = GL_DST_ALPHA;
					break;
				case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
					dstFactor = GL_ONE_MINUS_DST_ALPHA;
					break;
				default:
					dstFactor = GL_ONE;	// to get warning to shut up
					ri.Error(ERR_DROP, "GL_State: invalid dst blend state bits\n");
					break;
			}

			qglEnable(GL_BLEND);
			qglBlendFunc(srcFactor, dstFactor);
		}
		else
		{
			qglDisable(GL_BLEND);
		}
	}

	// check colormask
	if(diff & GLS_COLORMASK_BITS)
	{
		if(stateBits & GLS_COLORMASK_BITS)
		{
			qglColorMask((stateBits & GLS_REDMASK_FALSE) ? GL_FALSE : GL_TRUE,
						 (stateBits & GLS_GREENMASK_FALSE) ? GL_FALSE : GL_TRUE,
						 (stateBits & GLS_BLUEMASK_FALSE) ? GL_FALSE : GL_TRUE,
						 (stateBits & GLS_ALPHAMASK_FALSE) ? GL_FALSE : GL_TRUE);
		}
		else
		{
			qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}
	}

	// check depthmask
	if(diff & GLS_DEPTHMASK_TRUE)
	{
		if(stateBits & GLS_DEPTHMASK_TRUE)
		{
			qglDepthMask(GL_TRUE);
		}
		else
		{
			qglDepthMask(GL_FALSE);
		}
	}

	// fill/line mode
	if(diff & GLS_POLYMODE_LINE)
	{
		if(stateBits & GLS_POLYMODE_LINE)
		{
			qglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else
		{
			qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}

	// depthtest
	if(diff & GLS_DEPTHTEST_DISABLE)
	{
		if(stateBits & GLS_DEPTHTEST_DISABLE)
		{
			qglDisable(GL_DEPTH_TEST);
		}
		else
		{
			qglEnable(GL_DEPTH_TEST);
		}
	}

	// alpha test
	if(diff & GLS_ATEST_BITS)
	{
		switch (stateBits & GLS_ATEST_BITS)
		{
			case 0:
				qglDisable(GL_ALPHA_TEST);
				break;
			case GLS_ATEST_GT_0:
				qglEnable(GL_ALPHA_TEST);
				qglAlphaFunc(GL_GREATER, 0.0f);
				break;
			case GLS_ATEST_LT_80:
				qglEnable(GL_ALPHA_TEST);
				qglAlphaFunc(GL_LESS, 0.5f);
				break;
			case GLS_ATEST_GE_80:
				qglEnable(GL_ALPHA_TEST);
				qglAlphaFunc(GL_GEQUAL, 0.5f);
				break;
			case GLS_ATEST_GT_CUSTOM:
				// FIXME
				qglEnable(GL_ALPHA_TEST);
				qglAlphaFunc(GL_GREATER, 0.5f);
				break;
			default:
				assert(0);
				break;
		}
	}

	// stenciltest
	if(diff & GLS_STENCILTEST_ENABLE)
	{
		if(stateBits & GLS_STENCILTEST_ENABLE)
		{
			qglEnable(GL_STENCIL_TEST);
		}
		else
		{
			qglDisable(GL_STENCIL_TEST);
		}
	}

	glState.glStateBits = stateBits;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace(void)
{
	float           c;

	if(!backEnd.isHyperspace)
	{
		// do initialization shit
	}

	c = (backEnd.refdef.time & 255) / 255.0f;
	qglClearColor(c, c, c, 1);
	qglClear(GL_COLOR_BUFFER_BIT);

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor(void)
{
#if 0
	matrix_t        projectionMatrix;

	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	MatrixMultiply(backEnd.viewParms.projectionMatrix, quakeToOpenGLMatrix, projectionMatrix);

	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf(projectionMatrix);
	qglMatrixMode(GL_MODELVIEW);
#else
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
	qglMatrixMode(GL_MODELVIEW);
#endif

	// set the window clipping
	qglViewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
				backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
}


/*
================
RB_SetGL2D
================
*/
static void RB_SetGL2D(void)
{
	GLimp_LogComment("--- RB_SetGL2D ---\n");

	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho(0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

	qglDisable(GL_CULL_FACE);
	qglDisable(GL_CLIP_PLANE0);

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView(void)
{
	int             clearBits = 0;

	GLimp_LogComment("--- RB_BeginDrawingView ---\n");

	// sync with gl if needed
	if(r_finish->integer == 1 && !glState.finishCalled)
	{
		qglFinish();
		glState.finishCalled = qtrue;
	}
	if(r_finish->integer == 0)
	{
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// set the modelview matrix for the viewer
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State(GLS_DEFAULT);

	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if(r_measureOverdraw->integer || r_shadows->integer == 3)
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if(r_fastsky->integer && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
		qglClearColor(0.0f, 0.0f, 1.0f, 1.0f);	// FIXME: get color of sky
#else
		qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// FIXME: get color of sky
#endif
	}
	qglClear(clearBits);

	if((backEnd.refdef.rdflags & RDF_HYPERSPACE))
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;	// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];
		double          plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct(backEnd.viewParms.or.axis[0], plane);
		plane2[1] = DotProduct(backEnd.viewParms.or.axis[1], plane);
		plane2[2] = DotProduct(backEnd.viewParms.or.axis[2], plane);
		plane2[3] = DotProduct(plane, backEnd.viewParms.or.origin) - plane[3];

//      qglLoadIdentity();
		qglLoadMatrixf(quakeToOpenGLMatrix);
		qglClipPlane(GL_CLIP_PLANE0, plane2);
		qglEnable(GL_CLIP_PLANE0);
	}
	else
	{
		qglDisable(GL_CLIP_PLANE0);
	}
	
	GL_CheckErrors();
}

static void RB_RenderDrawSurfaces(float originalTime, drawSurf_t * drawSurfs, int numDrawSurfs, qboolean opaque)
{
	trRefEntity_t  *entity, *oldEntity;
	int             lightmapNum, oldLightmapNum;
	shader_t       *shader, *oldShader;
	int             fogNum, oldFogNum;
	qboolean        depthRange, oldDepthRange;
	int             i;
	drawSurf_t     *drawSurf;
	int             oldSort;

	GLimp_LogComment("--- RB_RenderDrawSurfaces ---\n");

	// draw everything
	oldEntity = NULL;
	oldShader = NULL;
	oldLightmapNum = -1;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	for(i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++)
	{
		// update locals
		entity = drawSurf->entity;
		shader = tr.sortedShaders[drawSurf->shaderNum];
		lightmapNum = drawSurf->lightmapNum;
		fogNum = drawSurf->fogNum;

		if(opaque)
		{
			// skip all translucent surfaces that don't matter for this pass
			if(shader->sort > SS_OPAQUE)
			{
				break;
			}
		}
		else
		{
			// skip all opaque surfaces that don't matter for this pass
			if(shader->sort <= SS_OPAQUE)
			{
				continue;
			}
		}

		if(entity == oldEntity && shader == oldShader && lightmapNum == oldLightmapNum && fogNum == oldFogNum)
		{
			// fast path, same as previous sort
			tess_surfaceTable[*drawSurf->surface] (drawSurf->surface, 0, NULL, 0, NULL);
			continue;
		}

		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if(shader != oldShader
		   || fogNum != oldFogNum || lightmapNum != oldLightmapNum || (entity != oldEntity && !shader->entityMergable))
		{
			if(oldShader != NULL)
			{
				Tess_End();
			}

			Tess_Begin(shader, NULL, lightmapNum, fogNum, qfalse, qfalse);
			oldShader = shader;
			oldLightmapNum = lightmapNum;
			oldFogNum = fogNum;
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				backEnd.currentEntity = entity;
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;

				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.or);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;

				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;
			}

			qglLoadMatrixf(backEnd.or.modelViewMatrix);

			// change depthrange if needed
			if(oldDepthRange != depthRange)
			{
				if(depthRange)
				{
					qglDepthRange(0, 0.3);
				}
				else
				{
					qglDepthRange(0, 1);
				}
				oldDepthRange = depthRange;
			}

			oldEntity = entity;
		}

		// add the triangles for this surface
		tess_surfaceTable[*drawSurf->surface] (drawSurf->surface, 0, NULL, 0, NULL);
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if(oldShader != NULL)
	{
		Tess_End();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}
	
	GL_CheckErrors();
}

/*
=================
RB_RenderInteractions
=================
*/
static void RB_RenderInteractions(float originalTime, interaction_t * interactions, int numInteractions)
{
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefDlight_t  *light, *oldLight;
	interaction_t  *ia;
	qboolean        depthRange, oldDepthRange;
	int             iaCount;
	surfaceType_t  *surface;
	vec3_t          tmp;
	matrix_t        modelToLight;

	GLimp_LogComment("--- RB_RenderInteractions ---\n");

	// draw everything
	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = qfalse;
	depthRange = qfalse;

	tess.currentStageIteratorType = SIT_LIGHTING;

	// render interactions
	for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
	{
		backEnd.currentLight = light = ia->dlight;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;
		
		if(glConfig2.occlusionQueryBits && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto nextInteraction;
		}
		
		if(!shader->interactLight)
		{
			// skip this interaction because the surface shader has no ability to interact with light
			// this will save texcoords and matrix calculations
			goto nextInteraction;
		}

		if(light != oldLight)
		{
			// set light scissor to reduce fillrate
			qglScissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);

			#if 0
			if(!light->additive)
			{
				GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);
			}
			else
			#endif
			{
				GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);
			}
		}

		// Tr3B - this should never happen in the first iteration
		if(!r_nobatching->integer && light == oldLight && entity == oldEntity && shader == oldShader)
		{
			if(ia->type != IA_SHADOWONLY)
			{
				// fast path, same as previous
				tess_surfaceTable[*surface] (surface, ia->numLightIndexes, ia->lightIndexes, 0, NULL);
				goto nextInteraction;
			}
		}

		// draw the contents of the last shader batch
		if(oldEntity != NULL || oldLight != NULL || oldShader != NULL)
		{
			Tess_End();
		}

		// we need a new batch
		Tess_Begin(shader, ia->dlightShader, -1, 0, qfalse, qfalse);

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.or);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;
			}

			qglLoadMatrixf(backEnd.or.modelViewMatrix);

			// change depthrange if needed
			if(oldDepthRange != depthRange)
			{
				if(depthRange)
				{
					qglDepthRange(0, 0.3);
				}
				else
				{
					qglDepthRange(0, 1);
				}
				oldDepthRange = depthRange;
			}
		}

		// change the attenuation matrix if needed
		if(light != oldLight || entity != oldEntity)
		{
			// transform light origin into model space for u_LightOrigin parameter
			if(entity != &tr.worldEntity)
			{
				VectorSubtract(light->origin, backEnd.or.origin, tmp);
				light->transformed[0] = DotProduct(tmp, backEnd.or.axis[0]);
				light->transformed[1] = DotProduct(tmp, backEnd.or.axis[1]);
				light->transformed[2] = DotProduct(tmp, backEnd.or.axis[2]);
			}
			else
			{
				VectorCopy(light->origin, light->transformed);
			}

			// build the attenuation matrix using the entity transform
			MatrixMultiply(light->viewMatrix, backEnd.or.transformMatrix, modelToLight);

			MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
			MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
			MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
			MatrixMultiply2(light->attenuationMatrix, modelToLight);
		}

		if(ia->type != IA_SHADOWONLY)
		{
			// add the triangles for this surface
			tess_surfaceTable[*surface] (surface, ia->numLightIndexes, ia->lightIndexes, 0, NULL);
		}

	  nextInteraction:
		if(!ia->next)
		{
			if(glConfig2.occlusionQueryBits && !ia->occlusionQuerySamples)
			{
				// do nothing
			}
			else if(!shader->interactLight)
			{
				// do nothing as well
			}
			else
			{
				// draw the contents of the current shader batch
				Tess_End();
			}
			
			if(iaCount < (numInteractions - 1))
			{
				// jump to next interaction and continue
				ia++;
				iaCount++;
			}
			else
			{
				// increase last time to leave for loop
				iaCount++;
			}
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
	qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor
	qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	// reset stage iterator
	tess.currentStageIteratorType = SIT_DEFAULT;
	
	GL_CheckErrors();
}


/*
=================
RB_RenderInteractionsStencilShadowed
=================
*/
static void RB_RenderInteractionsStencilShadowed(float originalTime, interaction_t * interactions, int numInteractions)
{
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefDlight_t  *light, *oldLight;
	interaction_t  *ia;
	int             iaCount;
	int             iaFirst = 0;
	surfaceType_t  *surface;
	qboolean        depthRange, oldDepthRange;
	vec3_t          tmp;
	matrix_t        modelToLight;
	qboolean        drawShadows;

	if(glConfig.stencilBits < 4 || !glConfig2.shadingLanguage100Available)
	{
		RB_RenderInteractions(originalTime, interactions, numInteractions);
		return;
	}

	GLimp_LogComment("--- RB_RenderInteractionsStencilShadowed ---\n");

	// draw everything
	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = qfalse;
	depthRange = qfalse;
	drawShadows = qtrue;

	tess.currentStageIteratorType = SIT_LIGHTING_STENCIL;

	// render interactions
	for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
	{
		backEnd.currentLight = light = ia->dlight;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;
		
		// only iaFirst == iaCount if first iteration or counters were reset
		if(light != oldLight || iaFirst == iaCount)
		{
			iaFirst = iaCount;

			if(drawShadows)
			{
				// set light scissor to reduce fillrate
				qglScissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);

				// set depth test to reduce fillrate
				if(qglDepthBoundsEXT)
				{
					if(!ia->noDepthBoundsTest)
					{
						qglEnable(GL_DEPTH_BOUNDS_TEST_EXT);
						qglDepthBoundsEXT(ia->depthNear, ia->depthFar);
					}
					else
					{
						qglDisable(GL_DEPTH_BOUNDS_TEST_EXT);
					}
				}
				
				// set the reference stencil value
				qglClearStencil(128);
				
				// reset stencil buffer
				qglClear(GL_STENCIL_BUFFER_BIT);
				
				// use less compare as depthfunc
				// don't write to the color buffer or depth buffer
				// enable stencil testing for this light
				GL_State(GLS_DEPTHFUNC_LESS | GLS_COLORMASK_BITS | GLS_STENCILTEST_ENABLE);

				qglStencilFunc(GL_ALWAYS, 128, ~0);
				qglStencilMask(~0);

				qglEnable(GL_POLYGON_OFFSET_FILL);
				qglPolygonOffset(r_shadowOffsetFactor->value, r_shadowOffsetUnits->value);

				// enable shadow volume extrusion shader
				#if 1
				GL_Program(tr.shadowShader.program);
				GL_ClientState(tr.shadowShader.attribs);
				#else
				GL_Program(0);
				GL_ClientState(GLCS_VERTEX);
				GL_SelectTexture(0);
				GL_Bind(tr.whiteImage);
				#endif

				qglVertexPointer(4, GL_FLOAT, 0, tess.xyz);
			}
			else
			{
				// Tr3B - see RobustShadowVolumes.pdf by Nvidia
				// Set stencil testing to render only pixels with a zero
				// stencil value, i.e., visible fragments illuminated by the
				// current light. Use equal depth testing to update only the
				// visible fragments, and then, increment stencil to avoid
				// double blending. Re-enable color buffer writes again.

				#if 0
				if(!light->additive)
				{
					GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL | GLS_STENCILTEST_ENABLE);
				}
				else
				#endif
				{
					GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL | GLS_STENCILTEST_ENABLE);
				}

				if(light->l.noShadows)
				{
					// don't consider shadow volumes
					qglStencilFunc(GL_ALWAYS, 128, ~0);
				}
				else
				{
					qglStencilFunc(GL_EQUAL, 128, ~0);
				}
				qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);//GL_INCR);

				qglDisable(GL_POLYGON_OFFSET_FILL);

				// disable shadow volume extrusion shader
				GL_Program(0);
			}
		}

		if(drawShadows)
		{
			if(!r_nobatching->integer && light == oldLight && entity == oldEntity && shader == oldShader)
			{
				if(!(entity->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK)) &&
				   shader->sort == SS_OPAQUE &&
				   !shader->noShadows &&
				   !light->l.noShadows &&
				   ia->type != IA_LIGHTONLY)
				{
					// fast path, same as previous
					tess_surfaceTable[*surface] (surface, 0, NULL, ia->numShadowIndexes, ia->shadowIndexes);
					goto nextInteraction;
				}
			}
			else
			{
				// draw the contents of the last shader batch
				if(oldEntity != NULL || oldLight != NULL || oldShader != NULL)
				{
					Tess_End();
				}

				// we don't need tangent space calculations here
				Tess_Begin(shader, ia->dlightShader, -1, 0, qtrue, qtrue);
			}
		}
		else
		{
			if(!r_nobatching->integer && light == oldLight && entity == oldEntity && shader == oldShader)
			{
				if(shader->interactLight && ia->type != IA_SHADOWONLY)
				{
					// fast path, same as previous
					tess_surfaceTable[*surface] (surface, ia->numLightIndexes, ia->lightIndexes, 0, NULL);
					goto nextInteraction;
				}
			}
			else
			{
				// draw the contents of the last shader batch
				if(oldEntity != NULL || oldLight != NULL || oldShader != NULL)
				{
					Tess_End();
				}

				Tess_Begin(shader, ia->dlightShader, -1, 0, qfalse, qfalse);
			}
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.or);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;
			}

			qglLoadMatrixf(backEnd.or.modelViewMatrix);

			// change depthrange if needed
			if(oldDepthRange != depthRange)
			{
				if(depthRange)
				{
					qglDepthRange(0, 0.3);
				}
				else
				{
					qglDepthRange(0, 1);
				}
				oldDepthRange = depthRange;
			}
		}

		// change the attenuation matrix if needed
		if(light != oldLight || entity != oldEntity)
		{
			// transform light origin into model space for u_LightOrigin parameter
			if(entity != &tr.worldEntity)
			{
				VectorSubtract(light->origin, backEnd.or.origin, tmp);
				light->transformed[0] = DotProduct(tmp, backEnd.or.axis[0]);
				light->transformed[1] = DotProduct(tmp, backEnd.or.axis[1]);
				light->transformed[2] = DotProduct(tmp, backEnd.or.axis[2]);
			}
			else
			{
				VectorCopy(light->origin, light->transformed);
			}

			if(drawShadows)
			{
				// set uniform parameter u_LightOrigin for GLSL shader
				#if 1
				qglUniform3fARB(tr.shadowShader.u_LightOrigin,
								light->transformed[0], light->transformed[1], light->transformed[2]);
				#endif
			}

			// build the attenuation matrix using the entity transform          
			MatrixMultiply(light->viewMatrix, backEnd.or.transformMatrix, modelToLight);

			MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
			MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
			MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
			MatrixMultiply2(light->attenuationMatrix, modelToLight);
		}

		if(drawShadows)
		{
			if(!(entity->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK)) &&
			   shader->sort == SS_OPAQUE &&
			   !shader->noShadows &&
			   !light->l.noShadows &&
			   ia->type != IA_LIGHTONLY)
			{
				// add the triangles for this surface
				tess_surfaceTable[*surface] (surface, 0, NULL, ia->numShadowIndexes, ia->shadowIndexes);
			}
		}
		else
		{
			if(shader->interactLight && ia->type != IA_SHADOWONLY)
			{
				// add the triangles for this surface
				tess_surfaceTable[*surface] (surface, ia->numLightIndexes, ia->lightIndexes, 0, NULL);
			}
		}

	  nextInteraction:
		if(!ia->next)
		{
			// if ia->next does not point to any other interaction then
			// this is the last interaction of the current light

			if(drawShadows)
			{
				// jump back to first interaction of this light and start lighting
				ia = &interactions[iaFirst];
				iaCount = iaFirst;
				drawShadows = qfalse;
			}
			else
			{
				if(iaCount < (numInteractions - 1))
				{
					// jump to next interaction and start shadowing
					ia++;
					iaCount++;
					drawShadows = qtrue;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}

			// draw the contents of the current shader batch
			Tess_End();
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
	qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor clamping
	qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	// reset depth clamping
	if(qglDepthBoundsEXT)
	{
		qglDisable(GL_DEPTH_BOUNDS_TEST_EXT);
	}

	// reset stage iterator
	tess.currentStageIteratorType = SIT_DEFAULT;
	
	GL_CheckErrors();
}

static void RB_RenderOcclusionQueries(interaction_t * interactions, int numInteractions)
{
	GLimp_LogComment("--- RB_RenderOcclusionQueries ---\n");
	
	if(glConfig2.occlusionQueryBits)
	{
		int				i;
		interaction_t  *ia;
		int             iaCount;
		int             iaFirst;
		trRefDlight_t  *light, *oldLight;
		int             ocCount;
		GLint           ocSamples = 0;
		qboolean        queryObjects;
		GLint			available;

		qglColor4f(1.0f, 0.0f, 0.0f, 0.05f);

		GL_Program(0);
		GL_Cull(CT_TWO_SIDED);
		GL_SelectTexture(0);
		qglDisable(GL_TEXTURE_2D);
		
		// don't write to the color buffer or depth buffer
		if(r_showOcclusionQueries->integer)
		{
			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
		}
		else
		{
			GL_State(GLS_COLORMASK_BITS);
		}

		// loop trough all light interactions and render the light OBB for each last interaction
		ocCount = -1;
		for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
		{
			backEnd.currentLight = light = ia->dlight;
			
			if(!ia->next)
			{
				ocCount++;
				
				// last interaction of current light
				if(ocCount < (MAX_OCCLUSION_QUERIES - 1) /*&& R_CullLightPoint(light, backEnd.viewParms.or.origin) == CULL_OUT */ )
				{
					R_RotateForDlight(light, &backEnd.viewParms, &backEnd.or);
					qglLoadMatrixf(backEnd.or.modelViewMatrix);
				
					// begin the occlusion query
					qglBeginQueryARB(GL_SAMPLES_PASSED, tr.occlusionQueryObjects[ocCount]);

					qglBegin(GL_QUADS);

					qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);

					qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);

					qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);

					qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
	
					qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
	
					qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
					qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
					qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);

					qglEnd();
				
					// end the query
					// don't read back immediately so that we give the query time to be ready
					qglEndQueryARB(GL_SAMPLES_PASSED);
				
					backEnd.pc.c_occlusionQueries++;
				}
				
				if(iaCount < (numInteractions - 1))
				{
					// jump to next interaction and continue
					ia++;
					iaCount++;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}
			else
			{
				// just continue
				ia = ia->next;
				iaCount++;
			}
		}

		// go back to the world modelview matrix
		backEnd.or = backEnd.viewParms.world;
		qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);
		
		if(!ocCount)
		{
			qglEnable(GL_TEXTURE_2D);
			return;	
		}
		
		qglFlush();
		
		// do other work until "most" of the queries are back, to avoid
        // wasting time spinning
        #if 1
        i = (int)(ocCount * 3 / 4); // instead of N-1, to prevent the GPU from going idle
        do
        {
        	i++;
        	
        	//if(i >= ocCount)
        	//	i = (int)(ocCount * 3 / 4);
        	
       		qglGetQueryObjectivARB(tr.occlusionQueryObjects[i], GL_QUERY_RESULT_AVAILABLE_ARB, &available);
	    }while(!available && i < ocCount);
	    #endif
		
		// reenable writes to depth and color buffers
		GL_State(GLS_DEPTHMASK_TRUE);
		qglEnable(GL_TEXTURE_2D);
		
		// loop trough all light interactions and fetch results for each last interaction
		// then copy result to all other interactions that belong to the same light
		ocCount = -1;
		iaFirst = 0;
		queryObjects = qtrue;
		oldLight = NULL;
		for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
		{
			backEnd.currentLight = light = ia->dlight;
			
			if(light != oldLight)
			{
				iaFirst = iaCount;
			}
			
			if(!queryObjects)
			{
				ia->occlusionQuerySamples = ocSamples;
			}
		
			if(!ia->next)
			{
				if(queryObjects)
				{
					ocCount++;
					
					if(ocCount < (MAX_OCCLUSION_QUERIES - 1) /*&& R_CullLightPoint(light, backEnd.viewParms.or.origin) == CULL_OUT */ )
					{
						qglGetQueryObjectivARB(tr.occlusionQueryObjects[ocCount], GL_QUERY_RESULT_AVAILABLE_ARB, &available);
						if(available)
						{
							backEnd.pc.c_occlusionQueriesAvailable++;
						
							// get the object and store it in the occlusion bits for the light
							qglGetQueryObjectivARB(tr.occlusionQueryObjects[ocCount], GL_QUERY_RESULT, &ocSamples);
					
							if(ocSamples <= 0)
							{
								backEnd.pc.c_occlusionQueriesCulled++;
							}
						}
						else
						{
							ocSamples = 1;
						}
					}
					else
					{
						ocSamples = 1;
					}
					
					// jump back to first interaction of this light copy query result
					ia = &interactions[iaFirst];
					iaCount = iaFirst;
					queryObjects = qfalse;
				}
				else
				{
					if(iaCount < (numInteractions - 1))
					{
						// jump to next interaction and start querying
						ia++;
						iaCount++;
						queryObjects = qtrue;
					}
					else
					{
						// increase last time to leave for loop
						iaCount++;
					}
				}
			}
			else
			{
				// just continue
				ia = ia->next;
				iaCount++;
			}
			
			oldLight = light;
		}
	}
#if 0
	// Tr3B - try to cull light interactions manually with stencil overdraw test
	else
	{
		interaction_t  *ia;
		int             iaCount;
		int             iaFirst = 0;
		trRefDlight_t  *light, *oldLight;
		int             i;
		long            sum = 0;
		unsigned char  *stencilReadback;
		qboolean        calcSum;

		qglColor4f(1.0f, 0.0f, 0.0f, 0.05f);

		GL_Program(0);
		GL_Cull(CT_TWO_SIDED);
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		
		stencilReadback = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight);		
		
		// don't write to the color buffer or depth buffer
		if(r_showOcclusionQueries->integer)
		{
			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
		}
		else
		{
			GL_State(GLS_COLORMASK_BITS | GLS_STENCILTEST_ENABLE);
		}
		
		oldLight = NULL;
		calcSum = qtrue;

		// loop trough all light interactions and render the light OBB for each last interaction
		for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
		{
			backEnd.currentLight = light = ia->dlight;
			
			if(light != oldLight)
			{
				iaFirst = iaCount;
			}
			
			if(!calcSum)
			{
				ia->occlusionQuerySamples = sum;
			}
			
			if(!ia->next)
			{
				// last interaction of current light
				if(calcSum)
				{
					if(R_CullLightPoint(light, backEnd.viewParms.or.origin) == CULL_OUT)
					{
						// clear stencil buffer
						qglClear(GL_STENCIL_BUFFER_BIT);
						
						// set the reference stencil value
						//qglClearStencil(0U);
						qglStencilMask(~0);
						qglStencilFunc(GL_ALWAYS, 0, ~0);
						qglStencilOp(GL_KEEP, GL_INCR, GL_INCR);
					
						R_RotateForDlight(light, &backEnd.viewParms, &backEnd.or);
						qglLoadMatrixf(backEnd.or.modelViewMatrix);
				
						qglBegin(GL_QUADS);

						qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);

						qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);

						qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);

						qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
		
						qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
	
						qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
						qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
						qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);

						qglEnd();
				
						backEnd.pc.c_occlusionQueries++;
						backEnd.pc.c_occlusionQueriesAvailable++;
					
						#if 1
						qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback);
						
						for(i = 0, sum = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++)
						{
							sum += stencilReadback[i];
						}
						#else
						// only consider the 2D light scissor of current light
						qglReadPixels(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback);
						
						for(i = 0, sum = 0; i < ia->scissorWidth * ia->scissorHeight; i++)
						{
							sum += stencilReadback[i];
						}
						#endif
					
						if(!sum)
						{
							backEnd.pc.c_occlusionQueriesCulled++;
						}
					}
					else
					{
						sum = 1;
					}
					
					// jump back to first interaction of this light copy sum to all interactions
					ia = &interactions[iaFirst];
					iaCount = iaFirst;
					calcSum = qfalse;
				}
				else
				{
					if(iaCount < (numInteractions - 1))
					{
						// jump to next interaction and continue
						ia++;
						iaCount++;
						calcSum = qtrue;
					}
					else
					{
						// increase last time to leave for loop
						iaCount++;
					}
				}
			}
			else
			{
				// just continue
				ia = ia->next;
				iaCount++;
			}
			
			oldLight = light;
		}

		// go back to the world modelview matrix
		backEnd.or = backEnd.viewParms.world;
		qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);
		
		// reenable writes to depth and color buffers
		GL_State(GLS_DEPTHMASK_TRUE);
		
		ri.Hunk_FreeTempMemory(stencilReadback);
	}
#endif

	GL_CheckErrors();
}

static void RB_RenderDebugUtils(interaction_t * interactions, int numInteractions)
{
	GLimp_LogComment("--- RB_RenderDebugUtils ---\n");
	
	if(r_showLightTransforms->integer)
	{
		int             i;
		trRefDlight_t  *dl;
		vec3_t          forward, left, up;
		vec3_t          tmp;

		if(r_dynamicLighting->integer)
		{
			GL_Program(0);
			GL_State(0);
			GL_SelectTexture(0);
			GL_Bind(tr.whiteImage);

			dl = backEnd.refdef.dlights;
			for(i = 0; i < backEnd.refdef.numDlights; i++, dl++)
			{
				// set up the transformation matrix
				R_RotateForDlight(dl, &backEnd.viewParms, &backEnd.or);
				qglLoadMatrixf(backEnd.or.modelViewMatrix);

				MatrixToVectorsFLU(matrixIdentity, forward, left, up);
				VectorMA(vec3_origin, 16, forward, forward);
				VectorMA(vec3_origin, 16, left, left);
				VectorMA(vec3_origin, 16, up, up);

				// draw axis
				//qglLineWidth(3);
				qglBegin(GL_LINES);

				qglColor4fv(colorRed);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(forward);

				qglColor4fv(colorGreen);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(left);

				qglColor4fv(colorBlue);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(up);

				qglColor4fv(colorYellow);
				qglVertex3fv(vec3_origin);
				VectorSubtract(dl->origin, backEnd.or.origin, tmp);
				dl->transformed[0] = DotProduct(tmp, backEnd.or.axis[0]);
				dl->transformed[1] = DotProduct(tmp, backEnd.or.axis[1]);
				dl->transformed[2] = DotProduct(tmp, backEnd.or.axis[2]);
				qglVertex3fv(dl->transformed);

				qglColor4fv(colorMagenta);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(dl->l.target);

				qglColor4fv(colorCyan);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(dl->l.right);

				qglColor4fv(colorWhite);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(dl->l.up);

				qglColor4fv(colorMdGrey);
				qglVertex3fv(vec3_origin);
				VectorAdd(dl->l.target, dl->l.up, tmp);
				qglVertex3fv(tmp);

				qglEnd();
				//qglLineWidth(1);

				R_DebugBoundingBox(vec3_origin, dl->localBounds[0], dl->localBounds[1], colorRed);

				// go back to the world modelview matrix
				backEnd.or = backEnd.viewParms.world;
				qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);

				R_DebugBoundingBox(vec3_origin, dl->worldBounds[0], dl->worldBounds[1], colorGreen);
			}
		}

		if(!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
		{
			GL_Program(0);
			GL_State(0);
			GL_SelectTexture(0);
			GL_Bind(tr.whiteImage);

			for(i = 0; i < tr.world->numDlights; i++)
			{
				dl = &tr.world->dlights[i];

				// set up the transformation matrix
				R_RotateForDlight(dl, &backEnd.viewParms, &backEnd.or);
				qglLoadMatrixf(backEnd.or.modelViewMatrix);

				MatrixToVectorsFLU(matrixIdentity, forward, left, up);
				VectorMA(vec3_origin, 16, forward, forward);
				VectorMA(vec3_origin, 16, left, left);
				VectorMA(vec3_origin, 16, up, up);

				// draw axis
				//qglLineWidth(3);
				qglBegin(GL_LINES);

				qglColor4fv(colorRed);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(forward);

				qglColor4fv(colorGreen);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(left);

				qglColor4fv(colorBlue);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(up);

				qglColor4fv(colorYellow);
				qglVertex3fv(vec3_origin);
				VectorSubtract(dl->origin, backEnd.or.origin, tmp);
				dl->transformed[0] = DotProduct(tmp, backEnd.or.axis[0]);
				dl->transformed[1] = DotProduct(tmp, backEnd.or.axis[1]);
				dl->transformed[2] = DotProduct(tmp, backEnd.or.axis[2]);
				qglVertex3fv(dl->transformed);

				qglColor4fv(colorMagenta);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(dl->l.target);

				qglColor4fv(colorCyan);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(dl->l.right);

				qglColor4fv(colorWhite);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(dl->l.up);

				qglColor4fv(colorMdGrey);
				qglVertex3fv(vec3_origin);
				VectorAdd(dl->l.target, dl->l.up, tmp);
				qglVertex3fv(tmp);

				qglEnd();
				//qglLineWidth(1);

				R_DebugBoundingBox(vec3_origin, dl->localBounds[0], dl->localBounds[1], colorBlue);

				// go back to the world modelview matrix
				backEnd.or = backEnd.viewParms.world;
				qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);

				R_DebugBoundingBox(vec3_origin, dl->worldBounds[0], dl->worldBounds[1], colorYellow);
			}
		}
	}

	if(r_showLightInteractions->integer)
	{
		interaction_t  *ia;
		int             iaCount;
		trRefEntity_t  *entity;
		surfaceType_t  *surface;

		GL_Program(0);
		GL_State(0);
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);

		for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
		{
			backEnd.currentEntity = entity = ia->entity;
			surface = ia->surface;

			R_RotateForEntity(entity, &backEnd.viewParms, &backEnd.or);
			qglLoadMatrixf(backEnd.or.modelViewMatrix);

			if(*surface == SF_FACE)
			{
				srfSurfaceFace_t *face;

				face = (srfSurfaceFace_t *) surface;
				R_DebugBoundingBox(vec3_origin, face->bounds[0], face->bounds[1], colorYellow);
			}
			else if(*surface == SF_GRID)
			{
				srfGridMesh_t  *grid;

				grid = (srfGridMesh_t *) surface;
				R_DebugBoundingBox(vec3_origin, grid->meshBounds[0], grid->meshBounds[1], colorMagenta);
			}
			else if(*surface == SF_TRIANGLES)
			{
				srfTriangles_t *tri;

				tri = (srfTriangles_t *) surface;
				R_DebugBoundingBox(vec3_origin, tri->bounds[0], tri->bounds[1], colorCyan);
			}
			else if(*surface == SF_MDX)
			{
				R_DebugBoundingBox(vec3_origin, entity->localBounds[0], entity->localBounds[1], colorMdGrey);
			}

			if(!ia->next)
			{
				if(iaCount < (numInteractions - 1))
				{
					// jump to next interaction and continue
					ia++;
					iaCount++;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}
			else
			{
				// just continue
				ia = ia->next;
				iaCount++;
			}
		}

		// go back to the world modelview matrix
		backEnd.or = backEnd.viewParms.world;
		qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);
	}

	if(r_showEntityTransforms->integer)
	{
		trRefEntity_t  *ent;
		int             i;

		GL_Program(0);
		GL_State(0);
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);

		ent = backEnd.refdef.entities;
		for(i = 0; i < backEnd.refdef.numEntities; i++, ent++)
		{
			if((ent->e.renderfx & RF_THIRD_PERSON) && !backEnd.viewParms.isPortal)
				continue;

			// set up the transformation matrix
			R_RotateForEntity(ent, &backEnd.viewParms, &backEnd.or);
			qglLoadMatrixf(backEnd.or.modelViewMatrix);

			R_DebugAxis(vec3_origin, matrixIdentity);
			R_DebugBoundingBox(vec3_origin, ent->localBounds[0], ent->localBounds[1], colorMagenta);

			// go back to the world modelview matrix
			backEnd.or = backEnd.viewParms.world;
			qglLoadMatrixf(backEnd.viewParms.world.modelViewMatrix);

			R_DebugBoundingBox(vec3_origin, ent->worldBounds[0], ent->worldBounds[1], colorCyan);
		}
	}

	if(r_showLightScissors->integer)
	{
		interaction_t  *ia;
		int             iaCount;

		GL_Program(0);
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		GL_State(GLS_POLYMODE_LINE | GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// set 2D virtual screen size
		qglPushMatrix();
		qglLoadIdentity();
		qglMatrixMode(GL_PROJECTION);
		qglPushMatrix();
		qglLoadIdentity();
		qglOrtho(backEnd.viewParms.viewportX,
				 backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
				 backEnd.viewParms.viewportY,
				 backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, -99999, 99999);

		for(iaCount = 0, ia = &interactions[0]; iaCount < numInteractions;)
		{
			if(qglDepthBoundsEXT)
			{
				if(ia->noDepthBoundsTest)
				{
					qglColor4fv(colorRed);
				}
				else
				{
					qglColor4fv(colorGreen);
				}
				
				qglBegin(GL_QUADS);
				qglVertex2f(ia->scissorX, ia->scissorY);
				qglVertex2f(ia->scissorX + ia->scissorWidth - 1, ia->scissorY);
				qglVertex2f(ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1);
				qglVertex2f(ia->scissorX, ia->scissorY + ia->scissorHeight - 1);
				qglEnd();
			}
			else
			{
				qglBegin(GL_QUADS);
				qglColor4fv(colorRed);
				qglVertex2f(ia->scissorX, ia->scissorY);
				qglColor4fv(colorGreen);
				qglVertex2f(ia->scissorX + ia->scissorWidth - 1, ia->scissorY);
				qglColor4fv(colorBlue);
				qglVertex2f(ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1);
				qglColor4fv(colorWhite);
				qglVertex2f(ia->scissorX, ia->scissorY + ia->scissorHeight - 1);
				qglEnd();
			}

			if(!ia->next)
			{
				if(iaCount < (numInteractions - 1))
				{
					// jump to next interaction and continue
					ia++;
					iaCount++;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}
			else
			{
				// just continue
				ia = ia->next;
				iaCount++;
			}
		}

		qglPopMatrix();
		qglMatrixMode(GL_MODELVIEW);
		qglPopMatrix();
	}
	
	GL_CheckErrors();
}

static void RB_RenderBloom(void)
{
	GLimp_LogComment("--- RB_RenderBloom ---\n");

	if((backEnd.refdef.rdflags & RDF_NOWORLDMODEL) || !r_drawBloom->integer)
		return;

	// set 2D virtual screen size
	qglPushMatrix();
	qglLoadIdentity();
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(backEnd.viewParms.viewportX,
		backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
		backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, -99999, 99999);

	if(r_drawBloom->integer == 1)
	{
		GL_State(GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// render contrast
		GL_Program(tr.contrastShader.program);
		GL_ClientState(tr.contrastShader.attribs);
		GL_SetVertexAttribs();

		GL_SelectTexture(0);
		GL_Bind(tr.currentRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);

		// draw viewport
		qglBegin(GL_QUADS);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
			backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglEnd();

		// render bloom
		GL_Program(tr.bloomShader.program);
		GL_ClientState(tr.bloomShader.attribs);
		GL_SetVertexAttribs();

		qglUniform1fARB(tr.bloomShader.u_BlurMagnitude, r_bloomBlur->value);

		GL_SelectTexture(1);
		GL_Bind(tr.currentRenderNearestImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderNearestImage->uploadWidth, tr.currentRenderNearestImage->uploadHeight);

		// draw viewport
		qglBegin(GL_QUADS);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
			backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglEnd();
	}
	else if(r_drawBloom->integer == 2)
	{
		GL_State(GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// render contrast
		GL_Program(tr.contrastShader.program);
		GL_ClientState(tr.contrastShader.attribs);
		GL_SetVertexAttribs();

		GL_SelectTexture(0);
		GL_Bind(tr.currentRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);

		// draw viewport
		qglBegin(GL_QUADS);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
			backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglEnd();

		// render blurX
		GL_Program(tr.blurXShader.program);
		GL_ClientState(tr.blurXShader.attribs);
		GL_SetVertexAttribs();

		qglUniform1fARB(tr.blurXShader.u_BlurMagnitude, r_bloomBlur->value);

		GL_Bind(tr.currentRenderNearestImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderNearestImage->uploadWidth, tr.currentRenderNearestImage->uploadHeight);

		// draw viewport
		qglBegin(GL_QUADS);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
			backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglEnd();

		// render blurY
		GL_Program(tr.blurYShader.program);
		GL_ClientState(tr.blurYShader.attribs);
		GL_SetVertexAttribs();

		qglUniform1fARB(tr.blurYShader.u_BlurMagnitude, r_bloomBlur->value);

		GL_Bind(tr.currentRenderNearestImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderNearestImage->uploadWidth, tr.currentRenderNearestImage->uploadHeight);

		// draw viewport
		qglBegin(GL_QUADS);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
			backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglEnd();

		// render bloom
		GL_Program(tr.bloomShader.program);
		GL_ClientState(tr.bloomShader.attribs);
		GL_SetVertexAttribs();

		qglUniform1fARB(tr.bloomShader.u_BlurMagnitude, r_bloomBlur->value);

		GL_SelectTexture(0);
		GL_Bind(tr.currentRenderImage);

		GL_SelectTexture(1);
		GL_Bind(tr.currentRenderNearestImage);

		// draw viewport
		qglBegin(GL_QUADS);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportY);
		qglVertex2f(backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
			backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglVertex2f(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight);
		qglEnd();
	}

	// go back to 3D
	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();
}

/*
==================
RB_RenderDrawSurfList
==================
*/
static void RB_RenderDrawSurfList(drawSurf_t * drawSurfs, int numDrawSurfs, interaction_t * interactions, int numInteractions)
{
	float           originalTime;

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va("--- RB_RenderDrawSurfList( %i surfaces, %i interactions ) ---\n", numDrawSurfs, numInteractions));
	}
	
	GL_CheckErrors();

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	backEnd.pc.c_surfaces += numDrawSurfs;

	// draw everything that is opaque
	RB_RenderDrawSurfaces(originalTime, drawSurfs, numDrawSurfs, qtrue);

	// try to cull lights using occlusion queries
	RB_RenderOcclusionQueries(interactions, numInteractions);

	if(r_shadows->integer == 3)
	{
		// render dynamic shadowing and lighting using stencil shadow volumes
		RB_RenderInteractionsStencilShadowed(originalTime, interactions, numInteractions);
	}
	else
	{
		// render dynamic lighting
		RB_RenderInteractions(originalTime, interactions, numInteractions);
	}

	// draw everything that is translucent
	RB_RenderDrawSurfaces(originalTime, drawSurfs, numDrawSurfs, qfalse);

	// add the sun flare
	RB_DrawSun();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();

	// render bloom post process effect
	RB_RenderBloom();

	// render debug information
	RB_RenderDebugUtils(interactions, numInteractions);
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte * data, int client, qboolean dirty)
{
	int             i, j;
	int             start, end;

	if(!tr.registered)
	{
		return;
	}
	R_SyncRenderThread();

	// we definately want to sync every frame for the cinematics
	qglFinish();

	start = end = 0;
	if(r_speeds->integer)
	{
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	for(i = 0; (1 << i) < cols; i++)
	{
	}
	for(j = 0; (1 << j) < rows; j++)
	{
	}
	if((1 << i) != cols || (1 << j) != rows)
	{
		ri.Error(ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_SelectTexture(0);
	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if(cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height)
	{
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else
	{
		if(dirty)
		{
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}

	if(r_speeds->integer)
	{
		end = ri.Milliseconds();
		ri.Printf(PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start);
	}

	RB_SetGL2D();

	qglColor3f(tr.identityLight, tr.identityLight, tr.identityLight);

	qglBegin(GL_QUADS);
	qglTexCoord2f(0.5f / cols, 0.5f / rows);
	qglVertex2f(x, y);
	qglTexCoord2f((cols - 0.5f) / cols, 0.5f / rows);
	qglVertex2f(x + w, y);
	qglTexCoord2f((cols - 0.5f) / cols, (rows - 0.5f) / rows);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(0.5f / cols, (rows - 0.5f) / rows);
	qglVertex2f(x, y + h);
	qglEnd();
}

void RE_UploadCinematic(int w, int h, int cols, int rows, const byte * data, int client, qboolean dirty)
{
	GL_Bind(tr.scratchImage[client]);

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if(cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height)
	{
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
	else
	{
		if(dirty)
		{
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}
}


/*
=============
RB_SetColor
=============
*/
const void     *RB_SetColor(const void *data)
{
	const setColorCommand_t *cmd;

	GLimp_LogComment("--- RB_SetColor ---\n");

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void     *RB_StretchPic(const void *data)
{
	const stretchPicCommand_t *cmd;
	shader_t       *shader;
	int             numVerts, numIndexes;

	GLimp_LogComment("--- RB_StretchPic ---\n");

	cmd = (const stretchPicCommand_t *)data;

	if(!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if(shader != tess.surfaceShader)
	{
		if(tess.numIndexes)
		{
			Tess_End();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		Tess_Begin(shader, NULL, -1, 0, qfalse, qfalse);
	}

	Tess_CheckOverflow(4, 6);
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[numIndexes] = numVerts + 3;
	tess.indexes[numIndexes + 1] = numVerts + 0;
	tess.indexes[numIndexes + 2] = numVerts + 2;
	tess.indexes[numIndexes + 3] = numVerts + 2;
	tess.indexes[numIndexes + 4] = numVerts + 0;
	tess.indexes[numIndexes + 5] = numVerts + 1;

	*(int *)tess.colors[numVerts] =
		*(int *)tess.colors[numVerts + 1] =
		*(int *)tess.colors[numVerts + 2] = *(int *)tess.colors[numVerts + 3] = *(int *)backEnd.color2D;

	tess.xyz[numVerts][0] = cmd->x;
	tess.xyz[numVerts][1] = cmd->y;
	tess.xyz[numVerts][2] = 0;
	tess.xyz[numVerts][3] = 1;

	tess.texCoords[numVerts][0][0] = cmd->s1;
	tess.texCoords[numVerts][0][1] = cmd->t1;

	tess.xyz[numVerts + 1][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 1][1] = cmd->y;
	tess.xyz[numVerts + 1][2] = 0;
	tess.xyz[numVerts + 1][3] = 1;

	tess.texCoords[numVerts + 1][0][0] = cmd->s2;
	tess.texCoords[numVerts + 1][0][1] = cmd->t1;

	tess.xyz[numVerts + 2][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 2][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 2][2] = 0;
	tess.xyz[numVerts + 2][3] = 1;

	tess.texCoords[numVerts + 2][0][0] = cmd->s2;
	tess.texCoords[numVerts + 2][0][1] = cmd->t2;

	tess.xyz[numVerts + 3][0] = cmd->x;
	tess.xyz[numVerts + 3][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 3][2] = 0;
	tess.xyz[numVerts + 3][3] = 1;

	tess.texCoords[numVerts + 3][0][0] = cmd->s1;
	tess.texCoords[numVerts + 3][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawSurfs
=============
*/
const void     *RB_DrawSurfs(const void *data)
{
	const drawSurfsCommand_t *cmd;

	GLimp_LogComment("--- RB_DrawSurfs ---\n");

	// finish any 2D drawing if needed
	if(tess.numIndexes)
	{
		Tess_End();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList(cmd->drawSurfs, cmd->numDrawSurfs, cmd->interactions, cmd->numInteractions);

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer
=============
*/
const void     *RB_DrawBuffer(const void *data)
{
	const drawBufferCommand_t *cmd;

	GLimp_LogComment("--- RB_DrawBuffer ---\n");

	cmd = (const drawBufferCommand_t *)data;

	qglDrawBuffer(cmd->buffer);

	// clear screen for debugging
	if(r_clear->integer)
	{
//      qglClearColor(1, 0, 0.5, 1);
		qglClearColor(0, 0, 0, 1);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages(void)
{
	int             i;
	image_t        *image;
	float           x, y, w, h;
	int             start, end;

	GLimp_LogComment("--- RB_ShowImages ---\n");

	if(!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	qglClear(GL_COLOR_BUFFER_BIT);

	qglFinish();

	GL_SelectTexture(0);

	start = ri.Milliseconds();

	for(i = 0; i < tr.numImages; i++)
	{
		image = tr.images[i];

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if(r_showImages->integer == 2)
		{
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind(image);
		qglBegin(GL_QUADS);
		qglTexCoord2f(0, 0);
		qglVertex2f(x, y);
		qglTexCoord2f(1, 0);
		qglVertex2f(x + w, y);
		qglTexCoord2f(1, 1);
		qglVertex2f(x + w, y + h);
		qglTexCoord2f(0, 1);
		qglVertex2f(x, y + h);
		qglEnd();
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf(PRINT_ALL, "%i msec to draw all images\n", end - start);
}


/*
=============
RB_SwapBuffers
=============
*/
const void     *RB_SwapBuffers(const void *data)
{
	const swapBuffersCommand_t *cmd;

	// finish any 2D drawing if needed
	if(tess.numIndexes)
	{
		Tess_End();
	}

	// texture swapping test
	if(r_showImages->integer)
	{
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if(r_measureOverdraw->integer)
	{
		int             i;
		long            sum = 0;
		unsigned char  *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight);
		qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback);

		for(i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++)
		{
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory(stencilReadback);
	}


	if(!glState.finishCalled)
	{
		qglFinish();
	}

	GLimp_LogComment("***************** RB_SwapBuffers *****************\n\n\n");

	GLimp_EndFrame();

	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called synchronously if running without
smp extensions, or asynchronously by another thread.
====================
*/
void RB_ExecuteRenderCommands(const void *data)
{
	int             t1, t2;

	GLimp_LogComment("--- RB_ExecuteRenderCommands ---\n");

	t1 = ri.Milliseconds();

	if(!r_smp->integer || data == backEndData[0]->commands.cmds)
	{
		backEnd.smpFrame = 0;
	}
	else
	{
		backEnd.smpFrame = 1;
	}

	while(1)
	{
		switch (*(const int *)data)
		{
			case RC_SET_COLOR:
				data = RB_SetColor(data);
				break;
			case RC_STRETCH_PIC:
				data = RB_StretchPic(data);
				break;
			case RC_DRAW_SURFS:
				data = RB_DrawSurfs(data);
				break;
			case RC_DRAW_BUFFER:
				data = RB_DrawBuffer(data);
				break;
			case RC_SWAP_BUFFERS:
				data = RB_SwapBuffers(data);
				break;
			case RC_SCREENSHOT:
				data = RB_TakeScreenshotCmd(data);
				break;

			case RC_END_OF_LIST:
			default:
				// stop rendering on this thread
				t2 = ri.Milliseconds();
				backEnd.pc.msec = t2 - t1;
				return;
		}
	}
}


/*
================
RB_RenderThread
================
*/
void RB_RenderThread(void)
{
	const void     *data;

	// wait for either a rendering command or a quit command
	while(1)
	{
		// sleep until we have work to do
		data = GLimp_RendererSleep();

		if(!data)
		{
			return;				// all done, renderer is shutting down
		}

		renderThreadActive = qtrue;

		RB_ExecuteRenderCommands(data);

		renderThreadActive = qfalse;
	}
}
