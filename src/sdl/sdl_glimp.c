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

#if !SDL_VERSION_ATLEAST(1, 2, 10)
#define SDL_GL_ACCELERATED_VISUAL 15
#define SDL_GL_SWAP_CONTROL 16
#elif MINSDL_PATCH >= 10
#error Code block no longer necessary, please remove
#endif

#ifdef USE_LOCAL_HEADERS
#	include "SDL_thread.h"
#else
#	include <SDL_thread.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"
#include "sdl_icon.h"

/* Just hack it for now. */
#ifdef MACOS_X
#include <OpenGL/OpenGL.h>
typedef CGLContextObj QGLContext;
#define GLimp_GetCurrentContext() CGLGetCurrentContext()
#define GLimp_SetCurrentContext(ctx) CGLSetCurrentContext(ctx)
#else
typedef void *QGLContext;
#define GLimp_GetCurrentContext() (NULL)
#define GLimp_SetCurrentContext(ctx)
#endif

static QGLContext opengl_context;

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

static SDL_Surface *screen = NULL;

cvar_t *r_allowSoftwareGL; // Don't abort out if a hardware visual can't be obtained

// GL_ARB_multitexture
void            (APIENTRY * qglMultiTexCoord2fARB) (GLenum texture, GLfloat s, GLfloat t);
void            (APIENTRY * qglActiveTextureARB) (GLenum texture);
void            (APIENTRY * qglClientActiveTextureARB) (GLenum texture);

// GL_ARB_vertex_program
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
GLvoid         *(APIENTRY * qglMapBufferARB) (GLenum target, GLenum access);

// GL_ARB_occlusion_query
void            (APIENTRY * qglGenQueriesARB) (GLsizei n, GLuint * ids);
void            (APIENTRY * qglDeleteQueriesARB) (GLsizei n, const GLuint * ids);
GLboolean       (APIENTRY * qglIsQueryARB) (GLuint id);
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

// GL_EXT_compiled_vertex_array
void            (APIENTRY * qglLockArraysEXT) (GLint, GLint);
void            (APIENTRY * qglUnlockArraysEXT) (void);

// GL_EXT_stencil_two_side
void            (APIENTRY * qglActiveStencilFaceEXT) (GLenum face);

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

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
	IN_Shutdown();

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
	screen = NULL;

	Com_Memset( &glConfig, 0, sizeof( glConfig ) );
	Com_Memset( &glConfig2, 0, sizeof( glConfig2 ) );
	Com_Memset( &glState, 0, sizeof( glState ) );
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
	float aspectDiffA = fabs( ( (float)modeA->w / (float)modeA->h ) - displayAspect );
	float aspectDiffB = fabs( ( (float)modeB->w / (float)modeB->h ) - displayAspect );
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
		ri.Printf( PRINT_ALL, "Display supports any resolution\n" );
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
		ri.Printf( PRINT_ALL, "Available modes: '%s'\n", buf );
		ri.Cvar_Set( "r_availableModes", buf );
	}
}

/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode( int mode, qboolean fullscreen )
{
	const char*   glstring;
	int sdlcolorbits;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int i = 0;
	SDL_Surface *vidscreen = NULL;
	Uint32 flags = SDL_OPENGL;
	const SDL_VideoInfo *videoInfo;

	ri.Printf( PRINT_ALL, "Initializing OpenGL display\n");

	if( displayAspect == 0.0f )
	{
#if !SDL_VERSION_ATLEAST(1, 2, 10)
		// 1.2.10 is needed to get the desktop resolution
		displayAspect = 4.0f / 3.0f;
#elif MINSDL_PATCH >= 10
#	error Ifdeffery no longer necessary, please remove
#else
		// Guess the display aspect ratio through the desktop resolution
		// by assuming (relatively safely) that it is set at or close to
		// the display's native aspect ratio
		videoInfo = SDL_GetVideoInfo( );
		displayAspect = (float)videoInfo->current_w / (float)videoInfo->current_h;
#endif

		ri.Printf( PRINT_ALL, "Estimated display aspect: %.3f\n", displayAspect );
	}

	ri.Printf (PRINT_ALL, "...setting mode %d:", mode );

	if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
		glConfig.isFullscreen = qfalse;

	if (!r_colorbits->value)
		colorbits = 24;
	else
		colorbits = r_colorbits->value;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;

	if (!r_stencilbits->value)
		stencilbits = 8;
	else
		stencilbits = r_stencilbits->value;

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
					if (colorbits == 24)
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
		{
			// reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2)
		{
			// reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1)
		{
			// reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		sdlcolorbits = 4;
		if (tcolorbits == 24)
			sdlcolorbits = 8;

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, tdepthbits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, tstencilbits );

#if 0
		if(r_stereoEnabled->integer)
		{
			glConfig.stereoEnabled = qtrue;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		}
		else
		{
			glConfig.stereoEnabled = qfalse;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
		}
#endif

		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

#if 0 // See http://bugzilla.icculus.org/show_bug.cgi?id=3526
		// If not allowing software GL, demand accelerated
		if( !r_allowSoftwareGL->integer )
		{
			if( SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 ) < 0 )
			{
				ri.Printf( PRINT_ALL, "Unable to guarantee accelerated "
						"visual with libSDL < 1.2.10\n" );
			}
		}
#endif

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

		opengl_context = GLimp_GetCurrentContext();

		ri.Printf( PRINT_ALL, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
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
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glstring );

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode( int mode, qboolean fullscreen )
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO )... ");
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			ri.Printf( PRINT_ALL, "FAILED (%s)\n", SDL_GetError());
			return qfalse;
		}
		ri.Printf( PRINT_ALL, "OK\n");
	}

	if (fullscreen && Cvar_VariableIntegerValue( "in_nograb" ) )
	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	err = GLimp_SetMode( mode, fullscreen );

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
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
static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	// GL_S3_s3tc
	if ( Q_stristr( glConfig.extensions_string, "GL_S3_s3tc" ) )
	{
		if ( r_ext_compressed_textures->value )
		{
			glConfig.textureCompression = TC_S3TC;
			ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
		}
		else
		{
			glConfig.textureCompression = TC_NONE;
			ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
		}
	}
	else
	{
		glConfig.textureCompression = TC_NONE;
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
	}

	// GL_ARB_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if(strstr(glConfig.extensions_string, "GL_ARB_texture_env_add"))
	{
		if(r_ext_texture_env_add->integer)
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_texture_env_add\n");
		}
		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_texture_env_add\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_texture_env_add not found\n");
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if(strstr(glConfig.extensions_string, "GL_ARB_multitexture"))
	{
		if(r_ext_multitexture->integer)
		{
			qglMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC) SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
			qglActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glActiveTextureARB");
			qglClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC) SDL_GL_GetProcAddress("glClientActiveTextureARB");

			if(qglActiveTextureARB)
			{
				qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glConfig.maxTextureUnits);

				if(glConfig.maxTextureUnits > 1)
				{
					ri.Printf(PRINT_ALL, "...using GL_ARB_multitexture\n");
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf(PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n");
				}
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_multitexture\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_multitexture not found\n");
	}

	// GL_ARB_texture_cube_map
	glConfig2.textureCubeAvailable = qfalse;
	if(strstr(glConfig.extensions_string, "GL_ARB_texture_cube_map"))
	{
		qglGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig2.maxCubeMapTextureSize);
		
		if(r_ext_texture_cube_map->integer)
		{
			glConfig2.textureCubeAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_texture_cube_map\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_texture_cube_map\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_texture_cube_map\n");
	}

	// GL_ARB_vertex_program
	glConfig2.vertexProgramAvailable = qfalse;
	qglVertexAttribPointerARB = NULL;
	qglEnableVertexAttribArrayARB = NULL;
	qglDisableVertexAttribArrayARB = NULL;
	if(strstr(glConfig.extensions_string, "GL_ARB_vertex_program"))
	{
		if(r_ext_vertex_program->value)
		{
			qglVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) SDL_GL_GetProcAddress("glVertexAttribPointerARB");
			qglEnableVertexAttribArrayARB =
				(PFNGLENABLEVERTEXATTRIBARRAYARBPROC) SDL_GL_GetProcAddress("glEnableVertexAttribArrayARB");
			qglDisableVertexAttribArrayARB =
				(PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) SDL_GL_GetProcAddress("glDisableVertexAttribArrayARB");
			glConfig2.vertexProgramAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_program\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_vertex_program\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_vertex_program not found\n");
	}

	// GL_ARB_vertex_buffer_object
	glConfig2.vertexBufferObjectAvailable = qfalse;
	qglBindBufferARB = NULL;
	qglDeleteBuffersARB = NULL;
	qglGenBuffersARB = NULL;
	qglIsBufferARB = NULL;
	qglBufferDataARB = NULL;
	qglBufferSubDataARB = NULL;
	qglGetBufferSubDataARB = NULL;
	qglMapBufferARB = NULL;
	qglUnmapBufferARB = NULL;
	qglGetBufferParameterivARB = NULL;
	qglGetBufferPointervARB = NULL;
	if(strstr(glConfig.extensions_string, "GL_ARB_vertex_buffer_object"))
	{
		if(r_ext_vertex_buffer_object->value)
		{
			qglBindBufferARB = (PFNGLBINDBUFFERARBPROC) SDL_GL_GetProcAddress("glBindBufferARB");
			qglDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC) SDL_GL_GetProcAddress("glDeleteBuffersARB");
			qglGenBuffersARB = (PFNGLGENBUFFERSARBPROC) SDL_GL_GetProcAddress("glGenBuffersARB");
			qglIsBufferARB = (PFNGLISBUFFERARBPROC) SDL_GL_GetProcAddress("glIsBufferARB");
			qglBufferDataARB = (PFNGLBUFFERDATAARBPROC) SDL_GL_GetProcAddress("glBufferDataARB");
			qglBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC) SDL_GL_GetProcAddress("glBufferSubDataARB");
			qglGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC) SDL_GL_GetProcAddress("glGetBufferSubDataARB");
			qglMapBufferARB = (PFNGLMAPBUFFERARBPROC) SDL_GL_GetProcAddress("glMapBufferARB");
			qglUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC) SDL_GL_GetProcAddress("glUnmapBufferARB");
			qglGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC) SDL_GL_GetProcAddress("glGetBufferParameterivARB");
			qglGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC) SDL_GL_GetProcAddress("glGetBufferPointervARB");
			glConfig2.vertexBufferObjectAvailable = qtrue;
			ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_buffer_object\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_vertex_buffer_object\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_vertex_buffer_object not found\n");
	}

	// GL_ARB_occlusion_query
	glConfig2.occlusionQueryAvailable = qfalse;
	glConfig2.occlusionQueryBits = 0;
	qglGenQueriesARB = NULL;
	qglDeleteQueriesARB = NULL;
	qglIsQueryARB = NULL;
	qglBeginQueryARB = NULL;
	qglEndQueryARB = NULL;
	qglGetQueryivARB = NULL;
	qglGetQueryObjectivARB = NULL;
	qglGetQueryObjectuivARB = NULL;
	if(strstr(glConfig.extensions_string, "GL_ARB_occlusion_query"))
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
			glConfig2.occlusionQueryAvailable = qtrue;
			qglGetQueryivARB(GL_SAMPLES_PASSED, GL_QUERY_COUNTER_BITS, &glConfig2.occlusionQueryBits);
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

	// GL_ARB_shading_language_100
	glConfig2.shadingLanguage100Available = qfalse;
	if(strstr(glConfig.extensions_string, "GL_ARB_shading_language_100") && strstr(glConfig.extensions_string, "GL_ARB_vertex_shader") && strstr(glConfig.extensions_string, "GL_ARB_fragment_shader") && strstr(glConfig.extensions_string, "GL_ARB_shader_objects"))
	{
		Q_strncpyz(glConfig2.shadingLanguageVersion, qglGetString(GL_SHADING_LANGUAGE_VERSION_ARB), sizeof(glConfig2.shadingLanguageVersion));
		
		if(r_ext_shading_language_100->value)
		{
			glConfig2.shadingLanguage100Available = qtrue;

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

			// GL_ARB_vertex_shader
			qglBindAttribLocationARB = NULL;
			qglGetActiveAttribARB = NULL;
			qglGetAttribLocationARB = NULL;
			qglBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC) SDL_GL_GetProcAddress("glBindAttribLocationARB");
			qglGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC) SDL_GL_GetProcAddress("glGetActiveAttribARB");
			qglGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC) SDL_GL_GetProcAddress("glGetAttribLocationARB");

			ri.Printf(PRINT_ALL, "...using GL_ARB_shading_language_100\n");
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_ARB_shading_language_100\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_ARB_shading_language_100 not found\n");
	}

	// GL_EXT_compiled_vertex_array
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	if(strstr(glConfig.extensions_string, "GL_EXT_compiled_vertex_array") && (glConfig.hardwareType != GLHW_RIVA128))
	{
		if(r_ext_compiled_vertex_array->integer)
		{
			ri.Printf(PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n");
			qglLockArraysEXT = (void (APIENTRY *) (int, int))SDL_GL_GetProcAddress("glLockArraysEXT");
			qglUnlockArraysEXT = (void (APIENTRY *) (void))SDL_GL_GetProcAddress("glUnlockArraysEXT");
			if(!qglLockArraysEXT || !qglUnlockArraysEXT)
			{
				ri.Error(ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf(PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n");
		}
	}
	else
	{
		ri.Printf(PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n");
	}

	// GL_EXT_stencil_wrap
	glConfig2.stencilWrapAvailable = qfalse;
	if(strstr(glConfig.extensions_string, "GL_EXT_stencil_wrap"))
	{
		if(r_ext_stencil_wrap->value)
		{
			glConfig2.stencilWrapAvailable = qtrue;
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
	glConfig2.textureAnisotropyAvailable = qfalse;
	if(strstr(glConfig.extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{
		qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig2.maxTextureAnisotropy);

		if(r_ext_texture_filter_anisotropic->value)
		{
			glConfig2.textureAnisotropyAvailable = qtrue;
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

	// GL_ARB_texture_non_power_of_two
	glConfig2.textureNPOTAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_ARB_texture_non_power_of_two"))
	{
		if(r_ext_texture_non_power_of_two->integer)
		{
			glConfig2.textureNPOTAvailable = qtrue;
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

	// GL_EXT_stencil_two_side
	qglActiveStencilFaceEXT = NULL;
	if(strstr(glConfig.extensions_string, "GL_EXT_stencil_two_side"))
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
	if(strstr(glConfig.extensions_string, "GL_EXT_depth_bounds_test"))
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
	glConfig2.framebufferObjectAvailable = qfalse;
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
	if(strstr(glConfig.extensions_string, "GL_EXT_framebuffer_object"))
	{
		qglGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &glConfig2.maxRenderbufferSize);
		qglGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &glConfig2.maxColorAttachments);

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
			qglCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) SDL_GL_GetProcAddress("glCheckFramebufferStatusEXT");
			qglFramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture1DEXT");
			qglFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture2DEXT");
			qglFramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC) SDL_GL_GetProcAddress("glFramebufferTexture3DEXT");
			qglFramebufferRenderbufferEXT =
				(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) SDL_GL_GetProcAddress("glFramebufferRenderbufferEXT");
			qglGetFramebufferAttachmentParameterivEXT =
				(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) SDL_GL_GetProcAddress("glGetFramebufferAttachmentParameterivEXT");
			qglGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC) SDL_GL_GetProcAddress("glGenerateMipmapEXT");
			glConfig2.framebufferObjectAvailable = qtrue;
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

	// GL_SGIS_generate_mipmap
	glConfig2.generateMipmapAvailable = qfalse;
	if(Q_stristr(glConfig.extensions_string, "GL_SGIS_generate_mipmap"))
	{
		if(r_ext_generate_mipmap->value)
		{
			glConfig2.generateMipmapAvailable = qtrue;
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

#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( void )
{
	qboolean success = qtrue;

	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );

	// create the window and set up the context
	if( !GLimp_StartDriverAndSetMode( r_mode->integer, r_fullscreen->integer ) )
	{
		if( r_mode->integer != R_MODE_FALLBACK )
		{
			ri.Printf( PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n",
					r_mode->integer, R_MODE_FALLBACK );
			if( !GLimp_StartDriverAndSetMode( R_MODE_FALLBACK, r_fullscreen->integer ) )
				success = qfalse;
		}
		else
			success = qfalse;
	}

	if( !success )
		ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n" );

	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;
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

	return;
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

	if( r_fullscreen->modified )
	{
		qboolean    fullscreen;
		qboolean    sdlToggled = qfalse;
		SDL_Surface *s = SDL_GetVideoSurface( );

		if( s )
		{
			// Find out the current state
			if( s->flags & SDL_FULLSCREEN )
				fullscreen = qtrue;
			else
				fullscreen = qfalse;
				
			if (r_fullscreen->integer && Cvar_VariableIntegerValue( "in_nograb" ))
			{
				ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set( "r_fullscreen", "0" );
				r_fullscreen->modified = qfalse;
			}

			// Is the state we want different from the current state?
			if( !!r_fullscreen->integer != fullscreen )
				sdlToggled = SDL_WM_ToggleFullScreen( s );
			else
				sdlToggled = qtrue;
		}

		// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
		if( !sdlToggled )
			Cbuf_AddText( "vid_restart" );

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
	Com_Printf( "Render thread starting\n" );

	glimpRenderThread();

	GLimp_SetCurrentContext(NULL);

	Com_Printf( "Render thread terminating\n" );

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

#ifndef MACOS_X
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

	GLimp_SetCurrentContext(NULL);

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

	GLimp_SetCurrentContext(opengl_context);

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

	GLimp_SetCurrentContext(opengl_context);
}

/*
===============
GLimp_WakeRenderer
===============
*/
void GLimp_WakeRenderer( void *data )
{
	GLimp_SetCurrentContext(NULL);

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

