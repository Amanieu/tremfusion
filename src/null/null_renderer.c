/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderer/tr_public.h"

void	Shutdown( qboolean destroyWindow ) {}
qhandle_t RegisterModel( const char *name ) {return 1;}
qhandle_t RegisterSkin( const char *name ) {return 1;}
qhandle_t RegisterShader( const char *name ) {return 1;}
qhandle_t RegisterShaderNoMip( const char *name ) {return 1;}
void	LoadWorld( const char *name ) {}
void	SetWorldVisData( const byte *vis ) {}
void	EndRegistration( void ) {}
void	ClearScene( void ) {}
void	AddRefEntityToScene( const refEntity_t *re ) {}
void	AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num ) {}
int		LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir ) {return 0;}
void	AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {}
void	AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {}
void	RenderScene( const refdef_t *fd ) {}
void	SetColor( const float *rgba ) {}
void	DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {}
void	DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {}
void	UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {}
void	BeginFrame( stereoFrame_t stereoFrame ) {}
void	EndFrame( int *frontEndMsec, int *backEndMsec ) {}
int		MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {return 0;}
int		LerpTag( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName ) {return 0;}
void	ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs ) {}
#ifdef __USEA3D
void    A3D_RenderGeometry(void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus) {}
#endif
void	RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {}
void	RemapShader(const char *oldShader, const char *newShader, const char *offsetTime) {}
qboolean GetEntityToken( char *buffer, int size ) {return qtrue;}
qboolean inPVS( const vec3_t p1, const vec3_t p2 ) {return qtrue;}
void	TakeVideoFrame( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg ) {}


void	BeginRegistration( glConfig_t *config )
{
	config->renderer_string[0] = '\0';
	config->vendor_string[0] = '\0';
	config->version_string[0] = '\0';
	config->extensions_string[0] = '\0';
	config->maxTextureSize = 512;
	config->numTextureUnits = 0;
	config->colorBits = 0;
	config->depthBits = 0;
	config->stencilBits = 0;
	config->driverType = GLDRV_ICD;
	config->hardwareType = GLHW_GENERIC;
	config->deviceSupportsGamma = qfalse;
	config->textureCompression = TC_NONE;
	config->textureEnvAddAvailable = qfalse;
	config->vidWidth = 640;
	config->vidHeight = 480;
	config->windowAspect = (float)4/3;
	config->displayAspect = (float)4/3;
	config->displayFrequency = 60;
	config->isFullscreen = qfalse;
	config->stereoEnabled = qfalse;
	config->smpActive = qfalse;
	config->textureFilterAnisotropic = qfalse;
	config->maxAnisotropy = 0;
}

refexport_t* GetRefAPI( int apiVersion, refimport_t *rimp )
{
	static refexport_t re;

	re.Shutdown = Shutdown;
	re.BeginRegistration = BeginRegistration;
	re.RegisterModel = RegisterModel;
	re.RegisterSkin = RegisterSkin;
	re.RegisterShader = RegisterShader;
	re.RegisterShaderNoMip = RegisterShaderNoMip;
	re.LoadWorld = LoadWorld;
	re.SetWorldVisData = SetWorldVisData;
	re.EndRegistration = EndRegistration;
	re.BeginFrame = BeginFrame;
	re.EndFrame = EndFrame;
	re.MarkFragments = MarkFragments;
	re.LerpTag = LerpTag;
	re.ModelBounds = ModelBounds;
	re.ClearScene = ClearScene;
	re.AddRefEntityToScene = AddRefEntityToScene;
	re.AddPolyToScene = AddPolyToScene;
	re.LightForPoint = LightForPoint;
	re.AddLightToScene = AddLightToScene;
	re.AddAdditiveLightToScene = AddAdditiveLightToScene;
	re.RenderScene = RenderScene;
	re.SetColor = SetColor;
	re.DrawStretchPic = DrawStretchPic;
	re.DrawStretchRaw = DrawStretchRaw;
	re.UploadCinematic = UploadCinematic;
	re.RegisterFont = RegisterFont;
	re.RemapShader = RemapShader;
	re.GetEntityToken = GetEntityToken;
	re.inPVS = inPVS;
	re.TakeVideoFrame = TakeVideoFrame;

	return &re;
}
