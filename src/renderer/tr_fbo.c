/*
===========================================================================
Copyright (C) 2006 Kirk Barnes
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
// tr_fbo.c
#include "tr_local.h"

/*
=============
R_CheckFBO
=============
*/
qboolean R_CheckFBO(const frameBuffer_t * fbo)
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
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Unsupported framebuffer format\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Framebuffer incomplete, missing attachment\n");
            break;
#if 0
        case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT:
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Framebuffer incomplete, duplicate attachment\n");
            break;
#endif
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Framebuffer incomplete, attached images must have same dimensions\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Framebuffer incomplete, attached images must have same format\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Framebuffer incomplete, missing draw buffer\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            ri.Printf(PRINT_WARNING, "R_CheckFBO: Framebuffer incomplete, missing read buffer\n");
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
frameBuffer_t  *R_CreateFBO(const char *name, int width, int height)
{
	frameBuffer_t  *fbo;
	
	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateFBO: \"%s\" is too long\n", name);
	}

	if(width <= 0 || width > glConfig2.maxRenderbufferSize)
	{
		ri.Error(ERR_DROP, "R_CreateFBO: bad width %i", width);
	}
		
	if(height <= 0 || height > glConfig2.maxRenderbufferSize)
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
void R_CreateFBOColorBuffer(frameBuffer_t * fbo, int format, int index)
{
	qboolean        absent;

	if(index < 0 || index >= glConfig2.maxColorAttachments)
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
void R_CreateFBODepthBuffer(frameBuffer_t * fbo, int format)
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
void R_CreateFBOStencilBuffer(frameBuffer_t * fbo, int format)
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
=================
R_AttachFBOTexture1D
=================
*/
void R_AttachFBOTexture1D(int texId, int index)
{
	if(index < 0 || index >= glConfig2.maxColorAttachments)
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

	if(index < 0 || index >= glConfig2.maxColorAttachments)
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
	if(index < 0 || index >= glConfig2.maxColorAttachments)
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
void R_BindFBO(frameBuffer_t * fbo)
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
	if(!glConfig2.framebufferObjectAvailable)
		return;

	tr.numFBOs = 0;
	
	/*
	tr.positionFBO = R_CreateFBO("_position", glConfig.vidWidth, glConfig.vidHeight);
	R_BindFBO(tr.positionFBO);
	R_CreateFBODepthBuffer(tr.positionFBO, GL_DEPTH_COMPONENT24_ARB);
//  R_CreateFBOStencilBuffer(tr.positionFBO, GL_STENCIL_INDEX8_EXT);
	R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.positionImage->texId, 0);
	R_CheckFBO(tr.positionFBO);
	*/

	/*
	tr.visibilityFBO = R_CreateFBO("_visibility", glConfig.vidWidth, glConfig.vidHeight);
	R_BindFBO(tr.visibilityFBO);
	R_CreateFBODepthBuffer(tr.visibilityFBO, GL_DEPTH_COMPONENT24_ARB);
//  R_CreateFBOStencilBuffer(tr.visibilityFBO, GL_STENCIL_INDEX8_EXT);
	R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.visibilityImage->texId, 0);
	R_CheckFBO(tr.visibilityFBO);
	*/

/*
	tr.mirrorFBO = R_CreateFBO("_mirror", 512, 512);
	R_BindFBO(tr.mirrorFBO);
	R_CreateFBODepthBuffer(tr.mirrorFBO, GL_DEPTH_COMPONENT24_ARB);
	R_CreateFBOStencilBuffer(tr.mirrorFBO, GL_STENCIL_INDEX);
	R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.mirrorRenderImage->texId, 0);
	R_CheckFBO(tr.mirrorFBO);
*/

//	tr.shadowMapFBO = R_CreateFBO("_shadowMap", MAX_SHADOWMAP_SIZE * 3, MAX_SHADOWMAP_SIZE * 2);
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
	frameBuffer_t  *fbo;

	if(!glConfig2.framebufferObjectAvailable)
		return;

	R_BindNullFBO();

	for(i = 0; i < tr.numFBOs; i++)
	{
		fbo = tr.fbos[i];

		for(j = 0; j < glConfig2.maxColorAttachments; j++)
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
	frameBuffer_t  *fbo;

	if(!glConfig2.framebufferObjectAvailable)
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
