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

// qgl.h
#ifndef __QGL_H__
#define __QGL_H__

#ifdef USE_LOCAL_HEADERS
#	include "SDL_opengl.h"
#else
#	include <SDL_opengl.h>
#endif


int             QGL_Init(void);
void            QGL_Shutdown(void);
void            QGL_EnableLogging(int enable);

/*
	This interface is similar to OpenGL ES 2.0.
*/

// OpenGL 2.x core API
extern void     (APIENTRY * qglBindTexture) (GLenum target, GLuint texture);
extern void     (APIENTRY * qglBlendFunc) (GLenum sfactor, GLenum dfactor);
extern void     (APIENTRY * qglClear) (GLbitfield mask);
extern void     (APIENTRY * qglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern void     (APIENTRY * qglClearDepth) (GLclampd depth);
extern void     (APIENTRY * qglClearStencil) (GLint s);
extern void     (APIENTRY * qglClipPlane) (GLenum plane, const GLdouble * equation);
extern void     (APIENTRY * qglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern void     (APIENTRY * qglCopyPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
extern void     (APIENTRY * qglCopyTexImage1D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
												GLsizei width, GLint border);
extern void     (APIENTRY * qglCopyTexImage2D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
												GLsizei width, GLsizei height, GLint border);
extern void     (APIENTRY * qglCopyTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
extern void     (APIENTRY * qglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
												   GLsizei width, GLsizei height);
extern void     (APIENTRY * qglCullFace) (GLenum mode);
extern void     (APIENTRY * qglDeleteTextures) (GLsizei n, const GLuint * textures);
extern void     (APIENTRY * qglDepthFunc) (GLenum func);
extern void     (APIENTRY * qglDepthMask) (GLboolean flag);
extern void     (APIENTRY * qglDepthRange) (GLclampd zNear, GLclampd zFar);
extern void     (APIENTRY * qglDisable) (GLenum cap);
extern void     (APIENTRY * qglDrawArrays) (GLenum mode, GLint first, GLsizei count);
extern void     (APIENTRY * qglDrawBuffer) (GLenum mode);
extern void     (APIENTRY * qglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
extern void     (APIENTRY * qglEnable) (GLenum cap);
extern void     (APIENTRY * qglFinish) (void);
extern void     (APIENTRY * qglFlush) (void);
extern void     (APIENTRY * qglFrontFace) (GLenum mode);
extern void     (APIENTRY * qglGenTextures) (GLsizei n, GLuint * textures);
extern void     (APIENTRY * qglGetBooleanv) (GLenum pname, GLboolean * params);
extern          GLenum(APIENTRY * qglGetError) (void);
extern void     (APIENTRY * qglGetFloatv) (GLenum pname, GLfloat * params);
extern void     (APIENTRY * qglGetIntegerv) (GLenum pname, GLint * params);
extern const GLubyte *(APIENTRY * qglGetString) (GLenum name);
extern void     (APIENTRY * qglGetTexParameterfv) (GLenum target, GLenum pname, GLfloat * params);
extern void     (APIENTRY * qglGetTexParameteriv) (GLenum target, GLenum pname, GLint * params);
extern void     (APIENTRY * qglHint) (GLenum target, GLenum mode);
extern          GLboolean(APIENTRY * qglIsEnabled) (GLenum cap);
extern          GLboolean(APIENTRY * qglIsTexture) (GLuint texture);
extern void     (APIENTRY * qglLineWidth) (GLfloat width);
extern void     (APIENTRY * qglPolygonMode) (GLenum face, GLenum mode);
extern void     (APIENTRY * qglPolygonOffset) (GLfloat factor, GLfloat units);
extern void     (APIENTRY * qglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
											GLvoid * pixels);
extern void     (APIENTRY * qglScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
extern void     (APIENTRY * qglStencilFunc) (GLenum func, GLint ref, GLuint mask);
extern void     (APIENTRY * qglStencilMask) (GLuint mask);
extern void     (APIENTRY * qglStencilOp) (GLenum fail, GLenum zfail, GLenum zpass);
extern void     (APIENTRY * qglTexImage1D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
											GLenum format, GLenum type, const GLvoid * pixels);
extern void     (APIENTRY * qglTexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
											GLint border, GLenum format, GLenum type, const GLvoid * pixels);
extern void     (APIENTRY * qglTexParameterf) (GLenum target, GLenum pname, GLfloat param);
extern void     (APIENTRY * qglTexParameterfv) (GLenum target, GLenum pname, const GLfloat * params);
extern void     (APIENTRY * qglTexParameteri) (GLenum target, GLenum pname, GLint param);
extern void     (APIENTRY * qglTexParameteriv) (GLenum target, GLenum pname, const GLint * params);
extern void     (APIENTRY * qglTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format,
											   GLenum type, const GLvoid * pixels);
extern void     (APIENTRY * qglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
											   GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
extern void     (APIENTRY * qglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);


// GL_ARB_multitexture
extern void     (APIENTRY * qglActiveTextureARB) (GLenum texture);

// GL_ARB_texture_compression
#ifndef GL_ARB_texture_compression
#define GL_COMPRESSED_ALPHA_ARB           0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB       0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB 0x84EB
#define GL_COMPRESSED_INTENSITY_ARB       0x84EC
#define GL_COMPRESSED_RGB_ARB             0x84ED
#define GL_COMPRESSED_RGBA_ARB            0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB   0x84EF
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB 0x86A0
#define GL_TEXTURE_COMPRESSED_ARB         0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB 0x86A3
#endif

extern void (APIENTRY * qglCompressedTexImage3DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
extern void (APIENTRY * qglCompressedTexImage2DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
extern void (APIENTRY * qglCompressedTexImage1DARB) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
extern void (APIENTRY * qglCompressedTexSubImage3DARB) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
extern void (APIENTRY * qglCompressedTexSubImage2DARB) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
extern void (APIENTRY * qglCompressedTexSubImage1DARB) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
extern void (APIENTRY * qglGetCompressedTexImageARB) (GLenum target, GLint level, GLvoid *img);

// GL_ARB_vertex_program
extern void     (APIENTRY * qglVertexAttrib4fARB) (GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void     (APIENTRY * qglVertexAttrib4fvARB) (GLuint, const GLfloat *);
extern void     (APIENTRY * qglVertexAttribPointerARB) (GLuint index, GLint size, GLenum type, GLboolean normalized,
														GLsizei stride, const GLvoid * pointer);
extern void     (APIENTRY * qglEnableVertexAttribArrayARB) (GLuint index);
extern void     (APIENTRY * qglDisableVertexAttribArrayARB) (GLuint index);

// GL_ARB_vertex_buffer_object
extern void     (APIENTRY * qglBindBufferARB) (GLenum target, GLuint buffer);
extern void     (APIENTRY * qglDeleteBuffersARB) (GLsizei n, const GLuint * buffers);
extern void     (APIENTRY * qglGenBuffersARB) (GLsizei n, GLuint * buffers);
extern          GLboolean(APIENTRY * qglIsBufferARB) (GLuint buffer);
extern void     (APIENTRY * qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage);
extern void     (APIENTRY * qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data);
extern void     (APIENTRY * qglGetBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid * data);
extern void     (APIENTRY * qglGetBufferParameterivARB) (GLenum target, GLenum pname, GLint * params);
extern void     (APIENTRY * qglGetBufferPointervARB) (GLenum target, GLenum pname, GLvoid * *params);

// GL_ARB_occlusion_query
extern void     (APIENTRY * qglGenQueriesARB) (GLsizei n, GLuint * ids);
extern void     (APIENTRY * qglDeleteQueriesARB) (GLsizei n, const GLuint * ids);
extern          GLboolean(APIENTRY * qglIsQueryARB) (GLuint id);
extern void     (APIENTRY * qglBeginQueryARB) (GLenum target, GLuint id);
extern void     (APIENTRY * qglEndQueryARB) (GLenum target);
extern void     (APIENTRY * qglGetQueryivARB) (GLenum target, GLenum pname, GLint * params);
extern void     (APIENTRY * qglGetQueryObjectivARB) (GLuint id, GLenum pname, GLint * params);
extern void     (APIENTRY * qglGetQueryObjectuivARB) (GLuint id, GLenum pname, GLuint * params);

// GL_ARB_shader_objects
extern void     (APIENTRY * qglDeleteObjectARB) (GLhandleARB obj);
extern          GLhandleARB(APIENTRY * qglGetHandleARB) (GLenum pname);
extern void     (APIENTRY * qglDetachObjectARB) (GLhandleARB containerObj, GLhandleARB attachedObj);
extern          GLhandleARB(APIENTRY * qglCreateShaderObjectARB) (GLenum shaderType);
extern void     (APIENTRY * qglShaderSourceARB) (GLhandleARB shaderObj, GLsizei count, const GLcharARB * *string,
												 const GLint * length);
extern void     (APIENTRY * qglCompileShaderARB) (GLhandleARB shaderObj);
extern          GLhandleARB(APIENTRY * qglCreateProgramObjectARB) (void);
extern void     (APIENTRY * qglAttachObjectARB) (GLhandleARB containerObj, GLhandleARB obj);
extern void     (APIENTRY * qglLinkProgramARB) (GLhandleARB programObj);
extern void     (APIENTRY * qglUseProgramObjectARB) (GLhandleARB programObj);
extern void     (APIENTRY * qglValidateProgramARB) (GLhandleARB programObj);
extern void     (APIENTRY * qglUniform1fARB) (GLint location, GLfloat v0);
extern void     (APIENTRY * qglUniform2fARB) (GLint location, GLfloat v0, GLfloat v1);
extern void     (APIENTRY * qglUniform3fARB) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void     (APIENTRY * qglUniform4fARB) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void     (APIENTRY * qglUniform1iARB) (GLint location, GLint v0);
extern void     (APIENTRY * qglUniform2iARB) (GLint location, GLint v0, GLint v1);
extern void     (APIENTRY * qglUniform3iARB) (GLint location, GLint v0, GLint v1, GLint v2);
extern void     (APIENTRY * qglUniform4iARB) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern void     (APIENTRY * qglUniform2fvARB) (GLint location, GLsizei count, const GLfloat * value);
extern void     (APIENTRY * qglUniform3fvARB) (GLint location, GLsizei count, const GLfloat * value);
extern void     (APIENTRY * qglUniform4fvARB) (GLint location, GLsizei count, const GLfloat * value);
extern void     (APIENTRY * qglUniform2ivARB) (GLint location, GLsizei count, const GLint * value);
extern void     (APIENTRY * qglUniform3ivARB) (GLint location, GLsizei count, const GLint * value);
extern void     (APIENTRY * qglUniform4ivARB) (GLint location, GLsizei count, const GLint * value);
extern void     (APIENTRY * qglUniformMatrix2fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
extern void     (APIENTRY * qglUniformMatrix3fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
extern void     (APIENTRY * qglUniformMatrix4fvARB) (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value);
extern void     (APIENTRY * qglGetObjectParameterfvARB) (GLhandleARB obj, GLenum pname, GLfloat * params);
extern void     (APIENTRY * qglGetObjectParameterivARB) (GLhandleARB obj, GLenum pname, GLint * params);
extern void     (APIENTRY * qglGetInfoLogARB) (GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * infoLog);
extern void     (APIENTRY * qglGetAttachedObjectsARB) (GLhandleARB containerObj, GLsizei maxCount, GLsizei * count,
													   GLhandleARB * obj);
extern          GLint(APIENTRY * qglGetUniformLocationARB) (GLhandleARB programObj, const GLcharARB * name);
extern void     (APIENTRY * qglGetActiveUniformARB) (GLhandleARB programObj, GLuint index, GLsizei maxIndex, GLsizei * length,
													 GLint * size, GLenum * type, GLcharARB * name);
extern void     (APIENTRY * qglGetUniformfvARB) (GLhandleARB programObj, GLint location, GLfloat * params);
extern void     (APIENTRY * qglGetUniformivARB) (GLhandleARB programObj, GLint location, GLint * params);
extern void     (APIENTRY * qglGetShaderSourceARB) (GLhandleARB obj, GLsizei maxLength, GLsizei * length, GLcharARB * source);

// GL_ARB_vertex_shader
extern void     (APIENTRY * qglBindAttribLocationARB) (GLhandleARB programObj, GLuint index, const GLcharARB * name);
extern void     (APIENTRY * qglGetActiveAttribARB) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei * length,
													GLint * size, GLenum * type, GLcharARB * name);
extern          GLint(APIENTRY * qglGetAttribLocationARB) (GLhandleARB programObj, const GLcharARB * name);

// GL_ARB_draw_buffers
extern void     (APIENTRY * qglDrawBuffersARB) (GLsizei n, const GLenum * bufs);

// GL_ARB_vertex_array_object
#ifndef GL_ARB_vertex_array_object
#define GL_VERTEX_ARRAY_BINDING           0x85B5
#endif

extern void		(APIENTRY * qglBindVertexArray) (GLuint array);
extern void		(APIENTRY * qglDeleteVertexArrays) (GLsizei n, const GLuint *arrays);
extern void		(APIENTRY * qglGenVertexArrays) (GLsizei n, GLuint *arrays);
extern GLboolean (APIENTRY * qglIsVertexArray) (GLuint array);




#if defined(WIN32)
// WGL_ARB_create_context
#ifndef WGL_ARB_create_context
#define WGL_CONTEXT_DEBUG_BIT_ARB      0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB    0x2093
#define WGL_CONTEXT_FLAGS_ARB          0x2094
#define ERROR_INVALID_VERSION_ARB      0x2095
#endif

extern          HGLRC(APIENTRY * qwglCreateContextAttribsARB) (HDC hdC, HGLRC hShareContext, const int *attribList);
#endif

#if 0 //defined(__linux__)
// GLX_ARB_create_context
#ifndef GLX_ARB_create_context
#define GLX_CONTEXT_DEBUG_BIT_ARB          0x00000001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define GLX_CONTEXT_MAJOR_VERSION_ARB      0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB      0x2092
#define GLX_CONTEXT_FLAGS_ARB              0x2094
#endif

extern GLXContext	(APIENTRY * qglXCreateContextAttribsARB) (Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);
#endif

// GL_EXT_texture3D
#ifndef GL_EXT_texture3D
#define GL_PACK_SKIP_IMAGES_EXT           0x806B
#define GL_PACK_IMAGE_HEIGHT_EXT          0x806C
#define GL_UNPACK_SKIP_IMAGES_EXT         0x806D
#define GL_UNPACK_IMAGE_HEIGHT_EXT        0x806E
#define GL_TEXTURE_3D_EXT                 0x806F
#define GL_PROXY_TEXTURE_3D_EXT           0x8070
#define GL_TEXTURE_DEPTH_EXT              0x8071
#define GL_TEXTURE_WRAP_R_EXT             0x8072
#define GL_MAX_3D_TEXTURE_SIZE_EXT        0x8073
#endif

extern void		(APIENTRY * qglTexImage3DEXT) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void		(APIENTRY * qglTexSubImage3DEXT) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

// GL_EXT_texture_comression_s3tc
#ifndef GL_EXT_texture_compression_s3tc
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

// GL_EXT_stencil_two_side
extern void     (APIENTRY * qglActiveStencilFaceEXT) (GLenum face);

// GL_EXT_depth_bounds_test
extern void     (APIENTRY * qglDepthBoundsEXT) (GLclampd zmin, GLclampd zmax);

// GL_ATI_separate_stencil
extern void     (APIENTRY * qglStencilFuncSeparateATI) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
extern void     (APIENTRY * qglStencilOpSeparateATI) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);

// GL_EXT_framebuffer_object
#ifndef GL_EXT_framebuffer_object
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
#define GL_MAX_RENDERBUFFER_SIZE_EXT      0x84E8
#define GL_FRAMEBUFFER_BINDING_EXT        0x8CA6
#define GL_RENDERBUFFER_BINDING_EXT       0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE_EXT       0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT 0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT 0x8CDA
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT    0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS_EXT      0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT          0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT          0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT          0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT          0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT          0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT          0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT          0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT          0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT          0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT          0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT         0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT         0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT         0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT         0x8CED
#define GL_COLOR_ATTACHMENT14_EXT         0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT         0x8CEF
#define GL_DEPTH_ATTACHMENT_EXT           0x8D00
#define GL_STENCIL_ATTACHMENT_EXT         0x8D20
#define GL_FRAMEBUFFER_EXT                0x8D40
#define GL_RENDERBUFFER_EXT               0x8D41
#define GL_RENDERBUFFER_WIDTH_EXT         0x8D42
#define GL_RENDERBUFFER_HEIGHT_EXT        0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT 0x8D44
#define GL_STENCIL_INDEX1_EXT             0x8D46
#define GL_STENCIL_INDEX4_EXT             0x8D47
#define GL_STENCIL_INDEX8_EXT             0x8D48
#define GL_STENCIL_INDEX16_EXT            0x8D49
#define GL_RENDERBUFFER_RED_SIZE_EXT      0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE_EXT    0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE_EXT     0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE_EXT    0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE_EXT    0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE_EXT  0x8D55
#endif

extern          GLboolean(APIENTRY * qglIsRenderbufferEXT) (GLuint renderbuffer);
extern void     (APIENTRY * qglBindRenderbufferEXT) (GLenum target, GLuint renderbuffer);
extern void     (APIENTRY * qglDeleteRenderbuffersEXT) (GLsizei n, const GLuint * renderbuffers);
extern void     (APIENTRY * qglGenRenderbuffersEXT) (GLsizei n, GLuint * renderbuffers);
extern void     (APIENTRY * qglRenderbufferStorageEXT) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern void     (APIENTRY * qglGetRenderbufferParameterivEXT) (GLenum target, GLenum pname, GLint * params);
extern          GLboolean(APIENTRY * qglIsFramebufferEXT) (GLuint framebuffer);
extern void     (APIENTRY * qglBindFramebufferEXT) (GLenum target, GLuint framebuffer);
extern void     (APIENTRY * qglDeleteFramebuffersEXT) (GLsizei n, const GLuint * framebuffers);
extern void     (APIENTRY * qglGenFramebuffersEXT) (GLsizei n, GLuint * framebuffers);
extern          GLenum(APIENTRY * qglCheckFramebufferStatusEXT) (GLenum target);
extern void     (APIENTRY * qglFramebufferTexture1DEXT) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
														 GLint level);
extern void     (APIENTRY * qglFramebufferTexture2DEXT) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
														 GLint level);
extern void     (APIENTRY * qglFramebufferTexture3DEXT) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
														 GLint level, GLint zoffset);
extern void     (APIENTRY * qglFramebufferRenderbufferEXT) (GLenum target, GLenum attachment, GLenum renderbuffertarget,
															GLuint renderbuffer);
extern void     (APIENTRY * qglGetFramebufferAttachmentParameterivEXT) (GLenum target, GLenum attachment, GLenum pname,
																		GLint * params);
extern void     (APIENTRY * qglGenerateMipmapEXT) (GLenum target);

// GL_EXT_packed_depth_stencil
#ifndef GL_EXT_packed_depth_stencil
#define GL_DEPTH_STENCIL_EXT              0x84F9
#define GL_UNSIGNED_INT_24_8_EXT          0x84FA
#define GL_DEPTH24_STENCIL8_EXT           0x88F0
#define GL_TEXTURE_STENCIL_SIZE_EXT       0x88F1
#endif

// GL_EXT_framebuffer_blit
#ifndef GL_EXT_framebuffer_blit
#define GL_READ_FRAMEBUFFER_EXT           0x8CA8
#define GL_DRAW_FRAMEBUFFER_EXT           0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT   GL_FRAMEBUFFER_BINDING_EXT
#define GL_READ_FRAMEBUFFER_BINDING_EXT   0x8CAA
#endif

extern void		(APIENTRY * qglBlitFramebufferEXT) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);


#endif							// __QGL_H__
