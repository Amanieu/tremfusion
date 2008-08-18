/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
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

#ifndef TR_LOCAL_H
#define TR_LOCAL_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "tr_public.h"
#include "qgl.h"

#define GL_INDEX_TYPE		GL_UNSIGNED_INT
typedef unsigned int glIndex_t;

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// fast float to int conversion
#if id386 && !( (defined __linux__ || defined __FreeBSD__ ) && (defined __i386__ ) )	// rb010123
long            myftol(float f);
#else
#define	myftol(x) ((int)(x))
#endif

// everything that is needed by the backend needs
// to be double buffered to allow it to run in
// parallel on a dual cpu machine
#define	SMP_FRAMES		2

#define	MAX_SHADERS				(1 << 12)
#define SHADERS_MASK			(MAX_SHADERS -1)

#define MAX_SHADER_TABLES		1024
#define MAX_SHADER_STAGES		16

//#define MAX_SHADER_STATES 2048
#define MAX_STATES_PER_SHADER 32
#define MAX_STATE_NAME 32

#define MAX_OCCLUSION_QUERIES	128

#define	MAX_FBOS				64

// can't be increased without changing bit packing for drawsurfs

typedef enum
{
	CULL_IN,				// completely unclipped
	CULL_CLIP,				// clipped by one or more planes
	CULL_OUT,				// completely outside the clipping planes
} cullResult_t;

typedef struct screenRect_s
{
	int             coords[4];
	struct screenRect_s *next;
} screenRect_t;

typedef enum
{
	FRUSTUM_NEAR,
	FRUSTUM_LEFT,
	FRUSTUM_RIGHT,
	FRUSTUM_BOTTOM,
	FRUSTUM_TOP,
	FRUSTUM_FAR,
	FRUSTUM_PLANES          = 5,
	FRUSTUM_CLIPALL         = 1 | 2 | 4 | 8 | 16 //| 32
} frustumBits_t;

typedef cplane_t        frustum_t[6];


// a trRefDlight_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct trRefDlight_s
{
	// public from client game
	refLight_t		l;
	
	// local
	qboolean        isStatic;				// loaded from the BSP entities lump
	qboolean        additive;				// texture detail is lost tho when the lightmap is dark
	vec3_t          origin;					// l.origin + rotated l.center
	vec3_t          transformed;			// origin in local coordinate system
	matrix_t		transformMatrix;		// light to world
	matrix_t		viewMatrix;				// object to light
	matrix_t		projectionMatrix;		// light frustum
	
	matrix_t		attenuationMatrix;		// attenuation * (light view * entity transform)
	matrix_t		attenuationMatrix2;		// attenuation * tcMod matrices		
	
	cullResult_t	cull;
	vec3_t			localBounds[2];
	vec3_t			worldBounds[2];
	
	float			depthBounds[2];			// zNear, zFar for GL_EXT_depth_bounds_test
	qboolean		noDepthBoundsTest;
	
	frustum_t       frustum;
	
	screenRect_t    scissor;
	
	struct interactionCache_s *firstInteractionCache;	// only used by static lights
	struct interactionCache_s *lastInteractionCache;	// only used by static lights
	
	int             numInteractions;		// total interactions
	int             numShadowOnlyInteractions;
	int             numLightOnlyInteractions;
	int             firstInteractionIndex;
	int             lastInteractionIndex;
	qboolean        noSort;					// don't sort interactions by material
	
	int             visCount;				// node needs to be traversed if current
	struct mnode_s **leafs;
	int             numLeafs;
} trRefDlight_t;


// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct
{
	// public from client game
	refEntity_t  e;

	// local
	float           axisLength;			// compensate for non-normalized axis
	qboolean        lightingCalculated;
	vec3_t          lightDir;			// normalized direction towards light
	vec3_t          ambientLight;		// color normalized to 0-255
	int             ambientLightInt;	// 32 bit rgba packed
	vec3_t          directedLight;
	qboolean		needZFail;
	
	cullResult_t	cull;
	vec3_t			localBounds[2];
	vec3_t			worldBounds[2];		// only set when not completely culled. use them for light interactions
} trRefEntity_t;

typedef struct
{
	vec3_t          origin;		// in world coordinates
	vec3_t          axis[3];	// orientation in world
	vec3_t          viewOrigin;	// viewParms->or.origin in local coordinates
	matrix_t        transformMatrix;	// transforms object to world: either used by camera, model or light
	matrix_t        viewMatrix;	// affine inverse of transform matrix to transform other objects into this space
	matrix_t        modelViewMatrix;	// only used by models, camera viewMatrix * transformMatrix
} orientationr_t;

enum
{
	IF_NONE,
	IF_INTERNAL			= (1 << 0),
	IF_NOPICMIP			= (1 << 1),
	IF_NOCOMPRESSION	= (1 << 2),
	IF_INTENSITY		= (1 << 3),
	IF_ALPHA			= (1 << 4),
	IF_NORMALMAP		= (1 << 5),
	IF_LIGHTMAP			= (1 << 6),
	IF_CUBEMAP			= (1 << 7)
};

typedef enum
{
	FT_DEFAULT,
	FT_LINEAR,
	FT_NEAREST
} filterType_t;

typedef enum
{
	WT_REPEAT,
	WT_CLAMP,					// don't repeat the texture for texture coords outside [0, 1]
	WT_EDGE_CLAMP,
	WT_ZERO_CLAMP,				// guarantee 0,0,0,255 edge for projected textures
	WT_ALPHA_ZERO_CLAMP			// guarante 0 alpha edge for projected textures
} wrapType_t;

typedef struct image_s
{
	char            name[1024];			// formerly MAX_QPATH, game path, including extension
										// can contain stuff like this now:
										// addnormals ( textures/base_floor/stetile4_local.tga ,
										// heightmap ( textures/base_floor/stetile4_bmp.tga , 4 ) )
	GLenum			type;
	int             width, height;	// source image
	int             uploadWidth, uploadHeight;	// after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	GLuint          texnum;		// gl texture binding

	int             frameUsed;	// for texture usage in frame statistics

	int             internalFormat;

	unsigned		bits;
	filterType_t	filterType;
	wrapType_t      wrapType;	// GL_CLAMP or GL_REPEAT

	struct image_s *next;
} image_t;

typedef struct frameBuffer_s
{
	char            name[MAX_QPATH];

	int             index;

	GLuint          frameBuffer;

	GLuint          colorBuffers[16];
	int             colorFormat;

	GLuint          depthBuffer;
	int             depthFormat;

	GLuint          stencilBuffer;
	int             stencilFormat;

	int             width;
	int             height;
	
//	struct frameBuffer_s *next;
} frameBuffer_t;

//===============================================================================

typedef enum
{
	SS_BAD,
	SS_PORTAL,					// mirrors, portals, viewscreens
	SS_ENVIRONMENT,				// sky box
	SS_OPAQUE,					// opaque

	SS_DECAL,					// scorch marks, etc.
	SS_SEE_THROUGH,				// ladders, grates, grills that may have small blended edges
	// in addition to alpha test
	SS_BANNER,

	SS_FOG,

	SS_UNDERWATER,				// for items that should be drawn in front of the water plane
	SS_WATER,
	
	SS_FAR,
	SS_MEDIUM,
	SS_CLOSE,

	SS_BLEND0,					// regular transparency and filters
	SS_BLEND1,					// generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,

	SS_ALMOST_NEAREST,			// gun smoke puffs

	SS_NEAREST,					// blood blobs
	SS_POST_PROCESS
} shaderSort_t;

typedef struct shaderTable_s
{
	char            name[MAX_QPATH];
	
	int             index;
	
	qboolean		clamp;
	qboolean		snap;
	
	float          *values;
	int				numValues;

	struct shaderTable_s *next;
} shaderTable_t;

typedef enum
{
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,

	GF_NOISE
} genFunc_t;


typedef enum
{
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7,
	DEFORM_SPRITE,
	DEFORM_FLARE
} deform_t;

typedef enum
{
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST,
	AGEN_CUSTOM
} alphaGen_t;

typedef enum
{
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,		// tr.identityLight
	CGEN_IDENTITY,				// always (1,1,1,1)
	CGEN_ENTITY,				// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,		// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,			// tess.vertexColors
	CGEN_VERTEX,				// tess.vertexColors * tr.identityLight
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,				// programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_FOG,					// standard fog
	CGEN_CONST,					// fixed color
	CGEN_CUSTOM_RGB,			// like fixed color but generated dynamically, single arithmetic expression
	CGEN_CUSTOM_RGBs,			// multiple expressions
} colorGen_t;

typedef enum
{
	TCGEN_BAD,
	TCGEN_SKIP,
	TCGEN_IDENTITY,				// clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_FOG,
	TCGEN_VECTOR				// S and T from world coordinates
} texCoordGen_t;

typedef enum
{
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
} acff_t;

typedef enum {
	OP_BAD,
	// logic operators
	OP_LAND,
	OP_LOR,
	OP_GE,
	OP_LE,
	OP_LEQ,
	OP_LNE,
	// arithmetic operators
	OP_ADD,
	OP_SUB,
	OP_DIV,
	OP_MOD,
	OP_MUL,
	OP_NEG,
	// logic operators
	OP_LT,
	OP_GT,
	// embracements
	OP_LPAREN,
	OP_RPAREN,
	OP_LBRACKET,
	OP_RBRACKET,
	// constants or variables
	OP_NUM,
	OP_TIME,
	OP_PARM0,
	OP_PARM1,
	OP_PARM2,
	OP_PARM3,
	OP_PARM4,
	OP_PARM5,
	OP_PARM6,
	OP_PARM7,
	OP_PARM8,
	OP_PARM9,
	OP_PARM10,
	OP_PARM11,
	OP_GLOBAL0,
	OP_GLOBAL1,
	OP_GLOBAL2,
	OP_GLOBAL3,
	OP_GLOBAL4,
	OP_GLOBAL5,
	OP_GLOBAL6,
	OP_GLOBAL7,
	OP_FRAGMENTPROGRAMS,
	OP_SOUND,
	// table access
	OP_TABLE
} opcode_t;

typedef struct
{
	const char     *s;
	opcode_t		type;
} opstring_t;

typedef struct
{
	opcode_t		type;
	float			value;
} expOperation_t;

#define MAX_EXPRESSION_OPS	32
typedef struct
{
	expOperation_t  ops[MAX_EXPRESSION_OPS];
	int             numOps;
	
	qboolean		active;	// no parsing problems
} expression_t;

typedef struct
{
	genFunc_t       func;

	float           base;
	float           amplitude;
	float           phase;
	float           frequency;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum
{
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE,
			
	TMOD_SCROLL2,
	TMOD_SCALE2,
	TMOD_CENTERSCALE,
	TMOD_SHEAR,
	TMOD_ROTATE2
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct
{
	deform_t        deformation;	// vertex coordinate modification type

	vec3_t          moveVector;
	waveForm_t      deformationWave;
	float           deformationSpread;

	float           bulgeWidth;
	float           bulgeHeight;
	float           bulgeSpeed;
	
	float			flareSize;
} deformStage_t;

typedef struct
{
	texMod_t        type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t      wave;

	// used for TMOD_TRANSFORM
	float           matrix[2][2];	// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float           translate[2];	// t' = s * m[0][1] + t * m[0][1] + trans[1]

	// used for TMOD_SCALE
	float           scale[2];	// s *= scale[0]
								// t *= scale[1]

	// used for TMOD_SCROLL
	float           scroll[2];	// s' = s + scroll[0] * time
								// t' = t + scroll[1] * time

	// + = clockwise
	// - = counterclockwise
	float           rotateSpeed;
	
	// used by everything else
	expression_t	sExp;
	expression_t	tExp;
	expression_t	rExp;

} texModInfo_t;


#define	MAX_IMAGE_ANIMATIONS	8

enum
{
	TB_COLORMAP = 0,
	TB_DIFFUSEMAP = 0,
	TB_NORMALMAP,
	TB_SPECULARMAP,
	TB_LIGHTMAP,
	MAX_TEXTURE_BUNDLES = 4
};

typedef struct
{
	image_t        *image[MAX_IMAGE_ANIMATIONS];
	int             numImageAnimations;
	float           imageAnimationSpeed;

	texCoordGen_t   tcGen;
	vec3_t          tcGenVectors[2];

	int             numTexMods;
	texModInfo_t   *texMods;

	int             videoMapHandle;
	qboolean        isLightMap;
	qboolean        isVideoMap;
} textureBundle_t;

typedef enum
{
	// material shader stage types
	ST_COLORMAP,							// vanilla Q3A style shader treatening
	ST_DIFFUSEMAP,
	ST_NORMALMAP,
	ST_SPECULARMAP,
	ST_HEATHAZEMAP,							// heatHaze post process effect
	ST_GLOWMAP,								// glow post process effect
	ST_LIGHTMAP,
	ST_DELUXEMAP,
	ST_REFLECTIONMAP,						// cubeMap based reflection
	ST_REFRACTIONMAP,
	ST_DISPERSIONMAP,
	ST_SKYBOXMAP,
	ST_LIQUIDMAP,							// reflective water shader
	
	ST_COLLAPSE_Generic_multi,				// two colormaps
	ST_COLLAPSE_lighting_D_radiosity,		// diffusemap + lightmap
	ST_COLLAPSE_lighting_DB_radiosity,		// diffusemap + bumpmap + lightmap
	ST_COLLAPSE_lighting_DBS_radiosity,		// diffusemap + bumpmap + specularmap + lightmap
	ST_COLLAPSE_lighting_DB_direct,			// directional entity lighting like rgbGen lightingDiffuse
	ST_COLLAPSE_lighting_DBS_direct,		// direction entity lighting with diffuse + bump + specular
	ST_COLLAPSE_lighting_DB_generic,		// diffusemap + bumpmap
	ST_COLLAPSE_lighting_DBS_generic,		// diffusemap + bumpmap + specularmap + lightmap
	
	// light shader stage types
	ST_ATTENUATIONMAP_XY,
	ST_ATTENUATIONMAP_Z
} stageType_t;

typedef enum
{
	COLLAPSE_none,
	COLLAPSE_Generic_multi,
	COLLAPSE_lighting_D_radiosity,
	COLLAPSE_lighting_DB_radiosity,
	COLLAPSE_lighting_DBS_radiosity,
	COLLAPSE_lighting_DB_direct,
	COLLAPSE_lighting_DBS_direct,
	COLLAPSE_lighting_DB_generic,
	COLLAPSE_lighting_DBS_generic,
} collapseType_t;

typedef struct
{
	stageType_t     type;

	qboolean        active;

	textureBundle_t bundle[MAX_TEXTURE_BUNDLES];
	
	expression_t    ifExp;

	waveForm_t      rgbWave;
	colorGen_t      rgbGen;
	expression_t    rgbExp;
	expression_t    redExp;
	expression_t    greenExp;
	expression_t    blueExp;

	waveForm_t      alphaWave;
	alphaGen_t      alphaGen;
	expression_t    alphaExp;
	
	expression_t    alphaTestExp;

	byte            constantColor[4];	// for CGEN_CONST and AGEN_CONST

	unsigned        stateBits;	// GLS_xxxx mask

	acff_t          adjustColorsForFog;

	qboolean        isDetail;
	
	qboolean        overrideNoPicMip;	// for images that must always be full resolution
	qboolean        overrideFilterType;	// for console fonts, 2D elements, etc.
	filterType_t	filterType;
	qboolean		overrideWrapType;
	wrapType_t		wrapType;
	
	qboolean		uncompressed;
	qboolean		highQuality;
	qboolean		forceHighQuality;
	
	qboolean        privatePolygonOffset;	// set for decals and other items that must be offset 
	float			privatePolygonOffsetValue;
	
	expression_t    specularExponentExp;
	
	expression_t    refractionIndexExp;
	
	expression_t    fresnelPowerExp;
	expression_t    fresnelScaleExp;
	expression_t    fresnelBiasExp;
	
	expression_t    etaExp;
	expression_t    etaDeltaExp;
	
	expression_t    heightScaleExp;
	expression_t    heightBiasExp;
	
	expression_t    deformMagnitudeExp;
	
	expression_t    blurMagnitudeExp;
} shaderStage_t;

struct shaderCommands_s;

typedef enum
{
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

typedef enum
{
	FP_NONE,					// surface is translucent and will just be adjusted properly
	FP_EQUAL,					// surface is opaque but possibly alpha tested
	FP_LE						// surface is trnaslucent, but still needs a fog pass (fog surface)
} fogPass_t;

typedef struct
{
	float           cloudHeight;
	image_t        *outerbox[6], *innerbox[6];
} skyParms_t;

typedef struct
{
	vec3_t          color;
	float           depthForOpaque;
} fogParms_t;

typedef enum
{
	SHADER_2D,				// surface material: shader is for 2D rendering
	SHADER_3D_DYNAMIC,		// surface material: shader is for cGen diffuseLighting lighting
	SHADER_3D_STATIC,		// surface material: pre-lit triangle models
	SHADER_3D_LIGHTMAP,
	SHADER_LIGHT			// light material: attenuation
} shaderType_t;

typedef struct shader_s
{
	char            name[MAX_QPATH];	// game path, including extension
	shaderType_t    type;
	
	int             index;		// this shader == tr.shaders[index]
	int             sortedIndex;	// this shader == tr.sortedShaders[sortedIndex]

	float           sort;		// lower numbered shaders draw before higher numbered

	qboolean        defaultShader;	// we want to return index 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again

	qboolean        explicitlyDefined;	// found in a .shader file
	qboolean        createdByGuide;		// created using a shader .guide template

	int             surfaceFlags;	// if explicitlyDefined, this will have SURF_* flags
	int             contentFlags;

	qboolean        entityMergable;	// merge across entites optimizable (smoke, blood)

	qboolean		noShadows;
	qboolean        fogLight;
	qboolean        blendLight;
	qboolean        ambientLight;
	qboolean		translucent;
	qboolean		forceOpaque;
	qboolean        isSky;
	skyParms_t      sky;
	fogParms_t      fogParms;

	float           portalRange;	// distance to fog out at

	collapseType_t	collapseType;
	int             collapseTextureEnv;	// 0, GL_MODULATE, GL_ADD (FIXME: put in stage)

	cullType_t      cullType;	// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
	qboolean        polygonOffset;	// set for decals and other items that must be offset 
	float			polygonOffsetValue;
	
	qboolean        noPicMip;	// for images that must always be full resolution
	filterType_t	filterType; // for console fonts, 2D elements, etc.
	wrapType_t		wrapType;
	
	fogPass_t       fogPass;	// draw a blended pass, possibly with depth test equals
	
	// spectrums are used for "invisible writing" that can only be illuminated by a light of matching spectrum
	qboolean		spectrum;	
	int				spectrumValue;
	
	qboolean        interactLight;	// this shader can interact with light shaders

	int             numDeforms;
	deformStage_t   deforms[MAX_SHADER_DEFORMS];

	int             numStages;
	shaderStage_t  *stages[MAX_SHADER_STAGES];

	void            (*optimalStageIteratorFunc) (void);

	float           clampTime;	// time this shader is clamped to
	float           timeOffset;	// current time offset for this shader

	int             numStates;	// if non-zero this is a state shader
	struct shader_s *currentShader;	// current state if this is a state shader
	struct shader_s *parentShader;	// current state if this is a state shader
	int             currentState;	// current state index for cycle purposes
	long            expireTime;	// time in milliseconds this expires

	struct shader_s *remappedShader;	// current shader this one is remapped too

	int             shaderStates[MAX_STATES_PER_SHADER];	// index to valid shader states

	struct shader_s *next;
} shader_t;

typedef struct shaderState_s
{
	char            shaderName[MAX_QPATH];	// name of shader this state belongs to
	char            name[MAX_STATE_NAME];	// name of this state
	char            stateShader[MAX_QPATH];	// shader this name invokes
	int             cycleTime;	// time this cycle lasts, <= 0 is forever
	shader_t       *shader;
} shaderState_t;

// Tr3B - shaderProgram_t represents a pair of one
// GLSL vertex and one GLSL fragment shader
typedef struct shaderProgram_s
{

	GLhandleARB     program;
	int             attribs;	// vertex array attributes

	// uniform parameters
	GLint			u_ColorMap;
	GLint			u_ContrastMap;
	GLint           u_DiffuseMap;
	GLint           u_NormalMap;
	GLint			u_SpecularMap;
	GLint			u_LightMap;
	GLint			u_DeluxeMap;
	GLint			u_AttenuationMapXY;
	GLint			u_AttenuationMapZ;

	GLint			u_ViewOrigin;
	
	GLint           u_AmbientColor;
	
	GLint           u_LightDir;
	GLint			u_LightOrigin;
	GLint           u_LightColor;
	GLint           u_LightScale;
	
	GLint			u_SpecularExponent;
	
	GLint			u_RefractionIndex;
	
	GLint			u_FresnelPower;
	GLint			u_FresnelScale;
	GLint			u_FresnelBias;
	
	GLint			u_EtaRatio;
	
	GLint			u_HeightScale;
	GLint			u_HeightBias;
	
	GLint			u_DeformMagnitude;
	GLint			u_BlurMagnitude;
	
	GLint			u_FBufScale;
	GLint			u_NPotScale;
	
	GLint			u_ProjectionMatrixTranspose;
} shaderProgram_t;


// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
typedef struct
{
	int             x, y, width, height;
	float           fov_x, fov_y;
	vec3_t          vieworg;
	vec3_t          viewaxis[3];	// transformation matrix

	int             time;		// time in milliseconds for shader effects and other time dependent rendering issues
	int             rdflags;	// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte            areamask[MAX_MAP_AREA_BYTES];
	qboolean        areamaskModified;	// qtrue if areamask changed since last scene

	float           floatTime;	// tr.refdef.time / 1000.0

	// text messages for deform text shaders
	char            text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	int             numEntities;
	trRefEntity_t  *entities;

	int             numDlights;
	trRefDlight_t  *dlights;

	int             numPolys;
	struct srfPoly_s *polys;

	int             numDrawSurfs;
	struct drawSurf_s *drawSurfs;
	
	int             numInteractions;
	struct interaction_s *interactions;
} trRefdef_t;


//=================================================================================

// skins allow models to be retextured without modifying the model file
typedef struct
{
	char            name[MAX_QPATH];
	shader_t       *shader;
} skinSurface_t;

typedef struct skin_s
{
	char            name[MAX_QPATH];	// game path, including extension
	int             numSurfaces;
	skinSurface_t  *surfaces[MD3_MAX_SURFACES];
} skin_t;


typedef struct
{
	int             originalBrushNumber;
	vec3_t          bounds[2];

	unsigned        colorInt;	// in packed byte format
	float           tcScale;	// texture coordinate vector scales
	fogParms_t      parms;

	// for clipping distance in fog when outside
	qboolean        hasSurface;
	float           surface[4];
} fog_t;

typedef struct
{
	orientationr_t  or;
	orientationr_t  world;
	vec3_t          pvsOrigin;	// may be different than or.origin for portals
	qboolean        isPortal;	// true if this view is through a portal
	qboolean        isMirror;	// the portal is a mirror, invert the face culling
	int             frameSceneNum;	// copied from tr.frameSceneNum
	int             frameCount;	// copied from tr.frameCount
	cplane_t        portalPlane;	// clip anything behind this if mirroring
	int             viewportX, viewportY, viewportWidth, viewportHeight;
	float           fovX, fovY;
	float           projectionMatrix[16];
	frustum_t       frustum;
	vec3_t          visBounds[2];
	float           skyFar;
} viewParms_t;


/*
==============================================================================

SURFACES

==============================================================================
*/

// any changes in surfaceType must be mirrored in tess_surfaceTable[]
typedef enum
{
	SF_BAD,
	SF_SKIP,					// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MDX,
	SF_MD5,
	SF_FLARE,
	SF_ENTITY,					// beams, rails, lightning, etc that can be determined by entity

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0x7fffffff			// ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

typedef struct drawSurf_s
{
	trRefEntity_t  *entity;
	int             shaderNum;
	int             lightmapNum;
	int             fogNum;
	
	surfaceType_t  *surface;	// any of surface*_t
} drawSurf_t;

typedef enum
{
	IA_DEFAULT,					// lighting and shadowing
	IA_SHADOWONLY,
	IA_LIGHTONLY
} interactionType_t;

// an interactionCache is a node between a dlight and a precached world surface
typedef struct interactionCache_s
{
	struct interactionCache_s *next;
	
	struct msurface_s *surface;
	
	int             numLightIndexes;
	int            *lightIndexes;	// precached triangle indices facing light
	
	int             numShadowIndexes;
	int            *shadowIndexes;	// precached triangle indices of shadow edges
} interactionCache_t;

// an interaction is a node between a dlight and any surface
typedef struct interaction_s
{
	struct interaction_s *next;
	
	interactionType_t type;
	
	trRefDlight_t  *dlight;
	shader_t       *dlightShader;
	
	trRefEntity_t  *entity;
	surfaceType_t  *surface;	// any of surface*_t
	shader_t       *surfaceShader;
	
	int             numLightIndexes;
	int            *lightIndexes;	// precached triangle indices facing light
	
	int             numShadowIndexes;
	int            *shadowIndexes;	// precached triangle indices of shadow edges
		
	int             scissorX, scissorY, scissorWidth, scissorHeight;
	
	float			depthNear;			// for GL_EXT_depth_bounds_test
	float           depthFar;
	qboolean		noDepthBoundsTest;
	
	int             occlusionQuerySamples;	// visible fragment count
} interaction_t;

#define	MAX_EDGES	32
typedef struct
{
	int             i2;
	qboolean        facing;
} edge_t;

typedef struct
{
	edge_t          edges[SHADER_MAX_VERTEXES][MAX_EDGES];
	int             numEdges[SHADER_MAX_VERTEXES];
    
    qboolean        facing[SHADER_MAX_INDEXES / 3];
    int				numFacing;
	
	int             numIndexes;
	int             indexes[SHADER_MAX_INDEXES];
} shadowState_t;

extern shadowState_t sh;

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32	// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65	// max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
typedef struct srfPoly_s
{
	surfaceType_t   surfaceType;
	qhandle_t       hShader;
	int             fogIndex;
	int             numVerts;
	polyVert_t     *verts;
} srfPoly_t;

typedef struct srfFlare_s
{
	surfaceType_t   surfaceType;
	vec3_t          origin;
	vec3_t          normal;
	vec3_t          color;
} srfFlare_t;

typedef struct
{
	vec3_t          xyz;
	vec2_t          st;
	vec2_t          lightmap;
	vec3_t          tangent;
	vec3_t          binormal;
	vec3_t          normal;
	byte            color[4];
} srfVert_t;

typedef struct
{
	int             indexes[3];
	int             neighbors[3];
	vec4_t          plane;
} srfTriangle_t;

typedef struct srfGridMesh_s
{
	surfaceType_t   surfaceType;

	// culling information
	vec3_t          meshBounds[2];
	vec3_t          localOrigin;
	float           meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t          lodOrigin;
	float           lodRadius;
	int             lodFixed;
	int             lodStitched;

	// triangle definitions
	int             width, height;
	float          *widthLodError;
	float          *heightLodError;
	
	int             numTriangles;
	srfTriangle_t  *triangles;
	GLuint          indexesVBO;

	int             numVerts;
	srfVert_t      *verts;
	GLuint          vertsVBO;
	GLuint          ofsXYZ;
	GLuint          ofsTexCoords;
	GLuint          ofsTexCoords2;
	GLuint          ofsTangents;
	GLuint          ofsBinormals;
	GLuint          ofsNormals;
	GLuint          ofsColors;
} srfGridMesh_t;

typedef struct
{
	surfaceType_t   surfaceType;
		
	// culling information
	cplane_t        plane;
	vec3_t          bounds[2];

	// triangle definitions
	int             numTriangles;
	srfTriangle_t  *triangles;
	GLuint          indexesVBO;

	int             numVerts;
	srfVert_t      *verts;
	GLuint          vertsVBO;
	GLuint          ofsXYZ;
	GLuint          ofsTexCoords;
	GLuint          ofsTexCoords2;
	GLuint          ofsTangents;
	GLuint          ofsBinormals;
	GLuint          ofsNormals;
	GLuint          ofsColors;
} srfSurfaceFace_t;


// misc_models in maps are turned into direct geometry by q3map
typedef struct
{
	surfaceType_t   surfaceType;

	// culling information (FIXME: use this!)
	vec3_t          bounds[2];
	vec3_t          localOrigin;
	float           radius;

	// triangle definitions
	int             numTriangles;
	srfTriangle_t  *triangles;
	GLuint          indexesVBO;

	int             numVerts;
	srfVert_t      *verts;
	GLuint          vertsVBO;
	GLuint          ofsXYZ;
	GLuint          ofsTexCoords;
	GLuint          ofsTangents;
	GLuint          ofsBinormals;
	GLuint          ofsNormals;
	GLuint          ofsColors;
} srfTriangles_t;


extern void     (*tess_surfaceTable[SF_NUM_SURFACE_TYPES]) (void *, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes);

/*
==============================================================================
BRUSH MODELS - in memory representation
==============================================================================
*/
typedef struct msurface_s
{
	int             viewCount;	// if == tr.viewCount, already added
	int             lightCount;
	struct shader_s *shader;
	int             lightmapNum;	// -1 = no lightmap
	int             fogIndex;

	surfaceType_t  *data;		// any of srf*_t
} msurface_t;


#define	CONTENTS_NODE		-1
typedef struct mnode_s
{
	// common with leaf and node
	int             contents;	// -1 for nodes, to differentiate from leafs
	int             visCount;	// node needs to be traversed if current
	int             lightCount;
	vec3_t          mins, maxs;	// for bounding box culling
	struct mnode_s *parent;

	// node specific
	cplane_t       *plane;
	struct mnode_s *children[2];

	// leaf specific
	int             cluster;
	int             area;

	msurface_t    **firstmarksurface;
	int             nummarksurfaces;
} mnode_t;

typedef struct
{
	vec3_t          bounds[2];	// for culling
	msurface_t     *firstSurface;
	int             numSurfaces;
} bmodel_t;

typedef struct
{
	char            name[MAX_QPATH];	// ie: maps/tim_dm2.bsp
	char            baseName[MAX_QPATH];	// ie: tim_dm2

	int             dataSize;

	int             numShaders;
	dshader_t      *shaders;

	bmodel_t       *bmodels;

	int             numplanes;
	cplane_t       *planes;

	int             numnodes;	// includes leafs
	int             numDecisionNodes;
	mnode_t        *nodes;

	int             numsurfaces;
	msurface_t     *surfaces;

	int             nummarksurfaces;
	msurface_t    **marksurfaces;

	int             numfogs;
	fog_t          *fogs;

	vec3_t          lightGridOrigin;
	vec3_t          lightGridSize;
	vec3_t          lightGridInverseSize;
	int             lightGridBounds[3];
	byte           *lightGridData;
	
	int             numDlights;
	trRefDlight_t  *dlights;
	
	int             numInteractions; // should be always numSurfaces * 32
	interactionCache_t  *interactions;

	int             numClusters;
	int             clusterBytes;
	const byte     *vis;		// may be passed in by CM_LoadMap to save space

	byte           *novis;		// clusterBytes of 0xff

	char           *entityString;
	char           *entityParsePoint;
} world_t;



/*
==============================================================================

MDX MODELS - meta format for .md2, .md3, .mdc and so on

==============================================================================
*/
typedef struct
{
	float           bounds[2][3];
	float           localOrigin[3];
	float           radius;
} mdxFrame_t;

typedef struct
{
	char            name[MAX_QPATH];	// tag name
	float           origin[3];
	float           axis[3][3];
} mdxTag_t;

typedef struct
{
	short           xyz[3];
} mdxVertex_t;

typedef struct
{
	float           st[2];
} mdxSt_t;

typedef struct
{
	int             shaderIndex;	// for in-game use
} mdxShader_t;

typedef struct
{
	surfaceType_t   surfaceType;

	char            name[MAX_QPATH];	// polyset name

	int             numShaders;	// all surfaces in a model should have the same
	mdxShader_t    *shaders;
	
	int             numVerts;
	mdxVertex_t    *verts;
	mdxSt_t        *st;

	int             numTriangles;
	srfTriangle_t  *triangles;
	
	struct mdxModel_s *model;
} mdxSurface_t;

typedef struct mdxModel_s
{
	int             numFrames;
	mdxFrame_t     *frames;
	
	int             numTags;
	mdxTag_t       *tags;
	
	int             numSurfaces;
	mdxSurface_t   *surfaces;

	int             numSkins;
} mdxModel_t;


/*
==============================================================================

MD5 MODELS - in memory representation

==============================================================================
*/
#define MD5_IDENTSTRING     "MD5Version"
#define MD5_VERSION			10
#define	MD5_MAX_BONES		128

typedef struct
{
	int             boneIndex;	// these are indexes into the boneReferences,
	float           boneWeight;	// not the global per-frame bone list
	float           offset[3];
} md5Weight_t;

typedef struct
{
	float           texCoords[2];
	int             firstWeight;
	int             numWeights;
	md5Weight_t   **weights;
} md5Vertex_t;

/*
typedef struct
{
	int             indexes[3];
	int             neighbors[3];
} md5Triangle_t;
*/

typedef struct
{
	surfaceType_t   surfaceType;

//	char            name[MAX_QPATH];	// polyset name
	char            shader[MAX_QPATH];
	int             shaderIndex;	// for in-game use
	
	int             numVerts;
	md5Vertex_t    *verts;

	int             numTriangles;
	srfTriangle_t  *triangles;
	
	int             numWeights;
	md5Weight_t    *weights;
	
	struct md5Model_s *model;
} md5Surface_t;

typedef struct
{
	char			name[MAX_QPATH];
	int				parentIndex; // parent index (-1 if root)
#ifdef USE_BONEMATRIX
	matrix_t		transform;
#else
	vec3_t			origin;
	quat_t			rotation;
#endif
} md5Bone_t;

typedef struct md5Model_s
{
	int             numBones;
	md5Bone_t      *bones;
	
	int             numSurfaces;
	md5Surface_t   *surfaces;
} md5Model_t;


typedef enum
{
	AT_BAD,
	AT_MD5
} animType_t;

enum
{
	COMPONENT_BIT_TX	= 1 << 0,
	COMPONENT_BIT_TY	= 1 << 1,
	COMPONENT_BIT_TZ	= 1 << 2,
	COMPONENT_BIT_QX	= 1 << 3,
	COMPONENT_BIT_QY	= 1 << 4,
	COMPONENT_BIT_QZ	= 1 << 5
};

typedef struct
{
	char			name[MAX_QPATH];
	int				parentIndex;
	
	int				componentsBits;		// e.g. (COMPONENT_BIT_TX | COMPONENT_BIT_TY | COMPONENT_BIT_TZ)
	int				componentsOffset;
		
	vec3_t			baseOrigin;
	quat_t			baseQuat;
} md5Channel_t;

typedef struct
{
	vec3_t          bounds[2];	// bounds of all surfaces of all LOD's for this frame
	float          *components; // numAnimatedComponents many
} md5Frame_t;

typedef struct md5Animation_s
{
	char            name[MAX_QPATH];	// game path, including extension
	animType_t		type;
	int             index;				// anim = tr.animations[anim->index]
	
	int             numFrames;
	md5Frame_t     *frames;
	
	int             numChannels;	// same as numBones in model
	md5Channel_t   *channels;
	
	int             frameRate;
	
	int             numAnimatedComponents;
	
//	struct md5Animation_s *next;
} md5Animation_t;

//======================================================================

typedef enum
{
	MOD_BAD,
	MOD_BRUSH,
	MOD_MDX,
	MOD_MD5
} modtype_t;

typedef struct model_s
{
	char            name[MAX_QPATH];
	modtype_t       type;
	int             index;		// model = tr.models[model->index]

	int             dataSize;	// just for listing purposes
	bmodel_t       *bmodel;		// only if type == MOD_BRUSH
	mdxModel_t     *mdx[MD3_MAX_LODS];	// only if type == MOD_MD3
	md5Model_t     *md5;		// only if type == MOD_MD5

	int             numLods;
} model_t;

void            R_ModelInit(void);
model_t        *R_GetModelByHandle(qhandle_t hModel);

int             RE_LerpTag(orientation_t * tag, qhandle_t handle, int startFrame, int endFrame,
						  float frac, const char *tagName);

int             RE_BoneIndex(qhandle_t hModel, const char *boneName);

void            R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs);

void            R_Modellist_f(void);

//====================================================

extern refimport_t ri;

#define	MAX_MOD_KNOWN			1024
#define	MAX_ANIMATIONFILES		4096

#define	MAX_DRAWIMAGES			4096
#define	MAX_LIGHTMAPS			256
#define	MAX_SKINS				1024


#define	MAX_DRAWSURFS			0x10000
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

#define MAX_INTERACTIONS		MAX_DRAWSURFS*16
#define INTERACTION_MASK		(MAX_INTERACTIONS-1)

extern int      gl_filter_min, gl_filter_max;

/*
** performanceCounters_t
*/
typedef struct
{
	int             c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int             c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;

	int             c_sphere_cull_mdx_in, c_sphere_cull_mdx_clip, c_sphere_cull_mdx_out;
	int             c_box_cull_mdx_in, c_box_cull_mdx_clip, c_box_cull_mdx_out;

	int             c_box_cull_md5_in, c_box_cull_md5_clip, c_box_cull_md5_out;

	int             c_box_cull_dlight_in, c_box_cull_dlight_clip, c_box_cull_dlight_out;
	int             c_box_cull_slight_in, c_box_cull_slight_clip, c_box_cull_slight_out;
	int             c_pvs_cull_slight_out;

	int             c_leafs;
	
	int             c_slights;
	int             c_slightSurfaces;
	int             c_slightInteractions;
	
	int             c_dlights;
	int             c_dlightSurfaces;
	int             c_dlightSurfacesCulled;
	int             c_dlightInteractions;
	
	int             c_depthBoundsTests, c_depthBoundsTestsRejected;
} frontEndCounters_t;

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)

// the renderer front end should never modify glstate_t
typedef struct
{
	int             currenttextures[32];
	int             currenttmu;
	qboolean        finishCalled;
	int             texEnv[2];
	int             faceCulling;
	unsigned long   glStateBits;
	unsigned long	glClientStateBits;
	GLhandleARB		currentProgram;
	frameBuffer_t  *currentFBO;
} glstate_t;

typedef struct
{
	int             c_batches;
	int             c_surfaces;
	int             c_vertexes;
	int             c_indexes;
	int             c_drawElements;
	float           c_overDraw;
	int             c_vboVertexBuffers;
	int             c_vboIndexBuffers;
	int             c_vboVertexes;
	int             c_vboIndexes;

	int             c_dlightVertexes;
	int             c_dlightIndexes;
	
	int             c_shadowBatches;
	int             c_shadowSurfaces;
	int             c_shadowVertexes;
	int             c_shadowIndexes;
	
	int				c_fogSurfaces;
	int				c_fogBatches;

	int             c_flareAdds;
	int             c_flareTests;
	int             c_flareRenders;
	
	int             c_occlusionQueries;
	int             c_occlusionQueriesAvailable;
	int             c_occlusionQueriesCulled;

	int             msec;		// total msec for backend run
} backEndCounters_t;

// all state modified by the back end is seperated
// from the front end state
typedef struct
{
	int             smpFrame;
	trRefdef_t      refdef;
	viewParms_t     viewParms;
	orientationr_t  or;
	backEndCounters_t pc;
	qboolean        isHyperspace;
	trRefEntity_t  *currentEntity;
	trRefDlight_t  *currentLight;		// only used when lighting interactions
	qboolean        skyRenderedThisView;	// flag for drawing sun

	qboolean        projection2D;	// if qtrue, drawstretchpic doesn't need to change modes
	byte            color2D[4];
	qboolean        vertexes2D;	// shader needs to be finished
	trRefEntity_t   entity2D;	// currentEntity will point at this when doing 2D rendering
} backEndState_t;

/*
** trGlobals_t 
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
typedef struct
{
	qboolean        registered;	// cleared at shutdown, set at beginRegistration

	int             visCount;	// incremented every time a new vis cluster is entered
	int             frameCount;	// incremented every frame
	int             sceneCount;	// incremented every scene
	int             viewCount;	// incremented every view (twice a scene if portaled)
	int             lightCount; // incremented every time a light traverses the world and every R_MarkFragments call

	int             smpFrame;	// toggles from 0 to 1 every endFrame

	int             frameSceneNum;	// zeroed at RE_BeginFrame

	qboolean        worldMapLoaded;
	qboolean        worldDeluxeMapping;
	world_t        *world;

	const byte     *externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t        *defaultImage;
	image_t        *scratchImage[32];
	image_t        *fogImage;
	image_t        *flareImage;
	image_t        *whiteImage;	// full of 0xff
	image_t        *blackImage;	// full of 0x0
	image_t        *flatImage; // use this as default normalmap
	image_t        *identityLightImage;	// full of tr.identityLightByte
	image_t        *noFalloffImage;
	image_t        *attenuationXYImage;
	image_t        *currentRenderImage;
	image_t        *currentRenderLinearImage;
	image_t        *currentRenderNearestImage;

	// internal shaders
	shader_t       *defaultShader;
	shader_t       *defaultPointLightShader;
	shader_t       *defaultProjectedLightShader;
	shader_t       *defaultDynamicLightShader;
	
	// external shaders
	shader_t       *projectionShadowShader;
	shader_t       *flareShader;
	shader_t       *sunShader;

	int             numLightmaps;
	image_t        *lightmaps[MAX_LIGHTMAPS];

	// render entities
	trRefEntity_t  *currentEntity;
	trRefEntity_t   worldEntity;	// point currentEntity at this when rendering world
	model_t        *currentModel;
	
	// render lights
	trRefDlight_t  *currentDlight;


	//
	// GPU shader programs
	//
	
	// Q3A standard simple vertex color rendering
	shaderProgram_t genericShader_single;
	
	// directional lighting
	shaderProgram_t lightShader_D_direct;
	shaderProgram_t lightShader_DB_direct;
	shaderProgram_t lightShader_DBS_direct;

	// directional light mapping
	shaderProgram_t lightShader_D_radiosity;
	shaderProgram_t lightShader_DB_radiosity;
	shaderProgram_t lightShader_DBS_radiosity;

	// black depth fill rendering with textures
	shaderProgram_t depthFillShader;
	
	// stencil shadow volume extrusion
	shaderProgram_t shadowShader;

	// Doom3 style omni-directional multi-pass lighting 
	shaderProgram_t lightShader_D_omni;
	shaderProgram_t lightShader_DB_omni;
	shaderProgram_t lightShader_DBS_omni;
	shaderProgram_t lightShader_D_proj;
	
	// environment mapping effects
	shaderProgram_t reflectionShader_C;
	shaderProgram_t refractionShader_C;
	shaderProgram_t dispersionShader_C;
	shaderProgram_t skyBoxShader;
	
	// post process effects
	shaderProgram_t heatHazeShader;
	shaderProgram_t glowShader;
	shaderProgram_t bloomShader;
	shaderProgram_t contrastShader;
	shaderProgram_t blurXShader;
	shaderProgram_t blurYShader;

	viewParms_t     viewParms;

	float           identityLight;	// 1.0 / ( 1 << overbrightBits )
	int             identityLightByte;	// identityLight * 255
	int             overbrightBits;	// r_overbrightBits->integer, but set to 0 if no hw gamma

	orientationr_t  or;			// for current entity

	trRefdef_t      refdef;

	int             viewCluster;

	vec3_t          sunLight;	// from the sky shader for this level
	vec3_t          sunDirection;

	frontEndCounters_t pc;
	int             frontEndMsec;	// not in pc due to clearing issue

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	model_t        *models[MAX_MOD_KNOWN];
	int             numModels;
	
	int             numAnimations;
	md5Animation_t *animations[MAX_ANIMATIONFILES];

	int             numImages;
	image_t        *images[MAX_DRAWIMAGES];
	
	int             numFBOs;
	frameBuffer_t  *fbos[MAX_FBOS];
	
	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	int             numShaders;
	shader_t       *shaders[MAX_SHADERS];
	shader_t       *sortedShaders[MAX_SHADERS];

	int             numSkins;
	skin_t         *skins[MAX_SKINS];
	
	int             numTables;
	shaderTable_t  *shaderTables[MAX_SHADER_TABLES];

	float           sinTable[FUNCTABLE_SIZE];
	float           squareTable[FUNCTABLE_SIZE];
	float           triangleTable[FUNCTABLE_SIZE];
	float           sawToothTable[FUNCTABLE_SIZE];
	float           inverseSawToothTable[FUNCTABLE_SIZE];
	float           fogTable[FOG_TABLE_SIZE];
	
	int				occlusionQueryObjects[MAX_OCCLUSION_QUERIES];
} trGlobals_t;

extern const matrix_t  quakeToOpenGLMatrix;
extern const matrix_t  openGLToQuakeMatrix;

extern backEndState_t backEnd;
extern trGlobals_t tr;
extern glConfig_t glConfig;		// outside of TR since it shouldn't be cleared during ref re-init
extern glConfig2_t glConfig2;	// outside of TR since it shouldn't be cleared during ref re-init
extern glstate_t glState;		// outside of TR since it shouldn't be cleared during ref re-init


//
// cvars
//
extern cvar_t  *r_flareSize;
extern cvar_t  *r_flareFade;
extern cvar_t  *r_flareCoeff;

extern cvar_t  *r_railWidth;
extern cvar_t  *r_railCoreWidth;
extern cvar_t  *r_railSegmentLength;

extern cvar_t  *r_ignore;		// used for debugging anything
extern cvar_t  *r_verbose;		// used for verbose debug spew

extern cvar_t  *r_znear;		// near Z clip plane
extern cvar_t  *r_zfar;

extern cvar_t  *r_stencilbits;	// number of desired stencil bits
extern cvar_t  *r_depthbits;	// number of desired depth bits
extern cvar_t  *r_colorbits;	// number of desired color bits, only relevant for fullscreen
extern cvar_t  *r_stereo;		// desired pixelformat stereo flag
extern cvar_t  *r_texturebits;	// number of desired texture bits

										// 0 = use framebuffer depth
										// 16 = use 16-bit textures
										// 32 = use 32-bit textures
										// all else = error

extern cvar_t  *r_measureOverdraw;	// enables stencil buffer overdraw measurement

extern cvar_t  *r_lodbias;		// push/pull LOD transitions
extern cvar_t  *r_lodscale;
										
extern cvar_t  *r_ambientScale;
extern cvar_t  *r_directedScale;
extern cvar_t  *r_lightScale;

extern cvar_t  *r_inGameVideo;		// controls whether in game video should be draw
extern cvar_t  *r_fastsky;			// controls whether sky should be cleared or drawn
extern cvar_t  *r_drawSun;			// controls drawing of sun quad
extern cvar_t  *r_drawBloom;		// controls drawing of bloom post process effect
extern cvar_t  *r_bloomBlur;
extern cvar_t  *r_lighting;			// lighting mode, 0 = diffuse, 1 = bump mapping and so on
extern cvar_t  *r_dynamicLighting;	// dynamic lights enabled/disabled

extern cvar_t  *r_norefresh;		// bypasses the ref rendering
extern cvar_t  *r_drawentities;		// disable/enable entity rendering
extern cvar_t  *r_drawworld;		// disable/enable world rendering
extern cvar_t  *r_speeds;			// various levels of information display
extern cvar_t  *r_detailTextures;	// enables/disables detail texturing stages
extern cvar_t  *r_novis;			// disable/enable usage of PVS
extern cvar_t  *r_nocull;
extern cvar_t  *r_nocurves;
extern cvar_t  *r_nobatching;
extern cvar_t  *r_noLightScissors;
extern cvar_t  *r_noLightVisCull;
extern cvar_t  *r_noInteractionSort;
extern cvar_t  *r_showcluster;

extern cvar_t  *r_mode;				// video mode
extern cvar_t  *r_fullscreen;
extern cvar_t  *r_gamma;
extern cvar_t  *r_displayRefresh;	// optional display refresh option
extern cvar_t  *r_ignorehwgamma;	// overrides hardware gamma capabilities

extern cvar_t  *r_allowExtensions;	// global enable/disable of OpenGL extensions
extern cvar_t  *r_ext_compressed_textures;	// these control use of specific extensions
extern cvar_t  *r_ext_multitexture;
extern cvar_t  *r_ext_compiled_vertex_array;
extern cvar_t  *r_ext_texture_env_add;
extern cvar_t  *r_ext_multisample;
extern cvar_t  *r_ext_texture_non_power_of_two;
extern cvar_t  *r_ext_texture_cube_map;
extern cvar_t  *r_ext_depth_texture;
extern cvar_t  *r_ext_vertex_program;
extern cvar_t  *r_ext_vertex_buffer_object;
extern cvar_t  *r_ext_occlusion_query;
extern cvar_t  *r_ext_shading_language_100;
extern cvar_t  *r_ext_stencil_wrap;
extern cvar_t  *r_ext_texture_filter_anisotropic;
extern cvar_t  *r_ext_stencil_two_side;
extern cvar_t  *r_ext_depth_bounds_test;
extern cvar_t  *r_ext_framebuffer_object;
extern cvar_t  *r_ext_generate_mipmap;

extern cvar_t  *r_nobind;		// turns off binding to appropriate textures
extern cvar_t  *r_collapseStages;
extern cvar_t  *r_singleShader;	// make most world faces use default shader
extern cvar_t  *r_roundImagesDown;
extern cvar_t  *r_colorMipLevels;	// development aid to see texture mip usage
extern cvar_t  *r_picmip;		// controls picmip values
extern cvar_t  *r_finish;
extern cvar_t  *r_drawBuffer;
extern cvar_t  *r_swapInterval;
extern cvar_t  *r_textureMode;
extern cvar_t  *r_offsetFactor;
extern cvar_t  *r_offsetUnits;
extern cvar_t  *r_specularExponent;

extern cvar_t  *r_fullbright;	// avoid lightmap pass
extern cvar_t  *r_uiFullScreen;	// ui is running fullscreen

extern cvar_t  *r_logFile;		// number of frames to emit GL logs
extern cvar_t  *r_showtris;		// enables wireframe rendering of the world
extern cvar_t  *r_showsky;		// forces sky in front of all surfaces
extern cvar_t  *r_shownormals;	// draws wireframe normals
extern cvar_t  *r_showTangentSpaces;	// draws wireframe tangents, binormals and normals
extern cvar_t  *r_clear;		// force screen clear every frame

extern cvar_t  *r_shadows;		// controls shadows: 0 = none, 1 = blur, 2 = planar, 3 = robust stencil shadow volumes
								
extern cvar_t  *r_shadowOffsetFactor;
extern cvar_t  *r_shadowOffsetUnits;
extern cvar_t  *r_noLightFrustums;
extern cvar_t  *r_drawFlares;	// light flares

extern cvar_t  *r_intensity;

extern cvar_t  *r_lockpvs;
extern cvar_t  *r_noportals;
extern cvar_t  *r_portalOnly;

extern cvar_t  *r_subdivisions;
extern cvar_t  *r_smp;
extern cvar_t  *r_showSmp;
extern cvar_t  *r_skipBackEnd;

extern cvar_t  *r_ignoreGLErrors;

extern cvar_t  *r_overBrightBits;
extern cvar_t  *r_mapOverBrightBits;

extern cvar_t  *r_simpleMipMaps;

extern cvar_t  *r_showImages;
extern cvar_t  *r_debugLight;
extern cvar_t  *r_debugSort;

extern cvar_t  *r_showLightMaps;		// render lightmaps only
extern cvar_t  *r_showDeluxeMaps;
extern cvar_t  *r_showNormalMaps;
extern cvar_t  *r_showShadowVolumes;
extern cvar_t  *r_showSkeleton;
extern cvar_t  *r_showEntityTransforms;
extern cvar_t  *r_showLightTransforms;
extern cvar_t  *r_showLightInteractions;
extern cvar_t  *r_showLightScissors;
extern cvar_t  *r_showOcclusionQueries;

extern cvar_t  *r_vboFaces;
extern cvar_t  *r_vboCurves;
extern cvar_t  *r_vboTriangles;

extern float	displayAspect;

//====================================================================

float           R_NoiseGet4f(float x, float y, float z, float t);
void            R_NoiseInit(void);

void            R_SwapBuffers(int);

void            R_RenderView(viewParms_t * parms);

void            R_AddMDXSurfaces(trRefEntity_t * e);
void            R_AddMDXInteractions(trRefEntity_t * e, trRefDlight_t * light);
void            R_AddNullModelSurfaces(trRefEntity_t * e);
void            R_AddBeamSurfaces(trRefEntity_t * e);
void            R_AddRailSurfaces(trRefEntity_t * e, qboolean isUnderwater);
void            R_AddLightningBoltSurfaces(trRefEntity_t * e);

void            R_AddPolygonSurfaces(void);

void            R_AddDrawSurf(surfaceType_t * surface, shader_t * shader, int lightmapIndex, int fogIndex);


void            R_LocalNormalToWorld(const vec3_t local, vec3_t world);
void            R_LocalPointToWorld(const vec3_t local, vec3_t world);

int             R_CullLocalBox(vec3_t bounds[2]);
int             R_CullLocalPointAndRadius(vec3_t origin, float radius);
int             R_CullPointAndRadius(vec3_t origin, float radius);

int             R_FogLocalPointAndRadius(const vec3_t pt, float radius);
int             R_FogPointAndRadius(const vec3_t pt, float radius);
int             R_FogWorldBox(vec3_t bounds[2]);

void            R_SetupEntityWorldBounds(trRefEntity_t * ent);

void            R_RotateForEntity(const trRefEntity_t * ent, const viewParms_t * viewParms, orientationr_t * or);
void            R_RotateForDlight(const trRefDlight_t * ent, const viewParms_t * viewParms, orientationr_t * or);


void            R_CalcNormalForTriangle(vec3_t normal, const vec3_t v0, const vec3_t v1, const vec3_t v2);

void            R_CalcTangentsForTriangle(vec3_t tangent, vec3_t binormal,
										  const vec3_t v0, const vec3_t v1, const vec3_t v2,
										  const vec2_t t0, const vec2_t t1, const vec2_t t2);

void            R_CalcTangentSpace(vec3_t tangent, vec3_t binormal, vec3_t normal,
								   const vec3_t v0, const vec3_t v1, const vec3_t v2,
								   const vec2_t t0, const vec2_t t1, const vec2_t t2,
								   const vec3_t n);

void            R_CalcSurfaceTriangleNeighbors(int numTriangles, srfTriangle_t * triangles);
void            R_CalcSurfaceTrianglePlanes(int numTriangles, srfTriangle_t * triangles, srfVert_t * verts);

float           R_CalcFov(float fovX, float width, float height);

// Tr3B - visualisation tools to help debugging the renderer frontend
void            R_DebugAxis(const vec3_t origin, const matrix_t transformMatrix);
void            R_DebugBoundingBox(const vec3_t origin, const vec3_t mins, const vec3_t maxs, vec4_t color);

/*
** GL wrapper/helper functions
*/
void            GL_Bind(image_t * image);
void            GL_Program(GLhandleARB program);
void            GL_SetDefaultState(void);
void            GL_SelectTexture(int unit);
void            GL_TextureMode(const char *string);

void            GL_CheckErrors_(const char *filename, int line);
#define         GL_CheckErrors()	GL_CheckErrors_(__FILE__, __LINE__)

void            GL_State(unsigned long stateVector);
void            GL_ClientState(unsigned long stateBits);
void            GL_TexEnv(int env);
void            GL_Cull(int cullType);

enum
{
	GLS_SRCBLEND_ZERO					= (1 << 0),
	GLS_SRCBLEND_ONE					= (1 << 1),
	GLS_SRCBLEND_DST_COLOR				= (1 << 2),
	GLS_SRCBLEND_ONE_MINUS_DST_COLOR	= (1 << 3),
	GLS_SRCBLEND_SRC_ALPHA				= (1 << 4),
	GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA	= (1 << 5),
	GLS_SRCBLEND_DST_ALPHA				= (1 << 6),
	GLS_SRCBLEND_ONE_MINUS_DST_ALPHA	= (1 << 7),
	GLS_SRCBLEND_ALPHA_SATURATE			= (1 << 8),
	
	GLS_SRCBLEND_BITS					= GLS_SRCBLEND_ZERO
											| GLS_SRCBLEND_ONE
											| GLS_SRCBLEND_DST_COLOR
											| GLS_SRCBLEND_ONE_MINUS_DST_COLOR
											| GLS_SRCBLEND_SRC_ALPHA
											| GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA
											| GLS_SRCBLEND_DST_ALPHA
											| GLS_SRCBLEND_ONE_MINUS_DST_ALPHA
											| GLS_SRCBLEND_ALPHA_SATURATE,
	
	GLS_DSTBLEND_ZERO					= (1 << 9),
	GLS_DSTBLEND_ONE					= (1 << 10),
	GLS_DSTBLEND_SRC_COLOR				= (1 << 11),
	GLS_DSTBLEND_ONE_MINUS_SRC_COLOR	= (1 << 12),
	GLS_DSTBLEND_SRC_ALPHA				= (1 << 13),
	GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA	= (1 << 14),
	GLS_DSTBLEND_DST_ALPHA				= (1 << 15),
	GLS_DSTBLEND_ONE_MINUS_DST_ALPHA	= (1 << 16),
	
	GLS_DSTBLEND_BITS					= GLS_DSTBLEND_ZERO
											| GLS_DSTBLEND_ONE
											| GLS_DSTBLEND_SRC_COLOR
											| GLS_DSTBLEND_ONE_MINUS_SRC_COLOR
											| GLS_DSTBLEND_SRC_ALPHA
											| GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA
											| GLS_DSTBLEND_DST_ALPHA
											| GLS_DSTBLEND_ONE_MINUS_DST_ALPHA,

	GLS_DEPTHMASK_TRUE					= (1 << 17),

	GLS_POLYMODE_LINE					= (1 << 18),

	GLS_DEPTHTEST_DISABLE				= (1 << 19),
	
	GLS_DEPTHFUNC_LESS					= (1 << 20),
	GLS_DEPTHFUNC_EQUAL					= (1 << 21),
	
	GLS_DEPTHFUNC_BITS					= GLS_DEPTHFUNC_LESS
											| GLS_DEPTHFUNC_EQUAL,

	GLS_ATEST_GT_0						= (1 << 22),
	GLS_ATEST_LT_80						= (1 << 23),
	GLS_ATEST_GE_80						= (1 << 24),
	GLS_ATEST_GT_CUSTOM					= (1 << 25),

	GLS_ATEST_BITS						= GLS_ATEST_GT_0
											| GLS_ATEST_LT_80
											| GLS_ATEST_GE_80
											| GLS_ATEST_GT_CUSTOM,
	
	GLS_REDMASK_FALSE					= (1 << 26),
	GLS_GREENMASK_FALSE					= (1 << 27),
	GLS_BLUEMASK_FALSE					= (1 << 28),
	GLS_ALPHAMASK_FALSE					= (1 << 29),
	
	GLS_COLORMASK_BITS					= GLS_REDMASK_FALSE
											| GLS_GREENMASK_FALSE
											| GLS_BLUEMASK_FALSE
											| GLS_ALPHAMASK_FALSE,
	
	GLS_STENCILTEST_ENABLE				= (1 << 30),
	
	GLS_DEFAULT							= GLS_DEPTHMASK_TRUE
};

enum
{
	GLCS_VERTEX			= (1 << 0),
	GLCS_TEXCOORD0		= (1 << 1),
	GLCS_TEXCOORD1		= (1 << 2),
	GLCS_TEXCOORD2		= (1 << 3),
	GLCS_TEXCOORD3		= (1 << 4),
	GLCS_TANGENT		= (1 << 5),
	GLCS_BINORMAL		= (1 << 6),
	GLCS_NORMAL			= (1 << 7),
	GLCS_COLOR			= (1 << 8),
	
	GLCS_DEFAULT		= GLCS_VERTEX
};


void            RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte * data, int client,
							  qboolean dirty);
void            RE_UploadCinematic(int w, int h, int cols, int rows, const byte * data, int client,
								   qboolean dirty);

void            RE_BeginFrame(stereoFrame_t stereoFrame);
void            RE_BeginRegistration(glConfig_t * glconfig, glConfig2_t * glconfig2);
void            RE_LoadWorldMap(const char *mapname);
void            RE_SetWorldVisData(const byte * vis);
qhandle_t       RE_RegisterModel(const char *name);
qhandle_t       RE_RegisterSkin(const char *name);
void            RE_Shutdown(qboolean destroyWindow);

qboolean        R_GetEntityToken(char *buffer, int size);

model_t        *R_AllocModel(void);

void            R_Init(void);
image_t        *R_FindImageFile(const char *name, int bits, filterType_t filterType, wrapType_t wrapType);
image_t        *R_FindCubeImage(const char *name, int bits, filterType_t filterType, wrapType_t wrapType);

image_t        *R_CreateImage(const char *name, const byte * pic, int width, int height, int bits, filterType_t filterType, wrapType_t wrapType);
qboolean        R_GetModeInfo(int *width, int *height, float *windowAspect, int mode);

void            R_SetColorMappings(void);
void            R_GammaCorrect(byte * buffer, int bufSize);

void            R_ImageList_f(void);
void            R_SkinList_f(void);

// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=516
const void     *RB_TakeScreenshotCmd(const void *data);
void            R_ScreenShot_f(void);

void            R_InitFogTable(void);
float           R_FogFactor(float s, float t);
void            R_InitImages(void);
void            R_ShutdownImages(void);
int             R_SumOfUsedImages(void);
void            R_InitSkins(void);
skin_t         *R_GetSkinByHandle(qhandle_t hSkin);

void            R_DeleteSurfaceVBOs(void);

//
// tr_shader.c
//
qhandle_t       RE_RegisterShaderLightMap(const char *name, int lightmapIndex);
qhandle_t       RE_RegisterShader(const char *name);
qhandle_t       RE_RegisterShaderNoMip(const char *name);
qhandle_t       RE_RegisterShaderLightAttenuation(const char *name);
qhandle_t       RE_RegisterShaderFromImage(const char *name, image_t * image, qboolean mipRawImage);

shader_t       *R_FindShader(const char *name, shaderType_t type, qboolean mipRawImage);
shader_t       *R_GetShaderByHandle(qhandle_t hShader);
shader_t       *R_GetShaderByState(int index, long *cycleTime);
shader_t       *R_FindShaderByName(const char *name);
void            R_InitShaders(void);
void            R_ShaderList_f(void);
void			R_ShaderExp_f(void);
void            R_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void            GLimp_Init(void);
void            GLimp_Shutdown(void);
void            GLimp_EndFrame(void);

qboolean        GLimp_SpawnRenderThread(void (*function) (void));
void           *GLimp_RendererSleep(void);
void            GLimp_FrontEndSleep(void);
void            GLimp_WakeRenderer(void *data);

void            GLimp_LogComment(char *comment);

// NOTE TTimo linux works with float gamma value, not the gamma table
//   the params won't be used, getting the r_gamma cvar directly
void            GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);


/*
====================================================================
TESSELATOR/SHADER DECLARATIONS
====================================================================
*/

typedef enum
{
	SIT_DEFAULT,
	SIT_LIGHTING,
	SIT_LIGHTING_STENCIL
} stageIteratorType_t;

typedef byte    color4ub_t[4];
typedef float   color4f_t[4];

typedef struct stageVars
{
	color4f_t       color;
	color4ub_t      colors[SHADER_MAX_VERTEXES];
	qboolean		skipCoords[MAX_TEXTURE_BUNDLES];
	vec2_t          texCoords[MAX_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
	matrix_t        texMatrices[MAX_TEXTURE_BUNDLES];
} stageVars_t;

typedef struct shaderCommands_s
{
	glIndex_t       indexes[SHADER_MAX_INDEXES];
	vec4_t          xyz[SHADER_MAX_VERTEXES];
	vec4_t          tangents[SHADER_MAX_VERTEXES];
	vec4_t          binormals[SHADER_MAX_VERTEXES];
	vec4_t          normals[SHADER_MAX_VERTEXES];
	vec2_t          texCoords[SHADER_MAX_VERTEXES][2];
	color4ub_t      colors[SHADER_MAX_VERTEXES];
	
	GLuint          indexesVBO;
	GLuint          vertexesVBO;
	GLuint          ofsXYZ;
	GLuint          ofsTexCoords;
	GLuint          ofsTexCoords2;
	GLuint          ofsTangents;
	GLuint          ofsBinormals;
	GLuint          ofsNormals;
	GLuint          ofsColors;

	stageVars_t     svars;

	color4ub_t      constantColor255[SHADER_MAX_VERTEXES];

	shader_t       *surfaceShader;
	shader_t       *lightShader;
	
	float           shaderTime;
	
	int             lightmapNum;
	
	int             fogNum;
	
	qboolean        skipTangentSpaces;
	qboolean        shadowVolume;

	int             numIndexes;
	int             numVertexes;

	// info extracted from current shader or backend mode
	stageIteratorType_t currentStageIteratorType;
	void            (*currentStageIteratorFunc) (void);
	
	int             numSurfaceStages;
	shaderStage_t **surfaceStages;
} shaderCommands_t;

extern shaderCommands_t tess;

void            Tess_Begin(shader_t * surfaceShader, shader_t * lightShader,
								int lightmapNum,
								int fogNum,
								qboolean skipTangentSpaces,
								qboolean shadowVolume);
void            Tess_End(void);
void            Tess_CheckOverflow(int verts, int indexes);

void            RB_InitGPUShaders(void);
void            RB_ShutdownGPUShaders(void);

void            Tess_StageIteratorLighting(void);
void            Tess_StageIteratorGeneric(void);
void            Tess_StageIteratorSky(void);

void            Tess_AddQuadStamp(vec3_t origin, vec3_t left, vec3_t up, byte * color);
void            Tess_AddQuadStampExt(vec3_t origin, vec3_t left, vec3_t up, byte * color, float s1, float t1,
								   float s2, float t2);

void            RB_ShowImages(void);


/*
============================================================

WORLD MAP

============================================================
*/

void            R_AddBrushModelSurfaces(trRefEntity_t * e);
void            R_AddWorldSurfaces(void);
qboolean        R_inPVS(const vec3_t p1, const vec3_t p2);

void            R_AddWorldInteractions(trRefDlight_t * light);
void            R_AddPrecachedWorldInteractions(trRefDlight_t * light);
void			R_ShutdownVBOs(void);

/*
============================================================

FLARES

============================================================
*/

void            R_ClearFlares(void);

void            RB_AddFlare(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal);
void            RB_AddDlightFlares(void);
void            RB_RenderFlares(void);

/*
============================================================

LIGHTS

============================================================
*/

void            R_AddBrushModelInteractions(trRefEntity_t * ent, trRefDlight_t * light);
void            R_SetupEntityLighting(const trRefdef_t * refdef, trRefEntity_t * ent);
void            R_TransformDlights(int count, trRefDlight_t * dl, orientationr_t * or);
int             R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);

void            R_SetupDlightOrigin(trRefDlight_t * dl);
void            R_SetupDlightLocalBounds(trRefDlight_t * light);
void            R_SetupDlightWorldBounds(trRefDlight_t * light);

void            R_SetupDlightFrustum(trRefDlight_t * dl);
void            R_SetupDlightProjection(trRefDlight_t * dl);

void            R_AddDlightInteraction(trRefDlight_t * light, surfaceType_t * surface, shader_t * surfaceShader,
									   int numLightIndexes, int *lightIndexes,
									   int numShadowIndexes, int *shadowIndexes,
									   interactionType_t iaType);

void            R_SortInteractions(trRefDlight_t * light);
qboolean        R_DlightIntersectsPoint(trRefDlight_t * light, const vec3_t p);

void            R_SetupDlightScissor(trRefDlight_t * light);
void            R_SetupDlightDepthBounds(trRefDlight_t * light);


int				R_CullLightPoint(trRefDlight_t * light, const vec3_t p);
int				R_CullLightTriangle(trRefDlight_t * light, vec3_t verts[3]);
int				R_CullLightWorldBounds(trRefDlight_t * light, vec3_t worldBounds[2]);

/*
============================================================

SKIES

============================================================
*/

void            R_BuildCloudData(shaderCommands_t * shader);
void            R_InitSkyTexCoords(float cloudLayerHeight);
void            R_DrawSkyBox(shaderCommands_t * shader);
void            RB_DrawSun(void);
void            RB_ClipSkyPolygons(shaderCommands_t * shader);

/*
============================================================

CURVE TESSELATION

============================================================
*/

//#define PATCH_STITCHING

srfGridMesh_t  *R_SubdividePatchToGrid(int width, int height, srfVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE]);
srfGridMesh_t  *R_GridInsertColumn(srfGridMesh_t * grid, int column, int row, vec3_t point, float loderror);
srfGridMesh_t  *R_GridInsertRow(srfGridMesh_t * grid, int row, int column, vec3_t point, float loderror);
void            R_FreeSurfaceGridMesh(srfGridMesh_t * grid);

/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/

int             R_MarkFragments(int numPoints, const vec3_t * points, const vec3_t projection,
								int maxPoints, vec3_t pointBuffer, int maxFragments,
								markFragment_t * fragmentBuffer);
								

/*
============================================================

FRAME BUFFER OBJECTS

============================================================
*/
frameBuffer_t  *R_CreateFBO(const char *name, int width, int height);

void            R_CreateFBOColorBuffer(frameBuffer_t * fbo, int format, int index);
void            R_CreateFBODepthBuffer(frameBuffer_t * fbo, int format);
void            R_CreateFBOStencilBuffer(frameBuffer_t * fbo, int format);

void            R_AttachFBOTexture1D(int texId, int attachmentIndex);
void            R_AttachFBOTexture2D(int target, int texId, int attachmentIndex);
void            R_AttachFBOTexture3D(int texId, int attachmentIndex, int zOffset);
void            R_AttachFBOTextureDepth(int texId);

void            R_BindFBO(frameBuffer_t * fbo);
void            R_BindNullFBO(void);

void            R_InitFBOs(void);
void            R_ShutdownFBOs(void);
void            R_FBOList_f(void);


/*
============================================================

SCENE GENERATION

============================================================
*/

void            R_ToggleSmpFrame(void);

void            RE_ClearScene(void);
void            RE_AddRefEntityToScene(const refEntity_t * ent);
void            RE_AddRefDlightToScene(const refLight_t * light);
void            RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t * verts, int num);
void            RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void            RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void            RE_RenderScene(const refdef_t * fd);

/*
=============================================================

ANIMATED MODELS

=============================================================
*/

void            R_InitAnimations(void);
qhandle_t       RE_RegisterAnimation(const char *name);
md5Animation_t *R_GetAnimationByHandle(qhandle_t hAnim);
void            R_AnimationList_f(void);

void            R_AddMDSSurfaces(trRefEntity_t * ent);

void            R_AddMD5Surfaces(trRefEntity_t * ent);
void            R_AddMD5Interactions(trRefEntity_t * ent, trRefDlight_t * light);

int             RE_BuildSkeleton(refSkeleton_t * skel, qhandle_t anim, int startFrame, int endFrame, float frac,
								 qboolean clearOrigin);
int             RE_BlendSkeleton(refSkeleton_t * skel, const refSkeleton_t * blend, float frac);
int             RE_AnimNumFrames(qhandle_t hAnim);
int             RE_AnimFrameRate(qhandle_t hAnim);

/*
=============================================================
=============================================================
*/

void            R_TransformWorldToClip(const vec3_t src, const float *cameraViewMatrix,
									   const float *projectionMatrix, vec4_t eye, vec4_t dst);
void            R_TransformModelToClip(const vec3_t src, const float *modelViewMatrix,
									   const float *projectionMatrix, vec4_t eye, vec4_t dst);
void            R_TransformClipToWindow(const vec4_t clip, const viewParms_t * view, vec4_t normalized, vec4_t window);

void            RB_DeformTessGeometry(void);

float			RB_EvalExpression(const expression_t * exp, float defaultValue);

void            RB_CalcEnvironmentTexCoords(float *dstTexCoords);
void            RB_CalcFogTexCoords(float *dstTexCoords);
void            RB_CalcScrollTexCoords(const float scroll[2], float *dstTexCoords);
void            RB_CalcRotateTexCoords(float rotSpeed, float *dstTexCoords);
void            RB_CalcScaleTexCoords(const float scale[2], float *dstTexCoords);
void            RB_CalcTurbulentTexCoords(const waveForm_t * wf, float *dstTexCoords);
void            RB_CalcTransformTexCoords(const texModInfo_t * tmi, float *dstTexCoords);
void            RB_CalcScrollTexCoords2(const expression_t * sExp, const expression_t * tExp, float *dstTexCoords);
void            RB_CalcScaleTexCoords2(const expression_t * sExp, const expression_t * tExp, float *dstTexCoords);
void            RB_CalcCenterScaleTexCoords(const expression_t * sExp, const expression_t * tExp, float *dstTexCoords);
void            RB_CalcShearTexCoords(const expression_t * sExp, const expression_t * tExp, float *dstTexCoords);
void            RB_CalcRotateTexCoords2(const expression_t * rExp, float *dstTexCoords);

void            RB_CalcModulateColorsByFog(unsigned char *dstColors);
void            RB_CalcModulateAlphasByFog(unsigned char *dstColors);
void            RB_CalcModulateRGBAsByFog(unsigned char *dstColors);
void            RB_CalcWaveAlpha(const waveForm_t * wf, unsigned char *dstColors);
void            RB_CalcWaveColor(const waveForm_t * wf, unsigned char *dstColors);
void            RB_CalcAlphaFromEntity(unsigned char *dstColors);
void            RB_CalcAlphaFromOneMinusEntity(unsigned char *dstColors);
void            RB_CalcStretchTexCoords(const waveForm_t * wf, float *texCoords);
void            RB_CalcColorFromEntity(unsigned char *dstColors);
void            RB_CalcColorFromOneMinusEntity(unsigned char *dstColors);
void            RB_CalcSpecularAlpha(unsigned char *alphas);
void            RB_CalcDiffuseColor(unsigned char *colors);
void			RB_CalcCustomColor(const expression_t * rgbExp, unsigned char *dstColors);
void			RB_CalcCustomColors(const expression_t * redExp, const expression_t * greenExp, const expression_t * blueExp, unsigned char *dstColors);
void			RB_CalcCustomAlpha(const expression_t * alphaExp, unsigned char *dstColors);

/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/

void            RB_RenderThread(void);
void            RB_ExecuteRenderCommands(const void *data);

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x40000

typedef struct
{
	byte            cmds[MAX_RENDER_COMMANDS];
	int             used;
} renderCommandList_t;

typedef struct
{
	int             commandId;
	float           color[4];
} setColorCommand_t;

typedef struct
{
	int             commandId;
	int             buffer;
} drawBufferCommand_t;

typedef struct
{
	int             commandId;
	image_t        *image;
	int             width;
	int             height;
	void           *data;
} subImageCommand_t;

typedef struct
{
	int             commandId;
} swapBuffersCommand_t;

typedef struct
{
	int             commandId;
	int             buffer;
} endFrameCommand_t;

typedef struct
{
	int             commandId;
	shader_t       *shader;
	float           x, y;
	float           w, h;
	float           s1, t1;
	float           s2, t2;
} stretchPicCommand_t;

typedef struct
{
	int             commandId;
	trRefdef_t      refdef;
	viewParms_t     viewParms;
	drawSurf_t     *drawSurfs;
	int             numDrawSurfs;
	interaction_t  *interactions;
	int             numInteractions;
} drawSurfsCommand_t;

typedef struct
{
	int             commandId;
	int             x;
	int             y;
	int             width;
	int             height;
	char           *fileName;
	qboolean        jpeg;
} screenshotCommand_t;

typedef enum
{
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_SCREENSHOT
} renderCommand_t;


// these are sort of arbitrary limits.
// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	MAX_POLYS		600
#define	MAX_POLYVERTS	3000

// all of the information needed by the back end must be
// contained in a backEndData_t.  This entire structure is
// duplicated so the front and back end can run in parallel
// on an SMP machine
typedef struct
{
	drawSurf_t      drawSurfs[MAX_DRAWSURFS];
	interaction_t   interactions[MAX_INTERACTIONS];
	
	trRefDlight_t   dlights[MAX_LIGHTS];
	trRefEntity_t   entities[MAX_ENTITIES];
	
	srfPoly_t      *polys;		//[MAX_POLYS];
	polyVert_t     *polyVerts;	//[MAX_POLYVERTS];
	
	renderCommandList_t commands;
} backEndData_t;

extern int      max_polys;
extern int      max_polyverts;

extern backEndData_t *backEndData[SMP_FRAMES];	// the second one may not be allocated

extern volatile renderCommandList_t *renderCommandList;

extern volatile qboolean renderThreadActive;


void           *R_GetCommandBuffer(int bytes);
void            RB_ExecuteRenderCommands(const void *data);

void            R_InitCommandBuffers(void);
void            R_ShutdownCommandBuffers(void);

void            R_SyncRenderThread(void);

void            R_AddDrawSurfCmd(drawSurf_t * drawSurfs, int numDrawSurfs, interaction_t * interactions, int numInteractions);

void            RE_SetColor(const float *rgba);
void            RE_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void            RE_BeginFrame(stereoFrame_t stereoFrame);
void            RE_EndFrame(int *frontEndMsec, int *backEndMsec);

// font stuff
void            R_InitFreeType(void);
void            R_DoneFreeType(void);
void            RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t * font);


#endif // TR_LOCAL_H
