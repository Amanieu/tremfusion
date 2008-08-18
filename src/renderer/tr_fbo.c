/*
===========================================================================
Copyright (C) 2006 Kirk Barnes
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
// tr_fbo.c
#include "tr_local.h"

/*
=============
R_CheckFBO
=============
*/
qboolean R_CheckFBO(const FBO_t * fbo)
{
	int             code;
	int             id;

	qglGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &id);
	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->frameBuffer);

	code = qglCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	if(code == GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, id);
		return qtrue;
	}

	// an error occured
	switch (code)
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Unsupported framebuffer format\n", fbo->name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Framebuffer incomplete, missing attachment\n", fbo->name);
			break;
			//case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT:
			//  ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Framebuffer incomplete, duplicate attachment\n", fbo->name);
			//  break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Framebuffer incomplete, attached images must have same dimensions\n",
					  fbo->name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Framebuffer incomplete, attached images must have same format\n",
					  fbo->name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Framebuffer incomplete, missing draw buffer\n", fbo->name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			ri.Printf(PRINT_WARNING, "R_CheckFBO: (%s) Framebuffer incomplete, missing read buffer\n", fbo->name);
			break;
		default:
			assert(0);
	}

	qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, id);

	return qfalse;
}

/*
============
R_CreateFBO
============
*/
FBO_t          *R_CreateFBO(const char *name, int width, int height)
{
	FBO_t          *fbo;

	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateFBO: \"%s\" is too long\n", name);
	}

	if(width <= 0 || width > glConfig.maxRenderbufferSize)
	{
		ri.Error(ERR_DROP, "R_CreateFBO: bad width %i", width);
	}

	if(height <= 0 || height > glConfig.maxRenderbufferSize)
	{
		ri.Error(ERR_DROP, "R_CreateFBO: bad height %i", height);
	}

	if(tr.numFBOs == MAX_FBOS)
	{
		ri.Error(ERR_DROP, "R_CreateFBO: MAX_FBOS hit");
	}

	fbo = tr.fbos[tr.numFBOs] = ri.Hunk_Alloc(sizeof(*fbo), h_low);
	Q_strncpyz(fbo->name, name, sizeof(fbo->name));
	fbo->index = tr.numFBOs++;
	fbo->width = width;
	fbo->height = height;

	qglGenFramebuffersEXT(1, &fbo->frameBuffer);

	return fbo;
}

/*
================
R_CreateFBOColorBuffer

Framebuffer must be bound
================
*/
void R_CreateFBOColorBuffer(FBO_t * fbo, int format, int index)
{
	qboolean        absent;

	if(index < 0 || index >= glConfig.maxColorAttachments)
	{
		ri.Printf(PRINT_WARNING, "R_CreateFBOColorBuffer: invalid attachment index %i\n", index);
		return;
	}

#if 0
	if(format != GL_RGB &&
	   format != GL_RGBA &&
	   format != GL_RGB16F_ARB && format != GL_RGBA16F_ARB && format != GL_RGB32F_ARB && format != GL_RGBA32F_ARB)
	{
		ri.Printf(PRINT_WARNING, "R_CreateFBOColorBuffer: format %i is not color-renderable\n", format);
		//return;
	}
#endif

	fbo->colorFormat = format;

	absent = fbo->colorBuffers[index] == 0;
	if(absent)
		qglGenRenderbuffersEXT(1, &fbo->colorBuffers[index]);

	qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo->colorBuffers[index]);
	qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, format, fbo->width, fbo->height);

	if(absent)
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, GL_RENDERBUFFER_EXT,
									  fbo->colorBuffers[index]);

	GL_CheckErrors();
}

/*
================
R_CreateFBODepthBuffer
================
*/
void R_CreateFBODepthBuffer(FBO_t * fbo, int format)
{
	qboolean        absent;

	if(format != GL_DEPTH_COMPONENT &&
	   format != GL_DEPTH_COMPONENT16_ARB && format != GL_DEPTH_COMPONENT24_ARB && format != GL_DEPTH_COMPONENT32_ARB)
	{
		ri.Printf(PRINT_WARNING, "R_CreateFBODepthBuffer: format %i is not depth-renderable\n", format);
		return;
	}

	fbo->depthFormat = format;

	absent = fbo->depthBuffer == 0;
	if(absent)
		qglGenRenderbuffersEXT(1, &fbo->depthBuffer);

	qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo->depthBuffer);
	qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, fbo->depthFormat, fbo->width, fbo->height);

	if(absent)
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->depthBuffer);

	GL_CheckErrors();
}

/*
================
R_CreateFBOStencilBuffer
================
*/
void R_CreateFBOStencilBuffer(FBO_t * fbo, int format)
{
	qboolean        absent;

	if(format != GL_STENCIL_INDEX &&
	   //format != GL_STENCIL_INDEX_EXT &&
	   format != GL_STENCIL_INDEX1_EXT &&
	   format != GL_STENCIL_INDEX4_EXT && format != GL_STENCIL_INDEX8_EXT && format != GL_STENCIL_INDEX16_EXT)
	{
		ri.Printf(PRINT_WARNING, "R_CreateFBOStencilBuffer: format %i is not stencil-renderable\n", format);
		return;
	}

	fbo->stencilFormat = format;

	absent = fbo->stencilBuffer == 0;
	if(absent)
		qglGenRenderbuffersEXT(1, &fbo->stencilBuffer);

	qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo->stencilBuffer);
	qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, fbo->stencilFormat, fbo->width, fbo->height);
	GL_CheckErrors();

	if(absent)
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->stencilBuffer);

	GL_CheckErrors();
}

/*
================
R_CreateFBOPackedDepthStencilBuffer
================
*/
void R_CreateFBOPackedDepthStencilBuffer(FBO_t * fbo, int format)
{
	qboolean        absent;

	// Tr3B: FIXME should be GL_DEPTH_STENCIL_EXT
	if(format != GL_DEPTH_STENCIL_NV)
	{
		ri.Printf(PRINT_WARNING, "R_CreateFBOPackedDepthStencilBuffer: format %i is not depth-stencil-renderable\n", format);
		return;
	}

	fbo->packedDepthStencilFormat = format;

	absent = fbo->packedDepthStencilBuffer == 0;
	if(absent)
		qglGenRenderbuffersEXT(1, &fbo->packedDepthStencilBuffer);

	qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo->packedDepthStencilBuffer);
	qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, fbo->packedDepthStencilFormat, fbo->width, fbo->height);
	GL_CheckErrors();

	if(absent)
	{
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->packedDepthStencilBuffer);
		qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->packedDepthStencilBuffer);
	}

	GL_CheckErrors();
}


/*
=================
R_AttachFBOTexture1D
=================
*/
void R_AttachFBOTexture1D(int texId, int index)
{
	if(index < 0 || index >= glConfig.maxColorAttachments)
	{
		ri.Printf(PRINT_WARNING, "R_AttachFBOTexture1D: invalid attachment index %i\n", index);
		return;
	}

	qglFramebufferTexture1DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, GL_TEXTURE_1D, texId, 0);
}

/*
=================
R_AttachFBOTexture2D
=================
*/
void R_AttachFBOTexture2D(int target, int texId, int index)
{
	if(target != GL_TEXTURE_2D && (target < GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB || target > GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB))
	{
		ri.Printf(PRINT_WARNING, "R_AttachFBOTexture2D: invalid target %i\n", target);
		return;
	}

	if(index < 0 || index >= glConfig.maxColorAttachments)
	{
		ri.Printf(PRINT_WARNING, "R_AttachFBOTexture2D: invalid attachment index %i\n", index);
		return;
	}

	qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, target, texId, 0);
}

/*
=================
R_AttachFBOTexture3D
=================
*/
void R_AttachFBOTexture3D(int texId, int index, int zOffset)
{
	if(index < 0 || index >= glConfig.maxColorAttachments)
	{
		ri.Printf(PRINT_WARNING, "R_AttachFBOTexture3D: invalid attachment index %i\n", index);
		return;
	}

	qglFramebufferTexture3DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, GL_TEXTURE_3D_EXT, texId, 0, zOffset);
}

/*
=================
R_AttachFBOTextureDepth
=================
*/
void R_AttachFBOTextureDepth(int texId)
{
	qglFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texId, 0);
}

/*
============
R_BindFBO
============
*/
void R_BindFBO(FBO_t * fbo)
{
	if(!fbo)
	{
		R_BindNullFBO();
		return;
	}

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va("--- R_BindFBO( %s ) ---\n", fbo->name));
	}

	if(glState.currentFBO != fbo)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->frameBuffer);

		/*
		   if(fbo->colorBuffers[0])
		   {
		   qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo->colorBuffers[0]);
		   }
		 */

		/*
		   if(fbo->depthBuffer)
		   {
		   qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, fbo->depthBuffer);
		   qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->depthBuffer);
		   }
		 */

		glState.currentFBO = fbo;
	}
}

/*
============
R_BindNullFBO
============
*/
void R_BindNullFBO(void)
{
	if(r_logFile->integer)
	{
		GLimp_LogComment("--- R_BindNullFBO ---\n");
	}

	if(glState.currentFBO)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		glState.currentFBO = NULL;
	}
}

/*
============
R_InitFBOs
============
*/
void R_InitFBOs(void)
{
	int             i;
	int             width, height;

	if(!glConfig.framebufferObjectAvailable)
		return;

	tr.numFBOs = 0;

	// make sure the render thread is stopped
	R_SyncRenderThread();

	if(r_deferredShading->integer && glConfig.maxColorAttachments >= 4 && glConfig.textureFloatAvailable &&
	   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		// geometricRender FBO as G-Buffer for deferred shading
		GLenum          drawbuffers[] =
		{
			GL_COLOR_ATTACHMENT0_EXT,
			GL_COLOR_ATTACHMENT1_EXT,
			GL_COLOR_ATTACHMENT2_EXT,
			GL_COLOR_ATTACHMENT3_EXT
				//GL_DEPTH_ATTACHMENT_EXT
		};

		if(glConfig.textureNPOTAvailable)
		{
			width = glConfig.vidWidth;
			height = glConfig.vidHeight;
		}
		else
		{
			width = NearestPowerOfTwo(glConfig.vidWidth);
			height = NearestPowerOfTwo(glConfig.vidHeight);
		}

#if 0
		if(glConfig.framebufferMixedFormatsAvailable)
		{
			// deferredRender FBO for the lighting pass
			tr.deferredRenderFBO = R_CreateFBO("_deferredRender", width, height);
			R_BindFBO(tr.deferredRenderFBO);

			R_CreateFBOColorBuffer(tr.deferredRenderFBO, GL_RGBA, 0);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredRenderFBOImage->texnum, 0);

			R_CreateFBODepthBuffer(tr.deferredRenderFBO, GL_DEPTH_COMPONENT24_ARB);

			R_CheckFBO(tr.deferredRenderFBO);



			tr.geometricRenderFBO = R_CreateFBO("_geometricRender", width, height);
			R_BindFBO(tr.geometricRenderFBO);

			// enable all attachments as draw buffers
			qglDrawBuffersARB(4, drawbuffers);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 0);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredDiffuseFBOImage->texnum, 0);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 1);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredNormalFBOImage->texnum, 1);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 2);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredSpecularFBOImage->texnum, 2);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, (r_deferredShading->integer == 2 ? GL_RGBA32F_ARB : GL_RGBA16F_ARB), 3);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredPositionFBOImage->texnum, 3);

			// share depth buffer
			tr.geometricRenderFBO->depthFormat = tr.deferredRenderFBO->depthFormat;
			tr.geometricRenderFBO->depthBuffer = tr.deferredRenderFBO->depthBuffer;

			//qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, tr.deferredRenderFBO->depthBuffer);
			//qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, tr.deferredRenderFBO->depthFormat, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height);

			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
										  tr.geometricRenderFBO->depthBuffer);
			//R_AttachFBOTextureDepth(tr.deferredPositionFBOImage->texnum);

			R_CheckFBO(tr.geometricRenderFBO);
		}
		else
#endif
		{
			// deferredRender FBO for the lighting pass
			tr.deferredRenderFBO = R_CreateFBO("_deferredRender", width, height);
			R_BindFBO(tr.deferredRenderFBO);

			R_CreateFBOColorBuffer(tr.deferredRenderFBO, GL_RGBA, 0);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredRenderFBOImage->texnum, 0);

			R_CreateFBODepthBuffer(tr.deferredRenderFBO, GL_DEPTH_COMPONENT24_ARB);
			//R_AttachFBOTextureDepth(tr.depthRenderImage->texnum);

			R_CheckFBO(tr.deferredRenderFBO);



			tr.geometricRenderFBO = R_CreateFBO("_geometricRender", width, height);
			R_BindFBO(tr.geometricRenderFBO);

			// enable all attachments as draw buffers
			qglDrawBuffersARB(3, drawbuffers);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 0);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredDiffuseFBOImage->texnum, 0);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 1);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredNormalFBOImage->texnum, 1);

			R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 2);
			R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredSpecularFBOImage->texnum, 2);

			//R_CreateFBOColorBuffer(tr.geometricRenderFBO, GL_RGBA, 3);
			//R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.deferredPositionFBOImage->texnum, 3);

			// share depth buffer
			tr.geometricRenderFBO->depthFormat = tr.deferredRenderFBO->depthFormat;
			tr.geometricRenderFBO->depthBuffer = tr.deferredRenderFBO->depthBuffer;

			//qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, tr.deferredRenderFBO->depthBuffer);
			//qglRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, tr.deferredRenderFBO->depthFormat, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height);

			qglFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
										  tr.geometricRenderFBO->depthBuffer);
			//R_AttachFBOTextureDepth(tr.depthRenderImage->texnum);

			R_CheckFBO(tr.geometricRenderFBO);
		}
	}

	if(r_shadows->integer >= 4 && glConfig.textureFloatAvailable)
	{
		// shadowMap FBOs for shadow mapping offscreen rendering
		for(i = 0; i < 5; i++)
		{
			width = height = shadowMapResolutions[i];

			tr.shadowMapFBO[i] = R_CreateFBO(va("_shadowMap%d", i), width, height);
			R_BindFBO(tr.shadowMapFBO[i]);

			if(glConfig.hardwareType == GLHW_ATI)
			{
				R_CreateFBOColorBuffer(tr.shadowMapFBO[i], GL_RGBA16, 0);
			}
			else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 4)
			{
				R_CreateFBOColorBuffer(tr.shadowMapFBO[i], GL_LUMINANCE_ALPHA16F_ARB, 0);
			}
			else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 5)
			{
				R_CreateFBOColorBuffer(tr.shadowMapFBO[i], GL_LUMINANCE_ALPHA32F_ARB, 0);
			}
			else if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 6)
			{
				R_CreateFBOColorBuffer(tr.shadowMapFBO[i], GL_ALPHA32F_ARB, 0);
			}
			else
			{
				R_CreateFBOColorBuffer(tr.shadowMapFBO[i], GL_RGBA16F_ARB, 0);
			}

			R_CreateFBODepthBuffer(tr.shadowMapFBO[i], GL_DEPTH_COMPONENT24_ARB);

			R_CheckFBO(tr.shadowMapFBO[i]);
		}
	}

	GL_CheckErrors();

	R_BindNullFBO();
}

/*
============
R_ShutdownFBOs
============
*/
void R_ShutdownFBOs(void)
{
	int             i, j;
	FBO_t          *fbo;

	if(!glConfig.framebufferObjectAvailable)
		return;

	R_BindNullFBO();

	for(i = 0; i < tr.numFBOs; i++)
	{
		fbo = tr.fbos[i];

		for(j = 0; j < glConfig.maxColorAttachments; j++)
		{
			if(fbo->colorBuffers[j])
				qglDeleteRenderbuffersEXT(1, &fbo->colorBuffers[j]);
		}

		if(fbo->depthBuffer)
			qglDeleteRenderbuffersEXT(1, &fbo->depthBuffer);

		if(fbo->stencilBuffer)
			qglDeleteRenderbuffersEXT(1, &fbo->stencilBuffer);

		if(fbo->frameBuffer)
			qglDeleteFramebuffersEXT(1, &fbo->frameBuffer);
	}
}

/*
============
R_FBOList_f
============
*/
void R_FBOList_f(void)
{
	int             i;
	FBO_t          *fbo;

	if(!glConfig.framebufferObjectAvailable)
	{
		ri.Printf(PRINT_ALL, "GL_EXT_framebuffer_object is not available.\n");
		return;
	}

	ri.Printf(PRINT_ALL, "             size       name\n");
	ri.Printf(PRINT_ALL, "----------------------------------------------------------\n");

	for(i = 0; i < tr.numFBOs; i++)
	{
		fbo = tr.fbos[i];

		ri.Printf(PRINT_ALL, "  %4i: %4i %4i %s\n", i, fbo->width, fbo->height, fbo->name);
	}

	ri.Printf(PRINT_ALL, " %i FBOs\n", tr.numFBOs);
}
