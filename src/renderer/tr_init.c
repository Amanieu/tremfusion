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
// tr_init.c -- functions that are not called every frame
#include "tr_local.h"

glConfig_t      glConfig;
glstate_t       glState;
float           displayAspect = 0.0f;

static void     GfxInfo_f(void);

cvar_t         *r_flares;
cvar_t         *r_flareSize;
cvar_t         *r_flareFade;

cvar_t         *r_railWidth;
cvar_t         *r_railCoreWidth;
cvar_t         *r_railSegmentLength;

cvar_t         *r_verbose;
cvar_t         *r_ignore;

cvar_t         *r_displayRefresh;

cvar_t         *r_znear;
cvar_t         *r_zfar;

cvar_t         *r_smp;
cvar_t         *r_showSmp;
cvar_t         *r_skipBackEnd;

cvar_t         *r_ignorehwgamma;
cvar_t         *r_measureOverdraw;

cvar_t         *r_inGameVideo;
cvar_t         *r_fastsky;
cvar_t         *r_drawSun;

cvar_t         *r_lodbias;
cvar_t         *r_lodscale;

cvar_t         *r_norefresh;
cvar_t         *r_drawentities;
cvar_t         *r_drawworld;
cvar_t         *r_speeds;
cvar_t         *r_novis;
cvar_t         *r_nocull;
cvar_t         *r_facePlaneCull;
cvar_t         *r_showcluster;
cvar_t         *r_nocurves;
cvar_t         *r_nobatching;
cvar_t         *r_noLightScissors;
cvar_t         *r_noLightVisCull;
cvar_t         *r_noInteractionSort;
cvar_t         *r_noDynamicLighting;
cvar_t         *r_noStaticLighting;
cvar_t         *r_dynamicLightsCastShadows;
cvar_t         *r_precomputedLighting;
cvar_t         *r_vertexLighting;
cvar_t         *r_heatHazeFix;
cvar_t         *r_atiFlippedImageFix;
cvar_t         *r_noMarksOnTrisurfs;

cvar_t         *r_ext_texture_compression;
cvar_t         *r_ext_occlusion_query;
cvar_t         *r_ext_texture_non_power_of_two;
cvar_t         *r_ext_draw_buffers;
cvar_t         *r_ext_vertex_array_object;
cvar_t         *r_ext_half_float_pixel;
cvar_t         *r_ext_texture_float;
cvar_t         *r_ext_stencil_wrap;
cvar_t         *r_ext_texture_filter_anisotropic;
cvar_t         *r_ext_stencil_two_side;
cvar_t         *r_ext_separate_stencil;
cvar_t         *r_ext_depth_bounds_test;
cvar_t         *r_ext_framebuffer_object;
cvar_t         *r_ext_packed_depth_stencil;
cvar_t         *r_ext_framebuffer_blit;
cvar_t         *r_extx_framebuffer_mixed_formats;
cvar_t         *r_ext_generate_mipmap;

cvar_t         *r_ignoreGLErrors;
cvar_t         *r_logFile;

cvar_t         *r_stencilbits;
cvar_t         *r_depthbits;
cvar_t         *r_colorbits;
cvar_t         *r_stereo;

cvar_t         *r_drawBuffer;
cvar_t         *r_uiFullScreen;
cvar_t         *r_shadows;
cvar_t         *r_softShadows;
cvar_t         *r_shadowBlur;
cvar_t         *r_shadowMapQuality;
cvar_t         *r_shadowMapSizeUltra;
cvar_t         *r_shadowMapSizeVeryHigh;
cvar_t         *r_shadowMapSizeHigh;
cvar_t         *r_shadowMapSizeMedium;
cvar_t         *r_shadowMapSizeLow;
cvar_t         *r_shadowOffsetFactor;
cvar_t         *r_shadowOffsetUnits;
cvar_t         *r_shadowLodBias;
cvar_t         *r_shadowLodScale;
cvar_t         *r_noShadowPyramids;
cvar_t         *r_cullShadowPyramidFaces;
cvar_t         *r_cullShadowPyramidCurves;
cvar_t         *r_cullShadowPyramidTriangles;
cvar_t         *r_debugShadowMaps;
cvar_t         *r_noShadowFrustums;
cvar_t         *r_noLightFrustums;
cvar_t         *r_shadowMapLuminanceAlpha;
cvar_t         *r_shadowMapLinearFilter;
cvar_t         *r_lightBleedReduction;
cvar_t         *r_overDarkeningFactor;
cvar_t         *r_shadowMapDepthScale;

cvar_t         *r_mode;
cvar_t         *r_collapseStages;
cvar_t         *r_nobind;
cvar_t         *r_singleShader;
cvar_t         *r_roundImagesDown;
cvar_t         *r_colorMipLevels;
cvar_t         *r_picmip;
cvar_t         *r_finish;
cvar_t         *r_clear;
cvar_t         *r_swapInterval;
cvar_t         *r_textureMode;
cvar_t         *r_offsetFactor;
cvar_t         *r_offsetUnits;
cvar_t         *r_forceSpecular;
cvar_t         *r_specularExponent;
cvar_t         *r_specularScale;
cvar_t         *r_normalScale;
cvar_t         *r_normalMapping;
cvar_t         *r_gamma;
cvar_t         *r_intensity;
cvar_t         *r_lockpvs;
cvar_t         *r_noportals;
cvar_t         *r_portalOnly;

cvar_t         *r_subdivisions;
cvar_t         *r_stitchCurves;

cvar_t         *r_fullscreen;

cvar_t         *r_customwidth;
cvar_t         *r_customheight;
cvar_t         *r_customaspect;

cvar_t         *r_overBrightBits;
cvar_t         *r_mapOverBrightBits;

cvar_t         *r_debugSurface;
cvar_t         *r_simpleMipMaps;

cvar_t         *r_showImages;

cvar_t         *r_forceFog;
cvar_t         *r_noFog;

cvar_t         *r_forceAmbient;
cvar_t         *r_ambientScale;
cvar_t         *r_lightScale;
cvar_t         *r_debugLight;
cvar_t         *r_debugSort;
cvar_t         *r_printShaders;

cvar_t         *r_maxPolys;
cvar_t         *r_maxPolyVerts;

cvar_t         *r_showTris;
cvar_t         *r_showSky;
cvar_t         *r_showShadowVolumes;
cvar_t         *r_showShadowLod;
cvar_t         *r_showSkeleton;
cvar_t         *r_showEntityTransforms;
cvar_t         *r_showLightTransforms;
cvar_t         *r_showLightInteractions;
cvar_t         *r_showLightScissors;
cvar_t         *r_showLightBatches;
cvar_t         *r_showOcclusionQueries;
cvar_t         *r_showBatches;
cvar_t         *r_showLightMaps;
cvar_t         *r_showDeluxeMaps;
cvar_t         *r_showAreaPortals;
cvar_t         *r_showCubeProbes;

cvar_t         *r_showDeferredDiffuse;
cvar_t         *r_showDeferredNormal;
cvar_t         *r_showDeferredSpecular;
cvar_t         *r_showDeferredPosition;
cvar_t         *r_showDeferredRender;

cvar_t         *r_vboFaces;
cvar_t         *r_vboCurves;
cvar_t         *r_vboTriangles;
cvar_t         *r_vboShadows;
cvar_t         *r_vboLighting;
cvar_t         *r_vboDynamicLighting;
cvar_t         *r_vboModels;
cvar_t         *r_vboWorld;
cvar_t         *r_vboOptimizeVertices;
cvar_t         *r_vboVertexSkinning;
cvar_t         *r_vboSmoothNormals;

cvar_t         *r_precacheLightIndexes;
cvar_t         *r_precacheShadowIndexes;

cvar_t         *r_deferredShading;
cvar_t         *r_parallaxMapping;
cvar_t         *r_parallaxDepthScale;

cvar_t         *r_hdrRendering;
cvar_t         *r_hdrMinLuminance;
cvar_t         *r_hdrMaxLuminance;
cvar_t         *r_hdrKey;
cvar_t         *r_hdrContrastThreshold;
cvar_t         *r_hdrContrastOffset;
cvar_t         *r_hdrLightmap;
cvar_t         *r_hdrLightmapExposure;
cvar_t         *r_hdrLightmapGamma;
cvar_t         *r_hdrLightmapCompensate;
cvar_t         *r_hdrToneMappingOperator;
cvar_t         *r_hdrGamma;

cvar_t         *r_screenSpaceAmbientOcclusion;
cvar_t         *r_depthOfField;
cvar_t         *r_bloom;
cvar_t         *r_bloomBlur;
cvar_t         *r_bloomPasses;
cvar_t         *r_rotoscope;

// GL_ARB_multitexture
void            (APIENTRY * qglActiveTextureARB) (GLenum texture);

// GL_ARB_texture_compression
void            (APIENTRY * qglCompressedTexImage3DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
void            (APIENTRY * qglCompressedTexImage2DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
void            (APIENTRY * qglCompressedTexImage1DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
void            (APIENTRY * qglCompressedTexSubImage3DARB) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
void            (APIENTRY * qglCompressedTexSubImage2DARB) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
void            (APIENTRY * qglCompressedTexSubImage1DARB) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
void            (APIENTRY * qglGetCompressedTexImageARB) (GLenum target, GLint level, GLvoid *img);

// GL_ARB_vertex_program
void            (APIENTRY * qglVertexAttrib4fARB) (GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
void            (APIENTRY * qglVertexAttrib4fvARB) (GLuint, const GLfloat *);
void            (APIENTRY * qglVertexAttribPointerARB) (GLuint index, GLint size, GLenum type, GLboolean normalized,
														GLsizei stride, const GLvoid * pointer);
void            (APIENTRY * qglEnableVertexAttribArrayARB) (GLuint index);
void            (APIENTRY * qglDisableVertexAttribArrayARB) (GLuint index);

// GL_ARB_vertex_buffer_object
void            (APIENTRY * qglBindBufferARB) (GLenum target, GLuint buffer);
void            (APIENTRY * qglDeleteBuffersARB) (GLsizei n, const GLuint * buffers);
void            (APIENTRY * qglGenBuffersARB) (GLsizei n, GLuint * buffers);

GLboolean(APIENTRY * qglIsBufferARB) (GLuint buffer);
void            (APIENTRY * qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage);
void            (APIENTRY * qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data);
void            (APIENTRY * qglGetBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid * data);

// GL_ARB_occlusion_query
void            (APIENTRY * qglGenQueriesARB) (GLsizei n, GLuint * ids);
void            (APIENTRY * qglDeleteQueriesARB) (GLsizei n, const GLuint * ids);

GLboolean(APIENTRY * qglIsQueryARB) (GLuint id);
void            (APIENTRY * qglBeginQueryARB) (GLenum target, GLuint id);
void            (APIENTRY * qglEndQueryARB) (GLenum target);
void            (APIENTRY * qglGetQueryivARB) (GLenum target, GLenum pname, GLint * params);
void            (APIENTRY * qglGetQueryObjectivARB) (GLuint id, GLenum pname, GLint * params);
void            (APIENTRY * qglGetQueryObjectuivARB) (GLuint id, GLenum pname, GLuint * params);

GLboolean(APIENTRY * qglUnmapBufferARB) (GLenum target);
void            (APIENTRY * qglGetBufferParameterivARB) (GLenum target, GLenum pname, GLint * params);
void            (APIENTRY * qglGetBufferPointervARB) (GLenum target, GLenum pname, GLvoid * *params);

// GL_ARB_shader_objects
void            (APIENTRY * qglDeleteObjectARB) (GLhandleARB obj);

GLhandleARB(APIENTRY * qglGetHandleARB) (GLenum pname);
void            (APIENTRY * qglDetachObjectARB) (GLhandleARB containerObj, GLhandleARB attachedObj);

GLhandleARB(APIENTRY * qglCreateShaderObjectARB) (GLenum shaderType);
void            (APIENTRY * qglShaderSourceARB) (GLhandleARB shaderObj, GLsizei count, const GLcharARB * *string,
												 const GLint * length);
void            (APIENTRY * qglCompileShaderARB) (GLhandleARB shaderObj);

GLhandleARB(APIENTRY * qglCreateProgramObjectARB) (void);
void            (APIENTRY * qglAttachObjectARB) (GLhandleARB containerObj, GLhandleARB obj);
void            (APIENTRY * qglLinkProgramARB) (GLhandleARB programObj);
void            (APIENTRY * qglUseProgramObjectARB) (GLhandleARB programObj);
void            (APIENTRY * qglValidateProgramARB) (GLhandleARB programObj);
void            (APIENTRY * qglUniform1fARB) (GLint location, GLfloat v0);
void            (APIENTRY * qglUniform2fARB) (GLint location, GLfloat v0, GLfloat v1);
void            (APIENTRY * qglUniform3fARB) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void            (APIENTRY * qglUniform4fARB) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void            (APIENTRY * qglUniform1iARB) (GLint location, GLint v0);
void            (APIENTRY * qglUniform2iARB) (GLint location, GLint v0, GLint v1);
void            (APIENTRY * qglUniform3iARB) (GLint location, GLint v0, GLint v1, GLint v2);
void            (APIENTRY * qglUniform4iARB) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void            (APIENTRY * qglUniform2fvARB) (GLint location, GLsizei count, const GLfloat * value);
void            (APIENTRY * qglUniform3fvARB) (GLint location, GLsizei count, const GLfloat * value);
void            (APIENTRY * qglUniform4fvARB) (GLint location, GLsizei count, const GLfloat * value);
void            (APIENTRY * qglUniform2ivARB) (GLint location, GLsizei count, const GLint * value);
void            (APIENTRY * qglUniform3ivARB) (GLint location, GLsizei count, const GLint * value);
void            (APIENTRY * qglUniform4ivARB) (GLint location, GLsizei count, const GLint * value);
void            (APIENTRY * qglUniformMatrix2fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
void            (APIENTRY * qglUniformMatrix3fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
void            (APIENTRY * qglUniformMatrix4fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
void            (APIENTRY * qglGetObjectParameterfvARB) (GLhandleARB obj, GLenum pname, GLfloat * params);
void            (APIENTRY * qglGetObjectParameterivARB) (GLhandleARB obj, GLenum pname, GLint * params);
void            (APIENTRY * qglGetInfoLogARB) (GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog);
void            (APIENTRY * qglGetAttachedObjectsARB) (GLhandleARB containerObj, GLsizei maxCount, GLsizei * count,
													   GLhandleARB * obj);
GLint(APIENTRY * qglGetUniformLocationARB) (GLhandleARB programObj, const GLcharARB * name);
void            (APIENTRY * qglGetActiveUniformARB) (GLhandleARB programObj, GLuint index, GLsizei maxIndex, GLsizei * length,
													 GLint * size, GLenum * type, GLcharARB * name);
void            (APIENTRY * qglGetUniformfvARB) (GLhandleARB programObj, GLint location, GLfloat * params);
void            (APIENTRY * qglGetUniformivARB) (GLhandleARB programObj, GLint location, GLint * params);
void            (APIENTRY * qglGetShaderSourceARB) (GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * source);

// GL_ARB_vertex_shader
void            (APIENTRY * qglBindAttribLocationARB) (GLhandleARB programObj, GLuint index, const GLcharARB * name);
void            (APIENTRY * qglGetActiveAttribARB) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length,
													GLint * size, GLenum * type, GLcharARB * name);
GLint(APIENTRY * qglGetAttribLocationARB) (GLhandleARB programObj, const GLcharARB * name);

// GL_ARB_draw_buffers
void            (APIENTRY * qglDrawBuffersARB) (GLsizei n, const GLenum * bufs);

// GL_ARB_vertex_array_object
void			(APIENTRY * qglBindVertexArray) (GLuint array);
void			(APIENTRY * qglDeleteVertexArrays) (GLsizei n, const GLuint *arrays);
void			(APIENTRY * qglGenVertexArrays) (GLsizei n, GLuint *arrays);
GLboolean		(APIENTRY * qglIsVertexArray) (GLuint array);

// GL_EXT_texture3D
void			(APIENTRY * qglTexImage3DEXT) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void			(APIENTRY * qglTexSubImage3DEXT) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

// GL_EXT_stencil_two_side
void            (APIENTRY * qglActiveStencilFaceEXT) (GLenum face);

// GL_ATI_separate_stencil
void            (APIENTRY * qglStencilFuncSeparateATI) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
void            (APIENTRY * qglStencilOpSeparateATI) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);

// GL_EXT_depth_bounds_test
void            (APIENTRY * qglDepthBoundsEXT) (GLclampd zmin, GLclampd zmax);

// GL_EXT_framebuffer_object
GLboolean(APIENTRY * qglIsRenderbufferEXT) (GLuint renderbuffer);
void            (APIENTRY * qglBindRenderbufferEXT) (GLenum target, GLuint renderbuffer);
void            (APIENTRY * qglDeleteRenderbuffersEXT) (GLsizei n, const GLuint * renderbuffers);
void            (APIENTRY * qglGenRenderbuffersEXT) (GLsizei n, GLuint * renderbuffers);
void            (APIENTRY * qglRenderbufferStorageEXT) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void            (APIENTRY * qglGetRenderbufferParameterivEXT) (GLenum target, GLenum pname, GLint * params);

GLboolean(APIENTRY * qglIsFramebufferEXT) (GLuint framebuffer);
void            (APIENTRY * qglBindFramebufferEXT) (GLenum target, GLuint framebuffer);
void            (APIENTRY * qglDeleteFramebuffersEXT) (GLsizei n, const GLuint * framebuffers);
void            (APIENTRY * qglGenFramebuffersEXT) (GLsizei n, GLuint * framebuffers);

GLenum(APIENTRY * qglCheckFramebufferStatusEXT) (GLenum target);
void            (APIENTRY * qglFramebufferTexture1DEXT) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
														 GLint level);
void            (APIENTRY * qglFramebufferTexture2DEXT) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
														 GLint level);
void            (APIENTRY * qglFramebufferTexture3DEXT) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
														 GLint level, GLint zoffset);
void            (APIENTRY * qglFramebufferRenderbufferEXT) (GLenum target, GLenum attachment, GLenum renderbuffertarget,
															GLuint renderbuffer);
void            (APIENTRY * qglGetFramebufferAttachmentParameterivEXT) (GLenum target, GLenum attachment, GLenum pname,
																		GLint * params);
void            (APIENTRY * qglGenerateMipmapEXT) (GLenum target);

// GL_EXT_framebuffer_blit
void			(APIENTRY * qglBlitFramebufferEXT) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);



/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL(void)
{
	char            renderer_buffer[1024];

	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//      - r_fullscreen
	//      - r_glDriver
	//      - r_mode
	//      - r_(color|depth|stencil)bits
	//      - r_ignorehwgamma
	//      - r_gamma
	//

	if(glConfig.vidWidth == 0)
	{
		GLint           temp;

		GLimp_Init();

		strcpy(renderer_buffer, glConfig.renderer_string);
		Q_strlwr(renderer_buffer);

		// OpenGL driver constants
		qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &temp);
		glConfig.maxTextureSize = temp;

		// stubbed or broken drivers may have reported 0...
		if(glConfig.maxTextureSize <= 0)
		{
			glConfig.maxTextureSize = 0;
		}
	}

	// init command buffers and SMP
	R_InitCommandBuffers();

	// print info
	GfxInfo_f();

	// set default state
	GL_SetDefaultState();
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors_(const char *filename, int line)
{
	int             err;
	char            s[128];

	if(glConfig.smpActive)
	{
		// we can't print onto the console while rendering in another thread
		return;
	}

	if(r_ignoreGLErrors->integer)
	{
		return;
	}

	err = qglGetError();
	if(err == GL_NO_ERROR)
	{
		return;
	}

	switch (err)
	{
		case GL_INVALID_ENUM:
			strcpy(s, "GL_INVALID_ENUM");
			break;
		case GL_INVALID_VALUE:
			strcpy(s, "GL_INVALID_VALUE");
			break;
		case GL_INVALID_OPERATION:
			strcpy(s, "GL_INVALID_OPERATION");
			break;
		case GL_STACK_OVERFLOW:
			strcpy(s, "GL_STACK_OVERFLOW");
			break;
		case GL_STACK_UNDERFLOW:
			strcpy(s, "GL_STACK_UNDERFLOW");
			break;
		case GL_OUT_OF_MEMORY:
			strcpy(s, "GL_OUT_OF_MEMORY");
			break;
		case GL_TABLE_TOO_LARGE:
			strcpy(s, "GL_TABLE_TOO_LARGE");
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
			strcpy(s, "GL_INVALID_FRAMEBUFFER_OPERATION_EXT");
			break;
		default:
			Com_sprintf(s, sizeof(s), "0x%X", err);
			break;
	}

	ri.Error(ERR_FATAL, "caught OpenGL error: %s in file %s line %i", s, filename, line);
}


/*
** R_GetModeInfo
*/
typedef struct vidmode_s
{
	const char     *description;
	int             width, height;
	float           pixelAspect;	// pixel width / height
} vidmode_t;

vidmode_t       r_vidModes[] = {
	{"Mode  0: 320x240", 320, 240, 1},
	{"Mode  1: 400x300", 400, 300, 1},
	{"Mode  2: 512x384", 512, 384, 1},
	{"Mode  3: 640x480", 640, 480, 1},
	{"Mode  4: 800x600", 800, 600, 1},
	{"Mode  5: 960x720", 960, 720, 1},
	{"Mode  6: 1024x768", 1024, 768, 1},
	{"Mode  7: 1152x864", 1152, 864, 1},
	{"Mode  9: 1280x720 (16:9)", 1280, 720, 1},
	{"Mode 10: 1280x768 (16:10)", 1280, 768, 1},
	{"Mode 11: 1280x800 (16:10)", 1280, 800, 1},
	{"Mode 12: 1280x1024", 1280, 1024, 1},
	{"Mode 13: 1360x768 (16:9)", 1360, 768, 1},
	{"Mode 14: 1440x900 (16:10)", 1440, 900, 1},
	{"Mode 15: 1680x1050 (16:10)", 1680, 1050, 1},
	{"Mode 16: 1600x1200", 1600, 1200, 1},
	{"Mode 17: 1920x1080 (16:9)", 1920, 1080, 1},
	{"Mode 18: 1920x1200 (16:10)", 1920, 1200, 1},
	{"Mode 19: 2048x1536", 2048, 1536, 1},
	{"Mode 20: 2560x1600 (16:10)", 2560, 1600, 1},
};
static int      s_numVidModes = (sizeof(r_vidModes) / sizeof(r_vidModes[0]));

qboolean R_GetModeInfo(int *width, int *height, float *windowAspect, int mode)
{
	vidmode_t      *vm;

	if(mode < -1)
	{
		return qfalse;
	}
	if(mode >= s_numVidModes)
	{
		return qfalse;
	}

	if(mode == -1)
	{
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		*windowAspect = r_customaspect->value;
		return qtrue;
	}

	vm = &r_vidModes[mode];

	*width = vm->width;
	*height = vm->height;
	*windowAspect = (float)vm->width / (vm->height * vm->pixelAspect);

	return qtrue;
}

/*
** R_ModeList_f
*/
static void R_ModeList_f(void)
{
	int             i;

	ri.Printf(PRINT_ALL, "\n");
	for(i = 0; i < s_numVidModes; i++)
	{
		ri.Printf(PRINT_ALL, "%s\n", r_vidModes[i].description);
	}
	ri.Printf(PRINT_ALL, "\n");
}



/*
==============================================================================

						SCREEN SHOTS

screenshots get written in fs_homepath + fs_gamedir
.. base/screenshots/*.*

three commands: "screenshot", "screenshotJPEG" and "screenshotPNG"

the format is xreal-YYYY_MM_DD-HH_MM_SS-MS.tga/jpeg/png

==============================================================================
*/

/*
==================
RB_TakeScreenshot
==================
*/
static void RB_TakeScreenshot(int x, int y, int width, int height, char *fileName)
{
	byte           *buffer;
	int             i, c, temp;

	buffer = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight * 3 + 18);

	Com_Memset(buffer, 0, 18);
	buffer[2] = 2;				// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;			// pixel size

	qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18);

	// swap rgb to bgr
	c = 18 + width * height * 3;
	for(i = 18; i < c; i += 3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}

	// gamma correct
	if((tr.overbrightBits > 0) && glConfig.deviceSupportsGamma)
	{
		R_GammaCorrect(buffer + 18, glConfig.vidWidth * glConfig.vidHeight * 3);
	}

	ri.FS_WriteFile(fileName, buffer, c);

	ri.Hunk_FreeTempMemory(buffer);
}

/*
==================
RB_TakeScreenshotJPEG
==================
*/
static void RB_TakeScreenshotJPEG(int x, int y, int width, int height, char *fileName)
{
	byte           *buffer;

	buffer = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight * 4);

	qglReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

	// gamma correct
	if((tr.overbrightBits > 0) && glConfig.deviceSupportsGamma)
	{
		R_GammaCorrect(buffer, glConfig.vidWidth * glConfig.vidHeight * 4);
	}

	ri.FS_WriteFile(fileName, buffer, 1);	// create path
	SaveJPG(fileName, 90, glConfig.vidWidth, glConfig.vidHeight, buffer);

	ri.Hunk_FreeTempMemory(buffer);
}

/*
==================
RB_TakeScreenshotPNG
==================
*/
/*static void RB_TakeScreenshotPNG(int x, int y, int width, int height, char *fileName)
{
	byte           *buffer;

	buffer = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight * 3);

	qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	// gamma correct
	if((tr.overbrightBits > 0) && glConfig.deviceSupportsGamma)
	{
		R_GammaCorrect(buffer, glConfig.vidWidth * glConfig.vidHeight * 3);
	}

	ri.FS_WriteFile(fileName, buffer, 1);	// create path
	SavePNG(fileName, buffer, glConfig.vidWidth, glConfig.vidHeight, 3, qfalse);

	ri.Hunk_FreeTempMemory(buffer);
}*/

/*
==================
RB_TakeScreenshotCmd
==================
*/
const void     *RB_TakeScreenshotCmd(const void *data)
{
	const screenshotCommand_t *cmd;

	cmd = (const screenshotCommand_t *)data;

	switch (cmd->format)
	{
		case SSF_TGA:
			RB_TakeScreenshot(cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
			break;

		case SSF_JPEG:
			RB_TakeScreenshotJPEG(cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
			break;

		/*case SSF_PNG:
			RB_TakeScreenshotPNG(cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
			break;*/
	}

	return (const void *)(cmd + 1);
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot(char *name, ssFormat_t format)
{
	static char     fileName[MAX_OSPATH];	// bad things may happen if two screenshots per frame are taken.
	screenshotCommand_t *cmd;
	int             lastNumber;

	cmd = R_GetCommandBuffer(sizeof(*cmd));
	if(!cmd)
	{
		return;
	}

	if(ri.Cmd_Argc() == 2)
	{
		Com_sprintf(fileName, sizeof(fileName), "screenshots/xreal-%s.%s", ri.Cmd_Argv(1), name);
	}
	else
	{
		qtime_t         t;

		Com_RealTime(&t);

		// scan for a free filename
		for(lastNumber = 0; lastNumber <= 999; lastNumber++)
		{
			Com_sprintf(fileName, sizeof(fileName), "screenshots/xreal-%04d%02d%02d-%02d%02d%02d-%03d.%s",
						1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, lastNumber, name);

			if(!ri.FS_FileExists(fileName))
			{
				break;			// file doesn't exist
			}
		}

		if(lastNumber == 1000)
		{
			ri.Printf(PRINT_ALL, "ScreenShot: Couldn't create a file\n");
			return;
		}

		lastNumber++;
	}
	ri.Printf(PRINT_ALL, "Wrote %s\n", fileName);

	cmd->commandId = RC_SCREENSHOT;
	cmd->x = 0;
	cmd->y = 0;
	cmd->width = glConfig.vidWidth;
	cmd->height = glConfig.vidHeight;
	cmd->fileName = fileName;
	cmd->format = format;
}

/*
==================
R_ScreenShot_f

screenshot
screenshot [filename]
==================
*/
static void R_ScreenShot_f(void)
{
	R_TakeScreenshot("tga", SSF_TGA);
}

static void R_ScreenShotJPEG_f(void)
{
	R_TakeScreenshot("jpg", SSF_JPEG);
}

/*static void R_ScreenShotPNG_f(void)
{
	R_TakeScreenshot("png", SSF_PNG);
}*/

/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
static void R_LevelShot(void)
{
	char            checkname[MAX_OSPATH];
	byte           *buffer;
	byte           *source;
	byte           *src, *dst;
	int             x, y;
	int             r, g, b;
	float           xScale, yScale;
	int             xx, yy;

	sprintf(checkname, "levelshots/%s.tga", tr.world->baseName);

	source = ri.Hunk_AllocateTempMemory(glConfig.vidWidth * glConfig.vidHeight * 3);

	buffer = ri.Hunk_AllocateTempMemory(128 * 128 * 3 + 18);
	Com_Memset(buffer, 0, 18);
	buffer[2] = 2;				// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;			// pixel size

	qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_RGB, GL_UNSIGNED_BYTE, source);

	// resample from source
	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for(y = 0; y < 128; y++)
	{
		for(x = 0; x < 128; x++)
		{
			r = g = b = 0;
			for(yy = 0; yy < 3; yy++)
			{
				for(xx = 0; xx < 4; xx++)
				{
					src = source + 3 * (glConfig.vidWidth * (int)((y * 3 + yy) * yScale) + (int)((x * 4 + xx) * xScale));
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * (y * 128 + x);
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correct
	if((tr.overbrightBits > 0) && glConfig.deviceSupportsGamma)
	{
		R_GammaCorrect(buffer + 18, 128 * 128 * 3);
	}

	ri.FS_WriteFile(checkname, buffer, 128 * 128 * 3 + 18);

	ri.Hunk_FreeTempMemory(buffer);
	ri.Hunk_FreeTempMemory(source);

	ri.Printf(PRINT_ALL, "Wrote %s\n", checkname);
}

static int QDECL MaterialNameCompare(const void *a, const void *b)
{
	char           *s1, *s2;
	int             c1, c2;

	s1 = *(char **)a;
	s2 = *(char **)b;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if(c1 >= 'a' && c1 <= 'z')
		{
			c1 -= ('a' - 'A');
		}
		if(c2 >= 'a' && c2 <= 'z')
		{
			c2 -= ('a' - 'A');
		}

		if(c1 == '\\' || c1 == ':')
		{
			c1 = '/';
		}
		if(c2 == '\\' || c2 == ':')
		{
			c2 = '/';
		}

		if(c1 < c2)
		{
			// strings not equal
			return -1;
		}
		if(c1 > c2)
		{
			return 1;
		}
	} while(c1);

	// strings are equal
	return 0;
}

static void R_GenerateMaterialFile_f(void)
{
	char          **dirnames;
	int             ndirs;
	int             i;
	fileHandle_t    f;
	char            fileName[MAX_QPATH];
	char            fileName2[MAX_QPATH];
	char            cleanName[MAX_QPATH];
	char            cleanName2[MAX_QPATH];
	char            baseName[MAX_QPATH];
	char            baseName2[MAX_QPATH];
	char            path[MAX_QPATH];
	char            extension[MAX_QPATH];
	int				len;

	if(Cmd_Argc() < 3)
	{
		Com_Printf("usage: generatemtr <directory> [image extension]\n");
		return;
	}

	Q_strncpyz(path, Cmd_Argv(1), sizeof(path));

	Q_strncpyz(extension, Cmd_Argv(2), sizeof(extension));
	Q_strreplace(extension, sizeof(extension), ".", "");

	Q_strncpyz(fileName, Cmd_Argv(1), sizeof(fileName));
	Com_DefaultExtension(fileName, sizeof(fileName), ".mtr");
	Com_Printf("Writing %s.\n", fileName);
	f = FS_FOpenFileWrite(fileName);
	if(!f)
	{
		Com_Printf("Couldn't write %s.\n", fileName);
		return;
	}

	FS_Printf(f, "// generated by XreaL\n\n");

	dirnames = FS_ListFiles(path, extension, &ndirs);

	qsort(dirnames, ndirs, sizeof(char *), MaterialNameCompare);

	for(i = 0; i < ndirs; i++)
	{
		// clean name
		Q_strncpyz(fileName, dirnames[i], sizeof(fileName));
		Q_strncpyz(cleanName, dirnames[i], sizeof(cleanName));

		Q_strreplace(cleanName, sizeof(cleanName), "MaPZone[", "");
		Q_strreplace(cleanName, sizeof(cleanName), "]", "");
		Q_strreplace(cleanName, sizeof(cleanName), "&", "_");

		if(strcmp(fileName, cleanName))
		{
			Com_sprintf(fileName2, sizeof(fileName2), "%s/%s", path, fileName);
			Com_sprintf(cleanName2, sizeof(cleanName2), "%s/%s", path, cleanName);

			Com_Printf("renaming '%s' into '%s'\n", fileName2, cleanName2);
			FS_Rename(fileName2, cleanName2);
		}

		Com_StripExtension(cleanName, cleanName, sizeof(cleanName));
		if(!Q_stristr(cleanName, "_nm") && !Q_stristr(cleanName, "blend"))
		{
			Q_strncpyz(baseName, cleanName, sizeof(baseName));
			Q_strreplace(baseName, sizeof(baseName), "_diffuse", "");

			FS_Printf(f, "%s/%s\n", path, baseName);
			FS_Printf(f, "{\n");
			FS_Printf(f, "\t qer_editorImage\t %s/%s.%s\n", path, cleanName, extension);
			FS_Printf(f, "\n");
			FS_Printf(f, "\t parallax\n");
			FS_Printf(f, "\n");
			FS_Printf(f, "\t diffuseMap\t\t %s/%s.%s\n", path, baseName, extension);

			Com_sprintf(fileName, sizeof(fileName), "%s/%s_nm.%s", path, baseName, extension);
			if(ri.FS_ReadFile(fileName, NULL) > 0)
			{
				FS_Printf(f, "\t normalMap\t\t %s/%s_nm.%s\n", path, baseName, extension);
			}

			Q_strncpyz(baseName2, baseName, sizeof(baseName2));
			len = strlen(baseName);
			baseName2[len -1] = '\0';

			Com_sprintf(fileName, sizeof(fileName), "%s/speculars/%s_s.%s", path, baseName2, extension);
			if(ri.FS_ReadFile(fileName, NULL) > 0)
			{
				FS_Printf(f, "\t specularMap\t %s/speculars/%s_s.%s\n", path, baseName2, extension);
			}

			Com_sprintf(fileName, sizeof(fileName), "%s/%s.blend.%s", path, baseName, extension);
			if(ri.FS_ReadFile(fileName, NULL) > 0)
			{
				FS_Printf(f, "\t glowMap\t\t %s/%s.blend.%s\n", path, baseName, extension);
			}

			FS_Printf(f, "}\n\n");
		}
	}
	FS_FreeFileList(dirnames);

	FS_FCloseFile(f);
}

//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void     *RB_TakeVideoFrameCmd(const void *data)
{
	const videoFrameCommand_t *cmd;
	int             frameSize;
	int             i;

	cmd = (const videoFrameCommand_t *)data;

	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGBA, GL_UNSIGNED_BYTE, cmd->captureBuffer);

	// gamma correct
	if((tr.overbrightBits > 0) && glConfig.deviceSupportsGamma)
		R_GammaCorrect(cmd->captureBuffer, cmd->width * cmd->height * 4);

	if(cmd->motionJpeg)
	{
		frameSize = SaveJPGToBuffer(cmd->encodeBuffer, 90, cmd->width, cmd->height, cmd->captureBuffer);
		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, frameSize);
	}
	else
	{
		frameSize = cmd->width * cmd->height;

		for(i = 0; i < frameSize; i++)	// Pack to 24bpp and swap R and B
		{
			cmd->encodeBuffer[i * 3] = cmd->captureBuffer[i * 4 + 2];
			cmd->encodeBuffer[i * 3 + 1] = cmd->captureBuffer[i * 4 + 1];
			cmd->encodeBuffer[i * 3 + 2] = cmd->captureBuffer[i * 4];
		}

		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, frameSize * 3);
	}

	return (const void *)(cmd + 1);
}

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState(void)
{
	int             i;

	GLimp_LogComment("--- GL_SetDefaultState ---\n");

	GL_ClearDepth(1.0f);

	if(glConfig.stencilBits >= 4)
	{
		GL_ClearStencil(128);
	}

	GL_FrontFace(GL_CCW);
	GL_CullFace(GL_FRONT);

	qglVertexAttrib4fARB(ATTR_INDEX_COLOR, 1, 1, 1, 1);

	// initialize downstream texture units if we're running
	// in a multitexture environment
	if(qglActiveTextureARB)
	{
		for(i = glConfig.maxTextureUnits - 1; i >= 0; i--)
		{
			GL_SelectTexture(i);
			GL_TextureMode(r_textureMode->string);

			if(i != 0)
				qglDisable(GL_TEXTURE_2D);
			else
				qglEnable(GL_TEXTURE_2D);
		}
	}

	GL_DepthFunc(GL_LEQUAL);

	// make sure our GL state vector is set correctly
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
	glState.vertexAttribsState = 0;
	glState.vertexAttribPointersSet = 0;

	glState.currentProgram = 0;
	qglUseProgramObjectARB(0);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	glState.currentVBO = NULL;
	glState.currentIBO = NULL;

	// the vertex array is always enabled, but the color and texture
	// arrays are enabled and disabled around the compiled vertex array call
	qglEnableVertexAttribArrayARB(ATTR_INDEX_POSITION);

	/*
	   OpenGL 3.0 spec: E.1. PROFILES AND DEPRECATED FEATURES OF OPENGL 3.0 405
	   Calling VertexAttribPointer when no buffer object or no
	   vertex array object is bound will generate an INVALID OPERATION error,
	   as will calling any array drawing command when no vertex array object is
	   bound.
	 */

	if(glConfig.framebufferObjectAvailable)
	{
		qglBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		qglBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		glState.currentFBO = NULL;
	}


	/*
	   if(glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	   {
	   // enable all attachments as draw buffers
	   GLenum drawbuffers[] = {GL_DRAW_BUFFER0_ARB,
	   GL_DRAW_BUFFER1_ARB,
	   GL_DRAW_BUFFER2_ARB,
	   GL_DRAW_BUFFER3_ARB};

	   qglDrawBuffersARB(4, drawbuffers);
	   }
	 */

	GL_PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	qglDepthMask(GL_TRUE);
	qglDisable(GL_DEPTH_TEST);
	qglEnable(GL_SCISSOR_TEST);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_BLEND);

	glState.stackIndex = 0;
	for(i = 0; i < MAX_GLSTACK; i++)
	{
		MatrixIdentity(glState.modelViewMatrix[i]);
		MatrixIdentity(glState.projectionMatrix[i]);
		MatrixIdentity(glState.modelViewProjectionMatrix[i]);
	}
}



/*
================
GfxInfo_f
================
*/
void GfxInfo_f(void)
{
	const char     *enablestrings[] = {
		"disabled",
		"enabled"
	};
	const char     *fsstrings[] = {
		"windowed",
		"fullscreen"
	};

	ri.Printf(PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string);
	ri.Printf(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	ri.Printf(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);
	ri.Printf(PRINT_ALL, "GL_EXTENSIONS: %s\n", glConfig.extensions_string);
	ri.Printf(PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize);
	ri.Printf(PRINT_ALL, "GL_MAX_TEXTURE_UNITS_ARB: %d\n", glConfig.maxTextureUnits);

	/*
	   if(glConfig.fragmentProgramAvailable)
	   {
	   ri.Printf(PRINT_ALL, "GL_MAX_TEXTURE_IMAGE_UNITS_ARB: %d\n", glConfig.maxTextureImageUnits);
	   }
	 */

	ri.Printf(PRINT_ALL, "GL_SHADING_LANGUAGE_VERSION_ARB: %s\n", glConfig.shadingLanguageVersion);

	ri.Printf(PRINT_ALL, "GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB %d\n", glConfig.maxVertexUniforms);
	ri.Printf(PRINT_ALL, "GL_MAX_VARYING_FLOATS_ARB %d\n", glConfig.maxVaryingFloats);
	ri.Printf(PRINT_ALL, "GL_MAX_VERTEX_ATTRIBS_ARB %d\n", glConfig.maxVertexAttribs);

	if(glConfig.occlusionQueryAvailable)
	{
		ri.Printf(PRINT_ALL, "%d occlusion query bits\n", glConfig.occlusionQueryBits);
	}

	if(glConfig.drawBuffersAvailable)
	{
		ri.Printf(PRINT_ALL, "GL_MAX_DRAW_BUFFERS_ARB: %d\n", glConfig.maxDrawBuffers);
	}

	if(glConfig.textureAnisotropyAvailable)
	{
		ri.Printf(PRINT_ALL, "GL_TEXTURE_MAX_ANISOTROPY_EXT: %f\n", glConfig.maxTextureAnisotropy);
	}

	if(glConfig.framebufferObjectAvailable)
	{
		ri.Printf(PRINT_ALL, "GL_MAX_RENDERBUFFER_SIZE_EXT: %d\n", glConfig.maxRenderbufferSize);
		ri.Printf(PRINT_ALL, "GL_MAX_COLOR_ATTACHMENTS_EXT: %d\n", glConfig.maxColorAttachments);
	}

	ri.Printf(PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits,
			  glConfig.depthBits, glConfig.stencilBits);
	ri.Printf(PRINT_ALL, "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight,
			  fsstrings[r_fullscreen->integer == 1]);

	if(glConfig.displayFrequency)
	{
		ri.Printf(PRINT_ALL, "%d\n", glConfig.displayFrequency);
	}
	else
	{
		ri.Printf(PRINT_ALL, "N/A\n");
	}

	if(glConfig.deviceSupportsGamma)
	{
		ri.Printf(PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits);
	}
	else
	{
		ri.Printf(PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits);
	}

	ri.Printf(PRINT_ALL, "texturemode: %s\n", r_textureMode->string);
	ri.Printf(PRINT_ALL, "picmip: %d\n", r_picmip->integer);

	if(glConfig.driverType == GLDRV_OPENGL3)
	{
		ri.Printf(PRINT_ALL, "Using OpenGL 3.1 context\n");
	}

	if(glConfig.hardwareType == GLHW_ATI)
	{
		ri.Printf(PRINT_ALL, "HACK: ATI approximations\n");
	}

	if(glConfig.textureCompression != TC_NONE)
	{
		ri.Printf(PRINT_ALL, "Using S3TC (DXTC) texture compression\n");
	}

	if(glConfig.hardwareType == GLHW_ATI_DX10)
	{
		ri.Printf(PRINT_ALL, "Using ATI DirectX 10 hardware features\n");
	}

	if(glConfig.hardwareType == GLHW_NV_DX10)
	{
		ri.Printf(PRINT_ALL, "Using NVIDIA DirectX 10 hardware features\n");
	}

	if(glConfig.vboVertexSkinningAvailable)
	{
		ri.Printf(PRINT_ALL, "Using GPU vertex skinning with max %i bones in a single pass\n", glConfig.maxVertexSkinningBones);
	}

	if(glConfig.smpActive)
	{
		ri.Printf(PRINT_ALL, "Using dual processor acceleration\n");
	}

	if(r_finish->integer)
	{
		ri.Printf(PRINT_ALL, "Forcing glFinish\n");
	}
}

/*
===============
R_Register
===============
*/
void R_Register(void)
{
	// latched and archived variables
	r_ext_texture_compression = ri.Cvar_Get("r_ext_texture_compression", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_occlusion_query = ri.Cvar_Get("r_ext_occlusion_query", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_texture_non_power_of_two = ri.Cvar_Get("r_ext_texture_non_power_of_two", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_draw_buffers = ri.Cvar_Get("r_ext_draw_buffers", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_vertex_array_object = ri.Cvar_Get("r_ext_vertex_array_object", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_half_float_pixel = ri.Cvar_Get("r_ext_half_float_pixel", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_texture_float = ri.Cvar_Get("r_ext_texture_float", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_stencil_wrap = ri.Cvar_Get("r_ext_stencil_wrap", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_texture_filter_anisotropic = ri.Cvar_Get("r_ext_texture_filter_anisotropic", "4", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_stencil_two_side = ri.Cvar_Get("r_ext_stencil_two_side", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_separate_stencil = ri.Cvar_Get("r_ext_separate_stencil", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_depth_bounds_test = ri.Cvar_Get("r_ext_depth_bounds_test", "1", CVAR_CHEAT | CVAR_LATCH);
	r_ext_framebuffer_object = ri.Cvar_Get("r_ext_framebuffer_object", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_packed_depth_stencil = ri.Cvar_Get("r_ext_packed_depth_stencil", "0", CVAR_CHEAT | CVAR_LATCH);
	r_ext_framebuffer_blit = ri.Cvar_Get("r_ext_framebuffer_blit", "1", CVAR_CHEAT | CVAR_LATCH);
	r_extx_framebuffer_mixed_formats = ri.Cvar_Get("r_extx_framebuffer_mixed_formats", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_generate_mipmap = ri.Cvar_Get("r_ext_generate_mipmap", "1", CVAR_CHEAT | CVAR_LATCH);

	r_collapseStages = ri.Cvar_Get("r_collapseStages", "1", CVAR_LATCH | CVAR_CHEAT);
	r_picmip = ri.Cvar_Get("r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_picmip, 0, 3, qtrue);
	r_roundImagesDown = ri.Cvar_Get("r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_colorMipLevels = ri.Cvar_Get("r_colorMipLevels", "0", CVAR_LATCH);
	r_colorbits = ri.Cvar_Get("r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_stereo = ri.Cvar_Get("r_stereo", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_stencilbits = ri.Cvar_Get("r_stencilbits", "8", CVAR_ARCHIVE | CVAR_LATCH);
	r_depthbits = ri.Cvar_Get("r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_ignorehwgamma = ri.Cvar_Get("r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_mode = ri.Cvar_Get("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
	r_fullscreen = ri.Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_customwidth = ri.Cvar_Get("r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH);
	r_customheight = ri.Cvar_Get("r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH);
	r_customaspect = ri.Cvar_Get("r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_simpleMipMaps = ri.Cvar_Get("r_simpleMipMaps", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_uiFullScreen = ri.Cvar_Get("r_uifullscreen", "0", 0);
	r_subdivisions = ri.Cvar_Get("r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH);
	r_deferredShading = ri.Cvar_Get("r_deferredShading", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_parallaxMapping = ri.Cvar_Get("r_parallaxMapping", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_dynamicLightsCastShadows = ri.Cvar_Get("r_dynamicLightsCastShadows", "1", CVAR_ARCHIVE);
	r_precomputedLighting = ri.Cvar_Get("r_precomputedLighting", "1", CVAR_CHEAT | CVAR_LATCH);
	r_vertexLighting = ri.Cvar_Get("r_vertexLighting", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_heatHazeFix = ri.Cvar_Get("r_heatHazeFix", "0", CVAR_CHEAT | CVAR_LATCH);
	r_atiFlippedImageFix = ri.Cvar_Get("r_atiFlippedImageFix", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_noMarksOnTrisurfs = ri.Cvar_Get("r_noMarksOnTrisurfs", "1", CVAR_CHEAT);

	r_forceFog = ri.Cvar_Get("r_forceFog", "0", CVAR_ARCHIVE /* | CVAR_LATCH */ );
	ri.Cvar_CheckRange(r_forceFog, 0.0f, 1.0f, qfalse);
	r_noFog = ri.Cvar_Get("r_noFog", "0", CVAR_ARCHIVE);

	r_screenSpaceAmbientOcclusion = ri.Cvar_Get("r_screenSpaceAmbientOcclusion", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_screenSpaceAmbientOcclusion, 0, 2, qtrue);

	r_depthOfField = ri.Cvar_Get("r_depthOfField", "0", CVAR_ARCHIVE | CVAR_LATCH);

	r_forceAmbient = ri.Cvar_Get("r_forceAmbient", "0.125", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_forceAmbient, 0.0f, 0.3f, qfalse);

	r_smp = ri.Cvar_Get("r_smp", "0", CVAR_ARCHIVE | CVAR_LATCH);

	// temporary latched variables that can only change over a restart
	r_displayRefresh = ri.Cvar_Get("r_displayRefresh", "0", CVAR_LATCH);
	ri.Cvar_CheckRange(r_displayRefresh, 0, 200, qtrue);
#if defined(COMPAT_Q3A)
	r_overBrightBits = ri.Cvar_Get("r_overBrightBits", "1", CVAR_CHEAT | CVAR_LATCH);
	r_mapOverBrightBits = ri.Cvar_Get("r_mapOverBrightBits", "2", CVAR_CHEAT | CVAR_LATCH);
#else
	r_overBrightBits = ri.Cvar_Get("r_overBrightBits", "0", CVAR_CHEAT | CVAR_LATCH);
	r_mapOverBrightBits = ri.Cvar_Get("r_mapOverBrightBits", "0", CVAR_CHEAT | CVAR_LATCH);
#endif
	r_intensity = ri.Cvar_Get("r_intensity", "1", CVAR_LATCH);
	r_singleShader = ri.Cvar_Get("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH);
	r_precacheLightIndexes = ri.Cvar_Get("r_precacheLightIndexes", "1", CVAR_CHEAT | CVAR_LATCH);
	r_precacheShadowIndexes = ri.Cvar_Get("r_precacheShadowIndexes", "1", CVAR_CHEAT | CVAR_LATCH);
	r_stitchCurves = ri.Cvar_Get("r_stitchCurves", "1", CVAR_CHEAT | CVAR_LATCH);
	r_debugShadowMaps = ri.Cvar_Get("r_debugShadowMaps", "0", CVAR_CHEAT | CVAR_LATCH);
	r_shadowMapLuminanceAlpha = ri.Cvar_Get("r_shadowMapLuminanceAlpha", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_shadowMapLinearFilter = ri.Cvar_Get("r_shadowMapLinearFilter", "1", CVAR_CHEAT | CVAR_LATCH);
	r_lightBleedReduction = ri.Cvar_Get("r_lightBleedReduction", "0", CVAR_CHEAT | CVAR_LATCH);
	r_overDarkeningFactor = ri.Cvar_Get("r_overDarkeningFactor", "40.0", CVAR_CHEAT | CVAR_LATCH);
	r_shadowMapDepthScale = ri.Cvar_Get("r_shadowMapDepthScale", "1.41", CVAR_CHEAT | CVAR_LATCH);

	// archived variables that can change at any time
	r_lodbias = ri.Cvar_Get("r_lodbias", "0", CVAR_ARCHIVE);
	r_flares = ri.Cvar_Get("r_flares", "0", CVAR_ARCHIVE);
	r_znear = ri.Cvar_Get("r_znear", "4", CVAR_CHEAT);
	r_zfar = ri.Cvar_Get("r_zfar", "0", CVAR_CHEAT);
	r_ignoreGLErrors = ri.Cvar_Get("r_ignoreGLErrors", "1", CVAR_ARCHIVE);
	r_fastsky = ri.Cvar_Get("r_fastsky", "0", CVAR_ARCHIVE);
	r_inGameVideo = ri.Cvar_Get("r_inGameVideo", "1", CVAR_ARCHIVE);
	r_drawSun = ri.Cvar_Get("r_drawSun", "0", CVAR_ARCHIVE);
	r_finish = ri.Cvar_Get("r_finish", "0", CVAR_CHEAT);
	r_textureMode = ri.Cvar_Get("r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	r_swapInterval = ri.Cvar_Get("r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_gamma = ri.Cvar_Get("r_gamma", "1", CVAR_ARCHIVE);
	r_facePlaneCull = ri.Cvar_Get("r_facePlaneCull", "1", CVAR_ARCHIVE);

	r_railWidth = ri.Cvar_Get("r_railWidth", "96", CVAR_ARCHIVE);
	r_railCoreWidth = ri.Cvar_Get("r_railCoreWidth", "16", CVAR_ARCHIVE);
	r_railSegmentLength = ri.Cvar_Get("r_railSegmentLength", "32", CVAR_ARCHIVE);

	r_ambientScale = ri.Cvar_Get("r_ambientScale", "0.6", CVAR_CHEAT);
	r_lightScale = ri.Cvar_Get("r_lightScale", "3", CVAR_CHEAT);

	r_vboFaces = ri.Cvar_Get("r_vboFaces", "0", CVAR_CHEAT);
	r_vboCurves = ri.Cvar_Get("r_vboCurves", "0", CVAR_CHEAT);
	r_vboTriangles = ri.Cvar_Get("r_vboTriangles", "1", CVAR_CHEAT);
	r_vboShadows = ri.Cvar_Get("r_vboShadows", "1", CVAR_CHEAT);
	r_vboLighting = ri.Cvar_Get("r_vboLighting", "1", CVAR_CHEAT);
	r_vboDynamicLighting = ri.Cvar_Get("r_vboDynamicLighting", "0", CVAR_CHEAT);
	r_vboModels = ri.Cvar_Get("r_vboModels", "1", CVAR_CHEAT);
	r_vboWorld = ri.Cvar_Get("r_vboWorld", "1", CVAR_CHEAT);
	r_vboOptimizeVertices = ri.Cvar_Get("r_vboOptimizeVertices", "1", CVAR_CHEAT | CVAR_LATCH);
	r_vboVertexSkinning = ri.Cvar_Get("r_vboVertexSkinning", "1", CVAR_CHEAT | CVAR_LATCH);
	r_vboSmoothNormals = ri.Cvar_Get("r_vboSmoothNormals", "1", CVAR_ARCHIVE | CVAR_LATCH);

	r_hdrRendering = ri.Cvar_Get("r_hdrRendering", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_hdrMinLuminance = ri.Cvar_Get("r_hdrMinLuminance", "0.18", CVAR_CHEAT);
	r_hdrMaxLuminance = ri.Cvar_Get("r_hdrMaxLuminance", "3000", CVAR_CHEAT);
	r_hdrKey = ri.Cvar_Get("r_hdrKey", "0.72", CVAR_CHEAT);
	r_hdrContrastThreshold = ri.Cvar_Get("r_hdrContrastThreshold", "0.71", CVAR_CHEAT | CVAR_LATCH);
	r_hdrContrastOffset = ri.Cvar_Get("r_hdrContrastOffset", "1.0", CVAR_CHEAT | CVAR_LATCH);
	r_hdrLightmap = ri.Cvar_Get("r_hdrLightmap", "1", CVAR_CHEAT | CVAR_LATCH);
	r_hdrLightmapExposure = ri.Cvar_Get("r_hdrLightmapExposure", "1.0", CVAR_CHEAT | CVAR_LATCH);
	r_hdrLightmapGamma = ri.Cvar_Get("r_hdrLightmapGamma", "1.7", CVAR_CHEAT | CVAR_LATCH);
	r_hdrLightmapCompensate = ri.Cvar_Get("r_hdrLightmapCompensate", "1.0", CVAR_CHEAT | CVAR_LATCH);
	r_hdrToneMappingOperator = ri.Cvar_Get("r_hdrToneMappingOperator", "1", CVAR_CHEAT | CVAR_LATCH);
	r_hdrGamma = ri.Cvar_Get("r_hdrGamma", "1.1", CVAR_CHEAT | CVAR_LATCH);

	r_printShaders = ri.Cvar_Get("r_printShaders", "0", CVAR_ARCHIVE);

	r_bloom = ri.Cvar_Get("r_bloom", "0", CVAR_ARCHIVE);
	r_bloomBlur = ri.Cvar_Get("r_bloomBlur", "7.0", CVAR_CHEAT);
	r_bloomPasses = ri.Cvar_Get("r_bloomPasses", "1", CVAR_CHEAT);
	r_rotoscope = ri.Cvar_Get("r_rotoscope", "0", CVAR_ARCHIVE);

	// temporary variables that can change at any time
	r_showImages = ri.Cvar_Get("r_showImages", "0", CVAR_TEMP);

	r_debugLight = ri.Cvar_Get("r_debuglight", "0", CVAR_TEMP);
	r_debugSort = ri.Cvar_Get("r_debugSort", "0", CVAR_CHEAT);

	r_nocurves = ri.Cvar_Get("r_nocurves", "0", CVAR_CHEAT);
	r_nobatching = ri.Cvar_Get("r_nobatching", "0", CVAR_CHEAT);
	r_noLightScissors = ri.Cvar_Get("r_noLightScissors", "0", CVAR_CHEAT);
	r_noLightVisCull = ri.Cvar_Get("r_noLightVisCull", "0", CVAR_CHEAT);
	r_noInteractionSort = ri.Cvar_Get("r_noInteractionSort", "0", CVAR_CHEAT);
	r_noDynamicLighting = ri.Cvar_Get("r_noDynamicLighting", "0", CVAR_CHEAT);
	r_noStaticLighting = ri.Cvar_Get("r_noStaticLighting", "0", CVAR_CHEAT);
	r_drawworld = ri.Cvar_Get("r_drawworld", "1", CVAR_CHEAT);
	r_portalOnly = ri.Cvar_Get("r_portalOnly", "0", CVAR_CHEAT);

	r_flareSize = ri.Cvar_Get("r_flareSize", "40", CVAR_CHEAT);
	r_flareFade = ri.Cvar_Get("r_flareFade", "7", CVAR_CHEAT);

	r_showSmp = ri.Cvar_Get("r_showSmp", "0", CVAR_CHEAT);
	r_skipBackEnd = ri.Cvar_Get("r_skipBackEnd", "0", CVAR_CHEAT);

	r_measureOverdraw = ri.Cvar_Get("r_measureOverdraw", "0", CVAR_CHEAT);
	r_lodscale = ri.Cvar_Get("r_lodscale", "5", CVAR_CHEAT);
	r_norefresh = ri.Cvar_Get("r_norefresh", "0", CVAR_CHEAT);
	r_drawentities = ri.Cvar_Get("r_drawentities", "1", CVAR_CHEAT);
	r_ignore = ri.Cvar_Get("r_ignore", "1", CVAR_CHEAT);
	r_nocull = ri.Cvar_Get("r_nocull", "0", CVAR_CHEAT);
	r_novis = ri.Cvar_Get("r_novis", "0", CVAR_CHEAT);
	r_showcluster = ri.Cvar_Get("r_showcluster", "0", CVAR_CHEAT);
	r_speeds = ri.Cvar_Get("r_speeds", "0", 0);
	r_verbose = ri.Cvar_Get("r_verbose", "0", CVAR_CHEAT);
	r_logFile = ri.Cvar_Get("r_logFile", "0", CVAR_CHEAT);
	r_debugSurface = ri.Cvar_Get("r_debugSurface", "0", CVAR_CHEAT);
	r_nobind = ri.Cvar_Get("r_nobind", "0", CVAR_CHEAT);
	r_clear = ri.Cvar_Get("r_clear", "1", CVAR_CHEAT);
	r_offsetFactor = ri.Cvar_Get("r_offsetFactor", "-1", CVAR_CHEAT);
	r_offsetUnits = ri.Cvar_Get("r_offsetUnits", "-2", CVAR_CHEAT);
	r_forceSpecular = ri.Cvar_Get("r_forceSpecular", "0", CVAR_CHEAT);
	r_specularExponent = ri.Cvar_Get("r_specularExponent", "16", CVAR_CHEAT);
	r_specularScale = ri.Cvar_Get("r_specularScale", "1.4", CVAR_CHEAT | CVAR_LATCH);
	r_normalScale = ri.Cvar_Get("r_normalScale", "1.1", CVAR_CHEAT | CVAR_LATCH);
	r_normalMapping = ri.Cvar_Get("r_normalMapping", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_parallaxDepthScale = ri.Cvar_Get("r_parallaxDepthScale", "0.03", CVAR_CHEAT);
	r_drawBuffer = ri.Cvar_Get("r_drawBuffer", "GL_BACK", CVAR_CHEAT);
	r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", CVAR_CHEAT);
	r_noportals = ri.Cvar_Get("r_noportals", "0", CVAR_CHEAT);

	r_shadows = ri.Cvar_Get("cg_shadows", "1", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadows, 0, 6, qtrue);

	r_softShadows = ri.Cvar_Get("r_softShadows", "0", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_softShadows, 0, 6, qtrue);

	r_shadowBlur = ri.Cvar_Get("r_shadowBlur", "2", CVAR_ARCHIVE);

	r_shadowMapQuality = ri.Cvar_Get("r_shadowMapQuality", "3", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadowMapQuality, 0, 4, qtrue);

	r_shadowMapSizeUltra = ri.Cvar_Get("r_shadowMapSizeUltra", "1024", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadowMapSizeUltra, 32, 2048, qtrue);

	r_shadowMapSizeVeryHigh = ri.Cvar_Get("r_shadowMapSizeVeryHigh", "512", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadowMapSizeVeryHigh, 32, 2048, qtrue);

	r_shadowMapSizeHigh = ri.Cvar_Get("r_shadowMapSizeHigh", "256", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadowMapSizeHigh, 32, 2048, qtrue);

	r_shadowMapSizeMedium = ri.Cvar_Get("r_shadowMapSizeMedium", "128", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadowMapSizeMedium, 32, 2048, qtrue);

	r_shadowMapSizeLow = ri.Cvar_Get("r_shadowMapSizeLow", "64", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_shadowMapSizeLow, 32, 2048, qtrue);


	shadowMapResolutions[0] = r_shadowMapSizeUltra->integer;
	shadowMapResolutions[1] = r_shadowMapSizeVeryHigh->integer;
	shadowMapResolutions[2] = r_shadowMapSizeHigh->integer;
	shadowMapResolutions[3] = r_shadowMapSizeMedium->integer;
	shadowMapResolutions[4] = r_shadowMapSizeLow->integer;

	r_shadowOffsetFactor = ri.Cvar_Get("r_shadowOffsetFactor", "0", CVAR_CHEAT);
	r_shadowOffsetUnits = ri.Cvar_Get("r_shadowOffsetUnits", "0", CVAR_CHEAT);
	r_shadowLodBias = ri.Cvar_Get("r_shadowLodBias", "0", CVAR_CHEAT);
	r_shadowLodScale = ri.Cvar_Get("r_shadowLodScale", "0.8", CVAR_CHEAT);
	r_noShadowPyramids = ri.Cvar_Get("r_noShadowPyramids", "0", CVAR_CHEAT);
	r_cullShadowPyramidFaces = ri.Cvar_Get("r_cullShadowPyramidFaces", "0", CVAR_CHEAT);
	r_cullShadowPyramidCurves = ri.Cvar_Get("r_cullShadowPyramidCurves", "1", CVAR_CHEAT);
	r_cullShadowPyramidTriangles = ri.Cvar_Get("r_cullShadowPyramidTriangles", "1", CVAR_CHEAT);
	r_noShadowFrustums = ri.Cvar_Get("r_noShadowFrustums", "0", CVAR_CHEAT);
	r_noLightFrustums = ri.Cvar_Get("r_noLightFrustums", "0", CVAR_CHEAT);

	r_maxPolys = ri.Cvar_Get("r_maxpolys", "20000", 0);	// 600 in vanilla Q3A
	ri.Cvar_CheckRange(r_maxPolys, 600, 30000, qtrue);

	r_maxPolyVerts = ri.Cvar_Get("r_maxpolyverts", "100000", 0);	// 3000 in vanilla Q3A
	ri.Cvar_CheckRange(r_maxPolyVerts, 3000, 200000, qtrue);

	r_showTris = ri.Cvar_Get("r_showTris", "0", CVAR_CHEAT);
	r_showSky = ri.Cvar_Get("r_showSky", "0", CVAR_CHEAT);
	r_showShadowVolumes = ri.Cvar_Get("r_showShadowVolumes", "0", CVAR_CHEAT);
	r_showShadowLod = ri.Cvar_Get("r_showShadowLod", "0", CVAR_CHEAT);
	r_showSkeleton = ri.Cvar_Get("r_showSkeleton", "0", CVAR_CHEAT);
	r_showEntityTransforms = ri.Cvar_Get("r_showEntityTransforms", "0", CVAR_CHEAT);
	r_showLightTransforms = ri.Cvar_Get("r_showLightTransforms", "0", CVAR_CHEAT);
	r_showLightInteractions = ri.Cvar_Get("r_showLightInteractions", "0", CVAR_CHEAT);
	r_showLightScissors = ri.Cvar_Get("r_showLightScissors", "0", CVAR_CHEAT);
	r_showLightBatches = ri.Cvar_Get("r_showLightBatches", "0", CVAR_CHEAT);
	r_showOcclusionQueries = ri.Cvar_Get("r_showOcclusionQueries", "0", CVAR_CHEAT);
	r_showBatches = ri.Cvar_Get("r_showBatches", "0", CVAR_CHEAT);
	r_showLightMaps = ri.Cvar_Get("r_showLightMaps", "0", CVAR_CHEAT | CVAR_LATCH);
	r_showDeluxeMaps = ri.Cvar_Get("r_showDeluxeMaps", "0", CVAR_CHEAT | CVAR_LATCH);
	r_showAreaPortals = ri.Cvar_Get("r_showAreaPortals", "0", CVAR_CHEAT);
	r_showCubeProbes = ri.Cvar_Get("r_showCubeProbes", "0", CVAR_CHEAT);

	r_showDeferredDiffuse = ri.Cvar_Get("r_showDeferredDiffuse", "0", CVAR_CHEAT);
	r_showDeferredNormal = ri.Cvar_Get("r_showDeferredNormal", "0", CVAR_CHEAT);
	r_showDeferredSpecular = ri.Cvar_Get("r_showDeferredSpecular", "0", CVAR_CHEAT);
	r_showDeferredPosition = ri.Cvar_Get("r_showDeferredPosition", "0", CVAR_CHEAT);
	r_showDeferredRender = ri.Cvar_Get("r_showDeferredRender", "0", CVAR_CHEAT);

	// make sure all the commands added here are also removed in R_Shutdown
	ri.Cmd_AddCommand("imagelist", R_ImageList_f);
	ri.Cmd_AddCommand("shaderlist", R_ShaderList_f);
	ri.Cmd_AddCommand("shaderexp", R_ShaderExp_f);
	ri.Cmd_AddCommand("skinlist", R_SkinList_f);
	ri.Cmd_AddCommand("modellist", R_Modellist_f);
	ri.Cmd_AddCommand("modelist", R_ModeList_f);
	ri.Cmd_AddCommand("animationlist", R_AnimationList_f);
	ri.Cmd_AddCommand("fbolist", R_FBOList_f);
	ri.Cmd_AddCommand("vbolist", R_VBOList_f);
	ri.Cmd_AddCommand("screenshot", R_ScreenShot_f);
	ri.Cmd_AddCommand("screenshotJPEG", R_ScreenShotJPEG_f);
	//ri.Cmd_AddCommand("screenshotPNG", R_ScreenShotPNG_f);
	ri.Cmd_AddCommand("gfxinfo", GfxInfo_f);
	ri.Cmd_AddCommand("generatemtr", R_GenerateMaterialFile_f);
}

/*
===============
R_Init
===============
*/
void R_Init(void)
{
	int             err;
	int             i;
	byte           *ptr;

	ri.Printf(PRINT_ALL, "----- R_Init -----\n");

	// clear all our internal state
	Com_Memset(&tr, 0, sizeof(tr));
	Com_Memset(&backEnd, 0, sizeof(backEnd));
	Com_Memset(&tess, 0, sizeof(tess));

	if((intptr_t) tess.xyz & 15)
		Com_Printf("WARNING: tess.xyz not 16 byte aligned\n");

	// init function tables
	for(i = 0; i < FUNCTABLE_SIZE; i++)
	{
		tr.sinTable[i] = sin(DEG2RAD(i * 360.0f / ((float)(FUNCTABLE_SIZE - 1))));
		tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if(i < FUNCTABLE_SIZE / 2)
		{
			if(i < FUNCTABLE_SIZE / 4)
			{
				tr.triangleTable[i] = (float)i / (FUNCTABLE_SIZE / 4);
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
		}
	}

	R_NoiseInit();

	R_Register();

	ptr =
		ri.Hunk_Alloc(sizeof(*backEndData[0]) + sizeof(srfPoly_t) * r_maxPolys->integer +
					  sizeof(polyVert_t) * r_maxPolyVerts->integer, h_low);
	backEndData[0] = (backEndData_t *) ptr;
	backEndData[0]->polys = (srfPoly_t *) ((char *)ptr + sizeof(*backEndData[0]));
	backEndData[0]->polyVerts = (polyVert_t *) ((char *)ptr + sizeof(*backEndData[0]) + sizeof(srfPoly_t) * r_maxPolys->integer);
	if(r_smp->integer)
	{
		ptr =
			ri.Hunk_Alloc(sizeof(*backEndData[1]) + sizeof(srfPoly_t) * r_maxPolys->integer +
						  sizeof(polyVert_t) * r_maxPolyVerts->integer, h_low);
		backEndData[1] = (backEndData_t *) ptr;
		backEndData[1]->polys = (srfPoly_t *) ((char *)ptr + sizeof(*backEndData[1]));
		backEndData[1]->polyVerts =
			(polyVert_t *) ((char *)ptr + sizeof(*backEndData[1]) + sizeof(srfPoly_t) * r_maxPolys->integer);
	}
	else
	{
		backEndData[1] = NULL;
	}

	R_ToggleSmpFrame();

	InitOpenGL();

	GLSL_InitGPUShaders();

	R_InitImages();

	R_InitFBOs();

	R_InitVBOs();

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	R_InitAnimations();

	R_InitFreeType();

	if(glConfig.textureAnisotropyAvailable)
	{
		ri.Cvar_CheckRange(r_ext_texture_filter_anisotropic, 0, glConfig.maxTextureAnisotropy, qfalse);
	}

	if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA)
	{
		qglGenQueriesARB(MAX_OCCLUSION_QUERIES, tr.occlusionQueryObjects);
	}

	err = qglGetError();
	if(err != GL_NO_ERROR)
	{
		ri.Error(ERR_FATAL, "R_Init() - glGetError() failed = 0x%x\n", err);
		//ri.Printf(PRINT_ALL, "glGetError() = 0x%x\n", err);
	}

	ri.Printf(PRINT_ALL, "----- finished R_Init -----\n");
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown(qboolean destroyWindow)
{
	ri.Printf(PRINT_ALL, "RE_Shutdown( destroyWindow = %i )\n", destroyWindow);

	ri.Cmd_RemoveCommand("modellist");
	ri.Cmd_RemoveCommand("screenshotPNG");
	ri.Cmd_RemoveCommand("screenshotJPEG");
	ri.Cmd_RemoveCommand("screenshot");
	ri.Cmd_RemoveCommand("imagelist");
	ri.Cmd_RemoveCommand("shaderlist");
	ri.Cmd_RemoveCommand("shaderexp");
	ri.Cmd_RemoveCommand("skinlist");
	ri.Cmd_RemoveCommand("gfxinfo");
	ri.Cmd_RemoveCommand("modelist");
	ri.Cmd_RemoveCommand("shaderstate");
	ri.Cmd_RemoveCommand("animationlist");
	ri.Cmd_RemoveCommand("fbolist");
	ri.Cmd_RemoveCommand("vbolist");
	ri.Cmd_RemoveCommand("generatemtr");

	if(tr.registered)
	{
		R_SyncRenderThread();

		R_ShutdownCommandBuffers();
		R_ShutdownImages();
		R_ShutdownVBOs();
		R_ShutdownFBOs();

		if(glConfig.occlusionQueryBits && glConfig.driverType != GLDRV_MESA)
		{
			qglDeleteQueriesARB(MAX_OCCLUSION_QUERIES, tr.occlusionQueryObjects);
		}

		GLSL_ShutdownGPUShaders();

		//GLimp_ShutdownRenderThread();
	}

	R_DoneFreeType();

	// shut down platform specific OpenGL stuff

	// Tr3B: this should be always executed if we want to avoid some GLSL problems with SMP
#if !defined(SMP)
	if(destroyWindow)
#endif
	{
		GLimp_Shutdown();
	}

	tr.registered = qfalse;
}


/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration(void)
{
	R_SyncRenderThread();

	/*
	   if(!Sys_LowPhysicalMemory())
	   {
	   RB_ShowImages();
	   }
	 */
}


/*
=====================
GetRefAPI
=====================
*/
refexport_t    *GetRefAPI(int apiVersion, refimport_t * rimp)
{
	static refexport_t re;

	ri = *rimp;

	Com_Memset(&re, 0, sizeof(re));

	if(apiVersion != REF_API_VERSION)
	{
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion);
		return NULL;
	}

	// the RE_ functions are Renderer Entry points
	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterAnimation = RE_RegisterAnimation;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.RegisterShaderLightAttenuation = RE_RegisterShaderLightAttenuation;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = RE_LerpTag;

	re.CheckSkeleton = RE_CheckSkeleton;
	re.BuildSkeleton = RE_BuildSkeleton;
	re.BlendSkeleton = RE_BlendSkeleton;
	re.BoneIndex = RE_BoneIndex;
	re.AnimNumFrames = RE_AnimNumFrames;
	re.AnimFrameRate = RE_AnimFrameRate;

	re.ModelBounds = R_ModelBounds;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddRefLightToScene = RE_AddRefLightToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;

	re.TakeVideoFrame = RE_TakeVideoFrame;

	return &re;
}
