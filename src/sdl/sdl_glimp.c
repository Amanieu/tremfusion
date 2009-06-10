/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#ifdef SMP
#	include <SDL_thread.h>
#	ifdef SDL_VIDEO_DRIVER_X11
#		include <X11/Xlib.h>
#	endif
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"
#include "sdl_icon.h"
#include "SDL_syswm.h"

/* Just hack it for now. */
#ifdef MACOS_X
#include <OpenGL/OpenGL.h>
typedef CGLContextObj QGLContext;

static QGLContext opengl_context;

static void GLimp_GetCurrentContext(void)
{
	opengl_context = CGLGetCurrentContext();
}

#ifdef SMP
static void GLimp_SetCurrentContext(qboolean enable)
{
	if(enable)
	{
		CGLSetCurrentContext(opengl_context);
	}
	else
	{
		CGLSetCurrentContext(NULL);
	}
}
#endif

#elif SDL_VIDEO_DRIVER_X11

#include <GL/glx.h>
typedef struct
{
	GLXContext      ctx;
	Display        *dpy;
	GLXDrawable     drawable;
} QGLContext_t;
typedef QGLContext_t QGLContext;

static QGLContext opengl_context;

static void GLimp_GetCurrentContext(void)
{
	opengl_context.ctx = glXGetCurrentContext();
	opengl_context.dpy = glXGetCurrentDisplay();
	opengl_context.drawable = glXGetCurrentDrawable();
}

#ifdef SMP
static void GLimp_SetCurrentContext(qboolean enable)
{
	if(enable)
		glXMakeCurrent(opengl_context.dpy, opengl_context.drawable, opengl_context.ctx);
	else
		glXMakeCurrent(opengl_context.dpy, None, NULL);
}
#endif

#elif WIN32

typedef struct
{
	HDC             hDC;		// handle to device context
	HGLRC           hGLRC;		// handle to GL rendering context
} QGLContext_t;
typedef QGLContext_t QGLContext;

static QGLContext opengl_context;

static void GLimp_GetCurrentContext(void)
{
	SDL_SysWMinfo   info;

	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info))
	{
		ri.Printf(PRINT_WARNING, "Failed to obtain HWND from SDL (InputRegistry)");
		return;
	}

	opengl_context.hDC = GetDC(info.window);
	opengl_context.hGLRC = info.hglrc;
}

#ifdef SMP
static void GLimp_SetCurrentContext(qboolean enable)
{
	if(enable)
		wglMakeCurrent(opengl_context.hDC, opengl_context.hGLRC);
	else
		wglMakeCurrent(opengl_context.hDC, NULL);
}
#endif
#else
static void GLimp_GetCurrentContext(void)
{
}

#ifdef SMP
static void GLimp_SetCurrentContext(qboolean enable)
{
}
#endif
#endif


#ifdef SMP
/*
===========================================================

SMP acceleration

===========================================================
*/

/*
 * I have no idea if this will even work...most platforms don't offer
 * thread-safe OpenGL libraries, and it looks like the original Linux
 * code counted on each thread claiming the GL context with glXMakeCurrent(),
 * which you can't currently do in SDL. We'll just have to hope for the best.
 */

static SDL_mutex *smpMutex = NULL;
static SDL_cond *renderCommandsEvent = NULL;
static SDL_cond *renderCompletedEvent = NULL;
static void     (*renderThreadFunction) (void) = NULL;
static SDL_Thread *renderThread = NULL;

/*
===============
GLimp_RenderThreadWrapper
===============
*/
static int GLimp_RenderThreadWrapper(void *arg)
{
	// These printfs cause race conditions which mess up the console output
	Com_Printf("Render thread starting\n");

	renderThreadFunction();

	GLimp_SetCurrentContext(qfalse);

	Com_Printf("Render thread terminating\n");

	return 0;
}

/*
===============
GLimp_SpawnRenderThread
===============
*/
qboolean GLimp_SpawnRenderThread(void (*function) (void))
{
	static qboolean warned = qfalse;

	if(!warned)
	{
		Com_Printf("WARNING: You enable r_smp at your own risk!\n");
		warned = qtrue;
	}

#if !defined(MACOS_X) && !defined(WIN32) && !defined (SDL_VIDEO_DRIVER_X11)
	return qfalse;				/* better safe than sorry for now. */
#endif

	if(renderThread != NULL)	/* hopefully just a zombie at this point... */
	{
		Com_Printf("Already a render thread? Trying to clean it up...\n");
		GLimp_ShutdownRenderThread();
	}

	smpMutex = SDL_CreateMutex();
	if(smpMutex == NULL)
	{
		Com_Printf("smpMutex creation failed: %s\n", SDL_GetError());
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCommandsEvent = SDL_CreateCond();
	if(renderCommandsEvent == NULL)
	{
		Com_Printf("renderCommandsEvent creation failed: %s\n", SDL_GetError());
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCompletedEvent = SDL_CreateCond();
	if(renderCompletedEvent == NULL)
	{
		Com_Printf("renderCompletedEvent creation failed: %s\n", SDL_GetError());
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderThreadFunction = function;
	renderThread = SDL_CreateThread(GLimp_RenderThreadWrapper, NULL);
	if(renderThread == NULL)
	{
		ri.Printf(PRINT_ALL, "SDL_CreateThread() returned %s", SDL_GetError());
		GLimp_ShutdownRenderThread();
		return qfalse;
	}
	else
	{
		// tma 01/09/07: don't think this is necessary anyway?
		//
		// !!! FIXME: No detach API available in SDL!
		//ret = pthread_detach( renderThread );
		//if ( ret ) {
		//ri.Printf( PRINT_ALL, "pthread_detach returned %d: %s", ret, strerror( ret ) );
		//}
	}

	return qtrue;
}

/*
===============
GLimp_ShutdownRenderThread
===============
*/
void GLimp_ShutdownRenderThread(void)
{
	if(renderThread != NULL)
	{
		SDL_WaitThread(renderThread, NULL);
		renderThread = NULL;
	}

	if(smpMutex != NULL)
	{
		SDL_DestroyMutex(smpMutex);
		smpMutex = NULL;
	}

	if(renderCommandsEvent != NULL)
	{
		SDL_DestroyCond(renderCommandsEvent);
		renderCommandsEvent = NULL;
	}

	if(renderCompletedEvent != NULL)
	{
		SDL_DestroyCond(renderCompletedEvent);
		renderCompletedEvent = NULL;
	}

	renderThreadFunction = NULL;
}

static volatile void *smpData = NULL;
static volatile qboolean smpDataReady;

/*
===============
GLimp_RendererSleep
===============
*/
void           *GLimp_RendererSleep(void)
{
	void           *data = NULL;

	GLimp_SetCurrentContext(qfalse);

	SDL_LockMutex(smpMutex);
	{
		smpData = NULL;
		smpDataReady = qfalse;

		// after this, the front end can exit GLimp_FrontEndSleep
		SDL_CondSignal(renderCompletedEvent);

		while(!smpDataReady)
			SDL_CondWait(renderCommandsEvent, smpMutex);

		data = (void *)smpData;
	}
	SDL_UnlockMutex(smpMutex);

	GLimp_SetCurrentContext(qtrue);

	return data;
}

/*
===============
GLimp_FrontEndSleep
===============
*/
void GLimp_FrontEndSleep(void)
{
	SDL_LockMutex(smpMutex);
	{
		while(smpData)
			SDL_CondWait(renderCompletedEvent, smpMutex);
	}
	SDL_UnlockMutex(smpMutex);

	GLimp_SetCurrentContext(qtrue);
}

/*
===============
GLimp_WakeRenderer
===============
*/
void GLimp_WakeRenderer(void *data)
{
	GLimp_SetCurrentContext(qfalse);

	SDL_LockMutex(smpMutex);
	{
		assert(smpData == NULL);
		smpData = data;
		smpDataReady = qtrue;

		// after this, the renderer can continue through GLimp_RendererSleep
		SDL_CondSignal(renderCommandsEvent);
	}
	SDL_UnlockMutex(smpMutex);
}

#else

// No SMP - stubs
void GLimp_RenderThreadWrapper(void *arg)
{
}

qboolean GLimp_SpawnRenderThread(void (*function) (void))
{
	ri.Printf(PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n");
	return qfalse;
}

void GLimp_ShutdownRenderThread(void)
{
}

void           *GLimp_RendererSleep(void)
{
	return NULL;
}

void GLimp_FrontEndSleep(void)
{
}

void GLimp_WakeRenderer(void *data)
{
}

#endif


typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

static SDL_Surface *screen = NULL;

cvar_t         *r_sdlDriver;

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown(void)
{
	IN_Shutdown();

	QGL_Shutdown();

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	screen = NULL;

#if defined(SMP)
	if(renderThread != NULL)
	{
		Com_Printf("Destroying renderer thread...\n");
		GLimp_ShutdownRenderThread();
	}
#endif

	Com_Memset(&glConfig, 0, sizeof(glConfig));
	Com_Memset(&glState, 0, sizeof(glState));
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes(const void *a, const void *b)
{
	const float     ASPECT_EPSILON = 0.001f;
	SDL_Rect       *modeA = *(SDL_Rect **) a;
	SDL_Rect       *modeB = *(SDL_Rect **) b;
	float           aspectDiffA = fabs(((float)modeA->w / (float)modeA->h) - displayAspect);
	float           aspectDiffB = fabs(((float)modeB->w / (float)modeB->h) - displayAspect);
	float           aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if(aspectDiffsDiff > ASPECT_EPSILON)
		return 1;
	else if(aspectDiffsDiff < -ASPECT_EPSILON)
		return -1;
	else
	{
		if(modeA->w == modeB->w)
			return modeA->h - modeB->h;
		else
			return modeA->w - modeB->w;
	}
}

/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes(void)
{
	char            buf[MAX_STRING_CHARS] = { 0 };
	SDL_Rect      **modes;
	int             numModes;
	int             i;

	modes = SDL_ListModes(NULL, SDL_OPENGL | SDL_FULLSCREEN);

	if(!modes)
	{
		ri.Printf(PRINT_WARNING, "Can't get list of available modes\n");
		return;
	}

	if(modes == (SDL_Rect **) - 1)
	{
		ri.Printf(PRINT_ALL, "Display supports any resolution\n");
		return;					// can set any resolution
	}

	for(numModes = 0; modes[numModes]; numModes++);

	if(numModes > 1)
		qsort(modes + 1, numModes - 1, sizeof(SDL_Rect *), GLimp_CompareModes);

	for(i = 0; i < numModes; i++)
	{
		const char     *newModeString = va("%ux%u ", modes[i]->w, modes[i]->h);

		if(strlen(newModeString) < (int)sizeof(buf) - strlen(buf))
			Q_strcat(buf, sizeof(buf), newModeString);
		else
			ri.Printf(PRINT_WARNING, "Skipping mode %ux%x, buffer too small\n", modes[i]->w, modes[i]->h);
	}

	if(*buf)
	{
		buf[strlen(buf) - 1] = 0;
		ri.Printf(PRINT_ALL, "Available modes: '%s'\n", buf);
		ri.Cvar_Set("r_availableModes", buf);
	}
}

/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode(int mode, qboolean fullscreen)
{
	const char     *glstring;
	int             sdlcolorbits;
	int             colorbits, depthbits, stencilbits;
	int             tcolorbits, tdepthbits, tstencilbits;
	int             i = 0;
	SDL_Surface    *vidscreen = NULL;
	Uint32          flags = SDL_OPENGL;
	const SDL_VideoInfo *videoInfo;

	ri.Printf(PRINT_ALL, "Initializing OpenGL display\n");

	if(displayAspect == 0.0f)
	{
		// Guess the display aspect ratio through the desktop resolution
		// by assuming (relatively safely) that it is set at or close to
		// the display's native aspect ratio
		videoInfo = SDL_GetVideoInfo();
		displayAspect = (float)videoInfo->current_w / (float)videoInfo->current_h;

		ri.Printf(PRINT_ALL, "Estimated display aspect: %.3f\n", displayAspect);
	}

	ri.Printf(PRINT_ALL, "...setting mode %d:", mode);

	if(!R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode))
	{
		ri.Printf(PRINT_ALL, " invalid mode\n");
		return RSERR_INVALID_MODE;
	}
	ri.Printf(PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	if(fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
		glConfig.isFullscreen = qfalse;

	if(!r_colorbits->value)
		colorbits = 24;
	else
		colorbits = r_colorbits->value;

	if(!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;
	stencilbits = r_stencilbits->value;

	for(i = 0; i < 16; i++)
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2:
					if(colorbits == 24)
						colorbits = 16;
					break;
				case 1:
					if(depthbits == 24)
						depthbits = 16;
					else if(depthbits == 16)
						depthbits = 8;
				case 3:
					if(stencilbits == 24)
						stencilbits = 16;
					else if(stencilbits == 16)
						stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if((i % 4) == 3)
		{						// reduce colorbits
			if(tcolorbits == 24)
				tcolorbits = 16;
		}

		if((i % 4) == 2)
		{						// reduce depthbits
			if(tdepthbits == 24)
				tdepthbits = 16;
			else if(tdepthbits == 16)
				tdepthbits = 8;
		}

		if((i % 4) == 1)
		{						// reduce stencilbits
			if(tstencilbits == 24)
				tstencilbits = 16;
			else if(tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		sdlcolorbits = 4;
		if(tcolorbits == 24)
			sdlcolorbits = 8;

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, sdlcolorbits);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, sdlcolorbits);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, sdlcolorbits);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, tdepthbits);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, tstencilbits);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		if(SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_swapInterval->integer) < 0)
			ri.Printf(PRINT_ALL, "r_swapInterval requires libSDL >= 1.2.10\n");

#ifdef USE_ICON
		{
			SDL_Surface    *icon = SDL_CreateRGBSurfaceFrom((void *)CLIENT_WINDOW_ICON.pixel_data,
															CLIENT_WINDOW_ICON.width,
															CLIENT_WINDOW_ICON.height,
															CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
															CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
															0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
															0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
				);

			SDL_WM_SetIcon(icon, NULL);
			SDL_FreeSurface(icon);
		}
#endif

		SDL_WM_SetCaption(CLIENT_WINDOW_TITLE, CLIENT_WINDOW_MIN_TITLE);
		SDL_ShowCursor(0);

		if(!(vidscreen = SDL_SetVideoMode(glConfig.vidWidth, glConfig.vidHeight, colorbits, flags)))
		{
			ri.Printf(PRINT_DEVELOPER, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
			continue;
		}

		GLimp_GetCurrentContext();

		ri.Printf(PRINT_ALL, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
				  sdlcolorbits, sdlcolorbits, sdlcolorbits, tdepthbits, tstencilbits);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	// try to initialize an OpenGL 3.0 context
#if 0//defined(WIN32)
	qwglCreateContextAttribsARB = SDL_GL_GetProcAddress("wglCreateContextAttribsARB");
	if(qwglCreateContextAttribsARB)
	{
		int             attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB,
			3,
			WGL_CONTEXT_MINOR_VERSION_ARB,
			1,
			WGL_CONTEXT_FLAGS_ARB,
			0,//WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
			0
		};

		ri.Printf(PRINT_ALL, "Initializing OpenGL 3.1 context...");

		opengl_context.hGLRC = qwglCreateContextAttribsARB(opengl_context.hDC, opengl_context.hGLRC, attribs);
		if(wglMakeCurrent(opengl_context.hDC, opengl_context.hGLRC))
		{
			ri.Printf(PRINT_ALL, " done\n");
			glConfig.driverType = GLDRV_OPENGL3;
		}
		else
		{
			ri.Printf(PRINT_ALL, " failed\n");
		}
	}
#elif 0 //defined(__linux__)

	// TODO

	/*
// GLX_ARB_create_context
#ifndef GLX_ARB_create_context
#define GLX_CONTEXT_DEBUG_BIT_ARB          0x00000001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define GLX_CONTEXT_MAJOR_VERSION_ARB      0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB      0x2092
#define GLX_CONTEXT_FLAGS_ARB              0x2094

extern GLXContext	(APIENTRY * qglXCreateContextAttribsARB) (Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);
*/

	qglXCreateContextAttribsARB = SDL_GL_GetProcAddress("glXCreateContextAttribsARB");
	if(qglXCreateContextAttribsARB)
	{
		int             attribs[3];

		ri.Printf(PRINT_ALL, "Initializing OpenGL 3.0 context...");

		attribs[0] = WGL_CONTEXT_MAJOR_VERSION_ARB;
		attribs[1] = 3;
		attribs[2] = 0;			//terminate first pair

		opengl_context->hGLRC = qglXCreateContextAttribsARB(opengl_context->, attribs);
		if(wglMakeCurrent(opengl_context->hDC, opengl_context->hGLRC))
		{
			ri.Printf(PRINT_ALL, " done\n");
			glConfig.driverType = GLDRV_OPENGL3;
		}
		else
		{
			ri.Printf(PRINT_ALL, " failed\n");
		}
	}
#endif

	QGL_Init();

	GLimp_DetectAvailableModes();

	if(!vidscreen)
	{
		ri.Printf(PRINT_ALL, "Couldn't get a visual\n");
		return RSERR_INVALID_MODE;
	}

	screen = vidscreen;

	glstring = (char *)qglGetString(GL_RENDERER);
	ri.Printf(PRINT_ALL, "GL_RENDERER: %s\n", glstring);

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(int mode, qboolean fullscreen)
{
	rserr_t         err;

	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		char            driverName[64];

		ri.Printf(PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO )... ");
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) == -1)
		{
			ri.Printf(PRINT_ALL, "SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) FAILED (%s)\n", SDL_GetError());
			return qfalse;
		}

		SDL_VideoDriverName(driverName, sizeof(driverName) - 1);
		ri.Printf(PRINT_ALL, "SDL using driver \"%s\"\n", driverName);
		Cvar_Set("r_sdlDriver", driverName);
	}

	if(fullscreen && Cvar_VariableIntegerValue("in_nograb"))
	{
		ri.Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set("r_fullscreen", "0");
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	err = GLimp_SetMode(mode, fullscreen);

	switch (err)
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
			return qfalse;
		default:
			break;
	}

	return qtrue;
}

/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions(void)
{
	ri.Printf(PRINT_ALL, "Initializing OpenGL extensions\n");

	// GL_ARB_multitexture
	qglActiveTextureARB = NULL;
	//if(Q_stristr(glConfig.extensions_string, "GL_ARB_multitexture"))
	{
		qglActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glActiveTextureARB");
		if(qglActiveTextureARB)
		{
			qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glConfig.maxTextureUnits);

			if(glConfig.maxTextureUnits > 1)
			{
				ri.Printf(PRINT_ALL, "...using GL_ARB_multitexture\n");
			}
			else
			{
				qglActiveTextureARB = NULL;
				ri.Error(ERR_FATAL, "...not using GL_ARB_multitexture, < 2 texture units\n");
			}
		}
	}
	/*
	   else
	   {
	   ri.Error(ERR_FATAL, "...GL_ARB_multitexture not found\n");
	   }
	 */

	// GL_ARB_depth_texture
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_depth_texture"))
	{
		ri.Printf(PRINT_ALL, "...using GL_ARB_depth_texture\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_depth_texture not found\n");
	}

	// GL_ARB_texture_cube_map
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_texture_cube_map"))
	{
		qglGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.maxCubeMapTextureSize);
		ri.Printf(PRINT_ALL, "...using GL_ARB_texture_cube_map\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_texture_cube_map not found\n");
	}

	// GL_ARB_vertex_program
	qglVertexAttrib4fARB = NULL;
	qglVertexAttrib4fvARB = NULL;
	qglVertexAttribPointerARB = NULL;
	qglEnableVertexAttribArrayARB = NULL;
	qglDisableVertexAttribArrayARB = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_vertex_program"))
	{
		qglVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC) SDL_GL_GetProcAddress("glVertexAttrib4fARB");
		qglVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC) SDL_GL_GetProcAddress("glVertexAttrib4fvARB");
		qglVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) SDL_GL_GetProcAddress("glVertexAttribPointerARB");
		qglEnableVertexAttribArrayARB =
			(PFNGLENABLEVERTEXATTRIBARRAYARBPROC) SDL_GL_GetProcAddress("glEnableVertexAttribArrayARB");
		qglDisableVertexAttribArrayARB =
			(PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) SDL_GL_GetProcAddress("glDisableVertexAttribArrayARB");
		ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_program\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_vertex_program not found\n");
	}

	// GL_ARB_vertex_buffer_object
	qglBindBufferARB = NULL;
	qglDeleteBuffersARB = NULL;
	qglGenBuffersARB = NULL;
	qglIsBufferARB = NULL;
	qglBufferDataARB = NULL;
	qglBufferSubDataARB = NULL;
	qglGetBufferSubDataARB = NULL;
	qglGetBufferParameterivARB = NULL;
	qglGetBufferPointervARB = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_vertex_buffer_object"))
	{
		qglBindBufferARB = (PFNGLBINDBUFFERARBPROC) SDL_GL_GetProcAddress("glBindBufferARB");
		qglDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC) SDL_GL_GetProcAddress("glDeleteBuffersARB");
		qglGenBuffersARB = (PFNGLGENBUFFERSARBPROC) SDL_GL_GetProcAddress("glGenBuffersARB");
		qglIsBufferARB = (PFNGLISBUFFERARBPROC) SDL_GL_GetProcAddress("glIsBufferARB");
		qglBufferDataARB = (PFNGLBUFFERDATAARBPROC) SDL_GL_GetProcAddress("glBufferDataARB");
		qglBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC) SDL_GL_GetProcAddress("glBufferSubDataARB");
		qglGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC) SDL_GL_GetProcAddress("glGetBufferSubDataARB");
		qglGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC) SDL_GL_GetProcAddress("glGetBufferParameterivARB");
		qglGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC) SDL_GL_GetProcAddress("glGetBufferPointervARB");
		ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_buffer_object\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_vertex_buffer_object not found\n");
	}

	// GL_ARB_occlusion_query
	glConfig.occlusionQueryAvailable = qfalse;
	glConfig.occlusionQueryBits = 0;
	qglGenQueriesARB = NULL;
	qglDeleteQueriesARB = NULL;
	qglIsQueryARB = NULL;
	qglBeginQueryARB = NULL;
	qglEndQueryARB = NULL;
	qglGetQueryivARB = NULL;
	qglGetQueryObjectivARB = NULL;
	qglGetQueryObjectuivARB = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_occlusion_query"))
	{
		if(r_ext_occlusion_query->value)
		{
			qglGenQueriesARB = (PFNGLGENQUERIESARBPROC) SDL_GL_GetProcAddress("glGenQueriesARB");
			qglDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC) SDL_GL_GetProcAddress("glDeleteQueriesARB");
			qglIsQueryARB = (PFNGLISQUERYARBPROC) SDL_GL_GetProcAddress("glIsQueryARB");
			qglBeginQueryARB = (PFNGLBEGINQUERYARBPROC) SDL_GL_GetProcAddress("glBeginQueryARB");
			qglEndQueryARB = (PFNGLENDQUERYARBPROC) SDL_GL_GetProcAddress("glEndQueryARB");
			qglGetQueryivARB = (PFNGLGETQUERYIVARBPROC) SDL_GL_GetProcAddress("glGetQueryivARB");
			qglGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC) SDL_GL_GetProcAddress("glGetQueryObjectivARB");
			qglGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC) SDL_GL_GetProcAddress("glGetQueryObjectuivARB");
			glConfig.occlusionQueryAvailable = qtrue;
			qglGetQueryivARB(GL_SAMPLES_PASSED, GL_QUERY_COUNTER_BITS, &glConfig.occlusionQueryBits);
			ri.Printf(PRINT_ALL, "...using GL_ARB_occlusion_query\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_occlusion_query\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_occlusion_query not found\n");
	}

	// GL_ARB_shader_objects
	qglDeleteObjectARB = NULL;
	qglGetHandleARB = NULL;
	qglDetachObjectARB = NULL;
	qglCreateShaderObjectARB = NULL;
	qglShaderSourceARB = NULL;
	qglCompileShaderARB = NULL;
	qglCreateProgramObjectARB = NULL;
	qglAttachObjectARB = NULL;
	qglLinkProgramARB = NULL;
	qglUseProgramObjectARB = NULL;
	qglValidateProgramARB = NULL;
	qglUniform1fARB = NULL;
	qglUniform2fARB = NULL;
	qglUniform3fARB = NULL;
	qglUniform4fARB = NULL;
	qglUniform1iARB = NULL;
	qglUniform2iARB = NULL;
	qglUniform3iARB = NULL;
	qglUniform4iARB = NULL;
	qglUniform2fvARB = NULL;
	qglUniform3fvARB = NULL;
	qglUniform4fvARB = NULL;
	qglUniform2ivARB = NULL;
	qglUniform3ivARB = NULL;
	qglUniform4ivARB = NULL;
	qglUniformMatrix2fvARB = NULL;
	qglUniformMatrix3fvARB = NULL;
	qglUniformMatrix4fvARB = NULL;
	qglGetObjectParameterfvARB = NULL;
	qglGetObjectParameterivARB = NULL;
	qglGetInfoLogARB = NULL;
	qglGetAttachedObjectsARB = NULL;
	qglGetUniformLocationARB = NULL;
	qglGetActiveUniformARB = NULL;
	qglGetUniformfvARB = NULL;
	qglGetUniformivARB = NULL;
	qglGetShaderSourceARB = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_shader_objects"))
	{
		qglDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC) SDL_GL_GetProcAddress("glDeleteObjectARB");
		qglGetHandleARB = (PFNGLGETHANDLEARBPROC) SDL_GL_GetProcAddress("glGetHandleARB");
		qglDetachObjectARB = (PFNGLDETACHOBJECTARBPROC) SDL_GL_GetProcAddress("glDetachObjectARB");
		qglCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) SDL_GL_GetProcAddress("glCreateShaderObjectARB");
		qglShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) SDL_GL_GetProcAddress("glShaderSourceARB");
		qglCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) SDL_GL_GetProcAddress("glCompileShaderARB");
		qglCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) SDL_GL_GetProcAddress("glCreateProgramObjectARB");
		qglAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) SDL_GL_GetProcAddress("glAttachObjectARB");
		qglLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) SDL_GL_GetProcAddress("glLinkProgramARB");
		qglUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) SDL_GL_GetProcAddress("glUseProgramObjectARB");
		qglValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC) SDL_GL_GetProcAddress("glValidateProgramARB");
		qglUniform1fARB = (PFNGLUNIFORM1FARBPROC) SDL_GL_GetProcAddress("glUniform1fARB");
		qglUniform2fARB = (PFNGLUNIFORM2FARBPROC) SDL_GL_GetProcAddress("glUniform2fARB");
		qglUniform3fARB = (PFNGLUNIFORM3FARBPROC) SDL_GL_GetProcAddress("glUniform3fARB");
		qglUniform4fARB = (PFNGLUNIFORM4FARBPROC) SDL_GL_GetProcAddress("glUniform4fARB");
		qglUniform1iARB = (PFNGLUNIFORM1IARBPROC) SDL_GL_GetProcAddress("glUniform1iARB");
		qglUniform2iARB = (PFNGLUNIFORM2IARBPROC) SDL_GL_GetProcAddress("glUniform2iARB");
		qglUniform3iARB = (PFNGLUNIFORM3IARBPROC) SDL_GL_GetProcAddress("glUniform3iARB");
		qglUniform4iARB = (PFNGLUNIFORM4IARBPROC) SDL_GL_GetProcAddress("glUniform4iARB");
		qglUniform2fvARB = (PFNGLUNIFORM2FVARBPROC) SDL_GL_GetProcAddress("glUniform2fvARB");
		qglUniform3fvARB = (PFNGLUNIFORM3FVARBPROC) SDL_GL_GetProcAddress("glUniform3fvARB");
		qglUniform4fvARB = (PFNGLUNIFORM4FVARBPROC) SDL_GL_GetProcAddress("glUniform4fvARB");
		qglUniform2ivARB = (PFNGLUNIFORM2IVARBPROC) SDL_GL_GetProcAddress("glUniform2ivARB");
		qglUniform3ivARB = (PFNGLUNIFORM3IVARBPROC) SDL_GL_GetProcAddress("glUniform3ivARB");
		qglUniform4ivARB = (PFNGLUNIFORM4IVARBPROC) SDL_GL_GetProcAddress("glUniform4ivARB");
		qglUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC) SDL_GL_GetProcAddress("glUniformMatrix2fvARB");
		qglUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC) SDL_GL_GetProcAddress("glUniformMatrix3fvARB");
		qglUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC) SDL_GL_GetProcAddress("glUniformMatrix4fvARB");
		qglGetObjectParameterfvARB = (PFNGLGETOBJECTPARAMETERFVARBPROC) SDL_GL_GetProcAddress("glGetObjectParameterfvARB");
		qglGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) SDL_GL_GetProcAddress("glGetObjectParameterivARB");
		qglGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) SDL_GL_GetProcAddress("glGetInfoLogARB");
		qglGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC) SDL_GL_GetProcAddress("glGetAttachedObjectsARB");
		qglGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) SDL_GL_GetProcAddress("glGetUniformLocationARB");
		qglGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC) SDL_GL_GetProcAddress("glGetActiveUniformARB");
		qglGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC) SDL_GL_GetProcAddress("glGetUniformfvARB");
		qglGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC) SDL_GL_GetProcAddress("glGetUniformivARB");
		qglGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC) SDL_GL_GetProcAddress("glGetShaderSourceARB");
		ri.Printf(PRINT_ALL, "...using GL_ARB_shader_objects\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_shader_objects not found\n");
	}

	// GL_ARB_vertex_shader
	qglBindAttribLocationARB = NULL;
	qglGetActiveAttribARB = NULL;
	qglGetAttribLocationARB = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_vertex_shader"))
	{
		int				reservedComponents;

		qglGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &glConfig.maxVertexUniforms);
		qglGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, &glConfig.maxVaryingFloats);
		qglGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &glConfig.maxVertexAttribs);

		reservedComponents = 16 * 10; // approximation how many uniforms we have besides the bone matrices
		glConfig.maxVertexSkinningBones = (int) Q_bound(0.0, (Q_max(glConfig.maxVertexUniforms - reservedComponents, 0) / 16), MAX_BONES);
		glConfig.vboVertexSkinningAvailable = (glConfig.maxVertexSkinningBones >= 12 && glConfig.driverType != GLDRV_MESA) ? qtrue : qfalse;

		qglBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC) SDL_GL_GetProcAddress("glBindAttribLocationARB");
		qglGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC) SDL_GL_GetProcAddress("glGetActiveAttribARB");
		qglGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC) SDL_GL_GetProcAddress("glGetAttribLocationARB");
		ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_shader\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_vertex_shader not found\n");
	}

	// GL_ARB_fragment_shader
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_fragment_shader"))
	{
		ri.Printf(PRINT_ALL, "...using GL_ARB_fragment_shader\n");
	}
	else
	{
		ri.Error(ERR_FATAL, "...GL_ARB_fragment_shader not found\n");
	}

	// GL_ARB_shading_language_100
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_shading_language_100"))
	{
		Q_strncpyz(glConfig.shadingLanguageVersion, qglGetString(GL_SHADING_LANGUAGE_VERSION_ARB),
				   sizeof(glConfig.shadingLanguageVersion));
		ri.Printf(PRINT_ALL, "...using GL_ARB_shading_language_100\n");
	}
	else
	{
		ri.Printf(ERR_FATAL, "...GL_ARB_shading_language_100 not found\n");
	}

	// GL_ARB_texture_non_power_of_two
	glConfig.textureNPOTAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_texture_non_power_of_two"))
	{
		if(r_ext_texture_non_power_of_two->integer)
		{
			glConfig.textureNPOTAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_texture_non_power_of_two\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_texture_non_power_of_two\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_texture_non_power_of_two not found\n");
	}

	// GL_ARB_draw_buffers
	glConfig.drawBuffersAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_draw_buffers"))
	{
		qglGetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &glConfig.maxDrawBuffers);

		if(r_ext_draw_buffers->integer)
		{
			qglDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC) SDL_GL_GetProcAddress("glDrawBuffersARB");
			glConfig.drawBuffersAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_draw_buffers\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_draw_buffers\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_draw_buffers not found\n");
	}

	// GL_ARB_half_float_pixel
	glConfig.textureHalfFloatAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_half_float_pixel"))
	{
		if(r_ext_half_float_pixel->integer)
		{
			glConfig.textureHalfFloatAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_half_float_pixel\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_half_float_pixel\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_half_float_pixel not found\n");
	}

	// GL_ARB_texture_float
	glConfig.textureFloatAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_texture_float"))
	{
		if(r_ext_texture_float->integer)
		{
			glConfig.textureFloatAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_texture_float\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_texture_float\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_texture_float not found\n");
	}

	// GL_ARB_texture_compression
	glConfig.textureCompression = TC_NONE;
	qglCompressedTexImage3DARB = NULL;
	qglCompressedTexImage2DARB = NULL;
	qglCompressedTexImage1DARB = NULL;
	qglCompressedTexSubImage3DARB = NULL;
	qglCompressedTexSubImage2DARB = NULL;
	qglCompressedTexSubImage1DARB = NULL;
	qglGetCompressedTexImageARB = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_texture_compression"))
	{
		if(r_ext_texture_compression->integer)
		{
			qglCompressedTexImage3DARB = SDL_GL_GetProcAddress("glCompressedTexImage3DARB");
			qglCompressedTexImage2DARB = SDL_GL_GetProcAddress("glCompressedTexImage2DARB");
			qglCompressedTexImage1DARB = SDL_GL_GetProcAddress("glCompressedTexImage1DARB");
			qglCompressedTexSubImage3DARB = SDL_GL_GetProcAddress("glCompressedTexSubImage3DARB");
			qglCompressedTexSubImage2DARB = SDL_GL_GetProcAddress("glCompressedTexSubImage2DARB");
			qglCompressedTexSubImage1DARB = SDL_GL_GetProcAddress("glCompressedTexSubImage1DARB");
			qglGetCompressedTexImageARB = SDL_GL_GetProcAddress("glGetCompressedTexImageARB");
			glConfig.ARBTextureCompressionAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_texture_compression\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_texture_compression\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_texture_compression not found\n");
	}

	// GL_ARB_vertex_array_object
	qglBindVertexArray = NULL;
	qglDeleteVertexArrays = NULL;
	qglGenVertexArrays = NULL;
	qglIsVertexArray = NULL;
	glConfig.vertexArrayObjectAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_vertex_array_object"))
	{
		if(r_ext_vertex_array_object->integer)
		{
			qglBindVertexArray = SDL_GL_GetProcAddress("glBindVertexArray");
			qglDeleteVertexArrays = SDL_GL_GetProcAddress("glDeleteVertexArrays");
			qglGenVertexArrays = SDL_GL_GetProcAddress("glGenVertexArrays");
			qglIsVertexArray = SDL_GL_GetProcAddress("glIsVertexArray");
			glConfig.vertexArrayObjectAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_array_object\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_vertex_array_object\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_vertex_array_object not found\n");
	}

	// GL_EXT_texture_compression_s3tc
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_texture_compression_s3tc"))
	{
		if(r_ext_texture_compression->integer)
		{
			glConfig.textureCompression = TC_S3TC;
			ri.Printf(PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n");
	}

	// GL_EXT_texture3D
	glConfig.texture3DAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_texture3D"))
	{
		//if(r_ext_texture3d->value)
		{
			qglTexImage3DEXT = SDL_GL_GetProcAddress("glTexImage3DEXT");
			qglTexSubImage3DEXT = SDL_GL_GetProcAddress("glTexSubImage3DEXT");
			glConfig.texture3DAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_texture3D\n");
		}
		/*
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture3D\n");
		}
		*/
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_texture3D not found\n");
	}

	// GL_EXT_stencil_wrap
	glConfig.stencilWrapAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_stencil_wrap"))
	{
		if(r_ext_stencil_wrap->value)
		{
			glConfig.stencilWrapAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_stencil_wrap\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_stencil_wrap\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_stencil_wrap not found\n");
	}

	// GL_EXT_texture_filter_anisotropic
	glConfig.textureAnisotropyAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{
		qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.maxTextureAnisotropy);

		if(r_ext_texture_filter_anisotropic->value)
		{
			glConfig.textureAnisotropyAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n");
	}

	// GL_EXT_stencil_two_side
	qglActiveStencilFaceEXT = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_stencil_two_side"))
	{
		if(r_ext_stencil_two_side->value)
		{
			qglActiveStencilFaceEXT = (void (APIENTRY *) (GLenum))SDL_GL_GetProcAddress("glActiveStencilFaceEXT");
			ri.Printf(PRINT_ALL, "...using GL_EXT_stencil_two_side\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_stencil_two_side\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_stencil_two_side not found\n");
	}

	// GL_EXT_depth_bounds_test
	qglDepthBoundsEXT = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_depth_bounds_test"))
	{
		if(r_ext_depth_bounds_test->value)
		{
			qglDepthBoundsEXT = (PFNGLDEPTHBOUNDSEXTPROC) SDL_GL_GetProcAddress("glDepthBoundsEXT");
			ri.Printf(PRINT_ALL, "...using GL_EXT_depth_bounds_test\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_depth_bounds_test\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_depth_bounds_test not found\n");
	}

	// GL_EXT_framebuffer_object
	glConfig.framebufferObjectAvailable = qfalse;
	qglIsRenderbufferEXT = NULL;
	qglBindRenderbufferEXT = NULL;
	qglDeleteRenderbuffersEXT = NULL;
	qglGenRenderbuffersEXT = NULL;
	qglRenderbufferStorageEXT = NULL;
	qglGetRenderbufferParameterivEXT = NULL;
	qglIsFramebufferEXT = NULL;
	qglBindFramebufferEXT = NULL;
	qglDeleteFramebuffersEXT = NULL;
	qglGenFramebuffersEXT = NULL;
	qglCheckFramebufferStatusEXT = NULL;
	qglFramebufferTexture1DEXT = NULL;
	qglFramebufferTexture2DEXT = NULL;
	qglFramebufferTexture3DEXT = NULL;
	qglFramebufferRenderbufferEXT = NULL;
	qglGetFramebufferAttachmentParameterivEXT = NULL;
	qglGenerateMipmapEXT = NULL;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_framebuffer_object"))
	{
		qglGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &glConfig.maxRenderbufferSize);
		qglGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &glConfig.maxColorAttachments);

		if(r_ext_framebuffer_object->value)
		{
			qglIsRenderbufferEXT = (PFNGLISRENDERBUFFEREXTPROC) SDL_GL_GetProcAddress("glIsRenderbufferEXT");
			qglBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC) SDL_GL_GetProcAddress("glBindRenderbufferEXT");
			qglDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC) SDL_GL_GetProcAddress("glDeleteRenderbuffersEXT");
			qglGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC) SDL_GL_GetProcAddress("glGenRenderbuffersEXT");
			qglRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC) SDL_GL_GetProcAddress("glRenderbufferStorageEXT");
			qglGetRenderbufferParameterivEXT =
				(PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC) SDL_GL_GetProcAddress("glGetRenderbufferParameterivEXT");
			qglIsFramebufferEXT = (PFNGLISFRAMEBUFFEREXTPROC) SDL_GL_GetProcAddress("glIsFramebufferEXT");
			qglBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC) SDL_GL_GetProcAddress("glBindFramebufferEXT");
			qglDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC) SDL_GL_GetProcAddress("glDeleteFramebuffersEXT");
			qglGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC) SDL_GL_GetProcAddress("glGenFramebuffersEXT");
			qglCheckFramebufferStatusEXT =
				(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) SDL_GL_GetProcAddress("glCheckFramebufferStatusEXT");
			qglFramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture1DEXT");
			qglFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
			qglFramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture3DEXT");
			qglFramebufferRenderbufferEXT =
				(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) SDL_GL_GetProcAddress("glFramebufferRenderbufferEXT");
			qglGetFramebufferAttachmentParameterivEXT =
				(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)
				SDL_GL_GetProcAddress("glGetFramebufferAttachmentParameterivEXT");
			qglGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC) SDL_GL_GetProcAddress("glGenerateMipmapEXT");
			glConfig.framebufferObjectAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_framebuffer_object\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_framebuffer_object\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_framebuffer_object not found\n");
	}

	// GL_EXT_packed_depth_stencil
	glConfig.framebufferPackedDepthStencilAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_packed_depth_stencil"))
	{
		if(r_ext_packed_depth_stencil->integer)
		{
			glConfig.framebufferPackedDepthStencilAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_packed_depth_stencil\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_packed_depth_stencil\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_packed_depth_stencil not found\n");
	}

	// GL_EXT_framebuffer_blit
	glConfig.framebufferBlitAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_EXT_framebuffer_blit"))
	{
		if(r_ext_framebuffer_blit->integer)
		{
			qglBlitFramebufferEXT = SDL_GL_GetProcAddress("glBlitFramebufferEXT");
			glConfig.framebufferBlitAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXT_framebuffer_blit\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_framebuffer_blit\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_framebuffer_blit not found\n");
	}

	// GL_EXTX_framebuffer_mixed_formats
	glConfig.framebufferMixedFormatsAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_EXTX_framebuffer_mixed_formats"))
	{
		if(r_extx_framebuffer_mixed_formats->integer)
		{
			glConfig.framebufferMixedFormatsAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_EXTX_framebuffer_mixed_formats\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXTX_framebuffer_mixed_formats\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXTX_framebuffer_mixed_formats not found\n");
	}

	// GL_ATI_separate_stencil
	qglStencilFuncSeparateATI = NULL;
	qglStencilOpSeparateATI = NULL;
	if(strstr(glConfig.extensions_string, "GL_ATI_separate_stencil"))
	{
		if(r_ext_separate_stencil->value)
		{
			qglStencilFuncSeparateATI = (void *)SDL_GL_GetProcAddress("glStencilFuncSeparateATI");
			qglStencilOpSeparateATI = (void *)SDL_GL_GetProcAddress("glStencilOpSeparateATI");
			ri.Printf(PRINT_ALL, "...using GL_ATI_separate_stencil\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ATI_separate_stencil\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ATI_separate_stencil not found\n");
	}

	// GL_SGIS_generate_mipmap
	glConfig.generateMipmapAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_SGIS_generate_mipmap"))
	{
		if(r_ext_generate_mipmap->value)
		{
			glConfig.generateMipmapAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_SGIS_generate_mipmap\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_SGIS_generate_mipmap\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_SGIS_generate_mipmap not found\n");
	}

}

#define R_MODE_FALLBACK 3		// 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init(void)
{
	qboolean        success = qtrue;

	r_sdlDriver = ri.Cvar_Get("r_sdlDriver", "", CVAR_ROM);

	Sys_GLimpInit();

	// create the window and set up the context
	if(!GLimp_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer))
	{
		if(r_mode->integer != R_MODE_FALLBACK)
		{
			ri.Printf(PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n", r_mode->integer, R_MODE_FALLBACK);
			if(!GLimp_StartDriverAndSetMode(R_MODE_FALLBACK, r_fullscreen->integer))
				success = qfalse;
		}
		else
			success = qfalse;
	}

	if(!success)
		ri.Error(ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n");

	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_DEFAULT;
	glConfig.hardwareType = GLHW_GENERIC;
	glConfig.deviceSupportsGamma = !!(SDL_SetGamma(1.0f, 1.0f, 1.0f) >= 0);

	// get our config strings
	Q_strncpyz(glConfig.vendor_string, (char *)qglGetString(GL_VENDOR), sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string, (char *)qglGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
	if(*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	Q_strncpyz(glConfig.version_string, (char *)qglGetString(GL_VERSION), sizeof(glConfig.version_string));
	Q_strncpyz(glConfig.extensions_string, (char *)qglGetString(GL_EXTENSIONS), sizeof(glConfig.extensions_string));


	if(Q_stristr(glConfig.renderer_string, "mesa") || Q_stristr(glConfig.vendor_string, "mesa"))
	{
		// suckage
		glConfig.driverType = GLDRV_MESA;
	}

	if(Q_stristr(glConfig.renderer_string, "geforce"))
	{
		if(Q_stristr(glConfig.renderer_string, "8400") ||
		   Q_stristr(glConfig.renderer_string, "8500") ||
		   Q_stristr(glConfig.renderer_string, "8600") ||
		   Q_stristr(glConfig.renderer_string, "8800") ||
		   Q_stristr(glConfig.renderer_string, "9500") ||
		   Q_stristr(glConfig.renderer_string, "9600") ||
		   Q_stristr(glConfig.renderer_string, "9800") ||
		   Q_stristr(glConfig.renderer_string, "gts 250") ||
		   Q_stristr(glConfig.renderer_string, "gtx 260") ||
		   Q_stristr(glConfig.renderer_string, "gtx 280") ||
		   Q_stristr(glConfig.renderer_string, "gtx 285") ||
		   Q_stristr(glConfig.renderer_string, "gtx 295"))
			glConfig.hardwareType = GLHW_NV_DX10;
	}
	else if(Q_stristr(glConfig.renderer_string, "quadro fx"))
	{
		if(Q_stristr(glConfig.renderer_string, "3600"))
			glConfig.hardwareType = GLHW_NV_DX10;
	}
	else if(Q_stristr(glConfig.renderer_string, "rv770"))
	{
		glConfig.hardwareType = GLHW_ATI_DX10;
	}
	else if(Q_stristr(glConfig.renderer_string, "radeon hd"))
	{
		glConfig.hardwareType = GLHW_ATI_DX10;
	}
	else if(Q_stristr(glConfig.renderer_string, "eah4850") || Q_stristr(glConfig.renderer_string, "eah4870"))
	{
		glConfig.hardwareType = GLHW_ATI_DX10;
	}
	else if(Q_stristr(glConfig.renderer_string, "radeon"))
	{
		glConfig.hardwareType = GLHW_ATI;
	}


	// initialize extensions
	GLimp_InitExtensions();

	ri.Cvar_Get("r_availableModes", "", CVAR_ROM);

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init();
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame(void)
{
	// don't flip if drawing to front buffer
	if(Q_stricmp(r_drawBuffer->string, "GL_FRONT") != 0)
	{
		SDL_GL_SwapBuffers();
	}

	if(r_fullscreen->modified)
	{
		qboolean        fullscreen;
		qboolean        needToToggle = qtrue;
		qboolean        sdlToggled = qfalse;
		SDL_Surface    *s = SDL_GetVideoSurface();

		if(s)
		{
			// Find out the current state
			fullscreen = !!(s->flags & SDL_FULLSCREEN);

			if(r_fullscreen->integer && Cvar_VariableIntegerValue("in_nograb"))
			{
				ri.Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set("r_fullscreen", "0");
				r_fullscreen->modified = qfalse;
			}

			// Is the state we want different from the current state?
			needToToggle = !!r_fullscreen->integer != fullscreen;

			if(needToToggle)
				sdlToggled = SDL_WM_ToggleFullScreen(s);
		}

		if(needToToggle)
		{
			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if(!sdlToggled)
				Cbuf_AddText("vid_restart");

			IN_Restart();
		}

		r_fullscreen->modified = qfalse;
	}

	QGL_EnableLogging(r_logFile->integer);
}



