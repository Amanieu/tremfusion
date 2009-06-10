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
/*
** QGL.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of XreaL you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/

#include "SDL.h"
#include "SDL_loadso.h"

#include "qgl.h"
#include "tr_local.h"


// OpenGL 2.x core API
void            (APIENTRY * qglBindTexture) (GLenum target, GLuint texture);
void            (APIENTRY * qglBlendFunc) (GLenum sfactor, GLenum dfactor);
void            (APIENTRY * qglClear) (GLbitfield mask);
void            (APIENTRY * qglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void            (APIENTRY * qglClearDepth) (GLclampd depth);
void            (APIENTRY * qglClearStencil) (GLint s);
void            (APIENTRY * qglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void            (APIENTRY * qglCopyPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void            (APIENTRY * qglCopyTexImage1D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
												GLsizei width, GLint border);
void            (APIENTRY * qglCopyTexImage2D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
												GLsizei width, GLsizei height, GLint border);
void            (APIENTRY * qglCopyTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void            (APIENTRY * qglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
												   GLsizei width, GLsizei height);
void            (APIENTRY * qglCullFace) (GLenum mode);
void            (APIENTRY * qglDeleteTextures) (GLsizei n, const GLuint * textures);
void            (APIENTRY * qglDepthFunc) (GLenum func);
void            (APIENTRY * qglDepthMask) (GLboolean flag);
void            (APIENTRY * qglDepthRange) (GLclampd zNear, GLclampd zFar);
void            (APIENTRY * qglDisable) (GLenum cap);
void            (APIENTRY * qglDrawArrays) (GLenum mode, GLint first, GLsizei count);
void            (APIENTRY * qglDrawBuffer) (GLenum mode);
void            (APIENTRY * qglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
void            (APIENTRY * qglEnable) (GLenum cap);
void            (APIENTRY * qglFinish) (void);
void            (APIENTRY * qglFlush) (void);
void            (APIENTRY * qglFrontFace) (GLenum mode);
void            (APIENTRY * qglGenTextures) (GLsizei n, GLuint * textures);
void            (APIENTRY * qglGetBooleanv) (GLenum pname, GLboolean * params);

GLenum(APIENTRY * qglGetError) (void);
void            (APIENTRY * qglGetFloatv) (GLenum pname, GLfloat * params);
void            (APIENTRY * qglGetIntegerv) (GLenum pname, GLint * params);
const GLubyte  *(APIENTRY * qglGetString) (GLenum name);
void            (APIENTRY * qglGetTexParameterfv) (GLenum target, GLenum pname, GLfloat * params);
void            (APIENTRY * qglGetTexParameteriv) (GLenum target, GLenum pname, GLint * params);
void            (APIENTRY * qglHint) (GLenum target, GLenum mode);

GLboolean(APIENTRY * qglIsEnabled) (GLenum cap);
GLboolean(APIENTRY * qglIsTexture) (GLuint texture);
void            (APIENTRY * qglLineWidth) (GLfloat width);
void            (APIENTRY * qglPolygonMode) (GLenum face, GLenum mode);
void            (APIENTRY * qglPolygonOffset) (GLfloat factor, GLfloat units);
void            (APIENTRY * qglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
											GLvoid * pixels);
void            (APIENTRY * qglScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
void            (APIENTRY * qglSelectBuffer) (GLsizei size, GLuint * buffer);
void            (APIENTRY * qglShadeModel) (GLenum mode);
void            (APIENTRY * qglStencilFunc) (GLenum func, GLint ref, GLuint mask);
void            (APIENTRY * qglStencilMask) (GLuint mask);
void            (APIENTRY * qglStencilOp) (GLenum fail, GLenum zfail, GLenum zpass);
void            (APIENTRY * qglTexImage1D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
											GLenum format, GLenum type, const GLvoid * pixels);
void            (APIENTRY * qglTexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
											GLint border, GLenum format, GLenum type, const GLvoid * pixels);
void            (APIENTRY * qglTexParameterf) (GLenum target, GLenum pname, GLfloat param);
void            (APIENTRY * qglTexParameterfv) (GLenum target, GLenum pname, const GLfloat * params);
void            (APIENTRY * qglTexParameteri) (GLenum target, GLenum pname, GLint param);
void            (APIENTRY * qglTexParameteriv) (GLenum target, GLenum pname, const GLint * params);
void            (APIENTRY * qglTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format,
											   GLenum type, const GLvoid * pixels);
void            (APIENTRY * qglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
											   GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
void            (APIENTRY * qglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);


#if defined(WIN32)
// WGL_ARB_create_context
HGLRC(APIENTRY * qwglCreateContextAttribsARB) (HDC hdC, HGLRC hShareContext, const int *attribList);
#endif

#if 0 //defined(__linux__)
// GLX_ARB_create_context
GLXContext      (APIENTRY * qglXCreateContextAttribsARB) (Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);
#endif


static void     (APIENTRY * dllBindTexture) (GLenum target, GLuint texture);
static void     (APIENTRY * dllBlendFunc) (GLenum sfactor, GLenum dfactor);
static void     (APIENTRY * dllClear) (GLbitfield mask);
static void     (APIENTRY * dllClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
static void     (APIENTRY * dllClearDepth) (GLclampd depth);
static void     (APIENTRY * dllClearStencil) (GLint s);
static void     (APIENTRY * dllColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
static void     (APIENTRY * dllCopyPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
static void     (APIENTRY * dllCopyTexImage1D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
												GLsizei width, GLint border);
static void     (APIENTRY * dllCopyTexImage2D) (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
												GLsizei width, GLsizei height, GLint border);
static void     (APIENTRY * dllCopyTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
static void     (APIENTRY * dllCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
												   GLsizei width, GLsizei height);
static void     (APIENTRY * dllCullFace) (GLenum mode);
static void     (APIENTRY * dllDeleteTextures) (GLsizei n, const GLuint * textures);
static void     (APIENTRY * dllDepthFunc) (GLenum func);
static void     (APIENTRY * dllDepthMask) (GLboolean flag);
static void     (APIENTRY * dllDepthRange) (GLclampd zNear, GLclampd zFar);
static void     (APIENTRY * dllDisable) (GLenum cap);
static void     (APIENTRY * dllDrawArrays) (GLenum mode, GLint first, GLsizei count);
static void     (APIENTRY * dllDrawBuffer) (GLenum mode);
static void     (APIENTRY * dllDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
static void     (APIENTRY * dllEnable) (GLenum cap);
static void     (APIENTRY * dllFinish) (void);
static void     (APIENTRY * dllFlush) (void);
static void     (APIENTRY * dllFrontFace) (GLenum mode);
static void     (APIENTRY * dllGenTextures) (GLsizei n, GLuint * textures);
static void     (APIENTRY * dllGetBooleanv) (GLenum pname, GLboolean * params);
static void     (APIENTRY * dllGetDoublev) (GLenum pname, GLdouble * params);

GLenum(APIENTRY * dllGetError) (void);
static void     (APIENTRY * dllGetFloatv) (GLenum pname, GLfloat * params);
static void     (APIENTRY * dllGetIntegerv) (GLenum pname, GLint * params);
const GLubyte  *(APIENTRY * dllGetString) (GLenum name);
static void     (APIENTRY * dllGetTexParameterfv) (GLenum target, GLenum pname, GLfloat * params);
static void     (APIENTRY * dllGetTexParameteriv) (GLenum target, GLenum pname, GLint * params);
static void     (APIENTRY * dllHint) (GLenum target, GLenum mode);

GLboolean(APIENTRY * dllIsEnabled) (GLenum cap);
GLboolean(APIENTRY * dllIsTexture) (GLuint texture);
static void     (APIENTRY * dllLineWidth) (GLfloat width);
static void     (APIENTRY * dllPolygonMode) (GLenum face, GLenum mode);
static void     (APIENTRY * dllPolygonOffset) (GLfloat factor, GLfloat units);
static void     (APIENTRY * dllReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
											GLvoid * pixels);
static void     (APIENTRY * dllScissor) (GLint x, GLint y, GLsizei width, GLsizei height);
static void     (APIENTRY * dllStencilFunc) (GLenum func, GLint ref, GLuint mask);
static void     (APIENTRY * dllStencilMask) (GLuint mask);
static void     (APIENTRY * dllStencilOp) (GLenum fail, GLenum zfail, GLenum zpass);
static void     (APIENTRY * dllTexImage1D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
											GLenum format, GLenum type, const GLvoid * pixels);
static void     (APIENTRY * dllTexImage2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
											GLint border, GLenum format, GLenum type, const GLvoid * pixels);
static void     (APIENTRY * dllTexParameterf) (GLenum target, GLenum pname, GLfloat param);
static void     (APIENTRY * dllTexParameterfv) (GLenum target, GLenum pname, const GLfloat * params);
static void     (APIENTRY * dllTexParameteri) (GLenum target, GLenum pname, GLint param);
static void     (APIENTRY * dllTexParameteriv) (GLenum target, GLenum pname, const GLint * params);
static void     (APIENTRY * dllTexSubImage1D) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format,
											   GLenum type, const GLvoid * pixels);
static void     (APIENTRY * dllTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
											   GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
static void     (APIENTRY * dllViewport) (GLint x, GLint y, GLsizei width, GLsizei height);

static FILE    *log_fp = NULL;

static const char *BooleanToString(GLboolean b)
{
	if(b == GL_FALSE)
		return "GL_FALSE";
	else if(b == GL_TRUE)
		return "GL_TRUE";
	else
		return "OUT OF RANGE FOR BOOLEAN";
}

static const char *FuncToString(GLenum f)
{
	switch (f)
	{
		case GL_ALWAYS:
			return "GL_ALWAYS";
		case GL_NEVER:
			return "GL_NEVER";
		case GL_LEQUAL:
			return "GL_LEQUAL";
		case GL_LESS:
			return "GL_LESS";
		case GL_EQUAL:
			return "GL_EQUAL";
		case GL_GREATER:
			return "GL_GREATER";
		case GL_GEQUAL:
			return "GL_GEQUAL";
		case GL_NOTEQUAL:
			return "GL_NOTEQUAL";
		default:
			return "!!! UNKNOWN !!!";
	}
}

static const char *PrimToString(GLenum mode)
{
	static char     prim[1024];

	if(mode == GL_TRIANGLES)
		strcpy(prim, "GL_TRIANGLES");
	else if(mode == GL_TRIANGLE_STRIP)
		strcpy(prim, "GL_TRIANGLE_STRIP");
	else if(mode == GL_TRIANGLE_FAN)
		strcpy(prim, "GL_TRIANGLE_FAN");
	else if(mode == GL_QUADS)
		strcpy(prim, "GL_QUADS");
	else if(mode == GL_QUAD_STRIP)
		strcpy(prim, "GL_QUAD_STRIP");
	else if(mode == GL_POLYGON)
		strcpy(prim, "GL_POLYGON");
	else if(mode == GL_POINTS)
		strcpy(prim, "GL_POINTS");
	else if(mode == GL_LINES)
		strcpy(prim, "GL_LINES");
	else if(mode == GL_LINE_STRIP)
		strcpy(prim, "GL_LINE_STRIP");
	else if(mode == GL_LINE_LOOP)
		strcpy(prim, "GL_LINE_LOOP");
	else
		sprintf(prim, "0x%x", mode);

	return prim;
}

static const char *CapToString(GLenum cap)
{
	static char     buffer[1024];

	switch (cap)
	{
		case GL_TEXTURE_2D:
			return "GL_TEXTURE_2D";
		case GL_BLEND:
			return "GL_BLEND";
		case GL_DEPTH_TEST:
			return "GL_DEPTH_TEST";
		case GL_CULL_FACE:
			return "GL_CULL_FACE";
		case GL_CLIP_PLANE0:
			return "GL_CLIP_PLANE0";
		case GL_COLOR_ARRAY:
			return "GL_COLOR_ARRAY";
		case GL_TEXTURE_COORD_ARRAY:
			return "GL_TEXTURE_COORD_ARRAY";
		case GL_VERTEX_ARRAY:
			return "GL_VERTEX_ARRAY";
		case GL_ALPHA_TEST:
			return "GL_ALPHA_TEST";
		case GL_STENCIL_TEST:
			return "GL_STENCIL_TEST";
		default:
			sprintf(buffer, "0x%x", cap);
	}

	return buffer;
}

static const char *TypeToString(GLenum t)
{
	switch (t)
	{
		case GL_BYTE:
			return "GL_BYTE";
		case GL_UNSIGNED_BYTE:
			return "GL_UNSIGNED_BYTE";
		case GL_SHORT:
			return "GL_SHORT";
		case GL_UNSIGNED_SHORT:
			return "GL_UNSIGNED_SHORT";
		case GL_INT:
			return "GL_INT";
		case GL_UNSIGNED_INT:
			return "GL_UNSIGNED_INT";
		case GL_FLOAT:
			return "GL_FLOAT";
		case GL_DOUBLE:
			return "GL_DOUBLE";
		default:
			return "!!! UNKNOWN !!!";
	}
}

static void APIENTRY logBindTexture(GLenum target, GLuint texture)
{
	fprintf(log_fp, "glBindTexture( 0x%x, %u )\n", target, texture);
	dllBindTexture(target, texture);
}

static void BlendToName(char *n, GLenum f)
{
	switch (f)
	{
		case GL_ONE:
			strcpy(n, "GL_ONE");
			break;
		case GL_ZERO:
			strcpy(n, "GL_ZERO");
			break;
		case GL_SRC_ALPHA:
			strcpy(n, "GL_SRC_ALPHA");
			break;
		case GL_ONE_MINUS_SRC_ALPHA:
			strcpy(n, "GL_ONE_MINUS_SRC_ALPHA");
			break;
		case GL_DST_COLOR:
			strcpy(n, "GL_DST_COLOR");
			break;
		case GL_ONE_MINUS_DST_COLOR:
			strcpy(n, "GL_ONE_MINUS_DST_COLOR");
			break;
		case GL_DST_ALPHA:
			strcpy(n, "GL_DST_ALPHA");
			break;
		default:
			sprintf(n, "0x%x", f);
	}
}
static void APIENTRY logBlendFunc(GLenum sfactor, GLenum dfactor)
{
	char            sf[128], df[128];

	BlendToName(sf, sfactor);
	BlendToName(df, dfactor);

	fprintf(log_fp, "glBlendFunc( %s, %s )\n", sf, df);
	dllBlendFunc(sfactor, dfactor);
}

static void APIENTRY logClear(GLbitfield mask)
{
	fprintf(log_fp, "glClear( 0x%x = ", mask);

	if(mask & GL_COLOR_BUFFER_BIT)
		fprintf(log_fp, "GL_COLOR_BUFFER_BIT ");
	if(mask & GL_DEPTH_BUFFER_BIT)
		fprintf(log_fp, "GL_DEPTH_BUFFER_BIT ");
	if(mask & GL_STENCIL_BUFFER_BIT)
		fprintf(log_fp, "GL_STENCIL_BUFFER_BIT ");
	if(mask & GL_ACCUM_BUFFER_BIT)
		fprintf(log_fp, "GL_ACCUM_BUFFER_BIT ");

	fprintf(log_fp, ")\n");
	dllClear(mask);
}

static void APIENTRY logClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	fprintf(log_fp, "glClearColor\n");
	dllClearColor(red, green, blue, alpha);
}

static void APIENTRY logClearDepth(GLclampd depth)
{
	fprintf(log_fp, "glClearDepth( %f )\n", (float)depth);
	dllClearDepth(depth);
}

static void APIENTRY logClearStencil(GLint s)
{
	fprintf(log_fp, "glClearStencil( %d )\n", s);
	dllClearStencil(s);
}



#define SIG( x ) fprintf( log_fp, x "\n" )


static void APIENTRY logColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	SIG("glColorMask");
	dllColorMask(red, green, blue, alpha);
}

static void APIENTRY logCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	SIG("glCopyPixels");
	dllCopyPixels(x, y, width, height, type);
}

static void APIENTRY logCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width,
									   GLint border)
{
	SIG("glCopyTexImage1D");
	dllCopyTexImage1D(target, level, internalFormat, x, y, width, border);
}

static void APIENTRY logCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width,
									   GLsizei height, GLint border)
{
	SIG("glCopyTexImage2D");
	dllCopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

static void APIENTRY logCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
	SIG("glCopyTexSubImage1D");
	dllCopyTexSubImage1D(target, level, xoffset, x, y, width);
}

static void APIENTRY logCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
										  GLsizei width, GLsizei height)
{
	SIG("glCopyTexSubImage2D");
	dllCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

static void APIENTRY logCullFace(GLenum mode)
{
	fprintf(log_fp, "glCullFace( %s )\n", (mode == GL_FRONT) ? "GL_FRONT" : "GL_BACK");
	dllCullFace(mode);
}

static void APIENTRY logDeleteTextures(GLsizei n, const GLuint * textures)
{
	SIG("glDeleteTextures");
	dllDeleteTextures(n, textures);
}

static void APIENTRY logDepthFunc(GLenum func)
{
	fprintf(log_fp, "glDepthFunc( %s )\n", FuncToString(func));
	dllDepthFunc(func);
}

static void APIENTRY logDepthMask(GLboolean flag)
{
	fprintf(log_fp, "glDepthMask( %s )\n", BooleanToString(flag));
	dllDepthMask(flag);
}

static void APIENTRY logDepthRange(GLclampd zNear, GLclampd zFar)
{
	fprintf(log_fp, "glDepthRange( %f, %f )\n", (float)zNear, (float)zFar);
	dllDepthRange(zNear, zFar);
}

static void APIENTRY logDisable(GLenum cap)
{
	fprintf(log_fp, "glDisable( %s )\n", CapToString(cap));
	dllDisable(cap);
}

static void APIENTRY logDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	SIG("glDrawArrays");
	dllDrawArrays(mode, first, count);
}

static void APIENTRY logDrawBuffer(GLenum mode)
{
	SIG("glDrawBuffer");
	dllDrawBuffer(mode);
}

static void APIENTRY logDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	fprintf(log_fp, "glDrawElements( %s, %d, %s, MEM )\n", PrimToString(mode), count, TypeToString(type));
	dllDrawElements(mode, count, type, indices);
}

static void APIENTRY logEnable(GLenum cap)
{
	fprintf(log_fp, "glEnable( %s )\n", CapToString(cap));
	dllEnable(cap);
}

static void APIENTRY logFinish(void)
{
	SIG("glFinish");
	dllFinish();
}

static void APIENTRY logFlush(void)
{
	SIG("glFlush");
	dllFlush();
}

static void APIENTRY logFrontFace(GLenum mode)
{
	SIG("glFrontFace");
	dllFrontFace(mode);
}

static void APIENTRY logGenTextures(GLsizei n, GLuint * textures)
{
	SIG("glGenTextures");
	dllGenTextures(n, textures);
}

static void APIENTRY logGetBooleanv(GLenum pname, GLboolean * params)
{
	SIG("glGetBooleanv");
	dllGetBooleanv(pname, params);
}

static GLenum APIENTRY logGetError(void)
{
	SIG("glGetError");
	return dllGetError();
}

static void APIENTRY logGetFloatv(GLenum pname, GLfloat * params)
{
	SIG("glGetFloatv");
	dllGetFloatv(pname, params);
}

static void APIENTRY logGetIntegerv(GLenum pname, GLint * params)
{
	SIG("glGetIntegerv");
	dllGetIntegerv(pname, params);
}

static const GLubyte *APIENTRY logGetString(GLenum name)
{
	SIG("glGetString");
	return dllGetString(name);
}

static void APIENTRY logGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params)
{
	SIG("glGetTexParameterfv");
	dllGetTexParameterfv(target, pname, params);
}

static void APIENTRY logGetTexParameteriv(GLenum target, GLenum pname, GLint * params)
{
	SIG("glGetTexParameteriv");
	dllGetTexParameteriv(target, pname, params);
}

static void APIENTRY logHint(GLenum target, GLenum mode)
{
	fprintf(log_fp, "glHint( 0x%x, 0x%x )\n", target, mode);
	dllHint(target, mode);
}

static GLboolean APIENTRY logIsEnabled(GLenum cap)
{
	SIG("glIsEnabled");
	return dllIsEnabled(cap);
}

static GLboolean APIENTRY logIsTexture(GLuint texture)
{
	SIG("glIsTexture");
	return dllIsTexture(texture);
}

static void APIENTRY logLineWidth(GLfloat width)
{
	SIG("glLineWidth");
	dllLineWidth(width);
}

static void APIENTRY logPolygonMode(GLenum face, GLenum mode)
{
	fprintf(log_fp, "glPolygonMode( 0x%x, 0x%x )\n", face, mode);
	dllPolygonMode(face, mode);
}

static void APIENTRY logPolygonOffset(GLfloat factor, GLfloat units)
{
	SIG("glPolygonOffset");
	dllPolygonOffset(factor, units);
}

static void APIENTRY logReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
	SIG("glReadPixels");
	dllReadPixels(x, y, width, height, format, type, pixels);
}

static void APIENTRY logScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	fprintf(log_fp, "glScissor( %d, %d, %d, %d )\n", x, y, width, height);
	dllScissor(x, y, width, height);
}

static void APIENTRY logStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	SIG("glStencilFunc");
	dllStencilFunc(func, ref, mask);
}

static void APIENTRY logStencilMask(GLuint mask)
{
	SIG("glStencilMask");
	dllStencilMask(mask);
}

static void APIENTRY logStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	SIG("glStencilOp");
	dllStencilOp(fail, zfail, zpass);
}

static void APIENTRY logTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format,
								   GLenum type, const void *pixels)
{
	SIG("glTexImage1D");
	dllTexImage1D(target, level, internalformat, width, border, format, type, pixels);
}
static void APIENTRY logTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
								   GLenum format, GLenum type, const void *pixels)
{
	SIG("glTexImage2D");
	dllTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

static void APIENTRY logTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	fprintf(log_fp, "glTexParameterf( 0x%x, 0x%x, %f )\n", target, pname, param);
	dllTexParameterf(target, pname, param);
}

static void APIENTRY logTexParameterfv(GLenum target, GLenum pname, const GLfloat * params)
{
	SIG("glTexParameterfv");
	dllTexParameterfv(target, pname, params);
}

static void APIENTRY logTexParameteri(GLenum target, GLenum pname, GLint param)
{
	fprintf(log_fp, "glTexParameteri( 0x%x, 0x%x, 0x%x )\n", target, pname, param);
	dllTexParameteri(target, pname, param);
}

static void APIENTRY logTexParameteriv(GLenum target, GLenum pname, const GLint * params)
{
	SIG("glTexParameteriv");
	dllTexParameteriv(target, pname, params);
}

static void APIENTRY logTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type,
									  const void *pixels)
{
	SIG("glTexSubImage1D");
	dllTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

static void APIENTRY logTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
									  GLenum format, GLenum type, const void *pixels)
{
	SIG("glTexSubImage2D");
	dllTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

static void APIENTRY logViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	fprintf(log_fp, "glViewport( %d, %d, %d, %d )\n", x, y, width, height);
	dllViewport(x, y, width, height);
}

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.  This
** is only called during a hard shutdown of the OGL subsystem (e.g. vid_restart).
*/
// *INDENT-OFF*
void QGL_Shutdown(void)
{
	ri.Printf(PRINT_ALL, "...shutting down QGL\n");

	qglBindTexture               = NULL;
	qglBlendFunc                 = NULL;
	qglClear                     = NULL;
	qglClearColor                = NULL;
	qglClearDepth                = NULL;
	qglClearStencil              = NULL;
	qglColorMask                 = NULL;
	qglCopyPixels                = NULL;
	qglCopyTexImage1D            = NULL;
	qglCopyTexImage2D            = NULL;
	qglCopyTexSubImage1D         = NULL;
	qglCopyTexSubImage2D         = NULL;
	qglCullFace                  = NULL;
	qglDeleteTextures            = NULL;
	qglDepthFunc                 = NULL;
	qglDepthMask                 = NULL;
	qglDepthRange                = NULL;
	qglDisable                   = NULL;
	qglDrawArrays                = NULL;
	qglDrawBuffer                = NULL;
	qglDrawElements              = NULL;
	qglEnable                    = NULL;
	qglFinish                    = NULL;
	qglFlush                     = NULL;
	qglFrontFace                 = NULL;
	qglGenTextures               = NULL;
	qglGetBooleanv               = NULL;
	qglGetError                  = NULL;
	qglGetFloatv                 = NULL;
	qglGetIntegerv               = NULL;
	qglGetString                 = NULL;
	qglGetTexParameterfv         = NULL;
	qglGetTexParameteriv         = NULL;
	qglHint                      = NULL;
	qglIsEnabled                 = NULL;
	qglIsTexture                 = NULL;
	qglLineWidth                 = NULL;
	qglPolygonMode               = NULL;
	qglPolygonOffset             = NULL;
	qglReadPixels                = NULL;
	qglScissor                   = NULL;
	qglStencilFunc               = NULL;
	qglStencilMask               = NULL;
	qglStencilOp                 = NULL;
	qglTexImage1D                = NULL;
	qglTexImage2D                = NULL;
	qglTexParameterf             = NULL;
	qglTexParameterfv            = NULL;
	qglTexParameteri             = NULL;
	qglTexParameteriv            = NULL;
	qglTexSubImage1D             = NULL;
	qglTexSubImage2D             = NULL;
	qglViewport                  = NULL;
}
// *INDENT-ON*

#define GPA(a) SDL_GL_GetProcAddress(a)

qboolean GLimp_sdl_init_video(void)
{
	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		ri.Printf(PRINT_ALL, "Calling SDL_Init(SDL_INIT_VIDEO)...\n");
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) == -1)
		{
			ri.Printf(PRINT_ALL, "SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
			return qfalse;
		}
		ri.Printf(PRINT_ALL, "SDL_Init(SDL_INIT_VIDEO) passed.\n");
	}

	return qtrue;
}

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to
** the appropriate GL stuff.  In Windows this means doing a
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
*/
// *INDENT-OFF*
int QGL_Init()
{
	ri.Printf(PRINT_ALL, "...initializing QGL\n");

	if(GLimp_sdl_init_video() == qfalse)
		return qfalse;

	qglBindTexture               = dllBindTexture = GPA( "glBindTexture" );
	qglBlendFunc                 = dllBlendFunc = GPA( "glBlendFunc" );
	qglClear                     = dllClear = GPA( "glClear" );
	qglClearColor                = dllClearColor = GPA( "glClearColor" );
	qglClearDepth                = dllClearDepth = GPA( "glClearDepth" );
	qglClearStencil              = dllClearStencil = GPA( "glClearStencil" );
	qglColorMask                 = dllColorMask = GPA( "glColorMask" );
	qglCopyPixels                = dllCopyPixels = GPA( "glCopyPixels" );
	qglCopyTexImage1D            = dllCopyTexImage1D = GPA( "glCopyTexImage1D" );
	qglCopyTexImage2D            = dllCopyTexImage2D = GPA( "glCopyTexImage2D" );
	qglCopyTexSubImage1D         = dllCopyTexSubImage1D = GPA( "glCopyTexSubImage1D" );
	qglCopyTexSubImage2D         = dllCopyTexSubImage2D = GPA( "glCopyTexSubImage2D" );
	qglCullFace                  = dllCullFace = GPA( "glCullFace" );
	qglDeleteTextures            = dllDeleteTextures = GPA( "glDeleteTextures" );
	qglDepthFunc                 = dllDepthFunc = GPA( "glDepthFunc" );
	qglDepthMask                 = dllDepthMask = GPA( "glDepthMask" );
	qglDepthRange                = dllDepthRange = GPA( "glDepthRange" );
	qglDisable                   = dllDisable = GPA( "glDisable" );
	qglDrawArrays                = dllDrawArrays = GPA( "glDrawArrays" );
	qglDrawBuffer                = dllDrawBuffer = GPA( "glDrawBuffer" );
	qglDrawElements              = dllDrawElements = GPA( "glDrawElements" );
	qglEnable                    = dllEnable                    = GPA( "glEnable" );
	qglFinish                    = dllFinish                    = GPA( "glFinish" );
	qglFlush                     = dllFlush                     = GPA( "glFlush" );
	qglFrontFace                 = dllFrontFace                 = GPA( "glFrontFace" );
	qglGenTextures               = dllGenTextures               = GPA( "glGenTextures" );
	qglGetBooleanv               = dllGetBooleanv               = GPA( "glGetBooleanv" );
	qglGetError                  = dllGetError                  = GPA( "glGetError" );
	qglGetFloatv                 = dllGetFloatv                 = GPA( "glGetFloatv" );
	qglGetIntegerv               = dllGetIntegerv               = GPA( "glGetIntegerv" );
	qglGetString                 = dllGetString                 = GPA( "glGetString" );
	qglGetTexParameterfv         = dllGetTexParameterfv         = GPA( "glGetTexParameterfv" );
	qglGetTexParameteriv         = dllGetTexParameteriv         = GPA( "glGetTexParameteriv" );
	qglHint                      = dllHint                      = GPA( "glHint" );
	qglIsEnabled                 = dllIsEnabled                 = GPA( "glIsEnabled" );
	qglIsTexture                 = dllIsTexture                 = GPA( "glIsTexture" );
	qglLineWidth                 = dllLineWidth                 = GPA( "glLineWidth" );
	qglPolygonMode               = dllPolygonMode               = GPA( "glPolygonMode" );
	qglPolygonOffset             = dllPolygonOffset             = GPA( "glPolygonOffset" );
	qglReadPixels                = dllReadPixels                = GPA( "glReadPixels" );
	qglScissor                   = dllScissor                   = GPA( "glScissor" );
	qglStencilFunc               = dllStencilFunc               = GPA( "glStencilFunc" );
	qglStencilMask               = dllStencilMask               = GPA( "glStencilMask" );
	qglStencilOp                 = dllStencilOp                 = GPA( "glStencilOp" );
	qglTexImage1D                = dllTexImage1D                = GPA( "glTexImage1D" );
	qglTexImage2D                = dllTexImage2D                = GPA( "glTexImage2D" );
	qglTexParameterf             = dllTexParameterf             = GPA( "glTexParameterf" );
	qglTexParameterfv            = dllTexParameterfv            = GPA( "glTexParameterfv" );
	qglTexParameteri             = dllTexParameteri             = GPA( "glTexParameteri" );
	qglTexParameteriv            = dllTexParameteriv            = GPA( "glTexParameteriv" );
	qglTexSubImage1D             = dllTexSubImage1D             = GPA( "glTexSubImage1D" );
	qglTexSubImage2D             = dllTexSubImage2D             = GPA( "glTexSubImage2D" );
	qglViewport                  = dllViewport                  = GPA( "glViewport" );

	// check logging
	QGL_EnableLogging(r_logFile->integer);

	return qtrue;
}
// *INDENT-ON*

// *INDENT-OFF*
void QGL_EnableLogging(int enable)
{
	// fixed for new countdown
	static qboolean isEnabled = qfalse;	// init

	// return if we're already active
	if(isEnabled && enable)
	{
		// decrement log counter and stop if it has reached 0
		ri.Cvar_Set("r_logFile", va("%d", r_logFile->integer - 1));
		if(r_logFile->integer)
		{
			return;
		}
		enable = qfalse;
	}

	// return if we're already disabled
	if(!enable && !isEnabled)
		return;

	isEnabled = enable;

	// old code starts here
	if(enable)
	{
		if(!log_fp)
		{
			struct tm      *newtime;
			time_t          aclock;
			char            buffer[1024];
			cvar_t         *basedir;

			time(&aclock);
			newtime = localtime(&aclock);

			asctime(newtime);

			basedir = ri.Cvar_Get("fs_basepath", "", 0);	// FIXME: userdir?
			assert(basedir);
			Com_sprintf(buffer, sizeof(buffer), "%s/gl.log", basedir->string);
			log_fp = fopen(buffer, "wt");
			assert(log_fp);
			ri.Printf(PRINT_ALL, "QGL_EnableLogging(%d): writing %s\n", r_logFile->integer, buffer);

			fprintf(log_fp, "%s\n", asctime(newtime));
		}

		qglBindTexture               = logBindTexture;
		qglBlendFunc                 = logBlendFunc;
		qglClear                     = logClear;
		qglClearColor                = logClearColor;
		qglClearDepth                = logClearDepth;
		qglClearStencil              = logClearStencil;
		qglColorMask                 = logColorMask;
		qglCopyPixels                = logCopyPixels;
		qglCopyTexImage1D            = logCopyTexImage1D;
		qglCopyTexImage2D            = logCopyTexImage2D;
		qglCopyTexSubImage1D         = logCopyTexSubImage1D;
		qglCopyTexSubImage2D         = logCopyTexSubImage2D;
		qglCullFace                  = logCullFace;
		qglDeleteTextures            = logDeleteTextures;
		qglDepthFunc                 = logDepthFunc;
		qglDepthMask                 = logDepthMask;
		qglDepthRange                = logDepthRange;
		qglDisable                   = logDisable;
		qglDrawArrays                = logDrawArrays;
		qglDrawBuffer                = logDrawBuffer;
		qglDrawElements              = logDrawElements;
		qglEnable                    = logEnable;
		qglFinish                    = logFinish;
		qglFlush                     = logFlush;
		qglFrontFace                 = logFrontFace;
		qglGenTextures               = logGenTextures;
		qglGetBooleanv               = logGetBooleanv;
		qglGetError                  = logGetError;
		qglGetFloatv                 = logGetFloatv;
		qglGetIntegerv               = logGetIntegerv;
		qglGetString                 = logGetString;
		qglGetTexParameterfv         = logGetTexParameterfv;
		qglGetTexParameteriv         = logGetTexParameteriv;
		qglHint                      = logHint;
		qglIsEnabled                 = logIsEnabled;
		qglIsTexture                 = logIsTexture;
		qglLineWidth                 = logLineWidth;
		qglPolygonMode               = logPolygonMode;
		qglPolygonOffset             = logPolygonOffset;
		qglReadPixels                = logReadPixels;
		qglScissor                   = logScissor;
		qglStencilFunc               = logStencilFunc;
		qglStencilMask               = logStencilMask;
		qglStencilOp                 = logStencilOp;
		qglTexImage1D                = logTexImage1D;
		qglTexImage2D                = logTexImage2D;
		qglTexParameterf             = logTexParameterf;
		qglTexParameterfv            = logTexParameterfv;
		qglTexParameteri             = logTexParameteri;
		qglTexParameteriv            = logTexParameteriv;
		qglTexSubImage1D             = logTexSubImage1D;
		qglTexSubImage2D             = logTexSubImage2D;
		qglViewport                  = logViewport;
	}
	else
	{
		if(log_fp)
		{
			fprintf(log_fp, "*** CLOSING LOG ***\n");
			fclose(log_fp);
			log_fp = NULL;
		}
		qglBindTexture               = dllBindTexture;
		qglBlendFunc                 = dllBlendFunc;
		qglClear                     = dllClear;
		qglClearColor                = dllClearColor;
		qglClearDepth                = dllClearDepth;
		qglClearStencil              = dllClearStencil;
		qglColorMask                 = dllColorMask;
		qglCopyPixels                = dllCopyPixels;
		qglCopyTexImage1D            = dllCopyTexImage1D;
		qglCopyTexImage2D            = dllCopyTexImage2D;
		qglCopyTexSubImage1D         = dllCopyTexSubImage1D;
		qglCopyTexSubImage2D         = dllCopyTexSubImage2D;
		qglCullFace                  = dllCullFace;
		qglDeleteTextures            = dllDeleteTextures;
		qglDepthFunc                 = dllDepthFunc;
		qglDepthMask                 = dllDepthMask;
		qglDepthRange                = dllDepthRange;
		qglDisable                   = dllDisable;
		qglDrawArrays                = dllDrawArrays;
		qglDrawBuffer                = dllDrawBuffer;
		qglDrawElements              = dllDrawElements;
		qglEnable                    = dllEnable;
		qglFinish                    = dllFinish;
		qglFlush                     = dllFlush;
		qglFrontFace                 = dllFrontFace;
		qglGenTextures               = dllGenTextures;
		qglGetBooleanv               = dllGetBooleanv;
		qglGetError                  = dllGetError;
		qglGetFloatv                 = dllGetFloatv;
		qglGetIntegerv               = dllGetIntegerv;
		qglGetString                 = dllGetString;
		qglGetTexParameterfv         = dllGetTexParameterfv;
		qglGetTexParameteriv         = dllGetTexParameteriv;
		qglHint                      = dllHint;
		qglIsEnabled                 = dllIsEnabled;
		qglIsTexture                 = dllIsTexture;
		qglLineWidth                 = dllLineWidth;
		qglPolygonMode               = dllPolygonMode;
		qglPolygonOffset             = dllPolygonOffset;
		qglReadPixels                = dllReadPixels;
		qglScissor                   = dllScissor;
		qglStencilFunc               = dllStencilFunc;
		qglStencilMask               = dllStencilMask;
		qglStencilOp                 = dllStencilOp;
		qglTexImage1D                = dllTexImage1D;
		qglTexImage2D                = dllTexImage2D;
		qglTexParameterf             = dllTexParameterf;
		qglTexParameterfv            = dllTexParameterfv;
		qglTexParameteri             = dllTexParameteri;
		qglTexParameteriv            = dllTexParameteriv;
		qglTexSubImage1D             = dllTexSubImage1D ;
		qglTexSubImage2D             = dllTexSubImage2D;
		qglViewport                  = dllViewport;
	}
}
// *INDENT-ON*


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment(char *comment)
{
	if(log_fp)
	{
		fprintf(log_fp, "%s", comment);
	}
}

#ifdef _MSC_VER
#pragma warning (default : 4113 4133 4047 )
#endif
