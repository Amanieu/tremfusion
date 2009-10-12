/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Tremfusion.

Tremfusion is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremfusion is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremfusion; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <SDL.h>

#include <SDL_thread.h>
#ifdef SDL_VIDEO_DRIVER_X11
#	include <X11/Xlib.h>
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

static void GLimp_SetCurrentContext(qboolean enable)
{
	if(enable)
		CGLSetCurrentContext(opengl_context);
	else
		CGLSetCurrentContext(NULL);
}
#elif SDL_VIDEO_DRIVER_X11
#include <GL/glx.h>
typedef struct
{
	GLXContext      ctx;
	Display         *dpy;
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

static void GLimp_SetCurrentContext(qboolean enable)
{
	if(enable)
		glXMakeCurrent(opengl_context.dpy, opengl_context.drawable, opengl_context.ctx);
	else
		glXMakeCurrent(opengl_context.dpy, None, NULL);
}
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
	SDL_SysWMinfo info;

	SDL_VERSION(&info.version);
	if(!SDL_GetWMInfo(&info))
	{
		ri.Printf(PRINT_WARNING, "Failed to obtain HWND from SDL (InputRegistry)");
		return;
	}

	opengl_context.hDC = GetDC(info.window);
	opengl_context.hGLRC = info.hglrc;
}

static void GLimp_SetCurrentContext(qboolean enable)
{
	if(enable)
		wglMakeCurrent(opengl_context.hDC, opengl_context.hGLRC);
	else
		wglMakeCurrent(opengl_context.hDC, NULL);
}
#else
static void GLimp_GetCurrentContext(void) {}
static void GLimp_SetCurrentContext(qboolean enable) {}
#endif

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

static SDL_Surface *screen = NULL;
static const SDL_VideoInfo *videoInfo = NULL;

cvar_t *r_allowSoftwareGL; // Don't abort out if a hardware visual can't be obtained
cvar_t *r_allowResize; // make window resizable
cvar_t *r_sdlDriver;

void (APIENTRYP qglActiveTextureARB) (GLenum texture);
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture);
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t);

void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count);
void (APIENTRYP qglUnlockArraysEXT) (void);

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
	float oldDisplayAspect = glConfig.displayAspect;

	IN_Shutdown();

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
	screen = NULL;

	Com_Memset( &glConfig, 0, sizeof( glConfig ) );
	glConfig.displayAspect = oldDisplayAspect;
	Com_Memset( &glState, 0, sizeof( glState ) );

#ifdef MACOS_X
	extern qboolean fullscreen_minimized;
	if( fullscreen_minimized )
		Cvar_Set( "r_fullscreen", "1" );
#endif
}

/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes( const void *a, const void *b )
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = *(SDL_Rect **)a;
	SDL_Rect *modeB = *(SDL_Rect **)b;
	float aspectDiffA = fabs( ( (float)modeA->w / (float)modeA->h ) - glConfig.displayAspect );
	float aspectDiffB = fabs( ( (float)modeB->w / (float)modeB->h ) - glConfig.displayAspect );
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if( aspectDiffsDiff > ASPECT_EPSILON )
		return 1;
	else if( aspectDiffsDiff < -ASPECT_EPSILON )
		return -1;
	else
	{
		if( modeA->w == modeB->w )
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
	char buf[ MAX_STRING_CHARS ] = { 0 };
	SDL_Rect **modes;
	int numModes;
	int i;

	modes = SDL_ListModes( NULL, SDL_OPENGL | SDL_FULLSCREEN );

	if( !modes )
	{
		ri.Printf( PRINT_WARNING, "Can't get list of available modes\n" );
		return;
	}

	if( modes == (SDL_Rect **)-1 )
	{
		ri.Printf( PRINT_DEVELOPER, "Display supports any resolution\n" );
		return; // can set any resolution
	}

	for( numModes = 0; modes[ numModes ]; numModes++ );

	if(numModes > 1)
		qsort( modes+1, numModes-1, sizeof( SDL_Rect* ), GLimp_CompareModes );

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ]->w, modes[ i ]->h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			ri.Printf( PRINT_WARNING, "Skipping mode %ux%x, buffer too small\n", modes[i]->w, modes[i]->h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		ri.Printf( PRINT_DEVELOPER, "Available modes: '%s'\n", buf );
		ri.Cvar_Set( "r_availableModes", buf );
	}
}

#define R_FAILSAFE_WIDTH  640
#define R_FAILSAFE_HEIGHT 480

/*
===============
GLimp_SetMode
===============
*/
typedef struct vidmode_s
{
	int width, height;
	float pixelAspect;		// pixel width / height
} vidmode_t;
vidmode_t vidModes[] =
{
	{ 320,	240,	1 },
	{ 400,	300,	1 },
	{ 512,	384,	1 },
	{ 640,	480,	1 },
	{ 800,	600,	1 },
	{ 960,	720,	1 },
	{ 1024,	768,	1 },
	{ 1152,	864,	1 },
	{ 1280,	1024,	1 },
	{ 1600,	1200,	1 },
	{ 2048,	1536,	1 },
	{ 856,	480,	1 }
};
static int GLimp_SetMode( qboolean failSafe, qboolean fullscreen )
{
	const char*   glstring;
	int sdlcolorbits;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int samples;
	int i = 0;
	SDL_Surface *vidscreen = NULL;
	Uint32 flags = SDL_OPENGL;
	static int desktop_w, desktop_h;  // desktop resolution 

	ri.Printf( PRINT_DEVELOPER, "Initializing OpenGL display\n");
 
	if ( r_allowResize->integer )
		flags |= SDL_RESIZABLE;

	// Guess the display aspect ratio through the desktop resolution
	// by assuming (relatively safely) that it is set at or close to
	// the display's native aspect ratio
	videoInfo = SDL_GetVideoInfo();
	if( videoInfo->current_h > 0 )
	{
		if( !desktop_w ) { // first time through, resolve desktop resolution
			desktop_w = videoInfo->current_w;
			desktop_h = videoInfo->current_h;
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "Cannot get display info, assuming 800x600\n" );
		if( !desktop_w ) { // first time through, resolve desktop resolution
			desktop_w = 800;
			desktop_h = 600;
		}
	}

	glConfig.displayAspect = (float)desktop_w / (float)desktop_h;

	ri.Printf( PRINT_DEVELOPER, "Estimated display aspect: %.3f\n", glConfig.displayAspect );

	if( !failSafe )
	{
		if ( r_width->modified || r_height->modified || r_pixelAspect->modified )
		{
			for ( i = 0; i < 12; i++ )
			{
				if ( r_width->integer == vidModes[ i ].width &&
				     r_height->integer == vidModes[ i ].height &&
				     r_pixelAspect->integer == vidModes[ i ].pixelAspect )
				{
					Cvar_SetValue( "r_mode", i );
					break;
				}
			}
			if ( i == 12 )
				Cvar_Set( "r_mode", "-1" );
		}
		else if ( r_mode->modified && r_mode->integer >= 0 )
		{
			Cvar_SetValue( "r_width", vidModes[ r_mode->integer ].width );
			Cvar_SetValue( "r_height", vidModes[ r_mode->integer ].height );
			Cvar_SetValue( "r_pixelAspect", vidModes[ r_mode->integer ].pixelAspect );
		}
		r_width->modified = qfalse;
		r_height->modified = qfalse;
		r_pixelAspect->modified = qfalse;
		r_mode->modified = qfalse;
		glConfig.vidWidth = ( r_width->integer ? r_width->integer : desktop_w );
		glConfig.vidHeight = ( r_height->integer ? r_height->integer : desktop_h );
		glConfig.windowAspect = glConfig.vidWidth / ( (float)glConfig.vidHeight * r_pixelAspect->value );
	}
	else if( glConfig.vidWidth != R_FAILSAFE_WIDTH &&
			glConfig.vidHeight != R_FAILSAFE_HEIGHT )
	{
		ri.Printf( PRINT_ALL, "Setting mode %dx%d failed, falling back on mode %dx%d\n",
			glConfig.vidWidth, glConfig.vidHeight, R_FAILSAFE_WIDTH, R_FAILSAFE_HEIGHT );

		glConfig.vidWidth = R_FAILSAFE_WIDTH;
		glConfig.vidHeight = R_FAILSAFE_HEIGHT;
		glConfig.windowAspect = 1.0f;
	}
	else
		return RSERR_INVALID_MODE;

	ri.Printf (PRINT_DEVELOPER, "...setting mode %dx%d\n", glConfig.vidWidth, glConfig.vidHeight);

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
		glConfig.isFullscreen = qfalse;

	colorbits = r_colorbits->value;
	if (!colorbits || colorbits > 32)
		colorbits = 32;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;
	stencilbits = r_stencilbits->value;
	samples = r_ext_multisample->value;

	for (i = 0; i < 16; i++)
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorbits == 32)
						colorbits = 24;
					else if (colorbits == 24)
						colorbits = 16;
					break;
				case 1 :
					if (depthbits == 24)
						depthbits = 16;
					else if (depthbits == 16)
						depthbits = 8;
				case 3 :
					if (stencilbits == 24)
						stencilbits = 16;
					else if (stencilbits == 16)
						stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3)
		{ // reduce colorbits
			if (tcolorbits == 32)
				tcolorbits = 24;
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1)
		{ // reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		if (tcolorbits >= 24)
			sdlcolorbits = 8;
		else
			sdlcolorbits = 4;

#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
		if (sdlcolorbits == 4)
			sdlcolorbits = 0; /* Use minimum size for 16-bit color */

		/* Need alpha or else SGIs choose 36+ bit RGB mode */
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1);
#endif

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, sdlcolorbits );
		if (tcolorbits == 32)
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, sdlcolorbits );
		else
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 0 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, tdepthbits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, tstencilbits );

		if(r_stereoEnabled->integer && !failSafe)
		{
			glConfig.stereoEnabled = qtrue;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		}
		else
		{
			glConfig.stereoEnabled = qfalse;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
		}
		
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

		if (failSafe)
			samples = 0;
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );

		if( SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, r_swapInterval->integer ) < 0 )
			ri.Printf( PRINT_ALL, "r_swapInterval requires libSDL >= 1.2.10\n" );

#ifdef USE_ICON
		{
			SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
					(void *)CLIENT_WINDOW_ICON.pixel_data,
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

			SDL_WM_SetIcon( icon, NULL );
			SDL_FreeSurface( icon );
		}
#endif

		SDL_WM_SetCaption(CLIENT_WINDOW_TITLE, CLIENT_WINDOW_MIN_TITLE);
		SDL_ShowCursor(0);

		if (!(vidscreen = SDL_SetVideoMode(glConfig.vidWidth, glConfig.vidHeight, colorbits, flags)))
		{
			ri.Printf( PRINT_DEVELOPER, "SDL_SetVideoMode failed: %s\n", SDL_GetError( ) );
			continue;
		}

		GLimp_GetCurrentContext();

		ri.Printf( PRINT_DEVELOPER, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
				sdlcolorbits, sdlcolorbits, sdlcolorbits, tdepthbits, tstencilbits);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	GLimp_DetectAvailableModes();

	if (!vidscreen)
	{
		ri.Printf( PRINT_ALL, "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	screen = vidscreen;

	glstring = (char *) qglGetString (GL_RENDERER);
	ri.Printf( PRINT_DEVELOPER, "GL_RENDERER: %s\n", glstring );

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode( qboolean failSafe, qboolean fullscreen )
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		char driverName[ 64 ];

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n",
					SDL_GetError());
			return qfalse;
		}

		SDL_VideoDriverName( driverName, sizeof( driverName ) - 1 );
		ri.Printf( PRINT_DEVELOPER, "SDL using driver \"%s\"\n", driverName );
		Cvar_Set( "r_sdlDriver", driverName );
	}

	if (fullscreen && Cvar_VariableIntegerValue( "in_nograb" ) )
	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	err = GLimp_SetMode( failSafe, fullscreen );

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode\n" );
			return qfalse;
		default:
			break;
	}

	return qtrue;
}

static qboolean GLimp_HaveExtension(const char *ext)
{
	const char *ptr = Q_stristr( glConfig.extensions_string, ext );
	if (ptr == NULL)
		return qfalse;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify it's complete string.
}


/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
		return;
	}

	ri.Printf( PRINT_DEVELOPER, "Initializing OpenGL extensions\n" );

	glConfig.textureCompression = TC_NONE;

	// GL_EXT_texture_compression_s3tc
	if ( GLimp_HaveExtension( "GL_ARB_texture_compression" ) &&
	     GLimp_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->value )
		{
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_DEVELOPER, "...using GL_EXT_texture_compression_s3tc\n" );
		}
		else
		{
			ri.Printf( PRINT_DEVELOPER, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_DEVELOPER, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc ... legacy extension before GL_EXT_texture_compression_s3tc.
	if (glConfig.textureCompression == TC_NONE)
	{
		if ( GLimp_HaveExtension( "GL_S3_s3tc" ) )
		{
			if ( r_ext_compressed_textures->value )
			{
				glConfig.textureCompression = TC_S3TC;
				ri.Printf( PRINT_DEVELOPER, "...using GL_S3_s3tc\n" );
			}
			else
			{
				ri.Printf( PRINT_DEVELOPER, "...ignoring GL_S3_s3tc\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_DEVELOPER, "...GL_S3_s3tc not found\n" );
		}
	}


	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( GLimp_HaveExtension( "EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_DEVELOPER, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_DEVELOPER, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_DEVELOPER, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( GLimp_HaveExtension( "GL_ARB_multitexture" ) )
	{
		if ( r_ext_multitexture->value )
		{
			qglMultiTexCoord2fARB = SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = SDL_GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = SDL_GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB )
			{
				GLint glint = 0;
				qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glint );
				glConfig.numTextureUnits = (int) glint;
				if ( glConfig.numTextureUnits > 1 )
				{
					ri.Printf( PRINT_DEVELOPER, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_DEVELOPER, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			ri.Printf( PRINT_DEVELOPER, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_DEVELOPER, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	if ( GLimp_HaveExtension( "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->value )
		{
			ri.Printf( PRINT_DEVELOPER, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ( void ( APIENTRY * )( GLint, GLint ) ) SDL_GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void ( APIENTRY * )( void ) ) SDL_GL_GetProcAddress( "glUnlockArraysEXT" );
			if (!qglLockArraysEXT || !qglUnlockArraysEXT)
			{
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_DEVELOPER, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_DEVELOPER, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	glConfig.textureFilterAnisotropic = qfalse;
	if ( GLimp_HaveExtension( "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( r_ext_texture_filter_anisotropic->integer ) {
			qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *)&glConfig.maxAnisotropy );
			if ( glConfig.maxAnisotropy <= 0 ) {
				ri.Printf( PRINT_DEVELOPER, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
				glConfig.maxAnisotropy = 0;
			}
			else
			{
				ri.Printf( PRINT_DEVELOPER, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", glConfig.maxAnisotropy );
				glConfig.textureFilterAnisotropic = qtrue;
			}
		}
		else
		{
			ri.Printf( PRINT_DEVELOPER, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_DEVELOPER, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
}

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( void )
{
	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	r_sdlDriver = ri.Cvar_Get( "r_sdlDriver", "", CVAR_ROM );
	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE );

	Sys_GLimpInit( );

#ifdef SDL_VIDEO_DRIVER_X11
	XInitThreads( );
#endif

	// create the window and set up the context
	if( GLimp_StartDriverAndSetMode( qfalse, r_fullscreen->integer ) )
		goto success;

	// Try again, this time in a platform specific "safe mode"
	Sys_GLimpSafeInit( );
	if( GLimp_StartDriverAndSetMode( qfalse, r_fullscreen->integer ) )
		goto success;

	// Finally, try the default screen resolution
	if( GLimp_StartDriverAndSetMode( qtrue, r_fullscreen->integer ) )
		goto success;

	// Nothing worked, give up
	ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n" );

success:
	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;
	glConfig.deviceSupportsGamma = !!( SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0 );
	glConfig.deviceSupportsGamma = !!( SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0 );

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (char *) qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *) qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	Q_strncpyz( glConfig.version_string, (char *) qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (char *) qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	// initialize extensions
	GLimp_InitExtensions( );

	ri.Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init( );
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapBuffers();
	}

	if( r_minimize && r_minimize->integer )
	{
		extern qboolean fullscreen_minimized;
		extern void IN_DeactivateMouse( void );
		SDL_Surface *s = SDL_GetVideoSurface( );
		qboolean    fullscreen = qfalse;
		qboolean    minimized = qfalse;

		fullscreen = ( s && ( s->flags & SDL_FULLSCREEN ) );

#ifdef MACOS_X
		// this is a bit crap, but the mac SDL_WM_IconifyWindow does not work
		// on fullscreen windows, nor does the SDL_WM_ToggleFullscreen work
		if( !fullscreen )
		{
			if( SDL_WM_IconifyWindow( ) )
				IN_DeactivateMouse();
			Cvar_Set( "r_minimize", "0" ); 
		}
		else if( r_fullscreen->integer ) 
		{
			Cvar_Set( "r_fullscreen", "0" );
			fullscreen_minimized = qtrue;
		}
#else
		minimized = ( SDL_WM_IconifyWindow( ) != 0 );
		if( fullscreen && minimized )
			fullscreen_minimized = qtrue;

		// this shouldn't be necessary, but seems to prevent X11 mouse problems
		if( minimized )
			IN_DeactivateMouse();

		Cvar_Set( "r_minimize", "0" ); 
#endif // MACOS_X
	}

	if( r_fullscreen->modified )
	{
		qboolean    fullscreen;
		qboolean    needToToggle = qtrue;
		qboolean    sdlToggled = qfalse;
		SDL_Surface *s = SDL_GetVideoSurface( );

		if( s )
		{
			// Find out the current state
			fullscreen = !!( s->flags & SDL_FULLSCREEN );
				
			if( r_fullscreen->integer && Cvar_VariableIntegerValue( "in_nograb" ) )
			{
				ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set( "r_fullscreen", "0" );
				r_fullscreen->modified = qfalse;
			}

			// Is the state we want different from the current state?
			needToToggle = !!r_fullscreen->integer != fullscreen;

			if( needToToggle )
				sdlToggled = SDL_WM_ToggleFullScreen( s );
		}

		if( needToToggle )
		{
			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				Cbuf_AddText( "vid_restart" );

			IN_Restart( );
		}

		r_fullscreen->modified = qfalse;
	}
}



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
static void (*glimpRenderThread)( void ) = NULL;
static SDL_Thread *renderThread = NULL;

/*
===============
GLimp_ShutdownRenderThread
===============
*/
static void GLimp_ShutdownRenderThread(void)
{
	if (smpMutex != NULL)
	{
		SDL_DestroyMutex(smpMutex);
		smpMutex = NULL;
	}

	if (renderCommandsEvent != NULL)
	{
		SDL_DestroyCond(renderCommandsEvent);
		renderCommandsEvent = NULL;
	}

	if (renderCompletedEvent != NULL)
	{
		SDL_DestroyCond(renderCompletedEvent);
		renderCompletedEvent = NULL;
	}

	glimpRenderThread = NULL;
}

/*
===============
GLimp_RenderThreadWrapper
===============
*/
static int GLimp_RenderThreadWrapper( void *arg )
{
	// These printfs cause race conditions which mess up the console output
	//Com_Printf( "Render thread starting\n" );

	glimpRenderThread();

	GLimp_SetCurrentContext(qfalse);

	//Com_Printf( "Render thread terminating\n" );

	return 0;
}

/*
===============
GLimp_SpawnRenderThread
===============
*/
qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	static qboolean warned = qfalse;
	if (!warned)
	{
		Com_Printf("WARNING: You enable r_smp at your own risk!\n");
		warned = qtrue;
	}

#if !defined(MACOS_X) && !defined(WIN32) && !defined (SDL_VIDEO_DRIVER_X11)
	return qfalse;  /* better safe than sorry for now. */
#endif

	if (renderThread != NULL)  /* hopefully just a zombie at this point... */
	{
		Com_Printf("Already a render thread? Trying to clean it up...\n");
		SDL_WaitThread(renderThread, NULL);
		renderThread = NULL;
		GLimp_ShutdownRenderThread();
	}

	smpMutex = SDL_CreateMutex();
	if (smpMutex == NULL)
	{
		Com_Printf( "smpMutex creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCommandsEvent = SDL_CreateCond();
	if (renderCommandsEvent == NULL)
	{
		Com_Printf( "renderCommandsEvent creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	renderCompletedEvent = SDL_CreateCond();
	if (renderCompletedEvent == NULL)
	{
		Com_Printf( "renderCompletedEvent creation failed: %s\n", SDL_GetError() );
		GLimp_ShutdownRenderThread();
		return qfalse;
	}

	glimpRenderThread = function;
	renderThread = SDL_CreateThread(GLimp_RenderThreadWrapper, NULL);
	if ( renderThread == NULL )
	{
		ri.Printf( PRINT_ALL, "SDL_CreateThread() returned %s", SDL_GetError() );
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

static volatile void    *smpData = NULL;
static volatile qboolean smpDataReady;

/*
===============
GLimp_RendererSleep
===============
*/
void *GLimp_RendererSleep( void )
{
	void  *data = NULL;

	GLimp_SetCurrentContext(qfalse);

	SDL_LockMutex(smpMutex);
	{
		smpData = NULL;
		smpDataReady = qfalse;

		// after this, the front end can exit GLimp_FrontEndSleep
		SDL_CondSignal(renderCompletedEvent);

		while ( !smpDataReady )
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
void GLimp_FrontEndSleep( void )
{
	SDL_LockMutex(smpMutex);
	{
		while ( smpData )
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
void GLimp_WakeRenderer( void *data )
{
	GLimp_SetCurrentContext(qfalse);

	SDL_LockMutex(smpMutex);
	{
		assert( smpData == NULL );
		smpData = data;
		smpDataReady = qtrue;

		// after this, the renderer can continue through GLimp_RendererSleep
		SDL_CondSignal(renderCommandsEvent);
	}
	SDL_UnlockMutex(smpMutex);
}
