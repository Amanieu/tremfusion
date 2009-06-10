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

void GL_Unbind()
{
	GLimp_LogComment("--- GL_Unbind() ---\n");

	qglBindTexture(GL_TEXTURE_2D, 0);
}

void BindAnimatedImage(textureBundle_t * bundle)
{
	int             index;

	if(bundle->isVideoMap)
	{
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if(bundle->numImages <= 1)
	{
		GL_Bind(bundle->image[0]);
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = Q_ftol(backEnd.refdef.floatTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE);
	index >>= FUNCTABLE_SIZE2;

	if(index < 0)
	{
		index = 0;				// may happen with shader time offsets
	}
	index %= bundle->numImages;

	GL_Bind(bundle->image[index]);
}

void GL_TextureFilter(image_t * image, filterType_t filterType)
{
	if(!image)
	{
		ri.Printf(PRINT_WARNING, "GL_TextureFilter: NULL image\n");
	}
	else
	{
		if(r_logFile->integer)
		{
			// don't just call LogComment, or we will get a call to va() every frame!
			GLimp_LogComment(va("--- GL_TextureFilter( %s ) ---\n", image->name));
		}
	}

	if(image->filterType == filterType)
		return;

	// set filter type
	switch (image->filterType)
	{
			/*
			   case FT_DEFAULT:
			   qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			   qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, gl_filter_max);

			   // set texture anisotropy
			   if(glConfig.textureAnisotropyAvailable)
			   qglTexParameterf(image->type, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value);
			   break;
			 */

		case FT_LINEAR:
			qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;

		case FT_NEAREST:
			qglTexParameterf(image->type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			qglTexParameterf(image->type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			break;

		default:
			break;
	}
}

void GL_BindProgram(shaderProgram_t * program)
{
	if(!program)
	{
		GL_BindNullProgram();
		return;
	}

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va("--- GL_BindProgram( %s ) ---\n", program->name));
	}

	if(glState.currentProgram != program)
	{
		qglUseProgramObjectARB(program->program);
		glState.currentProgram = program;
	}
}

void GL_BindNullProgram(void)
{
	if(r_logFile->integer)
	{
		GLimp_LogComment("--- GL_BindNullProgram ---\n");
	}

	if(glState.currentProgram)
	{
		qglUseProgramObjectARB(0);
		glState.currentProgram = NULL;
	}
}

void GL_SelectTexture(int unit)
{
	if(glState.currenttmu == unit)
	{
		return;
	}

	if(unit >= 0 && unit <= 7)
	{
		qglActiveTextureARB(GL_TEXTURE0_ARB + unit);

		if(r_logFile->integer)
		{
			GLimp_LogComment(va("glActiveTextureARB( GL_TEXTURE%i_ARB )\n", unit));
		}
	}
	else
	{
		ri.Error(ERR_DROP, "GL_SelectTexture: unit = %i", unit);
	}

	glState.currenttmu = unit;
}

void GL_BlendFunc(GLenum sfactor, GLenum dfactor)
{
	if(glState.blendSrc != sfactor || glState.blendDst != dfactor)
	{
		glState.blendSrc = sfactor;
		glState.blendDst = dfactor;

		qglBlendFunc(sfactor, dfactor);
	}
}

void GL_ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	if(glState.clearColorRed != red || glState.clearColorGreen != green || glState.clearColorBlue != blue || glState.clearColorAlpha != alpha)
	{
		glState.clearColorRed = red;
		glState.clearColorGreen = green;
		glState.clearColorBlue = blue;
		glState.clearColorAlpha = alpha;

		qglClearColor(red, green, blue, alpha);
	}
}

void GL_ClearDepth(GLclampd depth)
{
	if(glState.clearDepth != depth)
	{
		glState.clearDepth = depth;

		qglClearDepth(depth);
	}
}

void GL_ClearStencil(GLint s)
{
	if(glState.clearStencil != s)
	{
		glState.clearStencil = s;

		qglClearStencil(s);
	}
}

void GL_ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	if(glState.colorMaskRed != red || glState.colorMaskGreen != green || glState.colorMaskBlue != blue || glState.colorMaskAlpha != alpha)
	{
		glState.colorMaskRed = red;
		glState.colorMaskGreen = green;
		glState.colorMaskBlue = blue;
		glState.colorMaskAlpha = alpha;

		qglColorMask(red, green, blue, alpha);
	}
}

void GL_CullFace(GLenum mode)
{
	if(glState.cullFace != mode)
	{
		glState.cullFace = mode;

		qglCullFace(mode);
	}
}

void GL_DepthFunc(GLenum func)
{
	if(glState.depthFunc != func)
	{
		glState.depthFunc = func;

		qglDepthFunc(func);
	}
}

void GL_DepthMask(GLboolean flag)
{
	if(glState.depthMask != flag)
	{
		glState.depthMask = flag;

		qglDepthMask(flag);
	}
}

void GL_DrawBuffer(GLenum mode)
{
	if(glState.drawBuffer != mode)
	{
		glState.drawBuffer = mode;

		qglDrawBuffer(mode);
	}
}

void GL_FrontFace(GLenum mode)
{
	if(glState.frontFace != mode)
	{
		glState.frontFace = mode;

		qglFrontFace(mode);
	}
}

void GL_LoadModelViewMatrix(const matrix_t m)
{
#if 1
	if(MatrixCompare(glState.modelViewMatrix[glState.stackIndex], m))
	{
		return;
	}
#endif


	MatrixCopy(m, glState.modelViewMatrix[glState.stackIndex]);
	MatrixMultiply(glState.projectionMatrix[glState.stackIndex], glState.modelViewMatrix[glState.stackIndex],
				   glState.modelViewProjectionMatrix[glState.stackIndex]);
}

void GL_LoadProjectionMatrix(const matrix_t m)
{
#if 1
	if(MatrixCompare(glState.projectionMatrix[glState.stackIndex], m))
	{
		return;
	}
#endif

	MatrixCopy(m, glState.projectionMatrix[glState.stackIndex]);
	MatrixMultiply(glState.projectionMatrix[glState.stackIndex], glState.modelViewMatrix[glState.stackIndex],
				   glState.modelViewProjectionMatrix[glState.stackIndex]);
}

void GL_PushMatrix()
{
	glState.stackIndex++;

	if(glState.stackIndex >= MAX_GLSTACK)
	{
		glState.stackIndex = MAX_GLSTACK - 1;
		ri.Error(ERR_DROP, "GL_PushMatrix: stack overflow = %i", glState.stackIndex);
	}
}

void GL_PopMatrix()
{
	glState.stackIndex--;

	if(glState.stackIndex < 0)
	{
		glState.stackIndex = 0;
		ri.Error(ERR_DROP, "GL_PushMatrix: stack underflow");
	}
}

void GL_PolygonMode(GLenum face, GLenum mode)
{
	if(glState.polygonFace != face || glState.polygonMode != mode)
	{
		glState.polygonFace = face;
		glState.polygonMode = mode;

		qglPolygonMode(face, mode);
	}
}

static void GL_Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if(glState.scissorX != x || glState.scissorY != y || glState.scissorWidth != width || glState.scissorHeight != height)
	{
		glState.scissorX = x;
		glState.scissorY = y;
		glState.scissorWidth = width;
		glState.scissorHeight = height;

		qglScissor(x, y, width, height);
	}
}

static void GL_Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if(glState.viewportX != x || glState.viewportY != y || glState.viewportWidth != width || glState.viewportHeight != height)
	{
		glState.viewportX = x;
		glState.viewportY = y;
		glState.viewportWidth = width;
		glState.viewportHeight = height;

		qglViewport(x, y, width, height);
	}
}

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
			GL_CullFace(GL_BACK);

			if(backEnd.viewParms.isMirror)
			{
				GL_FrontFace(GL_CW);
			}
			else
			{
				GL_FrontFace(GL_CCW);
			}
		}
		else
		{
			GL_CullFace(GL_FRONT);

			if(backEnd.viewParms.isMirror)
			{
				GL_FrontFace(GL_CW);
			}
			else
			{
				GL_FrontFace(GL_CCW);
			}
		}
	}
}


/*
GL_State

This routine is responsible for setting the most commonly changed state
in Q3.
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
			default:
				GL_DepthFunc(GL_LEQUAL);
				break;
			case GLS_DEPTHFUNC_LESS:
				GL_DepthFunc(GL_LESS);
				break;
			case GLS_DEPTHFUNC_EQUAL:
				GL_DepthFunc(GL_EQUAL);
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
			GL_BlendFunc(srcFactor, dstFactor);
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
			GL_ColorMask((stateBits & GLS_REDMASK_FALSE) ? GL_FALSE : GL_TRUE,
						 (stateBits & GLS_GREENMASK_FALSE) ? GL_FALSE : GL_TRUE,
						 (stateBits & GLS_BLUEMASK_FALSE) ? GL_FALSE : GL_TRUE,
						 (stateBits & GLS_ALPHAMASK_FALSE) ? GL_FALSE : GL_TRUE);
		}
		else
		{
			GL_ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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
			GL_PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else
		{
			GL_PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
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

	// alpha test - deprecated in OpenGL 3.0
	/*
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
	 */

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


void GL_VertexAttribsState(unsigned int stateBits)
{
	unsigned int		diff;

	if(glConfig.vboVertexSkinningAvailable && tess.vboVertexSkinning)
		stateBits |= (ATTR_BONE_INDEXES | ATTR_BONE_WEIGHTS);

	GL_VertexAttribPointers(stateBits);

	diff = stateBits ^ glState.vertexAttribsState;
	if(!diff)
	{
		return;
	}

	if(diff & ATTR_POSITION)
	{
		if(stateBits & ATTR_POSITION)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_POSITION);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_POSITION )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_POSITION);
		}
	}

	if(diff & ATTR_TEXCOORD)
	{
		if(stateBits & ATTR_TEXCOORD)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD0);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD0);
		}
	}

	if(diff & ATTR_LIGHTCOORD)
	{
		if(stateBits & ATTR_LIGHTCOORD)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHTCOORD )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD1);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_LIGHTCOORD )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD1);
		}
	}

	if(diff & ATTR_TANGENT)
	{
		if(stateBits & ATTR_TANGENT)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TANGENT);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_TANGENT )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TANGENT);
		}
	}

	if(diff & ATTR_BINORMAL)
	{
		if(stateBits & ATTR_BINORMAL)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_BINORMAL);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_BINORMAL )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_BINORMAL);
		}
	}

	if(diff & ATTR_NORMAL)
	{
		if(stateBits & ATTR_NORMAL)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_NORMAL);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_NORMAL )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_NORMAL);
		}
	}

	if(diff & ATTR_COLOR)
	{
		if(stateBits & ATTR_COLOR)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_COLOR )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_COLOR);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_COLOR )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_COLOR);
		}
	}

	if(diff & ATTR_PAINTCOLOR)
	{
		if(stateBits & ATTR_PAINTCOLOR)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_PAINTCOLOR )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_PAINTCOLOR);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_PAINTCOLOR )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_PAINTCOLOR);
		}
	}

	if(diff & ATTR_LIGHTDIRECTION)
	{
		if(stateBits & ATTR_LIGHTDIRECTION)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHTDIRECTION )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_LIGHTDIRECTION);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_LIGHTDIRECTION )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_LIGHTDIRECTION);
		}
	}

	if(diff & ATTR_BONE_INDEXES)
	{
		if(stateBits & ATTR_BONE_INDEXES)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_BONE_INDEXES);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_BONE_INDEXES);
		}
	}

	if(diff & ATTR_BONE_WEIGHTS)
	{
		if(stateBits & ATTR_BONE_WEIGHTS)
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS )\n");
			}
			qglEnableVertexAttribArrayARB(ATTR_INDEX_BONE_WEIGHTS);
		}
		else
		{
			if(r_logFile->integer)
			{
				GLimp_LogComment("qglDisableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS )\n");
			}
			qglDisableVertexAttribArrayARB(ATTR_INDEX_BONE_WEIGHTS);
		}
	}

	glState.vertexAttribsState = stateBits;
}

void GL_VertexAttribPointers(unsigned int attribBits)
{
	if(!glState.currentVBO)
	{
		ri.Error(ERR_FATAL, "GL_VertexAttribPointers: no VBO bound");
		return;
	}

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va("--- GL_VertexAttribPointers( %s ) ---\n", glState.currentVBO->name));
	}

	if(glConfig.vboVertexSkinningAvailable && tess.vboVertexSkinning)
		attribBits |= (ATTR_BONE_INDEXES | ATTR_BONE_WEIGHTS);

	if((attribBits & ATTR_POSITION) && !(glState.vertexAttribPointersSet & ATTR_POSITION))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_POSITION )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_POSITION, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsXYZ));
		glState.vertexAttribPointersSet |= ATTR_POSITION;
	}

	if((attribBits & ATTR_TEXCOORD) && !(glState.vertexAttribPointersSet & ATTR_TEXCOORD))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsTexCoords));
		glState.vertexAttribPointersSet |= ATTR_TEXCOORD;
	}

	if((attribBits & ATTR_LIGHTCOORD) && !(glState.vertexAttribPointersSet & ATTR_LIGHTCOORD))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_LIGHTCOORD )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsLightCoords));
		glState.vertexAttribPointersSet |= ATTR_LIGHTCOORD;
	}

	if((attribBits & ATTR_TANGENT) && !(glState.vertexAttribPointersSet & ATTR_TANGENT))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_TANGENT )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_TANGENT, 3, GL_FLOAT, 0, 16, BUFFER_OFFSET(glState.currentVBO->ofsTangents));
		glState.vertexAttribPointersSet |= ATTR_TANGENT;
	}

	if((attribBits & ATTR_BINORMAL) && !(glState.vertexAttribPointersSet & ATTR_BINORMAL))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_BINORMAL )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_BINORMAL, 3, GL_FLOAT, 0, 16, BUFFER_OFFSET(glState.currentVBO->ofsBinormals));
		glState.vertexAttribPointersSet |= ATTR_BINORMAL;
	}

	if((attribBits & ATTR_NORMAL) && !(glState.vertexAttribPointersSet & ATTR_NORMAL))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_NORMAL )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_NORMAL, 3, GL_FLOAT, 0, 16, BUFFER_OFFSET(glState.currentVBO->ofsNormals));
		glState.vertexAttribPointersSet |= ATTR_NORMAL;
	}

	if((attribBits & ATTR_COLOR) && !(glState.vertexAttribPointersSet & ATTR_COLOR))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_COLOR )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_COLOR, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsColors));
		glState.vertexAttribPointersSet |= ATTR_COLOR;
	}

	if((attribBits & ATTR_PAINTCOLOR) && !(glState.vertexAttribPointersSet & ATTR_PAINTCOLOR))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_PAINTCOLOR )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_PAINTCOLOR, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsPaintColors));
		glState.vertexAttribPointersSet |= ATTR_PAINTCOLOR;
	}

	if((attribBits & ATTR_LIGHTDIRECTION) && !(glState.vertexAttribPointersSet & ATTR_LIGHTDIRECTION))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_LIGHTDIRECTION )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_LIGHTDIRECTION, 3, GL_FLOAT, 0, 16, BUFFER_OFFSET(glState.currentVBO->ofsLightDirections));
		glState.vertexAttribPointersSet |= ATTR_LIGHTDIRECTION;
	}

	if((attribBits & ATTR_BONE_INDEXES) && !(glState.vertexAttribPointersSet & ATTR_BONE_INDEXES))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_BONE_INDEXES, 4, GL_INT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsBoneIndexes));
		glState.vertexAttribPointersSet |= ATTR_BONE_INDEXES;
	}

	if((attribBits & ATTR_BONE_WEIGHTS) && !(glState.vertexAttribPointersSet & ATTR_BONE_WEIGHTS))
	{
		if(r_logFile->integer)
		{
			GLimp_LogComment("qglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS )\n");
		}

		qglVertexAttribPointerARB(ATTR_INDEX_BONE_WEIGHTS, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(glState.currentVBO->ofsBoneWeights));
		glState.vertexAttribPointersSet |= ATTR_BONE_WEIGHTS;
	}
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
	GL_ClearColor(c, c, c, 1);
	qglClear(GL_COLOR_BUFFER_BIT);

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor(void)
{
	GL_LoadProjectionMatrix(backEnd.viewParms.projectionMatrix);

	// set the window clipping
	GL_Viewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
				backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);
}




/*
================
RB_SetGL2D
================
*/
static void RB_SetGL2D(void)
{
	matrix_t        proj;

	GLimp_LogComment("--- RB_SetGL2D ---\n");

	// disable offscreen rendering
	if(glConfig.framebufferObjectAvailable)
	{
		R_BindNullFBO();
	}

	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	GL_Viewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	GL_Scissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);

	MatrixSetupOrthogonalProjection(proj, 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	GL_LoadProjectionMatrix(proj);
	GL_LoadModelViewMatrix(matrixIdentity);

	GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

	qglDisable(GL_CULL_FACE);
	qglDisable(GL_CLIP_PLANE0);

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}



static void RB_RenderDrawSurfaces(qboolean opaque, qboolean depthFill)
{
	trRefEntity_t  *entity, *oldEntity;
	shader_t       *shader, *oldShader;
	int             lightmapNum, oldLightmapNum;
	qboolean        depthRange, oldDepthRange;
	int             i;
	drawSurf_t     *drawSurf;

	GLimp_LogComment("--- RB_RenderDrawSurfaces ---\n");

	// draw everything
	oldEntity = NULL;
	oldShader = NULL;
	oldLightmapNum = -1;
	oldDepthRange = qfalse;
	depthRange = qfalse;
	backEnd.currentLight = NULL;

	for(i = 0, drawSurf = backEnd.viewParms.drawSurfs; i < backEnd.viewParms.numDrawSurfs; i++, drawSurf++)
	{
		// update locals
		entity = drawSurf->entity;
		shader = tr.sortedShaders[drawSurf->shaderNum];
		lightmapNum = drawSurf->lightmapNum;

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

		if(entity == oldEntity && shader == oldShader && lightmapNum == oldLightmapNum)
		{
			// fast path, same as previous sort
			rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
			continue;
		}

		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if(shader != oldShader || lightmapNum != oldLightmapNum || (entity != oldEntity && !shader->entityMergable))
		{
			if(oldShader != NULL)
			{
				Tess_End();
			}

			if(depthFill)
				Tess_Begin(Tess_StageIteratorDepthFill, shader, NULL, qtrue, qfalse, lightmapNum);
			else
				Tess_Begin(Tess_StageIteratorGeneric, shader, NULL, qfalse, qfalse, lightmapNum);

			oldShader = shader;
			oldLightmapNum = lightmapNum;
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				backEnd.currentEntity = entity;

				// set up the transformation matrix
				R_RotateEntityForViewParms(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.orientation = backEnd.viewParms.world;
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

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
		rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
	}

	// draw the contents of the last shader batch
	if(oldShader != NULL)
	{
		Tess_End();
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	GL_CheckErrors();
}

// *INDENT-OFF*
#ifdef VOLUMETRIC_LIGHTING
static void Render_lightVolume(trRefLight_t * light)
{
	int             j;
	shader_t       *lightShader;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;

	// rotate into light space
	R_RotateLightForViewParms(light, &backEnd.viewParms, &backEnd.orientation);
	GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

	switch (light->l.rlType)
	{
		case RL_PROJ:
		{
			MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
			MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 1.0 / light->l.distance);	// scale
			break;
		}

		case RL_OMNI:
		default:
		{
			MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
			MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
			break;
		}
	}
	MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
	MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);

	lightShader = light->shader;
	attenuationZStage = lightShader->stages[0];

	for(j = 1; j < MAX_SHADER_STAGES; j++)
	{
		attenuationXYStage = lightShader->stages[j];

		if(!attenuationXYStage)
		{
			break;
		}

		if(attenuationXYStage->type != ST_ATTENUATIONMAP_XY)
		{
			continue;
		}

		if(!RB_EvalExpression(&attenuationXYStage->ifExp, 1.0))
		{
			continue;
		}

		Tess_ComputeColor(attenuationXYStage);
		R_ComputeFinalAttenuation(attenuationXYStage, light);

		if(light->l.rlType == RL_OMNI)
		{
			vec3_t          viewOrigin;
			vec3_t          lightOrigin;
			vec4_t          lightColor;

			GLimp_LogComment("--- Render_lightVolume_omni ---\n");

			// enable shader, set arrays
			GL_BindProgram(tr.lightVolumeShader_omni.program);
			//GL_VertexAttribsState(tr.lightVolumeShader_omni.attribs);
			GL_Cull(CT_BACK_SIDED);
			GL_SelectTexture(0);
			GL_Bind(tr.whiteImage);

			// don't write to the depth buffer
			GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
			//GL_State(GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
			//GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			//GL_State(attenuationXYStage->stateBits & ~(GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE));

			// set uniforms
			VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
			VectorCopy(light->origin, lightOrigin);
			VectorCopy(tess.svars.color, lightColor);

			GLSL_SetUniform_ViewOrigin(&tr.lightVolumeShader, viewOrigin);
			qglUniform3fARB(tr.lightVolumeShader_omni.u_LightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2]);
			qglUniform3fARB(tr.lightVolumeShader_omni.u_LightColor, lightColor[0], lightColor[1], lightColor[2]);
			qglUniform1fARB(tr.lightVolumeShader_omni.u_LightRadius, light->sphereRadius);
			qglUniform1fARB(tr.lightVolumeShader_omni.u_LightScale, r_lightScale->value);
			qglUniformMatrix4fvARB(tr.lightVolumeShader_omni.u_LightAttenuationMatrix, 1, GL_FALSE, light->attenuationMatrix2);
			qglUniform1iARB(tr.lightVolumeShader_omni.u_ShadowCompare, !light->l.noShadows);
			qglUniformMatrix4fvARB(tr.lightVolumeShader_omni.u_ModelMatrix, 1, GL_FALSE, backEnd.orientation.transformMatrix);
			qglUniformMatrix4fvARB(tr.lightVolumeShader_omni.u_ModelViewProjectionMatrix, 1, GL_FALSE, glState.modelViewProjectionMatrix[glState.stackIndex]);

			// bind u_AttenuationMapXY
			GL_SelectTexture(0);
			BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

			// bind u_AttenuationMapZ
			GL_SelectTexture(1);
			BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

			// bind u_ShadowMap
			if(r_shadows->integer >= 4)
			{
				GL_SelectTexture(2);
				GL_Bind(tr.shadowCubeFBOImage[light->shadowLOD]);
			}

			// draw the volume
			qglBegin(GL_QUADS);

			qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorRed);
			qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);

			qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, (colorGreen);
			qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);

			qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorBlue);
			qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);

			qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorYellow);
			qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);

			qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorMagenta);
			qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2]);

			qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorCyan);
			qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2]);
			qglVertex3f(light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2]);
			qglVertex3f(light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2]);

			qglEnd();

			GL_CheckErrors();
		}
	}
}
#endif
// *INDENT-ON*

/*
=================
RB_RenderInteractions
=================
*/
static void RB_RenderInteractions()
{
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefLight_t   *light, *oldLight;
	interaction_t  *ia;
	qboolean        depthRange, oldDepthRange;
	int             iaCount;
	surfaceType_t  *surface;
	vec3_t          tmp;
	matrix_t        modelToLight;
	int             startTime = 0, endTime = 0;

	GLimp_LogComment("--- RB_RenderInteractions ---\n");

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	// draw everything
	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = qfalse;
	depthRange = qfalse;

	// render interactions
	for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
	{
		backEnd.currentLight = light = ia->light;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto skipInteraction;
		}

		if(!shader->interactLight)
		{
			// skip this interaction because the surface shader has no ability to interact with light
			// this will save texcoords and matrix calculations
			goto skipInteraction;
		}

		if(ia->type == IA_SHADOWONLY)
		{
			// skip this interaction because the interaction is meant for shadowing only
			goto skipInteraction;
		}

		if(light != oldLight)
		{
			GLimp_LogComment("----- Rendering new light -----\n");

			// set light scissor to reduce fillrate
			GL_Scissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);
		}

		// Tr3B: this should never happen in the first iteration
		if(light == oldLight && entity == oldEntity && shader == oldShader)
		{
			// fast path, same as previous
			rb_surfaceTable[*surface] (surface);
			goto nextInteraction;
		}

		// draw the contents of the last shader batch
		Tess_End();

		// begin a new batch
		Tess_Begin(Tess_StageIteratorLighting, shader, light->shader, qfalse, qfalse, -1);

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				// set up the transformation matrix
				R_RotateEntityForViewParms(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.orientation = backEnd.viewParms.world;
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

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
				VectorSubtract(light->origin, backEnd.orientation.origin, tmp);
				light->transformed[0] = DotProduct(tmp, backEnd.orientation.axis[0]);
				light->transformed[1] = DotProduct(tmp, backEnd.orientation.axis[1]);
				light->transformed[2] = DotProduct(tmp, backEnd.orientation.axis[2]);
			}
			else
			{
				VectorCopy(light->origin, light->transformed);
			}

			// build the attenuation matrix using the entity transform
			MatrixMultiply(light->viewMatrix, backEnd.orientation.transformMatrix, modelToLight);

			switch (light->l.rlType)
			{
				case RL_PROJ:
				{
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 1.0 / Q_min(light->l.distFar - light->l.distNear, 1.0));	// scale
					break;
				}

				case RL_OMNI:
				default:
				{
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
					break;
				}
			}
			MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
			MatrixMultiply2(light->attenuationMatrix, modelToLight);
		}

		// add the triangles for this surface
		rb_surfaceTable[*surface] (surface);

	  nextInteraction:

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;

	  skipInteraction:
		if(!ia->next)
		{
			// draw the contents of the last shader batch
			Tess_End();

#ifdef VOLUMETRIC_LIGHTING
			// draw the light volume if needed
			if(light->shader->volumetricLight)
			{
				Render_lightVolume(light);
			}
#endif

			if(iaCount < (backEnd.viewParms.numInteractions - 1))
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

			// force updates
			oldLight = NULL;
			oldEntity = NULL;
			oldShader = NULL;
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor
	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_forwardLightingTime = endTime - startTime;
	}
}


/*
=================
RB_RenderInteractionsStencilShadowed
=================
*/
static void RB_RenderInteractionsStencilShadowed()
{
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefLight_t   *light, *oldLight;
	interaction_t  *ia;
	int             iaCount;
	int             iaFirst;
	surfaceType_t  *surface;
	qboolean        depthRange, oldDepthRange;
	vec3_t          tmp;
	matrix_t        modelToLight;
	qboolean        drawShadows;
	int             startTime = 0, endTime = 0;

	if(glConfig.stencilBits < 4)
	{
		RB_RenderInteractions();
		return;
	}

	GLimp_LogComment("--- RB_RenderInteractionsStencilShadowed ---\n");

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	// draw everything
	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = qfalse;
	depthRange = qfalse;
	drawShadows = qtrue;

	/*
	   if(qglActiveStencilFaceEXT)
	   {
	   qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
	   }
	 */

	// render interactions
	for(iaCount = 0, iaFirst = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
	{
		backEnd.currentLight = light = ia->light;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto skipInteraction;
		}

		// only iaCount == iaFirst if first iteration or counters were reset
		if(iaCount == iaFirst)
		{
			if(r_logFile->integer)
			{
				// don't just call LogComment, or we will get
				// a call to va() every frame!
				GLimp_LogComment(va("----- First Interaction: %i -----\n", iaCount));
			}

			if(drawShadows)
			{
				// set light scissor to reduce fillrate
				GL_Scissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);

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

				if(!light->l.noShadows)
				{
					GLimp_LogComment("--- Rendering shadow volumes ---\n");

					// set the reference stencil value
					GL_ClearStencil(128);

					// reset stencil buffer
					qglClear(GL_STENCIL_BUFFER_BIT);

					// use less compare as depthfunc
					// don't write to the color buffer or depth buffer
					// enable stencil testing for this light
					GL_State(GLS_DEPTHFUNC_LESS | GLS_COLORMASK_BITS | GLS_STENCILTEST_ENABLE);

					qglStencilFunc(GL_ALWAYS, 128, 255);
					qglStencilMask(255);

					qglEnable(GL_POLYGON_OFFSET_FILL);
					qglPolygonOffset(r_shadowOffsetFactor->value, r_shadowOffsetUnits->value);

					// enable shadow volume extrusion shader
					GL_BindProgram(&tr.shadowExtrudeShader);
				}
			}
			else
			{
				GLimp_LogComment("--- Rendering lighting ---\n");

				if(!light->l.noShadows)
				{
					qglStencilFunc(GL_EQUAL, 128, 255);
				}
				else
				{
					// don't consider shadow volumes
					qglStencilFunc(GL_ALWAYS, 128, 255);
				}

				/*
				   if(qglActiveStencilFaceEXT)
				   {
				   qglActiveStencilFaceEXT(GL_BACK);
				   qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

				   qglActiveStencilFaceEXT(GL_FRONT);
				   qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				   }
				   else
				 */
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
				}

				//qglDisable(GL_POLYGON_OFFSET_FILL);

				// disable shadow volume extrusion shader
				GL_BindProgram(NULL);
			}
		}

		if(drawShadows)
		{
			if(entity->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			{
				goto skipInteraction;
			}

			if(shader->sort > SS_OPAQUE)
			{
				goto skipInteraction;
			}

			if(shader->noShadows || light->l.noShadows)
			{
				goto skipInteraction;
			}

			if(ia->type == IA_LIGHTONLY)
			{
				goto skipInteraction;
			}

			if(light == oldLight && entity == oldEntity)
			{
				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- Batching Shadow Interaction: %i -----\n", iaCount));
				}

				// fast path, same as previous
				rb_surfaceTable[*surface] (surface);
				goto nextInteraction;
			}
			else
			{
				if(oldLight)
				{
					// draw the contents of the last shader batch
					Tess_End();
				}

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- Beginning Shadow Interaction: %i -----\n", iaCount));
				}

				// we don't need tangent space calculations here
				Tess_Begin(Tess_StageIteratorStencilShadowVolume, shader, light->shader, qtrue, qtrue, -1);
			}
		}
		else
		{
			if(!shader->interactLight)
			{
				goto skipInteraction;
			}

			if(ia->type == IA_SHADOWONLY)
			{
				goto skipInteraction;
			}

			if(light == oldLight && entity == oldEntity && shader == oldShader)
			{
				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- Batching Light Interaction: %i -----\n", iaCount));
				}

				// fast path, same as previous
				rb_surfaceTable[*surface] (surface);
				goto nextInteraction;
			}
			else
			{
				if(oldLight)
				{
					// draw the contents of the last shader batch
					Tess_End();
				}

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- Beginning Light Interaction: %i -----\n", iaCount));
				}

				// begin a new batch
				Tess_Begin(Tess_StageIteratorStencilLighting, shader, light->shader, qfalse, qfalse, -1);
			}
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				// set up the transformation matrix
				R_RotateEntityForViewParms(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.orientation = backEnd.viewParms.world;
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

			if(drawShadows && !light->l.noShadows)
			{
				GLSL_SetUniform_ModelViewProjectionMatrix(&tr.shadowExtrudeShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
			}

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
				VectorSubtract(light->origin, backEnd.orientation.origin, tmp);
				light->transformed[0] = DotProduct(tmp, backEnd.orientation.axis[0]);
				light->transformed[1] = DotProduct(tmp, backEnd.orientation.axis[1]);
				light->transformed[2] = DotProduct(tmp, backEnd.orientation.axis[2]);
			}
			else
			{
				VectorCopy(light->origin, light->transformed);
			}

			if(drawShadows && !light->l.noShadows)
			{
				// set uniform parameter u_LightOrigin for GLSL shader
				GLSL_SetUniform_LightOrigin(&tr.shadowExtrudeShader, light->transformed);
			}

			// build the attenuation matrix using the entity transform
			MatrixMultiply(light->viewMatrix, backEnd.orientation.transformMatrix, modelToLight);

			switch (light->l.rlType)
			{
				case RL_PROJ:
				{
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 1.0 / Q_min(light->l.distFar - light->l.distNear, 1.0));	// scale
					break;
				}

				case RL_OMNI:
				default:
				{
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
					break;
				}
			}
			MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
			MatrixMultiply2(light->attenuationMatrix, modelToLight);
		}

		if(drawShadows)
		{
			// add the triangles for this surface
			rb_surfaceTable[*surface] (surface);
		}
		else
		{
			// add the triangles for this surface
			rb_surfaceTable[*surface] (surface);
		}

	  nextInteraction:

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;

	  skipInteraction:
		if(!ia->next)
		{
			// if ia->next does not point to any other interaction then
			// this is the last interaction of the current light

			if(r_logFile->integer)
			{
				// don't just call LogComment, or we will get
				// a call to va() every frame!
				GLimp_LogComment(va("----- Last Interaction: %i -----\n", iaCount));
			}

			// draw the contents of the last shader batch
			Tess_End();

			if(drawShadows)
			{
				// jump back to first interaction of this light and start lighting
				ia = &backEnd.viewParms.interactions[iaFirst];
				iaCount = iaFirst;
				drawShadows = qfalse;
			}
			else
			{
				if(iaCount < (backEnd.viewParms.numInteractions - 1))
				{
					// jump to next interaction and start shadowing
					ia++;
					iaCount++;
					iaFirst = iaCount;
					drawShadows = qtrue;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}

			// force updates
			oldLight = NULL;
			oldEntity = NULL;
			oldShader = NULL;
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor clamping
	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	// reset depth clamping
	if(qglDepthBoundsEXT)
	{
		qglDisable(GL_DEPTH_BOUNDS_TEST_EXT);
	}

	/*
	   if(qglActiveStencilFaceEXT)
	   {
	   qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
	   }
	 */

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_forwardLightingTime = endTime - startTime;
	}
}

/*
=================
RB_RenderInteractionsShadowMapped
=================
*/
static void RB_RenderInteractionsShadowMapped()
{
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefLight_t   *light, *oldLight;
	interaction_t  *ia;
	int             iaCount;
	int             iaFirst;
	surfaceType_t  *surface;
	qboolean        depthRange, oldDepthRange;
	qboolean        alphaTest, oldAlphaTest;
	vec3_t          tmp;
	matrix_t        modelToLight;
	qboolean        drawShadows;
	int             cubeSide;
	int             startTime = 0, endTime = 0;

	if(!glConfig.framebufferObjectAvailable || !glConfig.textureFloatAvailable)
	{
		RB_RenderInteractions();
		return;
	}

	GLimp_LogComment("--- RB_RenderInteractionsShadowMapped ---\n");

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	// draw everything
	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = depthRange = qfalse;
	oldAlphaTest = alphaTest = qfalse;
	drawShadows = qtrue;
	cubeSide = 0;

	// if we need to clear the FBO color buffers then it should be white
	GL_ClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// render interactions
	for(iaCount = 0, iaFirst = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
	{
		backEnd.currentLight = light = ia->light;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;
		alphaTest = shader->alphaTest;

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto skipInteraction;
		}

		if(light->l.inverseShadows)
		{
			// handle those lights in RB_RenderInteractionsDeferredInverseShadows
			goto skipInteraction;
		}

		// only iaCount == iaFirst if first iteration or counters were reset
		if(iaCount == iaFirst)
		{
			if(drawShadows)
			{
				// HACK: bring OpenGL into a safe state or strange FBO update problems will occur
				GL_BindProgram(NULL);
				GL_State(GLS_DEFAULT);
				//GL_VertexAttribsState(ATTR_POSITION);

				GL_SelectTexture(0);
				GL_Bind(tr.whiteImage);

				if(light->l.noShadows || light->shadowLOD < 0)
				{
					if(r_logFile->integer)
					{
						// don't just call LogComment, or we will get
						// a call to va() every frame!
						GLimp_LogComment(va("----- Skipping shadowCube side: %i -----\n", cubeSide));
					}

					goto skipInteraction;
				}
				else
				{
					R_BindFBO(tr.shadowMapFBO[light->shadowLOD]);

					switch (light->l.rlType)
					{
						case RL_OMNI:
						{
							float           xMin, xMax, yMin, yMax;
							float           width, height, depth;
							float           zNear, zFar;
							float           fovX, fovY;
							qboolean        flipX, flipY;
							float          *proj;
							vec3_t          angles;
							matrix_t        rotationMatrix, transformMatrix, viewMatrix;

							if(r_logFile->integer)
							{
								// don't just call LogComment, or we will get
								// a call to va() every frame!
								GLimp_LogComment(va("----- Rendering shadowCube side: %i -----\n", cubeSide));
							}
							/*
							   if(r_shadows->integer == 6)
							   {
							   R_AttachFBOTextureDepth(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + cubeSide,
							   tr.shadowCubeFBOImage[light->shadowLOD]->texnum);
							   }
							   else
							 */
							{
								R_AttachFBOTexture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + cubeSide,
													 tr.shadowCubeFBOImage[light->shadowLOD]->texnum, 0);
							}
							if(!r_ignoreGLErrors->integer)
							{
								R_CheckFBO(tr.shadowMapFBO[light->shadowLOD]);
							}

							// set the window clipping
							GL_Viewport(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);
							GL_Scissor(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

							switch (cubeSide)
							{
								case 0:
								{
									// view parameters
									VectorSet(angles, 0, 0, 90);

									// projection parameters
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 1:
								{
									VectorSet(angles, 0, 180, 90);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								case 2:
								{
									VectorSet(angles, 0, 90, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 3:
								{
									VectorSet(angles, 0, -90, 0);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								case 4:
								{
									VectorSet(angles, -90, 90, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 5:
								{
									VectorSet(angles, 90, 90, 0);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								default:
								{
									// shut up compiler
									VectorSet(angles, 0, 0, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}
							}

							// Quake -> OpenGL view matrix from light perspective
							MatrixFromAngles(rotationMatrix, angles[PITCH], angles[YAW], angles[ROLL]);
							MatrixSetupTransformFromRotation(transformMatrix, rotationMatrix, light->origin);
							MatrixAffineInverse(transformMatrix, viewMatrix);

							// convert from our coordinate system (looking down X)
							// to OpenGL's coordinate system (looking down -Z)
							MatrixMultiply(quakeToOpenGLMatrix, viewMatrix, light->viewMatrix);

							// OpenGL projection matrix
							fovX = 90;
							fovY = 90;	//R_CalcFov(fovX, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							zNear = 1.0;
							zFar = light->sphereRadius;

							if(!flipX)
							{
								xMax = zNear * tan(fovX * M_PI / 360.0f);
								xMin = -xMax;
							}
							else
							{
								xMin = zNear * tan(fovX * M_PI / 360.0f);
								xMax = -xMin;
							}

							if(!flipY)
							{
								yMax = zNear * tan(fovY * M_PI / 360.0f);
								yMin = -yMax;
							}
							else
							{
								yMin = zNear * tan(fovY * M_PI / 360.0f);
								yMax = -yMin;
							}

							width = xMax - xMin;
							height = yMax - yMin;
							depth = zFar - zNear;

							proj = light->projectionMatrix;
							proj[0] = (2 * zNear) / width;
							proj[4] = 0;
							proj[8] = (xMax + xMin) / width;
							proj[12] = 0;
							proj[1] = 0;
							proj[5] = (2 * zNear) / height;
							proj[9] = (yMax + yMin) / height;
							proj[13] = 0;
							proj[2] = 0;
							proj[6] = 0;
							proj[10] = -(zFar + zNear) / depth;
							proj[14] = -(2 * zFar * zNear) / depth;
							proj[3] = 0;
							proj[7] = 0;
							proj[11] = -1;
							proj[15] = 0;

							GL_LoadProjectionMatrix(light->projectionMatrix);
							break;
						}

						case RL_PROJ:
						{
							float           xMin, xMax, yMin, yMax;
							float           width, height, depth;
							float           zNear, zFar;
							float          *proj;

							// TODO LiSPSM

							zNear = light->l.distNear;
							zFar = light->l.distFar;

							xMax = zNear * tan(light->l.fovX * M_PI / 360.0f);
							xMin = -xMax;


							yMax = zNear * tan(light->l.fovY * M_PI / 360.0f);
							yMin = -yMax;

							width = xMax - xMin;
							height = yMax - yMin;
							depth = zFar - zNear;

							// OpenGL projection matrix
							proj = light->projectionMatrix;
							proj[0] = (2 * zNear) / width;
							proj[4] = 0;
							proj[8] = (xMax + xMin) / width;
							proj[12] = 0;
							proj[1] = 0;
							proj[5] = (2 * zNear) / height;
							proj[9] = (yMax + yMin) / height;
							proj[13] = 0;
							proj[2] = 0;
							proj[6] = 0;
							proj[10] = -(zFar + zNear) / depth;
							proj[14] = -(2 * zFar * zNear) / depth;
							proj[3] = 0;
							proj[7] = 0;
							proj[11] = -1;
							proj[15] = 0;

							GLimp_LogComment("--- Rendering projective shadowMap ---\n");

							/*
							   if(r_shadows->integer == 6)
							   {
							   R_AttachFBOTextureDepth(tr.shadowMapFBOImage[light->shadowLOD]->texnum);
							   }
							   else
							 */
							{
								R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.shadowMapFBOImage[light->shadowLOD]->texnum, 0);
							}
							if(!r_ignoreGLErrors->integer)
							{
								R_CheckFBO(tr.shadowMapFBO[light->shadowLOD]);
							}

							// set the window clipping
							GL_Viewport(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);
							GL_Scissor(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

							GL_LoadProjectionMatrix(light->projectionMatrix);
							break;
						}

						default:
							break;
					}
				}

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- First Shadow Interaction: %i -----\n", iaCount));
				}
			}
			else
			{
				GLimp_LogComment("--- Rendering lighting ---\n");

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- First Light Interaction: %i -----\n", iaCount));
				}

				if(r_hdrRendering->integer)
					R_BindFBO(tr.deferredRenderFBO);
				else
					R_BindNullFBO();

				// set the window clipping
				GL_Viewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
							backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
						   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				// restore camera matrices
				GL_LoadProjectionMatrix(backEnd.viewParms.projectionMatrix);
				GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

				// reset light view and projection matrices
				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						MatrixAffineInverse(light->transformMatrix, light->viewMatrix);
						MatrixSetupScale(light->projectionMatrix, 1.0 / light->l.radius[0], 1.0 / light->l.radius[1],
										 1.0 / light->l.radius[2]);
						break;
					}

					default:
						break;
				}
			}
		}						// end if(iaCount == iaFirst)

		if(drawShadows)
		{
			if(entity->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			{
				goto skipInteraction;
			}

			if(shader->isSky)
			{
				goto skipInteraction;
			}

			if(shader->sort > SS_OPAQUE)
			{
				goto skipInteraction;
			}

			if(shader->noShadows || light->l.noShadows || light->shadowLOD < 0)
			{
				goto skipInteraction;
			}

			/*
			   if(light->l.inverseShadows && (entity == &tr.worldEntity))
			   {
			   // this light only casts shadows by its player and their items
			   goto skipInteraction;
			   }
			 */

			if(ia->type == IA_LIGHTONLY)
			{
				goto skipInteraction;
			}

			if(light->l.rlType == RL_OMNI && !(ia->cubeSideBits & (1 << cubeSide)))
			{
				goto skipInteraction;
			}

			switch (light->l.rlType)
			{
				case RL_OMNI:
				case RL_PROJ:
				{
					if(light == oldLight && entity == oldEntity && (alphaTest ? shader == oldShader : alphaTest == oldAlphaTest))
					{
						if(r_logFile->integer)
						{
							// don't just call LogComment, or we will get
							// a call to va() every frame!
							GLimp_LogComment(va("----- Batching Shadow Interaction: %i -----\n", iaCount));
						}

						// fast path, same as previous
						rb_surfaceTable[*surface] (surface);
						goto nextInteraction;
					}
					else
					{
						if(oldLight)
						{
							// draw the contents of the last shader batch
							Tess_End();
						}

						if(r_logFile->integer)
						{
							// don't just call LogComment, or we will get
							// a call to va() every frame!
							GLimp_LogComment(va("----- Beginning Shadow Interaction: %i -----\n", iaCount));
						}

						// we don't need tangent space calculations here
						Tess_Begin(Tess_StageIteratorShadowFill, shader, light->shader, qtrue, qfalse, -1);
					}
					break;
				}

				default:
					break;
			}
		}
		else
		{
			if(!shader->interactLight)
			{
				goto skipInteraction;
			}

			if(ia->type == IA_SHADOWONLY)
			{
				goto skipInteraction;
			}

			if(light == oldLight && entity == oldEntity && shader == oldShader)
			{
				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- Batching Light Interaction: %i -----\n", iaCount));
				}

				// fast path, same as previous
				rb_surfaceTable[*surface] (surface);
				goto nextInteraction;
			}
			else
			{
				if(oldLight)
				{
					// draw the contents of the last shader batch
					Tess_End();
				}

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- Beginning Light Interaction: %i -----\n", iaCount));
				}

				// begin a new batch
				Tess_Begin(Tess_StageIteratorLighting, shader, light->shader, light->l.inverseShadows, qfalse, -1);
			}
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				// set up the transformation matrix
				if(drawShadows)
				{
					R_RotateEntityForLight(entity, light, &backEnd.orientation);
				}
				else
				{
					R_RotateEntityForViewParms(entity, &backEnd.viewParms, &backEnd.orientation);
				}

				if(entity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				// set up the transformation matrix
				if(drawShadows)
				{
					Com_Memset(&backEnd.orientation, 0, sizeof(backEnd.orientation));

					backEnd.orientation.axis[0][0] = 1;
					backEnd.orientation.axis[1][1] = 1;
					backEnd.orientation.axis[2][2] = 1;
					VectorCopy(light->l.origin, backEnd.orientation.viewOrigin);

					MatrixIdentity(backEnd.orientation.transformMatrix);
					//MatrixAffineInverse(backEnd.orientation.transformMatrix, backEnd.orientation.viewMatrix);
					MatrixMultiply(light->viewMatrix, backEnd.orientation.transformMatrix, backEnd.orientation.viewMatrix);
					MatrixCopy(backEnd.orientation.viewMatrix, backEnd.orientation.modelViewMatrix);
				}
				else
				{
					// transform by the camera placement
					backEnd.orientation = backEnd.viewParms.world;
				}
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

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
				VectorSubtract(light->origin, backEnd.orientation.origin, tmp);
				light->transformed[0] = DotProduct(tmp, backEnd.orientation.axis[0]);
				light->transformed[1] = DotProduct(tmp, backEnd.orientation.axis[1]);
				light->transformed[2] = DotProduct(tmp, backEnd.orientation.axis[2]);
			}
			else
			{
				VectorCopy(light->origin, light->transformed);
			}

			MatrixMultiply(light->viewMatrix, backEnd.orientation.transformMatrix, modelToLight);

			// build the attenuation matrix using the entity transform
			switch (light->l.rlType)
			{
				case RL_PROJ:
				{
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 1.0 / Q_min(light->l.distFar - light->l.distNear, 1.0));	// scale
					break;
				}

				case RL_OMNI:
				default:
				{
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
					break;
				}
			}
			MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);
			MatrixMultiply2(light->attenuationMatrix, modelToLight);
		}

		if(drawShadows)
		{
			switch (light->l.rlType)
			{
				case RL_OMNI:
				case RL_PROJ:
				{
					// add the triangles for this surface
					rb_surfaceTable[*surface] (surface);
					break;
				}

				default:
					break;
			}
		}
		else
		{
			// add the triangles for this surface
			rb_surfaceTable[*surface] (surface);
		}

	  nextInteraction:

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;
		oldAlphaTest = alphaTest;

	  skipInteraction:
		if(!ia->next)
		{
			// if ia->next does not point to any other interaction then
			// this is the last interaction of the current light

			if(r_logFile->integer)
			{
				// don't just call LogComment, or we will get
				// a call to va() every frame!
				GLimp_LogComment(va("----- Last Interaction: %i -----\n", iaCount));
			}

			// draw the contents of the last shader batch
			Tess_End();

			if(drawShadows)
			{
				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						if(cubeSide == 5)
						{
							cubeSide = 0;
							drawShadows = qfalse;
						}
						else
						{
							cubeSide++;
						}

						// jump back to first interaction of this light
						ia = &backEnd.viewParms.interactions[iaFirst];
						iaCount = iaFirst;
						break;
					}

					case RL_PROJ:
					{
						// jump back to first interaction of this light and start lighting
						ia = &backEnd.viewParms.interactions[iaFirst];
						iaCount = iaFirst;
						drawShadows = qfalse;
						break;
					}

					default:
						break;
				}
			}
			else
			{
#ifdef VOLUMETRIC_LIGHTING
				// draw the light volume if needed
				if(light->shader->volumetricLight)
				{
					Render_lightVolume(light);
				}
#endif

				if(iaCount < (backEnd.viewParms.numInteractions - 1))
				{
					// jump to next interaction and start shadowing
					ia++;
					iaCount++;
					iaFirst = iaCount;
					drawShadows = qtrue;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}

			// force updates
			oldLight = NULL;
			oldEntity = NULL;
			oldShader = NULL;
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor clamping
	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	// reset clear color
	GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_forwardLightingTime = endTime - startTime;
	}
}

static void RB_RenderDrawSurfacesIntoGeometricBuffer()
{
	trRefEntity_t  *entity, *oldEntity;
	shader_t       *shader, *oldShader;
	int             lightmapNum, oldLightmapNum;
	qboolean        depthRange, oldDepthRange;
	int             i;
	drawSurf_t     *drawSurf;
	int             startTime = 0, endTime = 0;

	GLimp_LogComment("--- RB_RenderDrawSurfacesIntoGeometricBuffer ---\n");

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	// draw everything
	oldEntity = NULL;
	oldShader = NULL;
	oldLightmapNum = -1;
	oldDepthRange = qfalse;
	depthRange = qfalse;
	backEnd.currentLight = NULL;

	GL_CheckErrors();

	for(i = 0, drawSurf = backEnd.viewParms.drawSurfs; i < backEnd.viewParms.numDrawSurfs; i++, drawSurf++)
	{
		// update locals
		entity = drawSurf->entity;
		shader = tr.sortedShaders[drawSurf->shaderNum];
		lightmapNum = drawSurf->lightmapNum;

		// skip all translucent surfaces that don't matter for this pass
		if(shader->sort > SS_OPAQUE)
		{
			break;
		}

		if(entity == oldEntity && shader == oldShader && lightmapNum == oldLightmapNum)
		{
			// fast path, same as previous sort
			rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
			continue;
		}

		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if(shader != oldShader || (entity != oldEntity && !shader->entityMergable))
		{
			if(oldShader != NULL)
			{
				Tess_End();
			}

			Tess_Begin(Tess_StageIteratorGBuffer, shader, NULL, qfalse, qfalse, lightmapNum);
			oldShader = shader;
			oldLightmapNum = lightmapNum;
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				backEnd.currentEntity = entity;

				// set up the transformation matrix
				R_RotateEntityForViewParms(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation);

				if(backEnd.currentEntity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.orientation = backEnd.viewParms.world;
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

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
		rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);
	}

	// draw the contents of the last shader batch
	if(oldShader != NULL)
	{
		Tess_End();
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// disable offscreen rendering
	R_BindNullFBO();

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_deferredGBufferTime = endTime - startTime;
	}
}

void RB_RenderInteractionsDeferred()
{
	interaction_t  *ia;
	int             iaCount;
	trRefLight_t   *light, *oldLight = NULL;
	shader_t       *lightShader;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;
	int             i, j;
	vec3_t          viewOrigin;
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	vec4_t          lightFrustum[6];
	cplane_t       *frust;
	matrix_t        ortho;
	vec4_t          quadVerts[4];
	int             startTime = 0, endTime = 0;

	GLimp_LogComment("--- RB_RenderInteractionsDeferred ---\n");

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	R_BindFBO(tr.deferredRenderFBO);

	// update uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);

	// update depth render image
	GL_SelectTexture(1);
	GL_Bind(tr.depthRenderImage);
	//qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);

	// loop trough all light interactions and render the light quad for each last interaction
	for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
	{
		backEnd.currentLight = light = ia->light;

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto skipInteraction;
		}

		if(light != oldLight)
		{
			// set light scissor to reduce fillrate
			GL_Scissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);

			// build world to light space matrix
			switch (light->l.rlType)
			{
				case RL_OMNI:
				{
					// build the attenuation matrix
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
					MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
					MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);
					break;
				}

				case RL_PROJ:
				{
					// build the attenuation matrix
					MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
					MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, Q_min(light->l.distFar - light->l.distNear, 1.0));	// scale
					MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);
					MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);
					break;
				}

				default:
					break;
			}

			// copy frustum planes for pixel shader
			for(i = 0; i < 6; i++)
			{
				frust = &light->frustum[i];

				VectorCopy(frust->normal, lightFrustum[i]);
				lightFrustum[i][3] = frust->dist;
			}
		}

	  skipInteraction:
		if(!ia->next)
		{
			if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && ia->occlusionQuerySamples)
			{
				// last interaction of current light
				lightShader = light->shader;
				attenuationZStage = lightShader->stages[0];

				for(j = 1; j < MAX_SHADER_STAGES; j++)
				{
					attenuationXYStage = lightShader->stages[j];

					if(!attenuationXYStage)
					{
						break;
					}

					if(attenuationXYStage->type != ST_ATTENUATIONMAP_XY)
					{
						continue;
					}

					if(!RB_EvalExpression(&attenuationXYStage->ifExp, 1.0))
					{
						continue;
					}

					Tess_ComputeColor(attenuationXYStage);
					R_ComputeFinalAttenuation(attenuationXYStage, light);

					if(light->l.rlType == RL_OMNI)
					{
						// enable shader, set arrays
						GL_BindProgram(&tr.deferredLightingShader_DBS_omni);

						// set OpenGL state for additive lighting
						GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE);

						GL_Cull(CT_TWO_SIDED);

						// set uniforms
						VectorCopy(light->origin, lightOrigin);
						VectorCopy(tess.svars.color, lightColor);


						GLSL_SetUniform_ViewOrigin(&tr.deferredLightingShader_DBS_omni, viewOrigin);
						GLSL_SetUniform_LightOrigin(&tr.deferredLightingShader_DBS_omni, lightOrigin);
						GLSL_SetUniform_LightColor(&tr.deferredLightingShader_DBS_omni, lightColor);
						GLSL_SetUniform_LightRadius(&tr.deferredLightingShader_DBS_omni, light->sphereRadius);
						GLSL_SetUniform_LightScale(&tr.deferredLightingShader_DBS_omni, light->l.scale);
						GLSL_SetUniform_LightAttenuationMatrix(&tr.deferredLightingShader_DBS_omni, light->attenuationMatrix2);
						qglUniform4fvARB(tr.deferredLightingShader_DBS_omni.u_LightFrustum, 6, &lightFrustum[0][0]);

						GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deferredLightingShader_DBS_omni, glState.modelViewProjectionMatrix[glState.stackIndex]);
						GLSL_SetUniform_UnprojectMatrix(&tr.deferredLightingShader_DBS_omni, backEnd.viewParms.unprojectionMatrix);

						GLSL_SetUniform_PortalClipping(&tr.deferredLightingShader_DBS_omni, backEnd.viewParms.isPortal);
						if(backEnd.viewParms.isPortal)
						{
							float           plane[4];

							// clipping plane in world space
							plane[0] = backEnd.viewParms.portalPlane.normal[0];
							plane[1] = backEnd.viewParms.portalPlane.normal[1];
							plane[2] = backEnd.viewParms.portalPlane.normal[2];
							plane[3] = backEnd.viewParms.portalPlane.dist;

							GLSL_SetUniform_PortalPlane(&tr.deferredLightingShader_DBS_omni, plane);
						}

						// bind u_DiffuseMap
						GL_SelectTexture(0);
						GL_Bind(tr.deferredDiffuseFBOImage);

						// bind u_NormalMap
						GL_SelectTexture(1);
						GL_Bind(tr.deferredNormalFBOImage);

						if(r_normalMapping->integer)
						{
							// bind u_SpecularMap
							GL_SelectTexture(2);
							GL_Bind(tr.deferredSpecularFBOImage);
						}

						// bind u_DepthMap
						GL_SelectTexture(3);
						GL_Bind(tr.depthRenderImage);

						// bind u_AttenuationMapXY
						GL_SelectTexture(4);
						BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

						// bind u_AttenuationMapZ
						GL_SelectTexture(5);
						BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

						// draw lighting
						VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						Tess_InstantQuad(quadVerts);
					}
					else if(light->l.rlType == RL_PROJ)
					{
						// enable shader, set arrays
						GL_BindProgram(&tr.deferredLightingShader_DBS_proj);

						// set OpenGL state for additive lighting
						GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE);

						GL_Cull(CT_TWO_SIDED);

						// set uniforms
						VectorCopy(light->origin, lightOrigin);
						VectorCopy(tess.svars.color, lightColor);

						GLSL_SetUniform_ViewOrigin(&tr.deferredLightingShader_DBS_proj, viewOrigin);
						GLSL_SetUniform_LightOrigin(&tr.deferredLightingShader_DBS_proj, lightOrigin);
						GLSL_SetUniform_LightColor(&tr.deferredLightingShader_DBS_proj, lightColor);
						GLSL_SetUniform_LightRadius(&tr.deferredLightingShader_DBS_proj, light->sphereRadius);
						GLSL_SetUniform_LightScale(&tr.deferredLightingShader_DBS_proj, light->l.scale);
						GLSL_SetUniform_LightAttenuationMatrix(&tr.deferredLightingShader_DBS_proj, light->attenuationMatrix2);
						qglUniform4fvARB(tr.deferredLightingShader_DBS_proj.u_LightFrustum, 6, &lightFrustum[0][0]);

						GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deferredLightingShader_DBS_proj, glState.modelViewProjectionMatrix[glState.stackIndex]);
						GLSL_SetUniform_UnprojectMatrix(&tr.deferredLightingShader_DBS_proj, backEnd.viewParms.unprojectionMatrix);

						GLSL_SetUniform_PortalClipping(&tr.deferredLightingShader_DBS_proj, backEnd.viewParms.isPortal);
						if(backEnd.viewParms.isPortal)
						{
							float           plane[4];

							// clipping plane in world space
							plane[0] = backEnd.viewParms.portalPlane.normal[0];
							plane[1] = backEnd.viewParms.portalPlane.normal[1];
							plane[2] = backEnd.viewParms.portalPlane.normal[2];
							plane[3] = backEnd.viewParms.portalPlane.dist;

							GLSL_SetUniform_PortalPlane(&tr.deferredLightingShader_DBS_proj, plane);
						}

						// bind u_DiffuseMap
						GL_SelectTexture(0);
						GL_Bind(tr.deferredDiffuseFBOImage);

						// bind u_NormalMap
						GL_SelectTexture(1);
						GL_Bind(tr.deferredNormalFBOImage);

						if(r_normalMapping->integer)
						{
							// bind u_SpecularMap
							GL_SelectTexture(2);
							GL_Bind(tr.deferredSpecularFBOImage);
						}

						// bind u_DepthMap
						GL_SelectTexture(3);
						GL_Bind(tr.depthRenderImage);

						// bind u_AttenuationMapXY
						GL_SelectTexture(4);
						BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

						// bind u_AttenuationMapZ
						GL_SelectTexture(5);
						BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

						// draw lighting
						VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						Tess_InstantQuad(quadVerts);
					}
					else
					{
						// TODO
					}
				}
			}

			if(iaCount < (backEnd.viewParms.numInteractions - 1))
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

		oldLight = light;
	}

	GL_PopMatrix();

	// go back to the world modelview matrix
	backEnd.orientation = backEnd.viewParms.world;
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);

	// reset scissor
	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_deferredLightingTime = endTime - startTime;
	}
}

static void RB_RenderInteractionsDeferredShadowMapped()
{
	interaction_t  *ia;
	int             iaCount;
	int             iaFirst;
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefLight_t   *light, *oldLight;
	surfaceType_t  *surface;
	qboolean        depthRange, oldDepthRange;
	qboolean        alphaTest, oldAlphaTest;
	qboolean        drawShadows;
	int             cubeSide;

	shader_t       *lightShader;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;
	int             i, j;
	vec3_t          viewOrigin;
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	vec4_t          lightFrustum[6];
	cplane_t       *frust;
	qboolean        shadowCompare;
	matrix_t        ortho;
	vec4_t          quadVerts[4];
	int             startTime = 0, endTime = 0;

	GLimp_LogComment("--- RB_RenderInteractionsDeferredShadowMapped ---\n");

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = depthRange = qfalse;
	oldAlphaTest = alphaTest = qfalse;
	drawShadows = qtrue;
	cubeSide = 0;

	// if we need to clear the FBO color buffers then it should be white
	GL_ClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// update depth render image
	R_BindFBO(tr.deferredRenderFBO);
	GL_SelectTexture(1);
	GL_Bind(tr.depthRenderImage);
	//qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);

	// render interactions
	for(iaCount = 0, iaFirst = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
	{
		backEnd.currentLight = light = ia->light;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;
		alphaTest = shader->alphaTest;

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto skipInteraction;
		}

		// only iaCount == iaFirst if first iteration or counters were reset
		if(iaCount == iaFirst)
		{
			if(drawShadows)
			{
				// HACK: bring OpenGL into a safe state or strange FBO update problems will occur
				GL_BindProgram(NULL);
				GL_State(GLS_DEFAULT);
				//GL_VertexAttribsState(ATTR_POSITION);

				GL_SelectTexture(0);
				GL_Bind(tr.whiteImage);

				if(light->l.noShadows || light->shadowLOD < 0)
				{
					if(r_logFile->integer)
					{
						// don't just call LogComment, or we will get
						// a call to va() every frame!
						GLimp_LogComment(va("----- Skipping shadowCube side: %i -----\n", cubeSide));
					}

					goto skipInteraction;
				}
				else
				{
					R_BindFBO(tr.shadowMapFBO[light->shadowLOD]);

					switch (light->l.rlType)
					{
						case RL_OMNI:
						{
							float           xMin, xMax, yMin, yMax;
							float           width, height, depth;
							float           zNear, zFar;
							float           fovX, fovY;
							qboolean        flipX, flipY;
							float          *proj;
							vec3_t          angles;
							matrix_t        rotationMatrix, transformMatrix, viewMatrix;

							if(r_logFile->integer)
							{
								// don't just call LogComment, or we will get
								// a call to va() every frame!
								GLimp_LogComment(va("----- Rendering shadowCube side: %i -----\n", cubeSide));
							}

							R_AttachFBOTexture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + cubeSide,
												 tr.shadowCubeFBOImage[light->shadowLOD]->texnum, 0);
							if(!r_ignoreGLErrors->integer)
							{
								R_CheckFBO(tr.shadowMapFBO[light->shadowLOD]);
							}

							// set the window clipping
							GL_Viewport(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);
							GL_Scissor(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

							switch (cubeSide)
							{
								case 0:
								{
									// view parameters
									VectorSet(angles, 0, 0, 90);

									// projection parameters
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 1:
								{
									VectorSet(angles, 0, 180, 90);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								case 2:
								{
									VectorSet(angles, 0, 90, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 3:
								{
									VectorSet(angles, 0, -90, 0);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								case 4:
								{
									VectorSet(angles, -90, 90, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 5:
								{
									VectorSet(angles, 90, 90, 0);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								default:
								{
									// shut up compiler
									VectorSet(angles, 0, 0, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}
							}

							// Quake -> OpenGL view matrix from light perspective
							MatrixFromAngles(rotationMatrix, angles[PITCH], angles[YAW], angles[ROLL]);
							MatrixSetupTransformFromRotation(transformMatrix, rotationMatrix, light->origin);
							MatrixAffineInverse(transformMatrix, viewMatrix);

							// convert from our coordinate system (looking down X)
							// to OpenGL's coordinate system (looking down -Z)
							MatrixMultiply(quakeToOpenGLMatrix, viewMatrix, light->viewMatrix);

							// OpenGL projection matrix
							fovX = 90;
							fovY = 90;	//R_CalcFov(fovX, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							zNear = 1.0;
							zFar = light->sphereRadius;

							if(!flipX)
							{
								xMax = zNear * tan(fovX * M_PI / 360.0f);
								xMin = -xMax;
							}
							else
							{
								xMin = zNear * tan(fovX * M_PI / 360.0f);
								xMax = -xMin;
							}

							if(!flipY)
							{
								yMax = zNear * tan(fovY * M_PI / 360.0f);
								yMin = -yMax;
							}
							else
							{
								yMin = zNear * tan(fovY * M_PI / 360.0f);
								yMax = -yMin;
							}

							width = xMax - xMin;
							height = yMax - yMin;
							depth = zFar - zNear;

							proj = light->projectionMatrix;
							proj[0] = (2 * zNear) / width;
							proj[4] = 0;
							proj[8] = (xMax + xMin) / width;
							proj[12] = 0;
							proj[1] = 0;
							proj[5] = (2 * zNear) / height;
							proj[9] = (yMax + yMin) / height;
							proj[13] = 0;
							proj[2] = 0;
							proj[6] = 0;
							proj[10] = -(zFar + zNear) / depth;
							proj[14] = -(2 * zFar * zNear) / depth;
							proj[3] = 0;
							proj[7] = 0;
							proj[11] = -1;
							proj[15] = 0;

							GL_LoadProjectionMatrix(light->projectionMatrix);
							break;
						}

						case RL_PROJ:
						{
							GLimp_LogComment("--- Rendering projective shadowMap ---\n");

							R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.shadowMapFBOImage[light->shadowLOD]->texnum, 0);
							if(!r_ignoreGLErrors->integer)
							{
								R_CheckFBO(tr.shadowMapFBO[light->shadowLOD]);
							}

							// set the window clipping
							GL_Viewport(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);
							GL_Scissor(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

							GL_LoadProjectionMatrix(light->projectionMatrix);
							break;
						}

						default:
							break;
					}
				}

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- First Shadow Interaction: %i -----\n", iaCount));
				}
			}
			else
			{
				GLimp_LogComment("--- Rendering lighting ---\n");

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- First Light Interaction: %i -----\n", iaCount));
				}

				// finally draw light
				R_BindFBO(tr.deferredRenderFBO);

				// set the window clipping
				GL_Viewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
							backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				//GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
				//        backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				// set light scissor to reduce fillrate
				GL_Scissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);

				// restore camera matrices
				GL_LoadProjectionMatrix(backEnd.viewParms.projectionMatrix);
				GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						// reset light view and projection matrices
						MatrixAffineInverse(light->transformMatrix, light->viewMatrix);
						MatrixSetupScale(light->projectionMatrix, 1.0 / light->l.radius[0], 1.0 / light->l.radius[1],
										 1.0 / light->l.radius[2]);

						// build the attenuation matrix
						MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
						MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
						MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
						MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);
						break;
					}

					case RL_PROJ:
					{
						// build the attenuation matrix
						MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
						MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 1.0 / Q_min(light->l.distFar - light->l.distNear, 1.0));	// scale
						MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);
						MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);
						break;
					}

					default:
						break;
				}

				// update uniforms
				VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);

				// copy frustum planes for pixel shader
				for(i = 0; i < 6; i++)
				{
					frust = &light->frustum[i];

					VectorCopy(frust->normal, lightFrustum[i]);
					lightFrustum[i][3] = frust->dist;
				}

				// set 2D virtual screen size
				GL_PushMatrix();
				MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
												backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
												backEnd.viewParms.viewportY,
												backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, -99999, 99999);
				GL_LoadProjectionMatrix(ortho);
				GL_LoadModelViewMatrix(matrixIdentity);

				// last interaction of current light
				lightShader = light->shader;
				attenuationZStage = lightShader->stages[0];

				for(j = 1; j < MAX_SHADER_STAGES; j++)
				{
					attenuationXYStage = lightShader->stages[j];

					if(!attenuationXYStage)
					{
						break;
					}

					if(attenuationXYStage->type != ST_ATTENUATIONMAP_XY)
					{
						continue;
					}

					if(!RB_EvalExpression(&attenuationXYStage->ifExp, 1.0))
					{
						continue;
					}

					Tess_ComputeColor(attenuationXYStage);
					R_ComputeFinalAttenuation(attenuationXYStage, light);

					if(light->l.rlType == RL_OMNI)
					{
						// enable shader, set arrays
						GL_BindProgram(&tr.deferredLightingShader_DBS_omni);

						// set OpenGL state for additive lighting
						GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE);

						GL_Cull(CT_TWO_SIDED);

						// set uniforms
						VectorCopy(light->origin, lightOrigin);
						VectorCopy(tess.svars.color, lightColor);
						shadowCompare = !light->l.noShadows && light->shadowLOD >= 0;

						GLSL_SetUniform_ViewOrigin(&tr.deferredLightingShader_DBS_omni, viewOrigin);
						GLSL_SetUniform_LightOrigin(&tr.deferredLightingShader_DBS_omni, lightOrigin);
						GLSL_SetUniform_LightColor(&tr.deferredLightingShader_DBS_omni, lightColor);
						GLSL_SetUniform_LightRadius(&tr.deferredLightingShader_DBS_omni, light->sphereRadius);
						GLSL_SetUniform_LightScale(&tr.deferredLightingShader_DBS_omni, light->l.scale);
						GLSL_SetUniform_LightAttenuationMatrix(&tr.deferredLightingShader_DBS_omni, light->attenuationMatrix2);
						qglUniform4fvARB(tr.deferredLightingShader_DBS_omni.u_LightFrustum, 6, &lightFrustum[0][0]);
						GLSL_SetUniform_ShadowCompare(&tr.deferredLightingShader_DBS_omni, shadowCompare);

						GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deferredLightingShader_DBS_omni, glState.modelViewProjectionMatrix[glState.stackIndex]);
						GLSL_SetUniform_UnprojectMatrix(&tr.deferredLightingShader_DBS_omni, backEnd.viewParms.unprojectionMatrix);

						GLSL_SetUniform_PortalClipping(&tr.deferredLightingShader_DBS_omni, backEnd.viewParms.isPortal);
						if(backEnd.viewParms.isPortal)
						{
							float           plane[4];

							// clipping plane in world space
							plane[0] = backEnd.viewParms.portalPlane.normal[0];
							plane[1] = backEnd.viewParms.portalPlane.normal[1];
							plane[2] = backEnd.viewParms.portalPlane.normal[2];
							plane[3] = backEnd.viewParms.portalPlane.dist;

							GLSL_SetUniform_PortalPlane(&tr.deferredLightingShader_DBS_omni, plane);
						}

						// bind u_DiffuseMap
						GL_SelectTexture(0);
						GL_Bind(tr.deferredDiffuseFBOImage);

						// bind u_NormalMap
						GL_SelectTexture(1);
						GL_Bind(tr.deferredNormalFBOImage);

						if(r_normalMapping->integer)
						{
							// bind u_SpecularMap
							GL_SelectTexture(2);
							GL_Bind(tr.deferredSpecularFBOImage);
						}

						// bind u_DepthMap
						GL_SelectTexture(3);
						GL_Bind(tr.depthRenderImage);

						// bind u_AttenuationMapXY
						GL_SelectTexture(4);
						BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

						// bind u_AttenuationMapZ
						GL_SelectTexture(5);
						BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

						// bind u_ShadowMap
						if(shadowCompare)
						{
							GL_SelectTexture(6);
							GL_Bind(tr.shadowCubeFBOImage[light->shadowLOD]);
						}

						// draw lighting
						VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						Tess_InstantQuad(quadVerts);
					}
					else if(light->l.rlType == RL_PROJ)
					{
						if(light->l.inverseShadows)
						{
							GL_BindProgram(&tr.deferredShadowingShader_proj);

							GL_State(GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR);
							//GL_State(GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA | GLS_DSTBLEND_SRC_ALPHA);
							GL_Cull(CT_TWO_SIDED);

							// set uniforms
							VectorCopy(light->origin, lightOrigin);
							VectorCopy(tess.svars.color, lightColor);
							shadowCompare = !light->l.noShadows && light->shadowLOD >= 0;

							GLSL_SetUniform_LightOrigin(&tr.deferredShadowingShader_proj, lightOrigin);
							GLSL_SetUniform_LightColor(&tr.deferredShadowingShader_proj, lightColor);
							GLSL_SetUniform_LightRadius(&tr.deferredShadowingShader_proj, light->sphereRadius);
							GLSL_SetUniform_LightAttenuationMatrix(&tr.deferredShadowingShader_proj, light->attenuationMatrix2);
							qglUniform4fvARB(tr.deferredShadowingShader_proj.u_LightFrustum, 6, &lightFrustum[0][0]);

							GLSL_SetUniform_ShadowMatrix(&tr.deferredShadowingShader_proj, light->attenuationMatrix);
							GLSL_SetUniform_ShadowCompare(&tr.deferredShadowingShader_proj, shadowCompare);

							GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deferredShadowingShader_proj, glState.modelViewProjectionMatrix[glState.stackIndex]);
							GLSL_SetUniform_UnprojectMatrix(&tr.deferredShadowingShader_proj, backEnd.viewParms.unprojectionMatrix);

							GLSL_SetUniform_PortalClipping(&tr.deferredShadowingShader_proj, backEnd.viewParms.isPortal);
							if(backEnd.viewParms.isPortal)
							{
								float           plane[4];

								// clipping plane in world space
								plane[0] = backEnd.viewParms.portalPlane.normal[0];
								plane[1] = backEnd.viewParms.portalPlane.normal[1];
								plane[2] = backEnd.viewParms.portalPlane.normal[2];
								plane[3] = backEnd.viewParms.portalPlane.dist;

								GLSL_SetUniform_PortalPlane(&tr.deferredShadowingShader_proj, plane);
							}

							// bind u_DepthMap
							GL_SelectTexture(0);
							GL_Bind(tr.depthRenderImage);

							// bind u_AttenuationMapXY
							GL_SelectTexture(1);
							BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

							// bind u_AttenuationMapZ
							GL_SelectTexture(2);
							BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

							// bind u_ShadowMap
							if(shadowCompare)
							{
								GL_SelectTexture(3);
								GL_Bind(tr.shadowMapFBOImage[light->shadowLOD]);
							}

							// draw lighting
							VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
							VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
							VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0,
									   1);
							VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
							Tess_InstantQuad(quadVerts);
						}
						else
						{
							GL_BindProgram(&tr.deferredLightingShader_DBS_proj);

							// set OpenGL state for additive lighting
							GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE);
							GL_Cull(CT_TWO_SIDED);

							// set uniforms
							VectorCopy(light->origin, lightOrigin);
							VectorCopy(tess.svars.color, lightColor);
							shadowCompare = !light->l.noShadows && light->shadowLOD >= 0;

							GLSL_SetUniform_ViewOrigin(&tr.deferredLightingShader_DBS_proj, viewOrigin);
							GLSL_SetUniform_LightOrigin(&tr.deferredLightingShader_DBS_proj, lightOrigin);
							GLSL_SetUniform_LightColor(&tr.deferredLightingShader_DBS_proj, lightColor);
							GLSL_SetUniform_LightRadius(&tr.deferredLightingShader_DBS_proj, light->sphereRadius);
							GLSL_SetUniform_LightScale(&tr.deferredLightingShader_DBS_proj, light->l.scale);
							GLSL_SetUniform_LightAttenuationMatrix(&tr.deferredLightingShader_DBS_proj, light->attenuationMatrix2);
							qglUniform4fvARB(tr.deferredLightingShader_DBS_proj.u_LightFrustum, 6, &lightFrustum[0][0]);

							GLSL_SetUniform_ShadowMatrix(&tr.deferredLightingShader_DBS_proj, light->attenuationMatrix);
							GLSL_SetUniform_ShadowCompare(&tr.deferredLightingShader_DBS_proj, shadowCompare);

							GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deferredLightingShader_DBS_proj, glState.modelViewProjectionMatrix[glState.stackIndex]);
							GLSL_SetUniform_UnprojectMatrix(&tr.deferredLightingShader_DBS_proj, backEnd.viewParms.unprojectionMatrix);

							GLSL_SetUniform_PortalClipping(&tr.deferredLightingShader_DBS_proj, backEnd.viewParms.isPortal);
							if(backEnd.viewParms.isPortal)
							{
								float           plane[4];

								// clipping plane in world space
								plane[0] = backEnd.viewParms.portalPlane.normal[0];
								plane[1] = backEnd.viewParms.portalPlane.normal[1];
								plane[2] = backEnd.viewParms.portalPlane.normal[2];
								plane[3] = backEnd.viewParms.portalPlane.dist;

								GLSL_SetUniform_PortalPlane(&tr.deferredLightingShader_DBS_proj, plane);
							}

							// bind u_DiffuseMap
							GL_SelectTexture(0);
							GL_Bind(tr.deferredDiffuseFBOImage);

							// bind u_NormalMap
							GL_SelectTexture(1);
							GL_Bind(tr.deferredNormalFBOImage);

							if(r_normalMapping->integer)
							{
								// bind u_SpecularMap
								GL_SelectTexture(2);
								GL_Bind(tr.deferredSpecularFBOImage);
							}

							// bind u_DepthMap
							GL_SelectTexture(3);
							GL_Bind(tr.depthRenderImage);

							// bind u_AttenuationMapXY
							GL_SelectTexture(4);
							BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

							// bind u_AttenuationMapZ
							GL_SelectTexture(5);
							BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

							// bind u_ShadowMap
							if(shadowCompare)
							{
								GL_SelectTexture(6);
								GL_Bind(tr.shadowMapFBOImage[light->shadowLOD]);
							}

							// draw lighting
							VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
							VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
							VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0,
									   1);
							VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
							Tess_InstantQuad(quadVerts);
						}
					}
					else
					{
						// TODO
					}

					// end of lighting
					GL_PopMatrix();

					R_BindNullFBO();
				}
			}
		}						// end if(iaCount == iaFirst)

		if(drawShadows)
		{
			if(entity->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			{
				goto skipInteraction;
			}

			if(shader->isSky)
			{
				goto skipInteraction;
			}

			if(shader->sort > SS_OPAQUE)
			{
				goto skipInteraction;
			}

			if(shader->noShadows || light->l.noShadows || light->shadowLOD < 0)
			{
				goto skipInteraction;
			}

			if(light->l.inverseShadows && (entity == &tr.worldEntity))
			{
				// this light only casts shadows by its player and their items
				goto skipInteraction;
			}

			if(ia->type == IA_LIGHTONLY)
			{
				goto skipInteraction;
			}

			if(light->l.rlType == RL_OMNI && !(ia->cubeSideBits & (1 << cubeSide)))
			{
				goto skipInteraction;
			}

			switch (light->l.rlType)
			{
				case RL_OMNI:
				case RL_PROJ:
				{
					if(light == oldLight && entity == oldEntity && (alphaTest ? shader == oldShader : alphaTest == oldAlphaTest))
					{
						if(r_logFile->integer)
						{
							// don't just call LogComment, or we will get
							// a call to va() every frame!
							GLimp_LogComment(va("----- Batching Shadow Interaction: %i -----\n", iaCount));
						}

						// fast path, same as previous
						rb_surfaceTable[*surface] (surface);
						goto nextInteraction;
					}
					else
					{
						if(oldLight)
						{
							// draw the contents of the last shader batch
							Tess_End();
						}

						if(r_logFile->integer)
						{
							// don't just call LogComment, or we will get
							// a call to va() every frame!
							GLimp_LogComment(va("----- Beginning Shadow Interaction: %i -----\n", iaCount));
						}

						// we don't need tangent space calculations here
						Tess_Begin(Tess_StageIteratorShadowFill, shader, light->shader, qtrue, qfalse, -1);
					}
					break;
				}

				default:
					break;
			}
		}
		else
		{
			// jump to !ia->next
			goto nextInteraction;
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				// set up the transformation matrix
				if(drawShadows)
				{
					R_RotateEntityForLight(entity, light, &backEnd.orientation);
				}
				else
				{
					R_RotateEntityForViewParms(entity, &backEnd.viewParms, &backEnd.orientation);
				}

				if(entity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				// set up the transformation matrix
				if(drawShadows)
				{
					Com_Memset(&backEnd.orientation, 0, sizeof(backEnd.orientation));

					backEnd.orientation.axis[0][0] = 1;
					backEnd.orientation.axis[1][1] = 1;
					backEnd.orientation.axis[2][2] = 1;
					VectorCopy(light->l.origin, backEnd.orientation.viewOrigin);

					MatrixIdentity(backEnd.orientation.transformMatrix);
					//MatrixAffineInverse(backEnd.orientation.transformMatrix, backEnd.orientation.viewMatrix);
					MatrixMultiply(light->viewMatrix, backEnd.orientation.transformMatrix, backEnd.orientation.viewMatrix);
					MatrixCopy(backEnd.orientation.viewMatrix, backEnd.orientation.modelViewMatrix);
				}
				else
				{
					// transform by the camera placement
					backEnd.orientation = backEnd.viewParms.world;
				}
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

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

		if(drawShadows)
		{
			switch (light->l.rlType)
			{
				case RL_OMNI:
				case RL_PROJ:
				{
					// add the triangles for this surface
					rb_surfaceTable[*surface] (surface);
					break;
				}

				default:
					break;
			}
		}
		else
		{
			// DO NOTHING
			//rb_surfaceTable[*surface] (surface, ia->numLightIndexes, ia->lightIndexes, 0, NULL);
		}

	  nextInteraction:

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;
		oldAlphaTest = alphaTest;

	  skipInteraction:
		if(!ia->next)
		{
			// if ia->next does not point to any other interaction then
			// this is the last interaction of the current light

			if(r_logFile->integer)
			{
				// don't just call LogComment, or we will get
				// a call to va() every frame!
				GLimp_LogComment(va("----- Last Interaction: %i -----\n", iaCount));
			}

			if(drawShadows)
			{
				// draw the contents of the last shader batch
				Tess_End();

				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						if(cubeSide == 5)
						{
							cubeSide = 0;
							drawShadows = qfalse;
						}
						else
						{
							cubeSide++;
						}

						// jump back to first interaction of this light
						ia = &backEnd.viewParms.interactions[iaFirst];
						iaCount = iaFirst;
						break;
					}

					case RL_PROJ:
					{
						// jump back to first interaction of this light and start lighting
						ia = &backEnd.viewParms.interactions[iaFirst];
						iaCount = iaFirst;
						drawShadows = qfalse;
						break;
					}

					default:
						break;
				}
			}
			else
			{
#ifdef VOLUMETRIC_LIGHTING
				// draw the light volume if needed
				if(light->shader->volumetricLight)
				{
					Render_lightVolume(light);
				}
#endif

				if(iaCount < (backEnd.viewParms.numInteractions - 1))
				{
					// jump to next interaction and start shadowing
					ia++;
					iaCount++;
					iaFirst = iaCount;
					drawShadows = qtrue;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}

			// force updates
			oldLight = NULL;
			oldEntity = NULL;
			oldShader = NULL;
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor clamping
	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	// reset clear color
	GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_deferredLightingTime = endTime - startTime;
	}
}

static void RB_RenderInteractionsDeferredInverseShadows()
{
	interaction_t  *ia;
	int             iaCount;
	int             iaFirst;
	shader_t       *shader, *oldShader;
	trRefEntity_t  *entity, *oldEntity;
	trRefLight_t   *light, *oldLight;
	surfaceType_t  *surface;
	qboolean        depthRange, oldDepthRange;
	qboolean        alphaTest, oldAlphaTest;
	qboolean        drawShadows;
	int             cubeSide;

	shader_t       *lightShader;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;
	int             i, j;
	vec3_t          viewOrigin;
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	vec4_t          lightFrustum[6];
	cplane_t       *frust;
	qboolean        shadowCompare;
	matrix_t        ortho;
	vec4_t          quadVerts[4];
	int             startTime = 0, endTime = 0;

	GLimp_LogComment("--- RB_RenderInteractionsDeferredInverseShadows ---\n");

	if(!glConfig.framebufferObjectAvailable)
		return;

	if(r_hdrRendering->integer && !glConfig.textureFloatAvailable)
		return;

	if(r_speeds->integer == 9)
	{
		startTime = ri.Milliseconds();
	}

	oldLight = NULL;
	oldEntity = NULL;
	oldShader = NULL;
	oldDepthRange = depthRange = qfalse;
	oldAlphaTest = alphaTest = qfalse;
	drawShadows = qtrue;
	cubeSide = 0;

	// if we need to clear the FBO color buffers then it should be white
	GL_ClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// update depth render image
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
					   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		// no update needed FBO handles it
		R_BindFBO(tr.deferredRenderFBO);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		// no update needed FBO handles it
		R_BindFBO(tr.deferredRenderFBO);
	}
	else
	{
		R_BindNullFBO();
		GL_SelectTexture(0);
		GL_Bind(tr.depthRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);
	}


	// render interactions
	for(iaCount = 0, iaFirst = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
	{
		backEnd.currentLight = light = ia->light;
		backEnd.currentEntity = entity = ia->entity;
		surface = ia->surface;
		shader = ia->surfaceShader;
		alphaTest = shader->alphaTest;

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA && !ia->occlusionQuerySamples)
		{
			// skip all interactions of this light because it failed the occlusion query
			goto skipInteraction;
		}

		if(!light->l.inverseShadows)
		{
			// we only care about inverse shadows as this is a post process effect
			goto skipInteraction;
		}

		// only iaCount == iaFirst if first iteration or counters were reset
		if(iaCount == iaFirst)
		{
			if(drawShadows)
			{
				// HACK: bring OpenGL into a safe state or strange FBO update problems will occur
				GL_BindProgram(NULL);
				GL_State(GLS_DEFAULT);
				//GL_VertexAttribsState(ATTR_POSITION);

				GL_SelectTexture(0);
				GL_Bind(tr.whiteImage);

				if(light->l.noShadows || light->shadowLOD < 0)
				{
					if(r_logFile->integer)
					{
						// don't just call LogComment, or we will get
						// a call to va() every frame!
						GLimp_LogComment(va("----- Skipping shadowCube side: %i -----\n", cubeSide));
					}

					goto skipInteraction;
				}
				else
				{
					R_BindFBO(tr.shadowMapFBO[light->shadowLOD]);

					switch (light->l.rlType)
					{
						case RL_OMNI:
						{
							float           xMin, xMax, yMin, yMax;
							float           width, height, depth;
							float           zNear, zFar;
							float           fovX, fovY;
							qboolean        flipX, flipY;
							float          *proj;
							vec3_t          angles;
							matrix_t        rotationMatrix, transformMatrix, viewMatrix;

							if(r_logFile->integer)
							{
								// don't just call LogComment, or we will get
								// a call to va() every frame!
								GLimp_LogComment(va("----- Rendering shadowCube side: %i -----\n", cubeSide));
							}

							R_AttachFBOTexture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + cubeSide,
												 tr.shadowCubeFBOImage[light->shadowLOD]->texnum, 0);
							if(!r_ignoreGLErrors->integer)
							{
								R_CheckFBO(tr.shadowMapFBO[light->shadowLOD]);
							}

							// set the window clipping
							GL_Viewport(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);
							GL_Scissor(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

							switch (cubeSide)
							{
								case 0:
								{
									// view parameters
									VectorSet(angles, 0, 0, 90);

									// projection parameters
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 1:
								{
									VectorSet(angles, 0, 180, 90);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								case 2:
								{
									VectorSet(angles, 0, 90, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 3:
								{
									VectorSet(angles, 0, -90, 0);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								case 4:
								{
									VectorSet(angles, -90, 90, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}

								case 5:
								{
									VectorSet(angles, 90, 90, 0);
									flipX = qtrue;
									flipY = qtrue;
									break;
								}

								default:
								{
									// shut up compiler
									VectorSet(angles, 0, 0, 0);
									flipX = qfalse;
									flipY = qfalse;
									break;
								}
							}

							// Quake -> OpenGL view matrix from light perspective
							MatrixFromAngles(rotationMatrix, angles[PITCH], angles[YAW], angles[ROLL]);
							MatrixSetupTransformFromRotation(transformMatrix, rotationMatrix, light->origin);
							MatrixAffineInverse(transformMatrix, viewMatrix);

							// convert from our coordinate system (looking down X)
							// to OpenGL's coordinate system (looking down -Z)
							MatrixMultiply(quakeToOpenGLMatrix, viewMatrix, light->viewMatrix);

							// OpenGL projection matrix
							fovX = 90;
							fovY = 90;	//R_CalcFov(fovX, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							zNear = 1.0;
							zFar = light->sphereRadius;

							if(!flipX)
							{
								xMax = zNear * tan(fovX * M_PI / 360.0f);
								xMin = -xMax;
							}
							else
							{
								xMin = zNear * tan(fovX * M_PI / 360.0f);
								xMax = -xMin;
							}

							if(!flipY)
							{
								yMax = zNear * tan(fovY * M_PI / 360.0f);
								yMin = -yMax;
							}
							else
							{
								yMin = zNear * tan(fovY * M_PI / 360.0f);
								yMax = -yMin;
							}

							width = xMax - xMin;
							height = yMax - yMin;
							depth = zFar - zNear;

							proj = light->projectionMatrix;
							proj[0] = (2 * zNear) / width;
							proj[4] = 0;
							proj[8] = (xMax + xMin) / width;
							proj[12] = 0;
							proj[1] = 0;
							proj[5] = (2 * zNear) / height;
							proj[9] = (yMax + yMin) / height;
							proj[13] = 0;
							proj[2] = 0;
							proj[6] = 0;
							proj[10] = -(zFar + zNear) / depth;
							proj[14] = -(2 * zFar * zNear) / depth;
							proj[3] = 0;
							proj[7] = 0;
							proj[11] = -1;
							proj[15] = 0;

							GL_LoadProjectionMatrix(light->projectionMatrix);
							break;
						}

						case RL_PROJ:
						{
							GLimp_LogComment("--- Rendering projective shadowMap ---\n");

							R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.shadowMapFBOImage[light->shadowLOD]->texnum, 0);
							if(!r_ignoreGLErrors->integer)
							{
								R_CheckFBO(tr.shadowMapFBO[light->shadowLOD]);
							}

							// set the window clipping
							GL_Viewport(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);
							GL_Scissor(0, 0, shadowMapResolutions[light->shadowLOD], shadowMapResolutions[light->shadowLOD]);

							qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

							GL_LoadProjectionMatrix(light->projectionMatrix);
							break;
						}

						default:
							break;
					}
				}

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- First Shadow Interaction: %i -----\n", iaCount));
				}
			}
			else
			{
				GLimp_LogComment("--- Rendering lighting ---\n");

				if(r_logFile->integer)
				{
					// don't just call LogComment, or we will get
					// a call to va() every frame!
					GLimp_LogComment(va("----- First Light Interaction: %i -----\n", iaCount));
				}

				// finally draw light
				R_BindFBO(tr.deferredRenderFBO);

				// set the window clipping
				GL_Viewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
							backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				//GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
				//        backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

				// set light scissor to reduce fillrate
				GL_Scissor(ia->scissorX, ia->scissorY, ia->scissorWidth, ia->scissorHeight);

				// restore camera matrices
				GL_LoadProjectionMatrix(backEnd.viewParms.projectionMatrix);
				GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						// reset light view and projection matrices
						MatrixAffineInverse(light->transformMatrix, light->viewMatrix);
						MatrixSetupScale(light->projectionMatrix, 1.0 / light->l.radius[0], 1.0 / light->l.radius[1],
										 1.0 / light->l.radius[2]);

						// build the attenuation matrix
						MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.5);	// bias
						MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 0.5);	// scale
						MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);	// light projection (frustum)
						MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);
						break;
					}

					case RL_PROJ:
					{
						// build the attenuation matrix
						MatrixSetupTranslation(light->attenuationMatrix, 0.5, 0.5, 0.0);	// bias
						MatrixMultiplyScale(light->attenuationMatrix, 0.5, 0.5, 1.0 / Q_min(light->l.distFar - light->l.distNear, 1.0));	// scale
						MatrixMultiply2(light->attenuationMatrix, light->projectionMatrix);
						MatrixMultiply2(light->attenuationMatrix, light->viewMatrix);
						break;
					}

					default:
						break;
				}

				// update uniforms
				VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);

				// copy frustum planes for pixel shader
				for(i = 0; i < 6; i++)
				{
					frust = &light->frustum[i];

					VectorCopy(frust->normal, lightFrustum[i]);
					lightFrustum[i][3] = frust->dist;
				}

				// set 2D virtual screen size
				GL_PushMatrix();
				MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
												backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
												backEnd.viewParms.viewportY,
												backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, -99999, 99999);
				GL_LoadProjectionMatrix(ortho);
				GL_LoadModelViewMatrix(matrixIdentity);

				// last interaction of current light
				lightShader = light->shader;
				attenuationZStage = lightShader->stages[0];

				for(j = 1; j < MAX_SHADER_STAGES; j++)
				{
					attenuationXYStage = lightShader->stages[j];

					if(!attenuationXYStage)
					{
						break;
					}

					if(attenuationXYStage->type != ST_ATTENUATIONMAP_XY)
					{
						continue;
					}

					if(!RB_EvalExpression(&attenuationXYStage->ifExp, 1.0))
					{
						continue;
					}

					Tess_ComputeColor(attenuationXYStage);
					R_ComputeFinalAttenuation(attenuationXYStage, light);

					if(light->l.rlType == RL_OMNI)
					{
						// TODO
					}
					else if(light->l.rlType == RL_PROJ)
					{

						GL_BindProgram(&tr.deferredShadowingShader_proj);

						GL_State(GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR);
						GL_Cull(CT_TWO_SIDED);

						// set uniforms
						VectorCopy(light->origin, lightOrigin);
						VectorCopy(tess.svars.color, lightColor);
						shadowCompare = !light->l.noShadows && light->shadowLOD >= 0;

						GLSL_SetUniform_LightOrigin(&tr.deferredShadowingShader_proj, lightOrigin);
						GLSL_SetUniform_LightColor(&tr.deferredShadowingShader_proj, lightColor);
						GLSL_SetUniform_LightRadius(&tr.deferredShadowingShader_proj, light->sphereRadius);
						GLSL_SetUniform_LightAttenuationMatrix(&tr.deferredShadowingShader_proj, light->attenuationMatrix2);
						qglUniform4fvARB(tr.deferredShadowingShader_proj.u_LightFrustum, 6, &lightFrustum[0][0]);

						GLSL_SetUniform_ShadowMatrix(&tr.deferredShadowingShader_proj, light->attenuationMatrix);
						GLSL_SetUniform_ShadowCompare(&tr.deferredShadowingShader_proj, shadowCompare);

						GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deferredShadowingShader_proj, glState.modelViewProjectionMatrix[glState.stackIndex]);
						GLSL_SetUniform_UnprojectMatrix(&tr.deferredShadowingShader_proj, backEnd.viewParms.unprojectionMatrix);

						GLSL_SetUniform_PortalClipping(&tr.deferredShadowingShader_proj, backEnd.viewParms.isPortal);
						if(backEnd.viewParms.isPortal)
						{
							float           plane[4];

							// clipping plane in world space
							plane[0] = backEnd.viewParms.portalPlane.normal[0];
							plane[1] = backEnd.viewParms.portalPlane.normal[1];
							plane[2] = backEnd.viewParms.portalPlane.normal[2];
							plane[3] = backEnd.viewParms.portalPlane.dist;

							GLSL_SetUniform_PortalPlane(&tr.deferredShadowingShader_proj, plane);
						}

						// bind u_DepthMap
						GL_SelectTexture(0);
						GL_Bind(tr.depthRenderImage);

						// bind u_AttenuationMapXY
						GL_SelectTexture(1);
						BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

						// bind u_AttenuationMapZ
						GL_SelectTexture(2);
						BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

						// bind u_ShadowMap
						if(shadowCompare)
						{
							GL_SelectTexture(3);
							GL_Bind(tr.shadowMapFBOImage[light->shadowLOD]);
						}

						// draw lighting
						VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
						VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
						Tess_InstantQuad(quadVerts);
					}
					else
					{
						// TODO
					}

					// end of lighting
					GL_PopMatrix();

					R_BindNullFBO();
				}
			}
		}						// end if(iaCount == iaFirst)

		if(drawShadows)
		{
			if(entity->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			{
				goto skipInteraction;
			}

			if(shader->isSky)
			{
				goto skipInteraction;
			}

			if(shader->sort > SS_OPAQUE)
			{
				goto skipInteraction;
			}

			if(shader->noShadows || light->l.noShadows || light->shadowLOD < 0)
			{
				goto skipInteraction;
			}

			if(light->l.inverseShadows && (entity == &tr.worldEntity))
			{
				// this light only casts shadows by its player and their items
				goto skipInteraction;
			}

			if(ia->type == IA_LIGHTONLY)
			{
				goto skipInteraction;
			}

			if(light->l.rlType == RL_OMNI && !(ia->cubeSideBits & (1 << cubeSide)))
			{
				goto skipInteraction;
			}

			switch (light->l.rlType)
			{
				case RL_OMNI:
				case RL_PROJ:
				{
					if(light == oldLight && entity == oldEntity && (alphaTest ? shader == oldShader : alphaTest == oldAlphaTest))
					{
						if(r_logFile->integer)
						{
							// don't just call LogComment, or we will get
							// a call to va() every frame!
							GLimp_LogComment(va("----- Batching Shadow Interaction: %i -----\n", iaCount));
						}

						// fast path, same as previous
						rb_surfaceTable[*surface] (surface);
						goto nextInteraction;
					}
					else
					{
						if(oldLight)
						{
							// draw the contents of the last shader batch
							Tess_End();
						}

						if(r_logFile->integer)
						{
							// don't just call LogComment, or we will get
							// a call to va() every frame!
							GLimp_LogComment(va("----- Beginning Shadow Interaction: %i -----\n", iaCount));
						}

						// we don't need tangent space calculations here
						Tess_Begin(Tess_StageIteratorShadowFill, shader, light->shader, qtrue, qfalse, -1);
					}
					break;
				}

				default:
					break;
			}
		}
		else
		{
			// jump to !ia->next
			goto nextInteraction;
		}

		// change the modelview matrix if needed
		if(entity != oldEntity)
		{
			depthRange = qfalse;

			if(entity != &tr.worldEntity)
			{
				// set up the transformation matrix
				if(drawShadows)
				{
					R_RotateEntityForLight(entity, light, &backEnd.orientation);
				}
				else
				{
					R_RotateEntityForViewParms(entity, &backEnd.viewParms, &backEnd.orientation);
				}

				if(entity->e.renderfx & RF_DEPTHHACK)
				{
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else
			{
				// set up the transformation matrix
				if(drawShadows)
				{
					Com_Memset(&backEnd.orientation, 0, sizeof(backEnd.orientation));

					backEnd.orientation.axis[0][0] = 1;
					backEnd.orientation.axis[1][1] = 1;
					backEnd.orientation.axis[2][2] = 1;
					VectorCopy(light->l.origin, backEnd.orientation.viewOrigin);

					MatrixIdentity(backEnd.orientation.transformMatrix);
					//MatrixAffineInverse(backEnd.orientation.transformMatrix, backEnd.orientation.viewMatrix);
					MatrixMultiply(light->viewMatrix, backEnd.orientation.transformMatrix, backEnd.orientation.viewMatrix);
					MatrixCopy(backEnd.orientation.viewMatrix, backEnd.orientation.modelViewMatrix);
				}
				else
				{
					// transform by the camera placement
					backEnd.orientation = backEnd.viewParms.world;
				}
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

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

		if(drawShadows)
		{
			switch (light->l.rlType)
			{
				case RL_OMNI:
				case RL_PROJ:
				{
					// add the triangles for this surface
					rb_surfaceTable[*surface] (surface);
					break;
				}

				default:
					break;
			}
		}
		else
		{
			// DO NOTHING
			//rb_surfaceTable[*surface] (surface, ia->numLightIndexes, ia->lightIndexes, 0, NULL);
		}

	  nextInteraction:

		// remember values
		oldLight = light;
		oldEntity = entity;
		oldShader = shader;
		oldAlphaTest = alphaTest;

	  skipInteraction:
		if(!ia->next)
		{
			// if ia->next does not point to any other interaction then
			// this is the last interaction of the current light

			if(r_logFile->integer)
			{
				// don't just call LogComment, or we will get
				// a call to va() every frame!
				GLimp_LogComment(va("----- Last Interaction: %i -----\n", iaCount));
			}

			if(drawShadows)
			{
				// draw the contents of the last shader batch
				Tess_End();

				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						if(cubeSide == 5)
						{
							cubeSide = 0;
							drawShadows = qfalse;
						}
						else
						{
							cubeSide++;
						}

						// jump back to first interaction of this light
						ia = &backEnd.viewParms.interactions[iaFirst];
						iaCount = iaFirst;
						break;
					}

					case RL_PROJ:
					{
						// jump back to first interaction of this light and start lighting
						ia = &backEnd.viewParms.interactions[iaFirst];
						iaCount = iaFirst;
						drawShadows = qfalse;
						break;
					}

					default:
						break;
				}
			}
			else
			{
#ifdef VOLUMETRIC_LIGHTING
				// draw the light volume if needed
				if(light->shader->volumetricLight)
				{
					Render_lightVolume(light);
				}
#endif

				if(iaCount < (backEnd.viewParms.numInteractions - 1))
				{
					// jump to next interaction and start shadowing
					ia++;
					iaCount++;
					iaFirst = iaCount;
					drawShadows = qtrue;
				}
				else
				{
					// increase last time to leave for loop
					iaCount++;
				}
			}

			// force updates
			oldLight = NULL;
			oldEntity = NULL;
			oldShader = NULL;
		}
		else
		{
			// just continue
			ia = ia->next;
			iaCount++;
		}
	}

	// go back to the world modelview matrix
	GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	if(depthRange)
	{
		qglDepthRange(0, 1);
	}

	// reset scissor clamping
	GL_Scissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			   backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight);

	// reset clear color
	GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	GL_CheckErrors();

	if(r_speeds->integer == 9)
	{
		qglFinish();
		endTime = ri.Milliseconds();
		backEnd.pc.c_deferredLightingTime = endTime - startTime;
	}
}

void RB_RenderScreenSpaceAmbientOcclusion(qboolean deferred)
{
#if 0
//  int             i;
//  vec3_t          viewOrigin;
//  static vec3_t   jitter[32];
//  static qboolean jitterInit = qfalse;
//  matrix_t        projectMatrix;
	matrix_t        ortho;

	GLimp_LogComment("--- RB_RenderScreenSpaceAmbientOcclusion ---\n");

	if(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if(!r_screenSpaceAmbientOcclusion->integer)
		return;

	// enable shader, set arrays
	GL_BindProgram(&tr.screenSpaceAmbientOcclusionShader);

	GL_State(GLS_DEPTHTEST_DISABLE);	// | GLS_DEPTHMASK_TRUE);
	GL_Cull(CT_TWO_SIDED);

	qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

	// set uniforms
	/*
	   VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin); // in world space

	   if(!jitterInit)
	   {
	   for(i = 0; i < 32; i++)
	   {
	   float *jit = &jitter[i][0];

	   float rad = crandom() * 1024.0f; // FIXME radius;
	   float a = crandom() * M_PI * 2;
	   float b = crandom() * M_PI * 2;

	   jit[0] = rad * sin(a) * cos(b);
	   jit[1] = rad * sin(a) * sin(b);
	   jit[2] = rad * cos(a);
	   }

	   jitterInit = qtrue;
	   }


	   MatrixCopy(backEnd.viewParms.projectionMatrix, projectMatrix);
	   MatrixInverse(projectMatrix);

	   qglUniform3fARB(tr.screenSpaceAmbientOcclusionShader.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);
	   qglUniform3fvARB(tr.screenSpaceAmbientOcclusionShader.u_SSAOJitter, 32, &jitter[0][0]);
	   qglUniform1fARB(tr.screenSpaceAmbientOcclusionShader.u_SSAORadius, r_screenSpaceAmbientOcclusionRadius->value);

	   qglUniformMatrix4fvARB(tr.screenSpaceAmbientOcclusionShader.u_UnprojectMatrix, 1, GL_FALSE, backEnd.viewParms.unprojectionMatrix);
	   qglUniformMatrix4fvARB(tr.screenSpaceAmbientOcclusionShader.u_ProjectMatrix, 1, GL_FALSE, projectMatrix);
	 */

	// capture current color buffer for u_CurrentMap
	GL_SelectTexture(0);
	GL_Bind(tr.currentRenderImage);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);

	// bind u_DepthMap
	GL_SelectTexture(1);
	if(deferred)
	{
		GL_Bind(tr.deferredPositionFBOImage);
	}
	else
	{
		GL_Bind(tr.depthRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);
	}

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenSpaceAmbientOcclusionShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// draw viewport
	Tess_InstantQuad(backEnd.viewParms.viewportVerts);

	// go back to 3D
	GL_PopMatrix();

	GL_CheckErrors();
#endif
}

void RB_RenderDepthOfField()
{
	matrix_t        ortho;

	GLimp_LogComment("--- RB_RenderDepthOfField ---\n");

	if(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if(!r_depthOfField->integer)
		return;

	// enable shader, set arrays
	GL_BindProgram(&tr.depthOfFieldShader);

	GL_State(GLS_DEPTHTEST_DISABLE);	// | GLS_DEPTHMASK_TRUE);
	GL_Cull(CT_TWO_SIDED);

	qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

	// set uniforms

	// capture current color buffer for u_CurrentMap
	GL_SelectTexture(0);
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
				   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GL_Bind(tr.deferredRenderFBOImage);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		GL_Bind(tr.deferredRenderFBOImage);
	}
	else
	{
		GL_Bind(tr.currentRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);
	}

	// bind u_DepthMap
	GL_SelectTexture(1);
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
			   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GL_Bind(tr.depthRenderImage);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		GL_Bind(tr.depthRenderImage);
	}
	else
	{
		// depth texture is not bound to a FBO
		GL_Bind(tr.depthRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);
	}

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.depthOfFieldShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// draw viewport
	Tess_InstantQuad(backEnd.viewParms.viewportVerts);

	// go back to 3D
	GL_PopMatrix();

	GL_CheckErrors();
}

void RB_RenderUniformFog()
{
	vec3_t          viewOrigin;
	float           fogDensity;
	vec3_t          fogColor;
	matrix_t        ortho;

	GLimp_LogComment("--- RB_RenderUniformFog ---\n");

	if(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if(r_noFog->integer)
		return;

	if(r_forceFog->value <= 0 && tr.fogDensity <= 0)
		return;

	if(r_forceFog->value <= 0 && VectorLength(tr.fogColor) <= 0)
		return;

	// enable shader, set arrays
	GL_BindProgram(&tr.uniformFogShader);

	GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA | GLS_DSTBLEND_SRC_ALPHA);
	GL_Cull(CT_TWO_SIDED);

	qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space

	if(r_forceFog->value)
	{
		fogDensity = r_forceFog->value;
		VectorCopy(colorMdGrey, fogColor);
	}
	else
	{
		fogDensity = tr.fogDensity;
		VectorCopy(tr.fogColor, fogColor);
	}

	GLSL_SetUniform_ViewOrigin(&tr.uniformFogShader, viewOrigin);
	qglUniform1fARB(tr.uniformFogShader.u_FogDensity, fogDensity);
	qglUniform3fARB(tr.uniformFogShader.u_FogColor, fogColor[0], fogColor[1], fogColor[2]);
	GLSL_SetUniform_UnprojectMatrix(&tr.uniformFogShader, backEnd.viewParms.unprojectionMatrix);

	// bind u_DepthMap
	GL_SelectTexture(0);
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
			   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GL_Bind(tr.depthRenderImage);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		GL_Bind(tr.depthRenderImage);
	}
	else
	{
		// depth texture is not bound to a FBO
		GL_Bind(tr.depthRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);
	}

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.uniformFogShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// draw viewport
	Tess_InstantQuad(backEnd.viewParms.viewportVerts);

	// go back to 3D
	GL_PopMatrix();

	GL_CheckErrors();
}

void RB_RenderBloom()
{
	int				i, j;
	matrix_t        ortho;
	matrix_t		modelView;

	GLimp_LogComment("--- RB_RenderBloom ---\n");

	if((backEnd.refdef.rdflags & (RDF_NOWORLDMODEL | RDF_NOBLOOM)) || !r_bloom->integer || backEnd.viewParms.isPortal || !glConfig.framebufferObjectAvailable)
		return;

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	MatrixIdentity(modelView);
	GL_LoadModelViewMatrix(modelView);

	// FIXME
	//if(glConfig.hardwareType != GLHW_ATI && glConfig.hardwareType != GLHW_ATI_DX10)
	{
		GL_State(GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// render contrast downscaled to 1/4th of the screen
		GL_BindProgram(&tr.contrastShader);

		GL_PushMatrix();
		GL_LoadModelViewMatrix(modelView);

#if 1
		MatrixSetupOrthogonalProjection(ortho, 0, tr.contrastRenderFBO->width, 0, tr.contrastRenderFBO->height, -99999, 99999);
		GL_LoadProjectionMatrix(ortho);
#endif
		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.contrastShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
		GL_PopMatrix();

		if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
						   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
		{
			if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
			{
				if(r_hdrKey->value <= 0)
				{
					float			key;

					// calculation from: Perceptual Effects in Real-time Tone Mapping - Krawczyk et al.
					key = 1.03 - 2.0 / (2.0 + log10f(backEnd.hdrAverageLuminance + 1.0f));
					qglUniform1fARB(tr.contrastShader.u_HDRKey, key);
				}
				else
				{
					qglUniform1fARB(tr.contrastShader.u_HDRKey, r_hdrKey->value);
				}

				qglUniform1fARB(tr.contrastShader.u_HDRAverageLuminance, backEnd.hdrAverageLuminance);
				qglUniform1fARB(tr.contrastShader.u_HDRMaxLuminance, backEnd.hdrMaxLuminance);
			}

			GL_SelectTexture(0);
			GL_Bind(tr.downScaleFBOImage_quarter);
		}
		else if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
		{
			if(r_hdrKey->value <= 0)
			{
				float			key;

				// calculation from: Perceptual Effects in Real-time Tone Mapping - Krawczyk et al.
				key = 1.03 - 2.0 / (2.0 + log10f(backEnd.hdrAverageLuminance + 1.0f));
				qglUniform1fARB(tr.contrastShader.u_HDRKey, key);
			}
			else
			{
				qglUniform1fARB(tr.contrastShader.u_HDRKey, r_hdrKey->value);
			}

			qglUniform1fARB(tr.contrastShader.u_HDRAverageLuminance, backEnd.hdrAverageLuminance);
			qglUniform1fARB(tr.contrastShader.u_HDRMaxLuminance, backEnd.hdrMaxLuminance);

			GL_SelectTexture(0);
			GL_Bind(tr.downScaleFBOImage_quarter);
		}
		else
		{
			GL_SelectTexture(0);
			//GL_Bind(tr.downScaleFBOImage_quarter);
			GL_Bind(tr.currentRenderImage);
			qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth,
												 tr.currentRenderImage->uploadHeight);
		}

		R_BindFBO(tr.contrastRenderFBO);
		GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		qglClear(GL_COLOR_BUFFER_BIT);

		// draw viewport
		Tess_InstantQuad(backEnd.viewParms.viewportVerts);


		// render bloom in multiple passes
#if 0
		GL_BindProgram(&tr.bloomShader);

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.bloomShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
		qglUniform1fARB(tr.bloomShader.u_BlurMagnitude, r_bloomBlur->value);
#endif
		for(i = 0; i < 2; i++)
		{
			for(j = 0; j < r_bloomPasses->integer; j++)
			{
				R_BindFBO(tr.bloomRenderFBO[(j + 1) % 2]);

				GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				qglClear(GL_COLOR_BUFFER_BIT);

				GL_State(GLS_DEPTHTEST_DISABLE);

				GL_SelectTexture(0);
				if(j == 0)
					GL_Bind(tr.contrastRenderFBOImage);
				else
					GL_Bind(tr.bloomRenderFBOImage[j % 2]);

				GL_PushMatrix();
				GL_LoadModelViewMatrix(modelView);

				MatrixSetupOrthogonalProjection(ortho, 0, tr.bloomRenderFBO[0]->width, 0, tr.bloomRenderFBO[0]->height, -99999, 99999);
				GL_LoadProjectionMatrix(ortho);

				if(i == 0)
				{
					GL_BindProgram(&tr.blurXShader);

					qglUniform1fARB(tr.blurXShader.u_BlurMagnitude, r_bloomBlur->value);
					GLSL_SetUniform_ModelViewProjectionMatrix(&tr.blurXShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
				}
				else
				{
					GL_BindProgram(&tr.blurYShader);

					qglUniform1fARB(tr.blurYShader.u_BlurMagnitude, r_bloomBlur->value);
					GLSL_SetUniform_ModelViewProjectionMatrix(&tr.blurYShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
				}

				GL_PopMatrix();

				Tess_InstantQuad(backEnd.viewParms.viewportVerts);
			}

			// add offscreen processed bloom to screen
			if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
				   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
			{
				R_BindFBO(tr.deferredRenderFBO);

				GL_BindProgram(&tr.screenShader);
				GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

				GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

				GL_SelectTexture(0);
				GL_Bind(tr.bloomRenderFBOImage[j % 2]);
			}
			else if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
			{
				R_BindFBO(tr.deferredRenderFBO);

				GL_BindProgram(&tr.screenShader);
				GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

				GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

				GL_SelectTexture(0);
				GL_Bind(tr.bloomRenderFBOImage[j % 2]);
				//GL_Bind(tr.contrastRenderFBOImage);
			}
			else
			{
				R_BindNullFBO();

				GL_BindProgram(&tr.screenShader);
				GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

				GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

				GL_SelectTexture(0);
				GL_Bind(tr.bloomRenderFBOImage[j % 2]);
				//GL_Bind(tr.contrastRenderFBOImage);
			}

			Tess_InstantQuad(backEnd.viewParms.viewportVerts);
		}
	}

	// go back to 3D
	GL_PopMatrix();

	GL_CheckErrors();
}

void RB_RenderRotoscope(void)
{
	matrix_t        ortho;

	GLimp_LogComment("--- RB_RenderRotoscope ---\n");

	if((backEnd.refdef.rdflags & RDF_NOWORLDMODEL) || !r_rotoscope->integer || backEnd.viewParms.isPortal)
		return;

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);

	GL_State(GLS_DEPTHTEST_DISABLE);
	GL_Cull(CT_TWO_SIDED);

	// enable shader, set arrays
	GL_BindProgram(&tr.rotoscopeShader);

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.rotoscopeShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
	qglUniform1fARB(tr.rotoscopeShader.u_BlurMagnitude, r_bloomBlur->value);

	GL_SelectTexture(0);
	GL_Bind(tr.currentRenderImage);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);

	// draw viewport
	Tess_InstantQuad(backEnd.viewParms.viewportVerts);

	// go back to 3D
	GL_PopMatrix();

	GL_CheckErrors();
}

static void RB_CalculateAdaptation()
{
	int				i;
	static float	image[64 * 64 * 4];
	float           curTime;
	float			deltaTime;
	float           luminance;
	float			avgLuminance;
	float			maxLuminance;
	double			sum;
	const vec3_t    LUMINANCE_VECTOR = {0.2125f, 0.7154f, 0.0721f};
	vec4_t			color;
	float			newAdaptation;
	float			newMaximum;

	curTime = ri.Milliseconds() / 1000.0f;

	// calculate the average scene luminance
	R_BindFBO(tr.downScaleFBO_64x64);

	// read back the contents
//	qglFinish();
	qglReadPixels(0, 0, 64, 64, GL_RGBA, GL_FLOAT, image);

	sum = 0.0f;
	maxLuminance = 0.0f;
	for(i = 0; i < (64 * 64 * 4); i += 4)
	{
		color[0] = image[i + 0];
		color[1] = image[i + 1];
		color[2] = image[i + 2];
		color[3] = image[i + 3];

		luminance = DotProduct(color, LUMINANCE_VECTOR) + 0.0001f;
		if(luminance > maxLuminance)
			maxLuminance = luminance;

		sum += log(luminance);
	}
	sum /= (64.0f * 64.0f);
	avgLuminance = exp(sum);

	// the user's adapted luminance level is simulated by closing the gap between
	// adapted luminance and current luminance by 2% every frame, based on a
	// 30 fps rate. This is not an accurate model of human adaptation, which can
	// take longer than half an hour.
	if(backEnd.hdrTime > curTime)
		backEnd.hdrTime = curTime;

	deltaTime = curTime - backEnd.hdrTime;

	//if(r_hdrMaxLuminance->value)
	{
		Q_clamp(backEnd.hdrAverageLuminance, r_hdrMinLuminance->value, r_hdrMaxLuminance->value);
		Q_clamp(avgLuminance, r_hdrMinLuminance->value, r_hdrMaxLuminance->value);

		Q_clamp(backEnd.hdrMaxLuminance, r_hdrMinLuminance->value, r_hdrMaxLuminance->value);
		Q_clamp(maxLuminance, r_hdrMinLuminance->value, r_hdrMaxLuminance->value);
	}

	newAdaptation = backEnd.hdrAverageLuminance + (avgLuminance - backEnd.hdrAverageLuminance) * (1.0f - powf(0.98f, 30.0f * deltaTime));
	newMaximum = backEnd.hdrMaxLuminance + (maxLuminance - backEnd.hdrMaxLuminance) * (1.0f - powf(0.98f, 30.0f * deltaTime));

	if(!Q_isnan(newAdaptation) && !Q_isnan(newMaximum))
	{
		#if 1
		backEnd.hdrAverageLuminance = newAdaptation;
		backEnd.hdrMaxLuminance = newMaximum;
		#else
		backEnd.hdrAverageLuminance = avgLuminance;
		backEnd.hdrMaxLuminance = maxLuminance;
		#endif
	}

	backEnd.hdrTime = curTime;

	//ri.Printf(PRINT_ALL, "RB_CalculateAdaptation: avg = %f  max = %f\n", backEnd.hdrAverageLuminance, backEnd.hdrMaxLuminance);

	GL_CheckErrors();
}

void RB_RenderDeferredShadingResultToFrameBuffer()
{
	matrix_t        ortho;

	GLimp_LogComment("--- RB_RenderDeferredShadingResultToFrameBuffer ---\n");

	R_BindNullFBO();

	/*
	   if(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
	   {
	   GL_State(GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
	   }
	   else
	 */
	{
		GL_State(GLS_DEPTHTEST_DISABLE);	// | GLS_DEPTHMASK_TRUE);
	}

	GL_Cull(CT_TWO_SIDED);

	// set uniforms

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);

	if(!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && r_hdrRendering->integer)
	{
		R_BindNullFBO();

		GL_BindProgram(&tr.toneMappingShader);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(tr.deferredRenderFBOImage);

		if(r_hdrKey->value <= 0)
		{
			float			key;

			// calculation from: Perceptual Effects in Real-time Tone Mapping - Krawczyk et al.
			key = 1.03 - 2.0 / (2.0 + log10f(backEnd.hdrAverageLuminance + 1.0f));
			qglUniform1fARB(tr.toneMappingShader.u_HDRKey, key);
		}
		else
		{
			qglUniform1fARB(tr.toneMappingShader.u_HDRKey, r_hdrKey->value);
		}

		qglUniform1fARB(tr.toneMappingShader.u_HDRAverageLuminance, backEnd.hdrAverageLuminance);
		qglUniform1fARB(tr.toneMappingShader.u_HDRMaxLuminance, backEnd.hdrMaxLuminance);

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.toneMappingShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
	}
	else
	{
		GL_BindProgram(&tr.screenShader);
		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

		// bind u_ColorMap
		GL_SelectTexture(0);

		if(r_showDeferredDiffuse->integer)
		{
			GL_Bind(tr.deferredDiffuseFBOImage);
		}
		else if(r_showDeferredNormal->integer)
		{
			GL_Bind(tr.deferredNormalFBOImage);
		}
		else if(r_showDeferredSpecular->integer)
		{
			GL_Bind(tr.deferredSpecularFBOImage);
		}
		else if(r_showDeferredPosition->integer)
		{
			GL_Bind(tr.depthRenderImage);
		}
		else
		{
			GL_Bind(tr.deferredRenderFBOImage);
		}

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
	}

	GL_CheckErrors();

	Tess_InstantQuad(backEnd.viewParms.viewportVerts);

	GL_PopMatrix();
}

void RB_RenderDeferredHDRResultToFrameBuffer()
{
	matrix_t        ortho;

	GLimp_LogComment("--- RB_RenderDeferredHDRResultToFrameBuffer ---\n");

	if(!r_hdrRendering->integer || !glConfig.framebufferObjectAvailable || !glConfig.textureFloatAvailable)
		return;

	GL_CheckErrors();

	R_BindNullFBO();

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(tr.deferredRenderFBOImage);

	GL_State(GLS_DEPTHTEST_DISABLE);
	GL_Cull(CT_TWO_SIDED);

	GL_CheckErrors();

	// set uniforms

	// set 2D virtual screen size
	GL_PushMatrix();
	MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
									backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
									backEnd.viewParms.viewportY, backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight,
									-99999, 99999);
	GL_LoadProjectionMatrix(ortho);
	GL_LoadModelViewMatrix(matrixIdentity);


	if(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		GL_BindProgram(&tr.screenShader);

		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
	}
	else
	{
		GL_BindProgram(&tr.toneMappingShader);

		if(r_hdrKey->value <= 0)
		{
			float			key;

			// calculation from: Perceptual Effects in Real-time Tone Mapping - Krawczyk et al.
			key = 1.03 - 2.0 / (2.0 + log10f(backEnd.hdrAverageLuminance + 1.0f));
			qglUniform1fARB(tr.toneMappingShader.u_HDRKey, key);
		}
		else
		{
			qglUniform1fARB(tr.toneMappingShader.u_HDRKey, r_hdrKey->value);
		}

		qglUniform1fARB(tr.toneMappingShader.u_HDRAverageLuminance, backEnd.hdrAverageLuminance);
		qglUniform1fARB(tr.toneMappingShader.u_HDRMaxLuminance, backEnd.hdrMaxLuminance);

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.toneMappingShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
	}

	GL_CheckErrors();

	Tess_InstantQuad(backEnd.viewParms.viewportVerts);

	GL_PopMatrix();
}

void RB_RenderLightOcclusionQueries()
{
	GLimp_LogComment("--- RB_RenderLightOcclusionQueries ---\n");

	if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA)
	{
		int             j;
		interaction_t  *ia;
		int             iaCount;
		int             iaFirst;
		trRefLight_t   *light, *oldLight;
		int             ocCount;
		GLint           ocSamples = 0;
		qboolean        queryObjects;
		GLint           available;
		vec4_t          quadVerts[4];

		qglVertexAttrib4fARB(ATTR_INDEX_COLOR, 1.0f, 0.0f, 0.0f, 0.05f);

		GL_BindProgram(&tr.genericSingleShader);
		GL_Cull(CT_TWO_SIDED);

		GL_LoadProjectionMatrix(backEnd.viewParms.projectionMatrix);

		// set uniforms
		GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
		GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
		GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
		}
		GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

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
		for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
		{
			backEnd.currentLight = light = ia->light;
			ia->occlusionQuerySamples = 1;

			if(!ia->next)
			{
				// last interaction of current light
				if(!ia->noOcclusionQueries &&
				   ocCount <
				   (MAX_OCCLUSION_QUERIES - 1) /*&& R_CullLightPoint(light, backEnd.viewParms.orientation.origin) == CULL_OUT */ )
				{
					ocCount++;

					R_RotateLightForViewParms(light, &backEnd.viewParms, &backEnd.orientation);
					GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);
					GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

					// begin the occlusion query
					qglBeginQueryARB(GL_SAMPLES_PASSED, tr.occlusionQueryObjects[ocCount]);

					switch (light->l.rlType)
					{
						case RL_OMNI:
						{
							tess.numIndexes = 0;
							tess.numVertexes = 0;

							VectorSet4(quadVerts[0], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[1], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[2], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[3], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2],
									   1);
							Tess_AddQuadStamp2(quadVerts, colorRed);

							VectorSet4(quadVerts[0], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[1], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[2], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[3], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2],
									   1);
							Tess_AddQuadStamp2(quadVerts, colorGreen);

							VectorSet4(quadVerts[0], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[1], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[2], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[3], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2],
									   1);
							Tess_AddQuadStamp2(quadVerts, colorBlue);

							VectorSet4(quadVerts[0], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[1], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[2], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[3], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2],
									   1);
							Tess_AddQuadStamp2(quadVerts, colorYellow);

							VectorSet4(quadVerts[0], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[1], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[2], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[3], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2],
									   1);
							Tess_AddQuadStamp2(quadVerts, colorMagenta);

							VectorSet4(quadVerts[0], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2],
									   1);
							VectorSet4(quadVerts[1], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[2], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2],
									   1);
							VectorSet4(quadVerts[3], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2],
									   1);
							Tess_AddQuadStamp2(quadVerts, colorCyan);

							Tess_UpdateVBOs(ATTR_POSITION | ATTR_COLOR);
							Tess_DrawElements();

							tess.numIndexes = 0;
							tess.numVertexes = 0;
							break;
						}

						case RL_PROJ:
						{
							float           xMin, xMax, yMin, yMax;
							float           zNear, zFar;
							vec4_t          corners[4];

							zNear = light->l.distNear;
							zFar = light->l.distFar;

							xMax = zNear * tan(light->l.fovX * M_PI / 360.0f);
							xMin = -xMax;

							yMax = zNear * tan(light->l.fovY * M_PI / 360.0f);
							yMin = -yMax;

							corners[0][0] = zFar;
							corners[0][1] = xMin * zFar;
							corners[0][2] = yMin * zFar;
							corners[0][3] = 1;

							corners[1][0] = zFar;
							corners[1][1] = xMax * zFar;
							corners[1][2] = yMin * zFar;
							corners[1][3] = 1;

							corners[2][0] = zFar;
							corners[2][1] = xMax * zFar;
							corners[2][2] = yMax * zFar;
							corners[2][3] = 1;

							corners[3][0] = zFar;
							corners[3][1] = xMin * zFar;
							corners[3][2] = yMax * zFar;
							corners[3][3] = 1;

							tess.numIndexes = 0;
							tess.numVertexes = 0;

							// draw side planes
							for(j = 0; j < 4; j++)
							{
								VectorSet4(tess.xyz[tess.numVertexes], 0, 0, 0, 1);
								VectorCopy4(g_color_table[j + 1], tess.colors[tess.numVertexes]);
								tess.indexes[tess.numIndexes++] = tess.numVertexes;
								tess.numVertexes++;

								VectorCopy(corners[j], tess.xyz[tess.numVertexes]);
								tess.xyz[tess.numVertexes][3] = 1;
								VectorCopy4(g_color_table[j + 1], tess.colors[tess.numVertexes]);
								tess.indexes[tess.numIndexes++] = tess.numVertexes;
								tess.numVertexes++;

								VectorCopy(corners[(j + 1) % 4], tess.xyz[tess.numVertexes]);
								tess.xyz[tess.numVertexes][3] = 1;
								VectorCopy4(g_color_table[j + 1], tess.colors[tess.numVertexes]);
								tess.indexes[tess.numIndexes++] = tess.numVertexes;
								tess.numVertexes++;
							}

							// draw far plane
							Tess_AddQuadStamp2(corners, g_color_table[j + 1]);

							Tess_UpdateVBOs(ATTR_POSITION | ATTR_COLOR);
							Tess_DrawElements();

							tess.numIndexes = 0;
							tess.numVertexes = 0;
							break;
						}

						default:
							break;
					}


					// end the query
					// don't read back immediately so that we give the query time to be ready
					qglEndQueryARB(GL_SAMPLES_PASSED);

					backEnd.pc.c_occlusionQueries++;
				}

				if(iaCount < (backEnd.viewParms.numInteractions - 1))
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
		backEnd.orientation = backEnd.viewParms.world;
		GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);

		if(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
		{
			// FIXME it ain't work in subviews
			return;
		}

		if(ocCount == -1)
		{
			// Tr3B: avoid the following qglFinish if there are no lights
			return;
		}

		/*
		   if(!ocCount)
		   {
		   qglEnable(GL_TEXTURE_2D);
		   return;
		   }
		 */

		qglFinish();

		// do other work until "most" of the queries are back, to avoid
		// wasting time spinning
		if(ocCount >= 0)
		{
			int             i;
			int             avCount;
			int             limit;
			int             startTime = 0, endTime = 0;

			if(r_speeds->integer == 7)
			{
				startTime = ri.Milliseconds();
			}

			limit = (int)(ocCount * 5 / 6);	// instead of N-1, to prevent the GPU from going idle
			//limit = ocCount;


			if(limit >= (MAX_OCCLUSION_QUERIES - 1))
				limit = (MAX_OCCLUSION_QUERIES - 1);

			i = 0;
			avCount = -1;
			do
			{
				if(i >= ocCount)
				{
					i = 0;
				}

				qglGetQueryObjectivARB(tr.occlusionQueryObjects[i], GL_QUERY_RESULT_AVAILABLE_ARB, &available);

				if(available)
					avCount++;

				i++;

			} while(avCount < limit);

			if(r_speeds->integer == 7)
			{
				endTime = ri.Milliseconds();
				backEnd.pc.c_occlusionQueriesResponseTime = endTime - startTime;
			}
		}

		// reenable writes to depth and color buffers
		GL_State(GLS_DEPTHMASK_TRUE);

		// loop trough all light interactions and fetch results for each last interaction
		// then copy result to all other interactions that belong to the same light
		ocCount = -1;
		iaFirst = 0;
		queryObjects = qtrue;
		oldLight = NULL;
		for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
		{
			backEnd.currentLight = light = ia->light;

			if(light != oldLight)
			{
				iaFirst = iaCount;
			}

			if(!queryObjects)
			{
				ia->occlusionQuerySamples = ocSamples;

				if(ocSamples <= 0)
				{
					backEnd.pc.c_occlusionQueriesInteractionsCulled++;
				}
			}

			if(!ia->next)
			{
				if(queryObjects)
				{
					if(!ia->noOcclusionQueries &&
					   ocCount <
					   (MAX_OCCLUSION_QUERIES - 1) /*&& R_CullLightPoint(light, backEnd.viewParms.orientation.origin) == CULL_OUT */ )
					{
						ocCount++;

						qglGetQueryObjectivARB(tr.occlusionQueryObjects[ocCount], GL_QUERY_RESULT_AVAILABLE_ARB, &available);
						if(available)
						{
							backEnd.pc.c_occlusionQueriesAvailable++;

							// get the object and store it in the occlusion bits for the light
							qglGetQueryObjectivARB(tr.occlusionQueryObjects[ocCount], GL_QUERY_RESULT, &ocSamples);

							if(ocSamples <= 0)
							{
								backEnd.pc.c_occlusionQueriesLightsCulled++;
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
					ia = &backEnd.viewParms.interactions[iaFirst];
					iaCount = iaFirst;
					queryObjects = qfalse;
				}
				else
				{
					if(iaCount < (backEnd.viewParms.numInteractions - 1))
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

	GL_CheckErrors();
}

/*
some debug utilities for displaying entity AABBs and so on
FIXME rewrite this to use the genericSingle GLSL shader
this fixed function pipeline stuff is broken
*/
static void RB_RenderDebugUtils()
{
	GLimp_LogComment("--- RB_RenderDebugUtils ---\n");

#if 1
	if(r_showLightTransforms->integer || r_showShadowLod->integer)
	{
		interaction_t  *ia;
		int             iaCount, j;
		trRefLight_t   *light;
		vec3_t          forward, left, up;
		vec4_t          tmp;
		vec4_t          lightColor;
		vec4_t          quadVerts[4];

		GL_BindProgram(&tr.genericSingleShader);
		GL_State(GLS_POLYMODE_LINE | GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// set uniforms
		GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
		GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
		GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
		}
		GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

		for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
		{
			light = ia->light;

			if(!ia->next)
			{
				if(r_showShadowLod->integer)
				{
					if(light->shadowLOD == 0)
					{
						VectorCopy4(colorRed, lightColor);
					}
					else if(light->shadowLOD == 1)
					{
						VectorCopy4(colorGreen, lightColor);
					}
					else if(light->shadowLOD == 2)
					{
						VectorCopy4(colorBlue, lightColor);
					}
					else if(light->shadowLOD == 3)
					{
						VectorCopy4(colorYellow, lightColor);
					}
					else if(light->shadowLOD == 4)
					{
						VectorCopy4(colorMagenta, lightColor);
					}
					else if(light->shadowLOD == 5)
					{
						VectorCopy4(colorCyan, lightColor);
					}
					else
					{
						VectorCopy4(colorMdGrey, lightColor);
					}
				}
				else
				{
					VectorCopy4(g_color_table[iaCount % 8], lightColor);
				}

				// set up the transformation matrix
				R_RotateLightForViewParms(light, &backEnd.viewParms, &backEnd.orientation);
				GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);
				GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

				MatrixToVectorsFLU(matrixIdentity, forward, left, up);
				VectorMA(vec3_origin, 16, forward, forward);
				VectorMA(vec3_origin, 16, left, left);
				VectorMA(vec3_origin, 16, up, up);

				/*
				// draw axis
				qglBegin(GL_LINES);

				// draw orientation
				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorRed);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(forward);

				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorGreen);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(left);

				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorBlue);
				qglVertex3fv(vec3_origin);
				qglVertex3fv(up);

				// draw special vectors
				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorYellow);
				qglVertex3fv(vec3_origin);
				VectorSubtract(light->origin, backEnd.orientation.origin, tmp);
				light->transformed[0] = DotProduct(tmp, backEnd.orientation.axis[0]);
				light->transformed[1] = DotProduct(tmp, backEnd.orientation.axis[1]);
				light->transformed[2] = DotProduct(tmp, backEnd.orientation.axis[2]);
				qglVertex3fv(light->transformed);

				qglEnd();
				*/

				switch (light->l.rlType)
				{
					case RL_OMNI:
					{
						tess.numIndexes = 0;
						tess.numVertexes = 0;

						VectorSet4(quadVerts[0], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[1], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[2], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[3], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						VectorSet4(quadVerts[0], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[1], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[2], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[3], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						VectorSet4(quadVerts[0], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[1], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[2], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[3], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						VectorSet4(quadVerts[0], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[1], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[2], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[3], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						VectorSet4(quadVerts[0], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[1], light->localBounds[0][0], light->localBounds[0][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[2], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[3], light->localBounds[1][0], light->localBounds[0][1], light->localBounds[0][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						VectorSet4(quadVerts[0], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[0][2], 1);
						VectorSet4(quadVerts[1], light->localBounds[1][0], light->localBounds[1][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[2], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[1][2], 1);
						VectorSet4(quadVerts[3], light->localBounds[0][0], light->localBounds[1][1], light->localBounds[0][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						Tess_UpdateVBOs(ATTR_POSITION | ATTR_COLOR);
						Tess_DrawElements();

						tess.numIndexes = 0;
						tess.numVertexes = 0;
						break;
					}

					case RL_PROJ:
					{
						float           xMin, xMax, yMin, yMax;
						float           zNear, zFar;
						vec3_t          corners[4];

						zNear = light->l.distNear;
						zFar = light->l.distFar;

						xMax = zNear * tan(light->l.fovX * M_PI / 360.0f);
						xMin = -xMax;

						yMax = zNear * tan(light->l.fovY * M_PI / 360.0f);
						yMin = -yMax;


						corners[0][0] = zFar;
						corners[0][1] = xMin * zFar;
						corners[0][2] = yMin * zFar;

						corners[1][0] = zFar;
						corners[1][1] = xMax * zFar;
						corners[1][2] = yMin * zFar;

						corners[2][0] = zFar;
						corners[2][1] = xMax * zFar;
						corners[2][2] = yMax * zFar;

						corners[3][0] = zFar;
						corners[3][1] = xMin * zFar;
						corners[3][2] = yMax * zFar;

						/*
						// draw pyramid
						qglBegin(GL_LINES);

						qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, lightColor);
						for(j = 0; j < 4; j++)
						{
							qglVertex3fv(corners[j]);
							qglVertex3fv(corners[(j + 1) % 4]);

							qglVertex3fv(vec3_origin);
							qglVertex3fv(corners[j]);
						}
						qglEnd();
						*/

						tess.numVertexes = 0;
						tess.numIndexes = 0;

						for(j = 0; j < 4; j++)
						{
							VectorCopy(corners[j], tess.xyz[tess.numVertexes]);
							VectorCopy4(lightColor, tess.colors[tess.numVertexes]);
							tess.indexes[tess.numIndexes++] = tess.numVertexes;
							tess.numVertexes++;

							VectorCopy(corners[(j + 1) % 4], tess.xyz[tess.numVertexes]);
							VectorCopy4(lightColor, tess.colors[tess.numVertexes]);
							tess.indexes[tess.numIndexes++] = tess.numVertexes;
							tess.numVertexes++;

							VectorCopy(vec3_origin, tess.xyz[tess.numVertexes]);
							VectorCopy4(lightColor, tess.colors[tess.numVertexes]);
							tess.indexes[tess.numIndexes++] = tess.numVertexes;
							tess.numVertexes++;
						}

						VectorSet4(quadVerts[0], corners[0][0], corners[0][1], corners[0][2], 1);
						VectorSet4(quadVerts[1], corners[1][0], corners[1][1], corners[1][2], 1);
						VectorSet4(quadVerts[2], corners[2][0], corners[2][1], corners[2][2], 1);
						VectorSet4(quadVerts[3], corners[3][0], corners[3][1], corners[3][2], 1);
						Tess_AddQuadStamp2(quadVerts, lightColor);

						Tess_UpdateVBOs(ATTR_POSITION | ATTR_COLOR);
						Tess_DrawElements();

						tess.numIndexes = 0;
						tess.numVertexes = 0;

						break;
					}

					default:
						break;
				}


				if(iaCount < (backEnd.viewParms.numInteractions - 1))
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
		backEnd.orientation = backEnd.viewParms.world;
		GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	}
#endif

#if 0
	if(r_showLightInteractions->integer)
	{
		int             i;
		int             cubeSides;
		interaction_t  *ia;
		int             iaCount;
		trRefLight_t   *light;
		trRefEntity_t  *entity;
		surfaceType_t  *surface;
		vec4_t          lightColor;

		GL_BindProgram(NULL);
		GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);

		for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
		{
			backEnd.currentEntity = entity = ia->entity;
			light = ia->light;
			surface = ia->surface;

			if(entity != &tr.worldEntity)
			{
				// set up the transformation matrix
				R_RotateEntityForViewParms(backEnd.currentEntity, &backEnd.viewParms, &backEnd.orientation);
			}
			else
			{
				backEnd.orientation = backEnd.viewParms.world;
			}

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);

			if(r_shadows->integer >= 4 && light->l.rlType == RL_OMNI)
			{
#if 0
				VectorCopy4(colorMdGrey, lightColor);

				if(ia->cubeSideBits & CUBESIDE_PX)
				{
					VectorCopy4(colorBlack, lightColor);
				}
				if(ia->cubeSideBits & CUBESIDE_PY)
				{
					VectorCopy4(colorRed, lightColor);
				}
				if(ia->cubeSideBits & CUBESIDE_PZ)
				{
					VectorCopy4(colorGreen, lightColor);
				}
				if(ia->cubeSideBits & CUBESIDE_NX)
				{
					VectorCopy4(colorYellow, lightColor);
				}
				if(ia->cubeSideBits & CUBESIDE_NY)
				{
					VectorCopy4(colorBlue, lightColor);
				}
				if(ia->cubeSideBits & CUBESIDE_NZ)
				{
					VectorCopy4(colorCyan, lightColor);
				}
				if(ia->cubeSideBits == CUBESIDE_CLIPALL)
				{
					VectorCopy4(colorMagenta, lightColor);
				}
#else
				// count how many cube sides are in use for this interaction
				cubeSides = 0;
				for(i = 0; i < 6; i++)
				{
					if(ia->cubeSideBits & (1 << i))
					{
						cubeSides++;
					}
				}

				VectorCopy4(g_color_table[cubeSides], lightColor);
#endif
			}
			else
			{
				VectorCopy4(colorMdGrey, lightColor);
			}

			lightColor[0] *= 0.5f;
			lightColor[1] *= 0.5f;
			lightColor[2] *= 0.5f;
			//lightColor[3] *= 0.2f;

			if(*surface == SF_FACE)
			{
				srfSurfaceFace_t *face;

				face = (srfSurfaceFace_t *) surface;
				R_DebugBoundingBox(vec3_origin, face->bounds[0], face->bounds[1], lightColor);
			}
			else if(*surface == SF_GRID)
			{
				srfGridMesh_t  *grid;

				grid = (srfGridMesh_t *) surface;
				R_DebugBoundingBox(vec3_origin, grid->meshBounds[0], grid->meshBounds[1], lightColor);
			}
			else if(*surface == SF_TRIANGLES)
			{
				srfTriangles_t *tri;

				tri = (srfTriangles_t *) surface;
				R_DebugBoundingBox(vec3_origin, tri->bounds[0], tri->bounds[1], lightColor);
			}
			else if(*surface == SF_VBO_MESH)
			{
				srfVBOMesh_t   *srf = (srfVBOMesh_t *) surface;

				R_DebugBoundingBox(vec3_origin, srf->bounds[0], srf->bounds[1], lightColor);
			}
			else if(*surface == SF_MDX)
			{
				R_DebugBoundingBox(vec3_origin, entity->localBounds[0], entity->localBounds[1], lightColor);
			}

			if(!ia->next)
			{
				if(iaCount < (backEnd.viewParms.numInteractions - 1))
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
		backEnd.orientation = backEnd.viewParms.world;
		GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	}
#endif

#if 1
	if(r_showEntityTransforms->integer)
	{
		trRefEntity_t  *ent;
		int             i;
		vec4_t          quadVerts[4];

		GL_BindProgram(&tr.genericSingleShader);
		GL_State(GLS_POLYMODE_LINE | GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// set uniforms
		GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
		GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
		GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
		}
		GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

		ent = backEnd.refdef.entities;
		for(i = 0; i < backEnd.refdef.numEntities; i++, ent++)
		{
			if((ent->e.renderfx & RF_THIRD_PERSON) && !backEnd.viewParms.isPortal)
				continue;

			// set up the transformation matrix
			R_RotateEntityForViewParms(ent, &backEnd.viewParms, &backEnd.orientation);
			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);
			GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

			R_DebugAxis(vec3_origin, matrixIdentity);
			//R_DebugBoundingBox(vec3_origin, ent->localBounds[0], ent->localBounds[1], colorMagenta);
			tess.numIndexes = 0;
			tess.numVertexes = 0;

			VectorSet4(quadVerts[0], ent->localBounds[0][0], ent->localBounds[0][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[1], ent->localBounds[0][0], ent->localBounds[1][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[2], ent->localBounds[0][0], ent->localBounds[1][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[3], ent->localBounds[0][0], ent->localBounds[0][1], ent->localBounds[1][2], 1);
			Tess_AddQuadStamp2(quadVerts, colorRed);

			VectorSet4(quadVerts[0], ent->localBounds[1][0], ent->localBounds[0][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[1], ent->localBounds[1][0], ent->localBounds[1][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[2], ent->localBounds[1][0], ent->localBounds[1][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[3], ent->localBounds[1][0], ent->localBounds[0][1], ent->localBounds[0][2], 1);
			Tess_AddQuadStamp2(quadVerts, colorGreen);

			VectorSet4(quadVerts[0], ent->localBounds[0][0], ent->localBounds[0][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[1], ent->localBounds[0][0], ent->localBounds[1][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[2], ent->localBounds[1][0], ent->localBounds[1][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[3], ent->localBounds[1][0], ent->localBounds[0][1], ent->localBounds[1][2], 1);
			Tess_AddQuadStamp2(quadVerts, colorBlue);

			VectorSet4(quadVerts[0], ent->localBounds[1][0], ent->localBounds[0][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[1], ent->localBounds[1][0], ent->localBounds[1][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[2], ent->localBounds[0][0], ent->localBounds[1][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[3], ent->localBounds[0][0], ent->localBounds[0][1], ent->localBounds[0][2], 1);
			Tess_AddQuadStamp2(quadVerts, colorYellow);

			VectorSet4(quadVerts[0], ent->localBounds[0][0], ent->localBounds[0][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[1], ent->localBounds[0][0], ent->localBounds[0][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[2], ent->localBounds[1][0], ent->localBounds[0][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[3], ent->localBounds[1][0], ent->localBounds[0][1], ent->localBounds[0][2], 1);
			Tess_AddQuadStamp2(quadVerts, colorMagenta);

			VectorSet4(quadVerts[0], ent->localBounds[1][0], ent->localBounds[1][1], ent->localBounds[0][2], 1);
			VectorSet4(quadVerts[1], ent->localBounds[1][0], ent->localBounds[1][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[2], ent->localBounds[0][0], ent->localBounds[1][1], ent->localBounds[1][2], 1);
			VectorSet4(quadVerts[3], ent->localBounds[0][0], ent->localBounds[1][1], ent->localBounds[0][2], 1);
			Tess_AddQuadStamp2(quadVerts, colorCyan);

			Tess_UpdateVBOs(ATTR_POSITION | ATTR_COLOR);
			Tess_DrawElements();

			tess.numIndexes = 0;
			tess.numVertexes = 0;


			// go back to the world modelview matrix
			//backEnd.orientation = backEnd.viewParms.world;
			//GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);

			//R_DebugBoundingBox(vec3_origin, ent->worldBounds[0], ent->worldBounds[1], colorCyan);
		}

		// go back to the world modelview matrix
		backEnd.orientation = backEnd.viewParms.world;
		GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	}
#endif

#if 1
	if(r_showSkeleton->integer)
	{
		int             i, j, k, parentIndex;
		trRefEntity_t  *ent;
		vec3_t          origin, offset;
		vec3_t          forward, right, up;
		vec3_t          diff, tmp, tmp2, tmp3;
		vec_t           length;
		vec4_t          tetraVerts[4];
		static refSkeleton_t skeleton;
		refSkeleton_t  *skel;

		GL_BindProgram(&tr.genericSingleShader);
		GL_Cull(CT_TWO_SIDED);

		// set uniforms
		GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
		GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
		GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
		}
		GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(tr.charsetImage);
		GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

		ent = backEnd.refdef.entities;
		for(i = 0; i < backEnd.refdef.numEntities; i++, ent++)
		{
			if((ent->e.renderfx & RF_THIRD_PERSON) && !backEnd.viewParms.isPortal)
				continue;

			// set up the transformation matrix
			R_RotateEntityForViewParms(ent, &backEnd.viewParms, &backEnd.orientation);
			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);
			GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);


			tess.numVertexes = 0;
			tess.numIndexes = 0;

			skel = NULL;
			if(ent->e.skeleton.type == SK_ABSOLUTE)
			{
				skel = &ent->e.skeleton;
			}
			else
			{
				model_t        *model;
				refBone_t      *bone;

				model = R_GetModelByHandle(ent->e.hModel);

				if(model)
				{
					switch (model->type)
					{
						case MOD_MD5:
						{
							// copy absolute bones
							skeleton.numBones = model->md5->numBones;
							for(j = 0, bone = &skeleton.bones[0]; j < skeleton.numBones; j++, bone++)
							{
								Q_strncpyz(bone->name, model->md5->bones[j].name, sizeof(bone->name));

								bone->parentIndex = model->md5->bones[j].parentIndex;
								VectorCopy(model->md5->bones[j].origin, bone->origin);
								VectorCopy(model->md5->bones[j].rotation, bone->rotation);
							}

							skel = &skeleton;
							break;
						}

						default:
							break;
					}
				}
			}

			if(skel)
			{
				static vec3_t	worldOrigins[MAX_BONES];

				GL_State(GLS_POLYMODE_LINE | GLS_DEPTHTEST_DISABLE);

				for(j = 0; j < skel->numBones; j++)
				{
					parentIndex = skel->bones[j].parentIndex;

					if(parentIndex < 0)
					{
						VectorClear(origin);
					}
					else
					{
						VectorCopy(skel->bones[parentIndex].origin, origin);
					}
					VectorCopy(skel->bones[j].origin, offset);
					QuatToVectorsFRU(skel->bones[j].rotation, forward, right, up);

					VectorSubtract(offset, origin, diff);
					if((length = VectorNormalize(diff)))
					{
						PerpendicularVector(tmp, diff);
						//VectorCopy(up, tmp);

						VectorScale(tmp, length * 0.1, tmp2);
						VectorMA(tmp2, length * 0.2, diff, tmp2);

						for(k = 0; k < 3; k++)
						{
							RotatePointAroundVector(tmp3, diff, tmp2, k * 120);
							VectorAdd(tmp3, origin, tmp3);
							VectorCopy(tmp3, tetraVerts[k]);
							tetraVerts[k][3] = 1;
						}

						VectorCopy(origin, tetraVerts[3]);
						tetraVerts[3][3] = 1;
						Tess_AddTetrahedron(tetraVerts, g_color_table[j % MAX_CCODES]);

						VectorCopy(offset, tetraVerts[3]);
						tetraVerts[3][3] = 1;
						Tess_AddTetrahedron(tetraVerts, g_color_table[j % MAX_CCODES]);
					}

					MatrixTransformPoint(backEnd.orientation.transformMatrix, skel->bones[j].origin, worldOrigins[j]);
				}

				Tess_UpdateVBOs(ATTR_POSITION | ATTR_TEXCOORD | ATTR_COLOR);

				Tess_DrawElements();

				tess.numVertexes = 0;
				tess.numIndexes = 0;

				//if(r_showSkeleton->integer == 2)
				{
					GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);

					// go back to the world modelview matrix
					backEnd.orientation = backEnd.viewParms.world;
					GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
					GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

					// draw names
					for(j = 0; j < skel->numBones; j++)
					{
						vec3_t          left, up;
						float           radius;
						vec3_t			origin;

						// calculate the xyz locations for the four corners
						radius = 0.4;
						VectorScale(backEnd.viewParms.orientation.axis[1], radius, left);
						VectorScale(backEnd.viewParms.orientation.axis[2], radius, up);

						if(backEnd.viewParms.isMirror)
						{
							VectorSubtract(vec3_origin, left, left);
						}

						for(k = 0; k < strlen(skel->bones[j].name); k++)
						{
							int				ch;
							int             row, col;
							float           frow, fcol;
							float           size;

							ch = skel->bones[j].name[k];
							ch &= 255;

							if(ch == ' ')
							{
								break;
							}

							row = ch >> 4;
							col = ch & 15;

							frow = row * 0.0625;
							fcol = col * 0.0625;
							size = 0.0625;

							VectorMA(worldOrigins[j], -(k + 2.0f), left, origin);
							Tess_AddQuadStampExt(origin, left, up, colorWhite, fcol, frow, fcol + size, frow + size);
						}

						Tess_UpdateVBOs(ATTR_POSITION | ATTR_TEXCOORD | ATTR_COLOR);

						Tess_DrawElements();

						tess.numVertexes = 0;
						tess.numIndexes = 0;
					}
				}
			}

			tess.numVertexes = 0;
			tess.numIndexes = 0;
		}
	}
#endif

	if(r_showLightScissors->integer)
	{
		interaction_t  *ia;
		int             iaCount;
		matrix_t        ortho;
		vec4_t          quadVerts[4];

		GL_BindProgram(&tr.genericSingleShader);
		GL_State(GLS_POLYMODE_LINE | GLS_DEPTHTEST_DISABLE);
		GL_Cull(CT_TWO_SIDED);

		// set uniforms
		GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
		GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
		GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
		}
		GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

		// set 2D virtual screen size
		GL_PushMatrix();
		MatrixSetupOrthogonalProjection(ortho, backEnd.viewParms.viewportX,
										backEnd.viewParms.viewportX + backEnd.viewParms.viewportWidth,
										backEnd.viewParms.viewportY,
										backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight, -99999, 99999);
		GL_LoadProjectionMatrix(ortho);
		GL_LoadModelViewMatrix(matrixIdentity);

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

		for(iaCount = 0, ia = &backEnd.viewParms.interactions[0]; iaCount < backEnd.viewParms.numInteractions;)
		{
			if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA)
			{
				if(!ia->occlusionQuerySamples)
				{
					qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorRed);
				}
				else
				{
					qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorGreen);
				}

				VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
				VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
				VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
				VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
				Tess_InstantQuad(quadVerts);
			}
			else if(r_shadows->integer == 3 && qglDepthBoundsEXT)
			{
				if(ia->noDepthBoundsTest)
				{
					qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorBlue);
				}
				else
				{
					qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorGreen);
				}

				VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
				VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
				VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
				VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
				Tess_InstantQuad(quadVerts);
			}
			else
			{
				qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

				VectorSet4(quadVerts[0], ia->scissorX, ia->scissorY, 0, 1);
				VectorSet4(quadVerts[1], ia->scissorX + ia->scissorWidth - 1, ia->scissorY, 0, 1);
				VectorSet4(quadVerts[2], ia->scissorX + ia->scissorWidth - 1, ia->scissorY + ia->scissorHeight - 1, 0, 1);
				VectorSet4(quadVerts[3], ia->scissorX, ia->scissorY + ia->scissorHeight - 1, 0, 1);
				Tess_InstantQuad(quadVerts);
			}

			if(!ia->next)
			{
				if(iaCount < (backEnd.viewParms.numInteractions - 1))
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

		GL_PopMatrix();
	}

#if 1
	if(r_showCubeProbes->integer)
	{
		cubemapProbe_t *cubeProbe;
		int             j;
		vec4_t          quadVerts[4];
		vec4_t			plane;
		vec3_t			mins = {-8, -8, -8};
		vec3_t			maxs = { 8,  8,  8};
		vec3_t			viewOrigin;

		if(tr.refdef.rdflags & (RDF_NOWORLDMODEL | RDF_NOCUBEMAP))
		{
			return;
		}

		// enable shader, set arrays
		GL_BindProgram(&tr.reflectionShader_C);
		GL_Cull(CT_FRONT_SIDED);

		// set uniforms
		VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
		GLSL_SetUniform_ViewOrigin(&tr.reflectionShader_C, viewOrigin);
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
		}

		for(j = 0; j < tr.cubeProbes.currentElements; j++)
		{
			cubeProbe = Com_GrowListElement(&tr.cubeProbes, j);

			// bind u_ColorMap
			GL_SelectTexture(0);
			GL_Bind(cubeProbe->cubemap);

			// set up the transformation matrix
			MatrixSetupTranslation(backEnd.orientation.transformMatrix, cubeProbe->origin[0], cubeProbe->origin[1], cubeProbe->origin[2]);
			MatrixMultiply(backEnd.viewParms.world.viewMatrix, backEnd.orientation.transformMatrix, backEnd.orientation.modelViewMatrix);

			GL_LoadModelViewMatrix(backEnd.orientation.modelViewMatrix);
			GLSL_SetUniform_ModelMatrix(&tr.reflectionShader_C, backEnd.orientation.transformMatrix);
			GLSL_SetUniform_ModelViewProjectionMatrix(&tr.reflectionShader_C, glState.modelViewProjectionMatrix[glState.stackIndex]);

			tess.numIndexes = 0;
			tess.numVertexes = 0;

			VectorSet4(quadVerts[0], mins[0], mins[1], mins[2], 1);
			VectorSet4(quadVerts[1], mins[0], maxs[1], mins[2], 1);
			VectorSet4(quadVerts[2], mins[0], maxs[1], maxs[2], 1);
			VectorSet4(quadVerts[3], mins[0], mins[1], maxs[2], 1);
			Tess_AddQuadStamp2WithNormals(quadVerts, colorWhite);

			VectorSet4(quadVerts[0], maxs[0], mins[1], maxs[2], 1);
			VectorSet4(quadVerts[1], maxs[0], maxs[1], maxs[2], 1);
			VectorSet4(quadVerts[2], maxs[0], maxs[1], mins[2], 1);
			VectorSet4(quadVerts[3], maxs[0], mins[1], mins[2], 1);
			Tess_AddQuadStamp2WithNormals(quadVerts, colorWhite);

			VectorSet4(quadVerts[0], mins[0], mins[1], maxs[2], 1);
			VectorSet4(quadVerts[1], mins[0], maxs[1], maxs[2], 1);
			VectorSet4(quadVerts[2], maxs[0], maxs[1], maxs[2], 1);
			VectorSet4(quadVerts[3], maxs[0], mins[1], maxs[2], 1);
			Tess_AddQuadStamp2WithNormals(quadVerts, colorWhite);

			VectorSet4(quadVerts[0], maxs[0], mins[1], mins[2], 1);
			VectorSet4(quadVerts[1], maxs[0], maxs[1], mins[2], 1);
			VectorSet4(quadVerts[2], mins[0], maxs[1], mins[2], 1);
			VectorSet4(quadVerts[3], mins[0], mins[1], mins[2], 1);
			Tess_AddQuadStamp2WithNormals(quadVerts, colorWhite);

			VectorSet4(quadVerts[0], mins[0], mins[1], mins[2], 1);
			VectorSet4(quadVerts[1], mins[0], mins[1], maxs[2], 1);
			VectorSet4(quadVerts[2], maxs[0], mins[1], maxs[2], 1);
			VectorSet4(quadVerts[3], maxs[0], mins[1], mins[2], 1);
			Tess_AddQuadStamp2WithNormals(quadVerts, colorWhite);

			VectorSet4(quadVerts[0], maxs[0], maxs[1], mins[2], 1);
			VectorSet4(quadVerts[1], maxs[0], maxs[1], maxs[2], 1);
			VectorSet4(quadVerts[2], mins[0], maxs[1], maxs[2], 1);
			VectorSet4(quadVerts[3], mins[0], maxs[1], mins[2], 1);
			Tess_AddQuadStamp2WithNormals(quadVerts, colorWhite);

			Tess_UpdateVBOs(ATTR_POSITION | ATTR_NORMAL);
			Tess_DrawElements();

			tess.numIndexes = 0;
			tess.numVertexes = 0;
		}

		// go back to the world modelview matrix
		backEnd.orientation = backEnd.viewParms.world;
		GL_LoadModelViewMatrix(backEnd.viewParms.world.modelViewMatrix);
	}
#endif

	GL_CheckErrors();
}

/*
==================
RB_RenderView
==================
*/
static void RB_RenderView(void)
{
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va
						 ("--- RB_RenderView( %i surfaces, %i interactions ) ---\n", backEnd.viewParms.numDrawSurfs,
						  backEnd.viewParms.numInteractions));
	}

	GL_CheckErrors();

	backEnd.pc.c_surfaces += backEnd.viewParms.numDrawSurfs;

	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
	   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		int             clearBits = 0;

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

		// clear frame buffer objects
		R_BindFBO(tr.deferredRenderFBO);
		//qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		clearBits = GL_DEPTH_BUFFER_BIT;

		/*
		   if(r_measureOverdraw->integer || r_shadows->integer == 3)
		   {
		   clearBits |= GL_STENCIL_BUFFER_BIT;
		   }
		 */
		if(!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
		{
			clearBits |= GL_COLOR_BUFFER_BIT;
			GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// FIXME: get color of sky
		}
		qglClear(clearBits);

		R_BindFBO(tr.geometricRenderFBO);
		if(!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
		{
			clearBits = GL_COLOR_BUFFER_BIT;
			GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// FIXME: get color of sky
		}
		else
		{
			if(glConfig.framebufferBlitAvailable)
			{
				// copy color of the main context to deferredRenderFBO
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   GL_COLOR_BUFFER_BIT,
									   GL_NEAREST);
			}
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

		GL_CheckErrors();

#if 1
		// draw everything that is opaque
		R_BindFBO(tr.deferredRenderFBO);
		RB_RenderDrawSurfaces(qtrue, qfalse);
#endif

		RB_RenderDrawSurfacesIntoGeometricBuffer();

		// try to cull lights using hardware occlusion queries
		R_BindFBO(tr.deferredRenderFBO);
		RB_RenderLightOcclusionQueries();

		if(!r_showDeferredRender->integer)
		{
			if(r_shadows->integer >= 4)
			{
				// render dynamic shadowing and lighting using shadow mapping
				RB_RenderInteractionsDeferredShadowMapped();
			}
			else
			{
				// render dynamic lighting
				RB_RenderInteractionsDeferred();
			}
		}

		// draw everything that is translucent
		R_BindFBO(tr.deferredRenderFBO);
		RB_RenderDrawSurfaces(qfalse, qfalse);

		// render global fog
		R_BindFBO(tr.deferredRenderFBO);
		RB_RenderUniformFog();

		// render debug information
		R_BindFBO(tr.deferredRenderFBO);
		RB_RenderDebugUtils();

		// scale down rendered HDR scene to 1 / 4th
		if(r_hdrRendering->integer)
		{
			if(glConfig.framebufferBlitAvailable)
			{
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.downScaleFBO_quarter->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
										0, 0, glConfig.vidWidth * 0.25f, glConfig.vidHeight * 0.25f,
										GL_COLOR_BUFFER_BIT,
										GL_LINEAR);

				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.downScaleFBO_64x64->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   0, 0, 64, 64,
									   GL_COLOR_BUFFER_BIT,
									   GL_LINEAR);
			}
			else
			{
				// FIXME add non EXT_framebuffer_blit code
			}

			RB_CalculateAdaptation();
		}
		else
		{
			if(glConfig.framebufferBlitAvailable)
			{
				// copy deferredRenderFBO to downScaleFBO_quarter
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.downScaleFBO_quarter->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
										0, 0, glConfig.vidWidth * 0.25f, glConfig.vidHeight * 0.25f,
										GL_COLOR_BUFFER_BIT,
										GL_LINEAR);
			}
			else
			{
				// FIXME add non EXT_framebuffer_blit code
			}
		}

		GL_CheckErrors();

		// render bloom post process effect
		RB_RenderBloom();

		// copy offscreen rendered scene to the current OpenGL context
		RB_RenderDeferredShadingResultToFrameBuffer();

		if(backEnd.viewParms.isPortal)
		{
			if(glConfig.framebufferBlitAvailable)
			{
				// copy deferredRenderFBO to portalRenderFBO
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.portalRenderFBO->frameBuffer);
				qglBlitFramebufferEXT(0, 0, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height,
									   0, 0, tr.portalRenderFBO->width, tr.portalRenderFBO->height,
									   GL_COLOR_BUFFER_BIT,
									   GL_NEAREST);
			}
			else
			{
				// capture current color buffer
				GL_SelectTexture(0);
				GL_Bind(tr.portalRenderImage);
				qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.portalRenderImage->uploadWidth, tr.portalRenderImage->uploadHeight);
			}
			backEnd.pc.c_portals++;
		}
	}
	else
	{
		int             clearBits = 0;
		int             startTime = 0, endTime = 0;

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

		// disable offscreen rendering
		if(glConfig.framebufferObjectAvailable)
		{
			if(r_hdrRendering->integer && glConfig.textureFloatAvailable)
				R_BindFBO(tr.deferredRenderFBO);
			else
				R_BindNullFBO();
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

		if(!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
		{
			clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
			GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// FIXME: get color of sky
		}
		else
		{
			if(r_hdrRendering->integer && glConfig.textureFloatAvailable && glConfig.framebufferObjectAvailable && glConfig.framebufferBlitAvailable)
			{
				// copy color of the main context to deferredRenderFBO
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   GL_COLOR_BUFFER_BIT,
									   GL_NEAREST);
			}
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

		GL_CheckErrors();

		if(r_speeds->integer == 9)
		{
			startTime = ri.Milliseconds();
		}

		// draw everything that is opaque into black so we can benefit from early-z rejections later
		//RB_RenderDrawSurfaces(qtrue, qtrue);

		// draw everything that is opaque
		RB_RenderDrawSurfaces(qtrue, qfalse);

		// render ambient occlusion process effect
		// Tr3B: needs way more work RB_RenderScreenSpaceAmbientOcclusion(qfalse);

		if(r_speeds->integer == 9)
		{
			qglFinish();
			endTime = ri.Milliseconds();
			backEnd.pc.c_forwardAmbientTime = endTime - startTime;
		}

		// try to cull lights using hardware occlusion queries
		RB_RenderLightOcclusionQueries();

		if(r_shadows->integer >= 4)
		{
			// render dynamic shadowing and lighting using shadow mapping
			RB_RenderInteractionsShadowMapped();

			// render player shadows if any
			RB_RenderInteractionsDeferredInverseShadows();
		}
		else if(r_shadows->integer == 3)
		{
			// render dynamic shadowing and lighting using stencil shadow volumes
			RB_RenderInteractionsStencilShadowed();
		}
		else
		{
			// render dynamic lighting
			RB_RenderInteractions();
		}

		if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
			R_BindFBO(tr.deferredRenderFBO);

		// draw everything that is translucent
		RB_RenderDrawSurfaces(qfalse, qfalse);

		// render global fog post process effect
		RB_RenderUniformFog(qfalse);

		// scale down rendered HDR scene to 1 / 4th
		if(r_hdrRendering->integer && glConfig.textureFloatAvailable && glConfig.framebufferObjectAvailable)
		{
			if(glConfig.framebufferBlitAvailable)
			{
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.downScaleFBO_quarter->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
										0, 0, glConfig.vidWidth * 0.25f, glConfig.vidHeight * 0.25f,
										GL_COLOR_BUFFER_BIT,
										GL_LINEAR);

				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.downScaleFBO_64x64->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   0, 0, 64, 64,
									   GL_COLOR_BUFFER_BIT,
									   GL_LINEAR);
			}
			else
			{
				// FIXME add non EXT_framebuffer_blit code
			}

			RB_CalculateAdaptation();
		}
		else
		{
			/*
			Tr3B: FIXME this causes: caught OpenGL error:
			GL_INVALID_OPERATION in file code/renderer/tr_backend.c line 6479

			if(glConfig.framebufferBlitAvailable)
			{
				// copy deferredRenderFBO to downScaleFBO_quarter
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.downScaleFBO_quarter->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
										0, 0, glConfig.vidWidth * 0.25f, glConfig.vidHeight * 0.25f,
										GL_COLOR_BUFFER_BIT,
										GL_NEAREST);
			}
			else
			{
				// FIXME add non EXT_framebuffer_blit code
			}
			*/
		}

		GL_CheckErrors();

		// render depth of field post process effect
		RB_RenderDepthOfField(qfalse);

		// render bloom post process effect
		RB_RenderBloom();

		// copy offscreen rendered HDR scene to the current OpenGL context
		RB_RenderDeferredHDRResultToFrameBuffer();

		// render rotoscope post process effect
		RB_RenderRotoscope();

#if 0
		// add the sun flare
		RB_DrawSun();
#endif

#if 0
		// add light flares on lights that aren't obscured
		RB_RenderFlares();
#endif

		// render debug information
		RB_RenderDebugUtils();

		if(backEnd.viewParms.isPortal)
		{
			if(r_hdrRendering->integer && glConfig.textureFloatAvailable && glConfig.framebufferObjectAvailable && glConfig.framebufferBlitAvailable)
			{
				// copy deferredRenderFBO to portalRenderFBO
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.portalRenderFBO->frameBuffer);
				qglBlitFramebufferEXT(0, 0, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height,
				                       0, 0, tr.portalRenderFBO->width, tr.portalRenderFBO->height,
				                       GL_COLOR_BUFFER_BIT,
				                       GL_NEAREST);
			}
#if 0
			// FIXME: this trashes the OpenGL context for an unknown reason
			else if(glConfig.framebufferObjectAvailable && glConfig.framebufferBlitAvailable)
			{
				// copy main context to portalRenderFBO
				qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
				qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.portalRenderFBO->frameBuffer);
				qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   0, 0, glConfig.vidWidth, glConfig.vidHeight,
									   GL_COLOR_BUFFER_BIT,
									   GL_NEAREST);
			}
#endif
			else
			{
				// capture current color buffer
				GL_SelectTexture(0);
				GL_Bind(tr.portalRenderImage);
				qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.portalRenderImage->uploadWidth, tr.portalRenderImage->uploadHeight);
			}
			backEnd.pc.c_portals++;
		}
	}

	// copy to given byte buffer that is NOT a FBO
	if(tr.refdef.pixelTarget != NULL)
	{
		int             i;

		// need to convert Y axis
#if 0
		qglReadPixels(0, 0, tr.refdef.pixelTargetWidth, tr.refdef.pixelTargetHeight, GL_RGBA, GL_UNSIGNED_BYTE, tr.refdef.pixelTarget);
#else
		// Bugfix: drivers absolutely hate running in high res and using qglReadPixels near the top or bottom edge.
		// Soo.. lets do it in the middle.
		qglReadPixels(glConfig.vidWidth / 2, glConfig.vidHeight / 2, tr.refdef.pixelTargetWidth, tr.refdef.pixelTargetHeight, GL_RGBA,
					  GL_UNSIGNED_BYTE, tr.refdef.pixelTarget);
#endif

		for(i = 0; i < tr.refdef.pixelTargetWidth * tr.refdef.pixelTargetHeight; i++)
		{
			tr.refdef.pixelTarget[(i * 4) + 3] = 255;	//set the alpha pure white
		}
	}

	GL_CheckErrors();

	backEnd.pc.c_views++;
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


	RB_SetGL2D();

	qglVertexAttrib4fARB(ATTR_INDEX_NORMAL, 0, 0, 1, 1);
	qglVertexAttrib4fARB(ATTR_INDEX_COLOR, tr.identityLight, tr.identityLight, tr.identityLight, 1);

	GL_BindProgram(&tr.genericSingleShader);

	// set uniforms
	GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
	GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
	GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
	//GLSL_SetUniform_Color(&tr.genericSingleShader, colorWhite);
	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
	}
	GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(tr.scratchImage[client]);
	GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

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

	/*
	   qglBegin(GL_QUADS);
	   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, 0.5f / cols, 0.5f / rows, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x, y, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, (cols - 0.5f) / cols, 0.5f / rows, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x + w, y, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, (cols - 0.5f) / cols, (rows - 0.5f) / rows, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x + w, y + h, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, 0.5f / cols, (rows - 0.5f) / rows, 0, 1);
	   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x, y + h, 0, 1);
	   qglEnd();
	 */

	tess.numVertexes = 0;
	tess.numIndexes = 0;

	tess.xyz[tess.numVertexes][0] = x;
	tess.xyz[tess.numVertexes][1] = y;
	tess.xyz[tess.numVertexes][2] = 0;
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0] = 0.5f / cols;
	tess.texCoords[tess.numVertexes][1] = 0.5f / rows;
	tess.texCoords[tess.numVertexes][2] = 0;
	tess.texCoords[tess.numVertexes][3] = 1;
	tess.numVertexes++;

	tess.xyz[tess.numVertexes][0] = x + w;
	tess.xyz[tess.numVertexes][1] = y;
	tess.xyz[tess.numVertexes][2] = 0;
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0] = (cols - 0.5f) / cols;
	tess.texCoords[tess.numVertexes][1] = 0.5f / rows;
	tess.texCoords[tess.numVertexes][2] = 0;
	tess.texCoords[tess.numVertexes][3] = 1;
	tess.numVertexes++;

	tess.xyz[tess.numVertexes][0] = x + w;
	tess.xyz[tess.numVertexes][1] = y + h;
	tess.xyz[tess.numVertexes][2] = 0;
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0] = (cols - 0.5f) / cols;
	tess.texCoords[tess.numVertexes][1] = (rows - 0.5f) / rows;
	tess.texCoords[tess.numVertexes][2] = 0;
	tess.texCoords[tess.numVertexes][3] = 1;
	tess.numVertexes++;

	tess.xyz[tess.numVertexes][0] = x;
	tess.xyz[tess.numVertexes][1] = y + h;
	tess.xyz[tess.numVertexes][2] = 0;
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0] = 0.5f / cols;
	tess.texCoords[tess.numVertexes][1] = (rows - 0.5f) / rows;
	tess.texCoords[tess.numVertexes][2] = 0;
	tess.texCoords[tess.numVertexes][3] = 1;
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = 0;
	tess.indexes[tess.numIndexes++] = 1;
	tess.indexes[tess.numIndexes++] = 2;
	tess.indexes[tess.numIndexes++] = 0;
	tess.indexes[tess.numIndexes++] = 2;
	tess.indexes[tess.numIndexes++] = 3;

	Tess_UpdateVBOs(ATTR_POSITION | ATTR_TEXCOORD);

	Tess_DrawElements();

	tess.numVertexes = 0;
	tess.numIndexes = 0;

	GL_CheckErrors();
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

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		qglTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, colorBlack);
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

	GL_CheckErrors();
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

	backEnd.color2D[0] = cmd->color[0];
	backEnd.color2D[1] = cmd->color[1];
	backEnd.color2D[2] = cmd->color[2];
	backEnd.color2D[3] = cmd->color[3];

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void     *RB_StretchPic(const void *data)
{
	int             i;
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
		Tess_Begin(Tess_StageIteratorGeneric, shader, NULL, qfalse, qfalse, -1);
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


	for(i = 0; i < 4; i++)
	{
		tess.colors[numVerts + i][0] = backEnd.color2D[0];
		tess.colors[numVerts + i][1] = backEnd.color2D[1];
		tess.colors[numVerts + i][2] = backEnd.color2D[2];
		tess.colors[numVerts + i][3] = backEnd.color2D[3];
	}

	tess.xyz[numVerts][0] = cmd->x;
	tess.xyz[numVerts][1] = cmd->y;
	tess.xyz[numVerts][2] = 0;
	tess.xyz[numVerts][3] = 1;

	tess.texCoords[numVerts][0] = cmd->s1;
	tess.texCoords[numVerts][1] = cmd->t1;
	tess.texCoords[numVerts][2] = 0;
	tess.texCoords[numVerts][3] = 1;

	tess.xyz[numVerts + 1][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 1][1] = cmd->y;
	tess.xyz[numVerts + 1][2] = 0;
	tess.xyz[numVerts + 1][3] = 1;

	tess.texCoords[numVerts + 1][0] = cmd->s2;
	tess.texCoords[numVerts + 1][1] = cmd->t1;
	tess.texCoords[numVerts + 1][2] = 0;
	tess.texCoords[numVerts + 1][3] = 1;

	tess.xyz[numVerts + 2][0] = cmd->x + cmd->w;
	tess.xyz[numVerts + 2][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 2][2] = 0;
	tess.xyz[numVerts + 2][3] = 1;

	tess.texCoords[numVerts + 2][0] = cmd->s2;
	tess.texCoords[numVerts + 2][1] = cmd->t2;
	tess.texCoords[numVerts + 2][2] = 0;
	tess.texCoords[numVerts + 2][3] = 1;

	tess.xyz[numVerts + 3][0] = cmd->x;
	tess.xyz[numVerts + 3][1] = cmd->y + cmd->h;
	tess.xyz[numVerts + 3][2] = 0;
	tess.xyz[numVerts + 3][3] = 1;

	tess.texCoords[numVerts + 3][0] = cmd->s1;
	tess.texCoords[numVerts + 3][1] = cmd->t2;
	tess.texCoords[numVerts + 3][2] = 0;
	tess.texCoords[numVerts + 3][3] = 1;

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawView
=============
*/
const void     *RB_DrawView(const void *data)
{
	const drawViewCommand_t *cmd;

	GLimp_LogComment("--- RB_DrawView ---\n");

	// finish any 2D drawing if needed
	if(tess.numIndexes)
	{
		Tess_End();
	}

	cmd = (const drawViewCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderView();

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

	GL_DrawBuffer(cmd->buffer);

	// clear screen for debugging
	if(r_clear->integer)
	{
//      GL_ClearColor(1, 0, 0.5, 1);
		GL_ClearColor(0, 0, 0, 1);
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
	vec4_t          quadVerts[4];
	int             start, end;

	GLimp_LogComment("--- RB_ShowImages ---\n");

	if(!backEnd.projection2D)
	{
		RB_SetGL2D();
	}

	qglClear(GL_COLOR_BUFFER_BIT);

	qglFinish();

	GL_BindProgram(&tr.genericSingleShader);
	GL_Cull(CT_TWO_SIDED);

	// set uniforms
	GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
	GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_VERTEX);
	GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_VERTEX);
	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, qfalse);
	}
	GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);
	GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, matrixIdentity);

	GL_SelectTexture(0);

	start = ri.Milliseconds();

	for(i = 0; i < tr.images.currentElements; i++)
	{
		image = Com_GrowListElement(&tr.images, i);

		/*
		   if(image->bits & (IF_RGBA16F | IF_RGBA32F | IF_LA16F | IF_LA32F))
		   {
		   // don't render float textures using FFP
		   continue;
		   }
		 */

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

		// bind u_ColorMap
		GL_Bind(image);

		VectorSet4(quadVerts[0], x, y, 0, 1);
		VectorSet4(quadVerts[1], x + w, y, 0, 1);
		VectorSet4(quadVerts[2], x + w, y + h, 0, 1);
		VectorSet4(quadVerts[3], x, y + h, 0, 1);

		Tess_InstantQuad(quadVerts);

		/*
		   qglBegin(GL_QUADS);
		   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, 0, 0, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x, y, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, 1, 0, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x + w, y, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, 1, 1, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x + w, y + h, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_TEXCOORD0, 0, 1, 0, 1);
		   qglVertexAttrib4fARB(ATTR_INDEX_POSITION, x, y + h, 0, 1);
		   qglEnd();
		 */
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf(PRINT_ALL, "%i msec to draw all images\n", end - start);

	GL_CheckErrors();
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
			case RC_DRAW_VIEW:
				data = RB_DrawView(data);
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
			case RC_VIDEOFRAME:
				data = RB_TakeVideoFrameCmd(data);
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
