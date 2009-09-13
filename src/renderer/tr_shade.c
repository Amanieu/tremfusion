/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

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
// tr_shade.c

#include "tr_local.h" 
#if idppc_altivec && !defined(MACOS_X)
#include <altivec.h>
#endif

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

// "pseudo pointers" that tell the shader to bind some static data
#define COLOR_BIND_NONE ((color4ub_t *)0)
#define COLOR_BIND_VERT ((color4ub_t *)1)

#define TC_BIND_NONE ((vec2_t *)0)
#define TC_BIND_TEXT ((vec2_t *)1)
#define TC_BIND_LMAP ((vec2_t *)2)

/*
================
R_ArrayElementDiscrete

This is just for OpenGL conformance testing, it should never be the fastest
================
*/
static void APIENTRY R_ArrayElementDiscrete( GLint index ) {
	qglColor4ubv( tess.svars.colors[ index ] );
	if ( glState.currenttmu ) {
		qglMultiTexCoord2fARB( 0, tess.svars.texcoords[ 0 ][ index ][0], tess.svars.texcoords[ 0 ][ index ][1] );
		qglMultiTexCoord2fARB( 1, tess.svars.texcoords[ 1 ][ index ][0], tess.svars.texcoords[ 1 ][ index ][1] );
	} else {
		qglTexCoord2fv( tess.svars.texcoords[ 0 ][ index ] );
	}
	qglVertex3fv( &tess.vertexPtr[index].xyz[0] );
}

/*
===================
R_DrawStripElements

===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void R_DrawStripElements( int numIndexes, const GLushort *indexes, void ( APIENTRY *element )(GLint) ) {
	int i;
	int last[3] = { -1, -1, -1 };
	qboolean even;
	
	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	qglBegin( GL_TRIANGLE_STRIP );

	// prime the strip
	element( indexes[0] );
	element( indexes[1] );
	element( indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;
				assert( indexes[i+2] < tess.numVertexes );
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	qglEnd();
}
static void R_DrawStripElements32( int numIndexes, const GLuint *indexes, void ( APIENTRY *element )(GLint) ) {
	int i;
	int last[3] = { -1, -1, -1 };
	qboolean even;
	
	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	qglBegin( GL_TRIANGLE_STRIP );

	// prime the strip
	element( indexes[0] );
	element( indexes[1] );
	element( indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;
				assert( indexes[i+2] < tess.numVertexes );
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	qglEnd();
}



/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void R_DrawElements( int numIndexes, const void *indexes,
			    GLuint start, GLuint end, GLuint max ) {
	int		primitives;
	GLenum          type = max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}


	if ( primitives == 2 ) {
		if ( qglDrawRangeElementsEXT )
			qglDrawRangeElementsEXT( GL_TRIANGLES, 
						 start, end,
						 numIndexes,
						 type,
						 indexes );
		else
			qglDrawElements( GL_TRIANGLES,
					 numIndexes,
					 type,
					 indexes );
		return;
	}

	if ( primitives == 1 ) {
		if ( type == GL_UNSIGNED_SHORT )
			R_DrawStripElements( numIndexes, indexes, qglArrayElement );
		else
			R_DrawStripElements32( numIndexes, indexes, qglArrayElement );
		return;
	}
	
	if ( primitives == 3 ) {
		if ( type == GL_UNSIGNED_SHORT )
			R_DrawStripElements( numIndexes, indexes, R_ArrayElementDiscrete );
		else
			R_DrawStripElements32( numIndexes, indexes, R_ArrayElementDiscrete );
		return;
	}

	// anything else will cause no drawing
}

//R_DRAWCEL
static void R_DrawCel( int numIndexes, const void *indexes,
		       GLuint start, GLuint end, GLuint max ) {
	int		primitives;
	GLuint          type = max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	
	if(
		//. ignore the 2d projection. do i smell the HUD?
		(backEnd.projection2D == qtrue) ||
		//. ignore general entitites that are sprites. SEE NOTE #3.
		(backEnd.currentEntity->e.reType == RT_SPRITE) ||
		//. ignore these liquids. why? ever see liquid with tris on the surface? exactly. SEE NOTE #4.
		(tess.shader->contentFlags & (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_FOG)) ||
		//. ignore things that are two sided, meaning mostly things that have transparency. SEE NOTE #1.		
		(tess.shader->cullType == CT_TWO_SIDED)
		
		) {
		return;
	}

	primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}

	//. correction for mirrors. SEE NOTE #2.
	if(backEnd.viewParms.isMirror == qtrue) { qglCullFace (GL_FRONT); }
	else { qglCullFace (GL_BACK); }	

	qglEnable (GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA ,GL_ONE_MINUS_SRC_ALPHA);
	qglColor3f (0.0f,0.0f,0.0f);
	qglLineWidth( (float) r_celoutline->integer );	

	if(primitives == 2) {
		if ( qglDrawRangeElementsEXT )
			qglDrawRangeElementsEXT( GL_TRIANGLES, start, end, numIndexes, type, indexes );
		else
			qglDrawElements( GL_TRIANGLES, numIndexes, type, indexes );
	} else if(primitives == 1) {
		if ( type == GL_UNSIGNED_SHORT )
			R_DrawStripElements( numIndexes,  indexes, qglArrayElement );
		else
			R_DrawStripElements32( numIndexes,  indexes, qglArrayElement );
	} else if(primitives == 3) {
		if ( type == GL_UNSIGNED_SHORT )
			R_DrawStripElements( numIndexes,  indexes, R_ArrayElementDiscrete );
		else
			R_DrawStripElements32( numIndexes,  indexes, R_ArrayElementDiscrete );
	}

	//. correction for mirrors. SEE NOTE #2.
	if(backEnd.viewParms.isMirror == qtrue) { qglCullFace (GL_BACK); }
	else { qglCullFace (GL_FRONT); }
	
	qglDisable (GL_BLEND);
	
	return;

/* Notes

1. this is going to be a pain in the arse. it fixes things like light `beams` from being cel'd but it
also will ignore any other shader set with no culling. this usually is everything that is translucent.
but this is a good hack to clean up the screen untill something more selective comes along. or who knows
group desision might actually be that this is liked. if so i take back calling it a `hack`, lol.
	= bob.

2. mirrors display correctly because the normals of the displayed are inverted of normal space. so to
continue to have them display correctly, we must invert them inversely from a normal inversion.
	= bob.
	
3. this turns off a lot of space hogging sprite cel outlines. picture if you will five people in a small
room all shooting rockets. each smoke puff gets a big black square around it, each explosion gets a big
black square around it, and now nobody can see eachother because everyones screen is solid black.
	= bob.

4. ignoring liquids means you will not get black tris lines all over the top of your liquid. i put this in
after seeing the lava on q3dm7 and water on q3ctf2 that had black lines all over the top, making the
liquids look solid instead of... liquid.
	= bob.

*/
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;

/*
=================
R_BindAnimatedImage

=================
*/
static void R_BindAnimatedImage( textureBundle_t *bundle ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_Bind( bundle->image[0] );
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = myftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	GL_Bind( bundle->image[ index ] );
}

//DRAWCEL
static void DrawCelVBO (vboInfo_t *VBO, GLuint max) {

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	R_DrawCel( VBO->numIndexes, NULL , VBO->minIndex, VBO->maxIndex, max );
}
static void DrawCel ( void ) {

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t), tess.vertexPtr[0].xyz);

	if (qglLockArraysEXT) {
		qglLockArraysEXT(0, tess.numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawCel( tess.numIndexes, tess.indexPtr, 0, tess.numVertexes-1, tess.numVertexes-1 );

	if (qglUnlockArraysEXT) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTrisVBO (vboInfo_t *VBO) {
	GLuint   max;
	
	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	
	qglColor3f (0,0,1);
	
	if ( VBO->vbo ) {
		GL_VBO( VBO->vbo, VBO->ibo );
		max = VBO->maxIndex;
	} else {
		GL_VBO( backEnd.worldVBO, VBO->ibo );
		max = backEnd.worldVertexes-1;
	}
	
	qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t),
			  vboOffset(xyz));

	R_DrawElements( VBO->numIndexes, NULL, VBO->minIndex, VBO->maxIndex, max );

	qglDepthRange( 0, 1 );
}
static void DrawTris ( void ) {
	GLuint   max;

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	qglColor3f (1,1,1);
	
	if ( tess.vertexPtr ) {
		GL_VBO( 0, 0 );
		max = tess.numVertexes-1;
	} else {
		GL_VBO( backEnd.worldVBO, 0 );
		max = backEnd.worldVertexes-1;
	}
	
	qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t), tess.vertexPtr[0].xyz);

	if (qglLockArraysEXT) {
		qglLockArraysEXT(0, tess.numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawElements( tess.numIndexes, tess.indexPtr, tess.minIndex, tess.maxIndex, max );

	if (qglUnlockArraysEXT) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals ( void ) {
	int		i;
	vec3_t	temp;

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < tess.numVertexes ; i++) {
		qglVertex3fv( &tess.vertexPtr[i].xyz[0] );
		VectorMA( tess.vertexPtr[i].xyz, 2,
			  tess.vertexPtr[i].normal, temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {

	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.minIndex = backEnd.worldVertexes;
	tess.maxIndex = 0;
	tess.numVertexes = 0;
	tess.renderVBO = NULL;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}


}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( shaderCommands_t *input, int stage, GLuint max ) {
	shaderStage_t	*pStage;
	int		bundle;

	pStage = tess.xstages[stage];

	GL_State( pStage->stateBits );

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.isPortal ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	if ( input->svars.texcoords[0] == TC_BIND_NONE ) {
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	} else if ( input->svars.texcoords[0] == TC_BIND_TEXT ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), &input->vertexPtr[0].tc1[0] );
	} else if ( input->svars.texcoords[0] == TC_BIND_LMAP ) {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), &input->vertexPtr[0].tc2[0] );
	} else {
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
	}
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary passes
	//
	for ( bundle = 1; bundle < glConfig.numTextureUnits; bundle++ ) {
		if ( !pStage->bundle[bundle].multitextureEnv )
			break;
		
		GL_SelectTexture( bundle );
		qglEnable( GL_TEXTURE_2D );
		if ( input->svars.texcoords[bundle] == TC_BIND_NONE ) {
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		} else if ( input->svars.texcoords[bundle] == TC_BIND_TEXT ) {
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), &input->vertexPtr[0].tc1[0] );
		} else if ( input->svars.texcoords[bundle] == TC_BIND_LMAP ) {
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), &input->vertexPtr[0].tc2[0] );
		} else {
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[bundle] );
		}
		
		if ( r_lightmap->integer ) {
			GL_TexEnv( GL_REPLACE );
		} else {
			GL_TexEnv( pStage->bundle[bundle].multitextureEnv );
		}

		R_BindAnimatedImage( &pStage->bundle[bundle] );
	}

	R_DrawElements( input->numIndexes, input->indexPtr, input->minIndex, input->maxIndex, max );

	//
	// disable texturing on TEXTURE>=1, then select TEXTURE0
	//
	--bundle;
	while ( bundle > 0 ) {
		if ( input->svars.texcoords[bundle] != TC_BIND_NONE &&
		     input->svars.texcoords[bundle] != TC_BIND_TEXT &&
		     input->svars.texcoords[bundle] != TC_BIND_LMAP ) {
			ri.Hunk_FreeTempMemory( input->svars.texcoords[bundle] );
		}
		qglDisable( GL_TEXTURE_2D );
		bundle--;
		GL_SelectTexture( bundle );
	}
}



/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
#if idppc_altivec
static void ProjectDlightTexture_altivec( void ) {
	int		i, l;
	vec_t	origin0, origin1, origin2;
	float   texCoords0, texCoords1;
	vector float floatColorVec0, floatColorVec1;
	vector float modulateVec, colorVec, zero;
	vector short colorShort;
	vector signed int colorInt;
	vector unsigned char floatColorVecPerm, modulatePerm, colorChar;
	vector unsigned char vSel = VECCONST_UINT8(0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff);
	vec2_t	*texCoords;
	color4ub_t	*colors;
	byte	*clipBits;
	vec2_t	*texCoordsArray;
	color4ub_t	*colorArray;
	GLushort	*hitIndexes;
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate = 0.0f;

	if ( !backEnd.refdef.num_dlights ||
	     tess.numVertexes > 65535 ) { // to avoid GLushort overflow
		return;
	}

	clipBits = ri.Hunk_AllocateTempMemory( sizeof(byte) * tess.numVertexes );
	texCoordsArray = ri.Hunk_AllocateTempMemory( sizeof(vec2_t) * tess.numVertexes );
	colorArray = ri.Hunk_AllocateTempMemory( sizeof(color4ub_t) * tess.numVertexes );
	hitIndexes = ri.Hunk_AllocateTempMemory( sizeof(GLushort) * tess.numIndexes );

	// There has to be a better way to do this so that floatColor
	// and/or modulate are already 16-byte aligned.
	floatColorVecPerm = vec_lvsl(0,(float *)floatColor);
	modulatePerm = vec_lvsl(0,(float *)&modulate);
	modulatePerm = (vector unsigned char)vec_splat((vector unsigned int)modulatePerm,0);
	zero = (vector float)vec_splat_s8(0);

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray;
		colors = colorArray;

		dl = &backEnd.refdef.dlights[l];
		origin0 = dl->transformed[0];
		origin1 = dl->transformed[1];
		origin2 = dl->transformed[2];
		radius = dl->radius;
		scale = 1.0f / radius;

		if(r_greyscale->integer)
		{
			float luminance;
			
			luminance = (dl->color[0] * 255.0f + dl->color[1] * 255.0f + dl->color[2] * 255.0f) / 3;
			floatColor[0] = floatColor[1] = floatColor[2] = luminance;
		}
		else
		{
			floatColor[0] = dl->color[0] * 255.0f;
			floatColor[1] = dl->color[1] * 255.0f;
			floatColor[2] = dl->color[2] * 255.0f;
		}
		floatColorVec0 = vec_ld(0, floatColor);
		floatColorVec1 = vec_ld(11, floatColor);
		floatColorVec0 = vec_perm(floatColorVec0,floatColorVec0,floatColorVecPerm);
		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords++, colors++ ) {
			int		clip = 0;
			vec_t dist0, dist1, dist2;
			
			dist0 = origin0 - tess.vertexPtr[i].xyz[0];
			dist1 = origin1 - tess.vertexPtr[i].xyz[1];
			dist2 = origin2 - tess.vertexPtr[i].xyz[2];

			backEnd.pc.c_dlightVertexes++;

			texCoords0 = 0.5f + dist0 * scale;
			texCoords1 = 0.5f + dist1 * scale;

			if( !r_dlightBacks->integer &&
			    // dist . tess.normal[i]
			    ( dist0 * tess.vertexPtr[i].normal[0] +
			      dist1 * tess.vertexPtr[i].normal[1] +
			      dist2 * tess.vertexPtr[i].normal[2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( texCoords0 < 0.0f ) {
					clip |= 1;
				} else if ( texCoords0 > 1.0f ) {
					clip |= 2;
				}
				if ( texCoords1 < 0.0f ) {
					clip |= 4;
				} else if ( texCoords1 > 1.0f ) {
					clip |= 8;
				}
				(*texCoords)[0] = texCoords0;
				(*texCoords)[1] = texCoords1;

				// modulate the strength based on the height and color
				if ( dist2 > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist2 < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					dist2 = Q_fabs(dist2);
					if ( dist2 < radius * 0.5f ) {
						modulate = 1.0f;
					} else {
						modulate = 2.0f * (radius - dist2) * scale;
					}
				}
			}
			clipBits[i] = clip;

			modulateVec = vec_ld(0,(float *)&modulate);
			modulateVec = vec_perm(modulateVec,modulateVec,modulatePerm);
			colorVec = vec_madd(floatColorVec0,modulateVec,zero);
			colorInt = vec_cts(colorVec,0);	// RGBx
			colorShort = vec_pack(colorInt,colorInt);		// RGBxRGBx
			colorChar = vec_packsu(colorShort,colorShort);	// RGBxRGBxRGBxRGBx
			colorChar = vec_sel(colorChar,vSel,vSel);		// RGBARGBARGBARGBA replace alpha with 255
			vec_ste((vector unsigned int)colorChar,0,(unsigned int *)colors);	// store color
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			GLushort	a, b, c;

			a = tess.indexPtr[i];
			b = tess.indexPtr[i+1];
			c = tess.indexPtr[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray );

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

		GL_Bind( tr.dlightImage );
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		R_DrawElements( numIndexes, hitIndexes, 0, tess.numVertexes-1, tess.numVertexes-1 );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	ri.Hunk_FreeTempMemory( hitIndexes );
	ri.Hunk_FreeTempMemory( colorArray );
	ri.Hunk_FreeTempMemory( texCoordsArray );
	ri.Hunk_FreeTempMemory( clipBits );
}
#endif


static void ProjectDlightTexture_scalar( void ) {
	int		i, l;
	vec3_t	origin;
	vec2_t	*texCoords;
	color4ub_t	*colors;
	byte	*clipBits;
	vec2_t	*texCoordsArray;
	color4ub_t	*colorArray;
	GLushort	*hitIndexes;
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	vec4_t	*ptr;
	float	modulate = 0.0f;

	if ( !backEnd.refdef.num_dlights ||
	     tess.numVertexes > 65535 ) { // to avoid GLushort overflow
		return;
	}

	clipBits = ri.Hunk_AllocateTempMemory( sizeof(byte) * tess.numVertexes );
	texCoordsArray = ri.Hunk_AllocateTempMemory( sizeof(vec2_t) * tess.numVertexes );
	colorArray = ri.Hunk_AllocateTempMemory( sizeof(color4ub_t) * tess.numVertexes );
	hitIndexes = ri.Hunk_AllocateTempMemory( sizeof(GLushort) * tess.numIndexes );

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray;
		colors = colorArray;

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		if(r_greyscale->integer)
		{
			float luminance;
			
			luminance = (dl->color[0] * 255.0f + dl->color[1] * 255.0f + dl->color[2] * 255.0f) / 3;
			floatColor[0] = floatColor[1] = floatColor[2] = luminance;
		}
		else
		{
			floatColor[0] = dl->color[0] * 255.0f;
			floatColor[1] = dl->color[1] * 255.0f;
			floatColor[2] = dl->color[2] * 255.0f;
		}

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords++, colors++ ) {
			int		clip = 0;
			vec3_t	dist;
			
			VectorSubtract( origin, tess.vertexPtr[i].xyz, dist );

			backEnd.pc.c_dlightVertexes++;

			(*texCoords)[0] = 0.5f + dist[0] * scale;
			(*texCoords)[1] = 0.5f + dist[1] * scale;

			ptr = &tess.vertexPtr[i].normal;
			if( !r_dlightBacks->integer &&
			    // dist . tess.normal[i]
			    ( dist[0] * (*ptr)[0] +
			      dist[1] * (*ptr)[1] +
			      dist[2] * (*ptr)[2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( (*texCoords)[0] < 0.0f ) {
					clip |= 1;
				} else if ( (*texCoords)[0] > 1.0f ) {
					clip |= 2;
				}
				if ( (*texCoords)[1] < 0.0f ) {
					clip |= 4;
				} else if ( (*texCoords)[1] > 1.0f ) {
					clip |= 8;
				}

				// modulate the strength based on the height and color
				if ( dist[2] > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist[2] < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					dist[2] = Q_fabs(dist[2]);
					if ( dist[2] < radius * 0.5f ) {
						modulate = 1.0f;
					} else {
						modulate = 2.0f * (radius - dist[2]) * scale;
					}
				}
			}
			clipBits[i] = clip;
			(*colors)[0] = myftol(floatColor[0] * modulate);
			(*colors)[1] = myftol(floatColor[1] * modulate);
			(*colors)[2] = myftol(floatColor[2] * modulate);
			(*colors)[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			GLushort		a, b, c;

			a = tess.indexPtr[i];
			b = tess.indexPtr[i+1];
			c = tess.indexPtr[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}
		
		GL_VBO( 0, 0 );
		
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray );

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

		GL_Bind( tr.dlightImage );
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		R_DrawElements( numIndexes, hitIndexes, 0, tess.numVertexes-1, tess.numVertexes-1 );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	ri.Hunk_FreeTempMemory( hitIndexes );
	ri.Hunk_FreeTempMemory( colorArray );
	ri.Hunk_FreeTempMemory( texCoordsArray );
	ri.Hunk_FreeTempMemory( clipBits );
}

static void ProjectDlightTexture( void ) {
#if idppc_altivec
	if (com_altivec->integer) {
		// must be in a seperate function or G3 systems will crash.
		ProjectDlightTexture_altivec();
		return;
	}
#endif
	ProjectDlightTexture_scalar();
}


/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	fog_t		*fog;
	int			i;

	tess.svars.colors = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(color4ub_t) );
	tess.svars.texcoords[0] = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(vec2_t) );

	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	fog = tr.world->fogs + tess.fogNum;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		* ( int * )&tess.svars.colors[i] = fog->colorInt;
	}

	RB_CalcFogTexCoords( tess.svars.texcoords[0], tess.numVertexes );

	GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexPtr, 0, tess.numVertexes-1, tess.numVertexes-1 );

	ri.Hunk_FreeTempMemory( tess.svars.texcoords[0] );
	ri.Hunk_FreeTempMemory( tess.svars.colors );
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors( shaderStage_t *pStage, qboolean skipFog )
{
	int		i;
	alphaGen_t	aGen = pStage->alphaGen;
	colorGen_t	rgbGen = pStage->rgbGen;
	color4ub_t	color;
	fog_t		*fog;
	qboolean	constRGB, constA;
	
	// get rid of AGEN_SKIP
	if ( aGen == AGEN_SKIP ) {
		if ( rgbGen == CGEN_EXACT_VERTEX ||
		     rgbGen == CGEN_VERTEX ) {
			aGen = AGEN_VERTEX;
		} else {
			aGen = AGEN_IDENTITY;
		}
	}
	
	// no need to multiply by 1
	if ( tr.identityLight == 1 && rgbGen == CGEN_VERTEX ) {
		rgbGen = CGEN_EXACT_VERTEX;
	}

	// check for constant RGB
	switch( rgbGen ) {
	case CGEN_IDENTITY_LIGHTING:
		color[0] = color[1] = color[2] = tr.identityLightByte;
		constRGB = qtrue;
		break;
	case CGEN_IDENTITY:
		color[0] = color[1] = color[2] = 255;
		constRGB = qtrue;
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( &pStage->rgbWave, &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_FOG:
		fog = tr.world->fogs + tess.fogNum;
		*(int *)(&color) = fog->colorInt;
		constRGB = qtrue;
		break;
	case CGEN_CONST:
		*(int *)(&color) = *(int *)pStage->constantColor;		
		constRGB = qtrue;
		break;
	default:
		constRGB = qfalse;
		break;
	}

	// check for constant ALPHA
	switch( aGen ) {
	case AGEN_IDENTITY:
		color[3] = 255;
		constA = qtrue;
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( &color, 1 );
		constA = qtrue;
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( &color, 1 );
		constA = qtrue;
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, &color, 1 );
		constA = qtrue;
		break;
	case AGEN_CONST:
		color[3] = pStage->constantColor[3];
		constA = qtrue;
		break;
	default:
		constA = qfalse;
		break;
	}

	if ( !r_greyscale->integer &&
	     (skipFog || !tess.fogNum || pStage->adjustColorsForFog == ACFF_NONE) ) {
		// if RGB and ALPHA are constant, just set the GL color
		if ( constRGB && constA ) {
			qglDisableClientState( GL_COLOR_ARRAY );
			qglColor4ub( color[0], color[1], color[2], color[3] );
			tess.svars.colors = COLOR_BIND_NONE;
			return;
		}
		
		// if RGB and ALPHA are identical to vertex data, bind that
		if ( aGen == AGEN_VERTEX && rgbGen == CGEN_EXACT_VERTEX ) {
			tess.svars.colors = COLOR_BIND_VERT;
			return;
		}
	}
	
	// we have to allocate a per-Vertex color value
	tess.svars.colors = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(color4ub_t) );

	//
	// rgbGen
	//
	switch ( rgbGen )
	{
	case CGEN_BAD:
	case CGEN_IDENTITY_LIGHTING:
	case CGEN_IDENTITY:
	case CGEN_ENTITY:
	case CGEN_ONE_MINUS_ENTITY:
	case CGEN_WAVEFORM:
	case CGEN_FOG:
	case CGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			*(int *)tess.svars.colors[i] = *(int *)(&color);
		}
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( tess.svars.colors, tess.numVertexes );
		break;
	case CGEN_EXACT_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][0] = tess.vertexPtr[i].color[0];
			tess.svars.colors[i][1] = tess.vertexPtr[i].color[1];
			tess.svars.colors[i][2] = tess.vertexPtr[i].color[2];
			tess.svars.colors[i][3] = tess.vertexPtr[i].color[3];
		}
		break;
	case CGEN_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][0] = tess.vertexPtr[i].color[0] * tr.identityLight;
			tess.svars.colors[i][1] = tess.vertexPtr[i].color[1] * tr.identityLight;
			tess.svars.colors[i][2] = tess.vertexPtr[i].color[2] * tr.identityLight;
			tess.svars.colors[i][3] = tess.vertexPtr[i].color[3];
		}
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if ( tr.identityLight == 1 )
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i][0] = 255 - tess.vertexPtr[i].color[0];
				tess.svars.colors[i][1] = 255 - tess.vertexPtr[i].color[1];
				tess.svars.colors[i][2] = 255 - tess.vertexPtr[i].color[2];
			}
		}
		else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i][0] = ( 255 - tess.vertexPtr[i].color[0] ) * tr.identityLight;
				tess.svars.colors[i][1] = ( 255 - tess.vertexPtr[i].color[1] ) * tr.identityLight;
				tess.svars.colors[i][2] = ( 255 - tess.vertexPtr[i].color[2] ) * tr.identityLight;
			}
		}
		break;
	}

	//
	// alphaGen
	//
	switch ( aGen )
	{
	case AGEN_SKIP:
	case AGEN_IDENTITY:
	case AGEN_ENTITY:
	case AGEN_ONE_MINUS_ENTITY:
	case AGEN_WAVEFORM:
	case AGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i][3] = color[3];
		}
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( tess.svars.colors, tess.numVertexes );
		break;
	case AGEN_VERTEX:
		if ( pStage->rgbGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = tess.vertexPtr[i].color[3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][3] = 255 - tess.vertexPtr[i].color[3];
		}
		break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;
					
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.vertexPtr[i].xyz, backEnd.viewParms.or.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_NONE:
			break;
		}
	}
	
	// if in greyscale rendering mode turn all color values into greyscale.
	if(r_greyscale->integer)
	{
		int scale;
		
		for(i = 0; i < tess.numVertexes; i++)
		{
			scale = (tess.svars.colors[i][0] + tess.svars.colors[i][1] + tess.svars.colors[i][2]) / 3;
			tess.svars.colors[i][0] = tess.svars.colors[i][1] = tess.svars.colors[i][2] = scale;
		}
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;
	qboolean	noTexMods;
	
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

		noTexMods = (pStage->bundle[b].numTexMods == 0) ||
			(pStage->bundle[b].texMods[0].type == TMOD_NONE);
		
		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_BAD ) {
			tess.svars.texcoords[b] = TC_BIND_NONE;
			continue;
		}

		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_TEXTURE ) {
			tess.svars.texcoords[b] = TC_BIND_TEXT;
			continue;
		}

		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_LIGHTMAP ) {
			tess.svars.texcoords[b] = TC_BIND_LMAP;
			continue;
		}

		tess.svars.texcoords[b] = ri.Hunk_AllocateTempMemory( tess.numVertexes * sizeof(vec2_t) );
		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			Com_Memset( tess.svars.texcoords[b], 0, sizeof( vec2_t ) * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.vertexPtr[i].tc1[0];
				tess.svars.texcoords[b][i][1] = tess.vertexPtr[i].tc1[1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.vertexPtr[i].tc2[0];
				tess.svars.texcoords[b][i][1] = tess.vertexPtr[i].tc2[1];
			}
			break;
		case TCGEN_VECTOR:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = DotProduct( tess.vertexPtr[i].xyz, pStage->bundle[b].tcGenVectors[0] );
				tess.svars.texcoords[b][i][1] = DotProduct( tess.vertexPtr[i].xyz, pStage->bundle[b].tcGenVectors[1] );
			}
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords( tess.svars.texcoords[b], tess.numVertexes );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			RB_CalcEnvironmentTexCoords( tess.svars.texcoords[b], tess.numVertexes );
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
							   tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].scroll,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].scale,
						       tess.svars.texcoords[b], tess.numVertexes );
				break;
			
			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
							 tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
							   tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].rotateSpeed,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			default:
				ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGenericVBO( vboInfo_t *VBO, GLuint max )
{
	int stage;
	int bundle;
	qboolean useQuery = tess.shader->QueryID != 0 &&
	  backEnd.currentEntity == &tr.worldEntity && qglBeginQueryARB;

	if ( tess.shader->needsNormal ) {
		qglEnableClientState( GL_NORMAL_ARRAY );
		qglNormalPointer( GL_FLOAT, sizeof(vboVertex_t),
				  vboOffset(normal) );
	}
	
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = tess.xstages[stage];

		if ( !pStage )
		{
			break;
		}

		// if the surface was invisible in the last frame, skip
		// all detail stages
		if ( useQuery && tess.shader->QueryResult == 0 && pStage->isDetail )
			continue;

		GL_State( pStage->stateBits );

		ComputeColors( pStage, qtrue );
		
		if ( tess.svars.colors == COLOR_BIND_NONE) {
			// constant color is already set
		} else if ( tess.svars.colors == COLOR_BIND_VERT ) {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof(vboVertex_t),
					 vboOffset(color) );
		} else {
			// per vertex color should not happen with VBO
			ri.Printf( PRINT_WARNING, "ERROR: per vertex color in VBO shader '%s'\n", tess.shader->name );
			ri.Hunk_FreeTempMemory( tess.svars.colors );
			return;
		}

		ComputeTexCoords( pStage );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
		if ( tess.svars.texcoords[0] == TC_BIND_TEXT ) {
			qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t),
					    vboOffset(tc1) );
		} else if ( tess.svars.texcoords[0] == TC_BIND_LMAP ) {
			qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t),
					    vboOffset(tc2) );
		} else {
			// per vertex texcoords should not happen with VBO
			ri.Printf( PRINT_WARNING, "ERROR: per vertex texture coords in VBO shader '%s'\n", tess.shader->name );
			ri.Hunk_FreeTempMemory( tess.svars.texcoords[0] );
			return;
		}

		//
		// set state
		//
		if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
		{
			GL_Bind( tr.whiteImage );
		}
		else 
			R_BindAnimatedImage( &pStage->bundle[0] );
		
		//
		// do multitexture
		//
		for ( bundle = 1; bundle < glConfig.numTextureUnits; bundle++ ) {
			if ( !pStage->bundle[bundle].multitextureEnv )
				break;

			// this is an ugly hack to work around a GeForce driver
			// bug with multitexture and clip planes
			if ( backEnd.viewParms.isPortal ) {
				qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			}
			
			//
			// lightmap/secondary pass
			//
			GL_SelectTexture( bundle );
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
			if ( tess.svars.texcoords[bundle] == TC_BIND_TEXT ) {
				qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t),
						    vboOffset(tc1) );
			} else if ( tess.svars.texcoords[bundle] == TC_BIND_LMAP ) {
				qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t),
						    vboOffset(tc2) );
			} else {
				// per vertex texcoords should not happen with VBO
				ri.Error( ERR_DROP, "ERROR: per vertex texture coords in VBO shader '%s'\n", tess.shader->name );			
			}
			
			if ( r_lightmap->integer ) {
				GL_TexEnv( GL_REPLACE );
			} else {
				GL_TexEnv( pStage->bundle[bundle].multitextureEnv );
			}
			R_BindAnimatedImage( &pStage->bundle[bundle] );
		}
		
		if ( stage == 0 && useQuery ) {
			qglBeginQueryARB( GL_SAMPLES_PASSED_ARB, tess.shader->QueryID );
		}
		
		if ( qglDrawRangeElementsEXT )
			qglDrawRangeElementsEXT( GL_TRIANGLES,
						 VBO->minIndex, VBO->maxIndex,
						 VBO->numIndexes,
						 max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT,
						 NULL );
		else
			qglDrawElements( GL_TRIANGLES, VBO->numIndexes,
					 max > 65535 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, NULL );
		
		if ( stage == 0 && useQuery ) {
			qglEndQueryARB( GL_SAMPLES_PASSED_ARB );
		}

		//
		// disable texturing on TEXTURE1, then select TEXTURE0
		//
		bundle--;
		while ( bundle > 0 ) {
			qglDisable( GL_TEXTURE_COORD_ARRAY );
			qglDisable( GL_TEXTURE_2D );
			bundle--;
			GL_SelectTexture( bundle );
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}

	if ( tess.shader->needsNormal )
		qglDisableClientState( GL_NORMAL_ARRAY );
}
static void RB_IterateStagesGeneric( shaderCommands_t *input, GLuint max )
{
	int stage;
	qboolean useQuery = tess.shader->QueryID != 0 &&
	  backEnd.currentEntity == &tr.worldEntity && qglBeginQueryARB;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = tess.xstages[stage];

		if ( !pStage )
		{
			break;
		}

		// if the surface was invisible in the last frame, skip
		// all detail stages
		if ( useQuery && tess.shader->QueryResult == 0 && pStage->isDetail )
			continue;

		ComputeColors( pStage, qfalse );
		ComputeTexCoords( pStage );

		if ( input->svars.colors == COLOR_BIND_NONE ) {
		} else if (input->svars.colors == COLOR_BIND_VERT ) {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE,
					 sizeof(vboVertex_t),
					 input->vertexPtr[0].color );
		} else {
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors );
		}

		if ( stage == 0 && useQuery ) {
			qglBeginQueryARB( GL_SAMPLES_PASSED_ARB, tess.shader->QueryID );
		}
		
		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != 0 )
		{
		  DrawMultitextured( input, stage, max );
		}
		else
		{
			if ( input->svars.texcoords[0] == TC_BIND_NONE ) {
				qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
			} else if ( input->svars.texcoords[0] == TC_BIND_TEXT ) {
				qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
				qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].tc1 );
			} else if ( input->svars.texcoords[0] == TC_BIND_LMAP ) {
				qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
				qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].tc2 );
			} else {
				qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
				qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
			}

			//
			// set state
			//
			if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
			{
				GL_Bind( tr.whiteImage );
			}
			else 
				R_BindAnimatedImage( &pStage->bundle[0] );

			GL_State( pStage->stateBits );

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexPtr, input->minIndex, input->maxIndex, max );
		}
		if ( stage == 0 && useQuery ) {
			qglEndQueryARB( GL_SAMPLES_PASSED_ARB );
		}

		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		if ( input->svars.texcoords[0] != TC_BIND_NONE &&
		     input->svars.texcoords[0] != TC_BIND_TEXT &&
		     input->svars.texcoords[0] != TC_BIND_LMAP ) {
			ri.Hunk_FreeTempMemory( input->svars.texcoords[0] );
		}
		qglDisableClientState( GL_COLOR_ARRAY );
		if ( input->svars.colors == COLOR_BIND_NONE ) {
		} else if (input->svars.colors == COLOR_BIND_VERT ) {
			qglDisableClientState( GL_COLOR_ARRAY );
		} else {
			qglDisableClientState( GL_COLOR_ARRAY );
			ri.Hunk_FreeTempMemory( tess.svars.colors );
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	GLuint            max;

	input = &tess;

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}
	
	//. show me cel outlines.
	//. there has to be a better place to put this.
	if(r_celoutline->integer > 0) {
		if ( tess.numIndexes > 0 )
			DrawCel();
	}

	if ( tess.renderVBO ) {
		if ( tess.renderVBO->vbo ) {
			GL_VBO( tess.renderVBO->vbo, tess.renderVBO->ibo );
			max = tess.renderVBO->maxIndex;
		} else {
			GL_VBO( backEnd.worldVBO, tess.renderVBO->ibo );
			max = backEnd.worldVertexes-1;
		}

		qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t),
				  vboOffset(xyz));

		if(r_celoutline->integer > 0) {
			DrawCelVBO( tess.renderVBO, max );
		}

		RB_IterateStagesGenericVBO( tess.renderVBO, max );
	}

	if ( tess.numIndexes > 0 ) {
		//

		if ( input->vertexPtr ) {
			GL_VBO( 0, 0 );
			max = tess.numIndexes-1;
		} else {
			GL_VBO( backEnd.worldVBO, 0 );
			max = backEnd.worldVertexes-1;
		}
		//
		// lock XYZ
		//
		qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].xyz);
		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		//
		// call shader function
		//
		if ( !tess.renderVBO )
			RB_IterateStagesGeneric( input, max );
		
		// 
		// unlock arrays
		//
		if (qglUnlockArraysEXT)
		{
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}


/*
** RB_StageIteratorVertexLitTexture
*/
void RB_StageIteratorVertexLitTexture( void )
{
	shaderCommands_t *input;
	shader_t		*shader;
	GLuint            max;

	input = &tess;

	shader = input->shader;

	//
	// compute colors
	//
	RB_CalcDiffuseColor( tess.svars.colors, tess.numVertexes );

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	R_BindAnimatedImage( &tess.xstages[0]->bundle[0] );
	GL_State( tess.xstages[0]->stateBits );

	//
	// set arrays and lock
	//
	if ( input->renderVBO ) {
		if ( input->renderVBO->vbo ) {
			GL_VBO( input->renderVBO->vbo, input->renderVBO->ibo );
			max = input->renderVBO->maxIndex;
		} else {
			GL_VBO( backEnd.worldVBO, input->renderVBO->ibo );
			max = backEnd.worldVertexes-1;
		}

		qglEnableClientState( GL_COLOR_ARRAY);
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
		
		qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof(vboVertex_t),
				 vboOffset(color) );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), 
				    vboOffset(tc1) );
		qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t),
				  vboOffset(xyz) );
		
		//
		// call special shade routine
		//
		R_DrawElements( input->renderVBO->numIndexes, NULL, input->renderVBO->minIndex, input->renderVBO->maxIndex, max );
	}
	if ( input->numIndexes > 0 ) {
		if ( input->vertexPtr ) {
			GL_VBO( 0, 0 );
			max = tess.numVertexes-1;
		} else {
			GL_VBO( backEnd.worldVBO, 0 );
			max = backEnd.worldVertexes-1;
		}
		
		qglEnableClientState( GL_COLOR_ARRAY);
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
		
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );
		qglTexCoordPointer(2, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].tc1);
		qglVertexPointer(3, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].xyz);
		
		//
		// lock arrays
		//
		if ( qglLockArraysEXT ) {
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		//
		// call special shade routine
		//
		R_DrawElements( input->numIndexes, input->indexPtr, input->minIndex, input->maxIndex, max );

		//
		// unlock arrays
		//
		if ( qglUnlockArraysEXT ) {
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
}

void RB_StageIteratorLightmappedMultitexture( void ) {
	shaderCommands_t *input;
	GLuint            max;

	input = &tess;

	//
	// log this call
	//
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	GL_State( GLS_DEFAULT );

	qglDisableClientState( GL_COLOR_ARRAY );
	qglColor3f( 1, 1, 1 );
	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &tess.xstages[0]->bundle[0] );
	
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( GL_MODULATE );
	}
	R_BindAnimatedImage( &tess.xstages[0]->bundle[1] );

	if ( tess.renderVBO ) {
		if ( input->renderVBO->vbo ) {
			GL_VBO( input->renderVBO->vbo, input->renderVBO->ibo );
			max = input->renderVBO->maxIndex;
		} else {
			GL_VBO( backEnd.worldVBO, input->renderVBO->ibo );
			max = backEnd.worldVertexes-1;
		}
		//
		// set color, pointers, and lock
		//
		qglVertexPointer( 3, GL_FLOAT, sizeof(vboVertex_t), vboOffset(xyz) );
		
		//
		// select base stage
		//
		GL_SelectTexture( 0 );
		
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t),
				    vboOffset(tc1) );
		
		//
		// configure second stage
		//
		GL_SelectTexture( 1 );
		qglEnable( GL_TEXTURE_2D );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t),
				    vboOffset(tc2) );
		
		R_DrawElements( input->renderVBO->numIndexes, NULL, input->renderVBO->minIndex, input->renderVBO->maxIndex, max );
		
		//
		// disable texturing on TEXTURE1, then select TEXTURE0
		//
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		qglDisable( GL_TEXTURE_2D );

		GL_SelectTexture( 0 );
	}
	if ( tess.numIndexes > 0 ) {
		if ( tess.vertexPtr ) {
			GL_VBO( 0, 0 );
			max = tess.numVertexes-1;
		} else {
			GL_VBO( backEnd.worldVBO, 0 );
			max = backEnd.worldVertexes-1;
		}

		//
		// set color, pointers, and lock
		//
		qglVertexPointer( 3, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].xyz );
		
		//
		// select base stage
		//
		GL_SelectTexture( 0 );
		
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].tc1 );
		
		//
		// configure second stage
		//
		GL_SelectTexture( 1 );
		qglEnable( GL_TEXTURE_2D );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].tc2 );
		
		//
		// lock arrays
		//
		if ( qglLockArraysEXT ) {
			qglLockArraysEXT(0, input->numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
		
		R_DrawElements( input->numIndexes, input->indexPtr, input->minIndex, input->maxIndex, max );
		
		//
		// disable texturing on TEXTURE1, then select TEXTURE0
		//
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		qglDisable( GL_TEXTURE_2D );
		
		GL_SelectTexture( 0 );
		
		//
		// unlock arrays
		//
		if ( qglUnlockArraysEXT ) {
			qglUnlockArraysEXT();
			GLimp_LogComment( "glUnlockArraysEXT\n" );
		}
	}
}

/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0 && !input->renderVBO ) {
		return;
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer ) {
		if ( input->renderVBO )
			DrawTrisVBO (input->renderVBO);
		if ( input->numIndexes > 0 )
			DrawTris ();
	}
	if ( r_shownormals->integer ) {
		DrawNormals ();
	}
	if ( r_flush->integer ) {
		qglFlush();
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.renderVBO = NULL;

	GLimp_LogComment( "----------\n" );
}

/*
** RB_LightSurface
*/
void RB_LightSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_LightSurface( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}
	
	GL_VBO( 0, 0 );
	
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	
	//
	// lock XYZ
	//
	qglVertexPointer (3, GL_FLOAT, sizeof(vboVertex_t), input->vertexPtr[0].xyz);
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}
	
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglEnableClientState( GL_COLOR_ARRAY );
	
	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
	     && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		ProjectDlightTexture();
	}
	
	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}
	
	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	
	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	if ( r_flush->integer ) {
		qglFlush();
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.renderVBO = NULL;

	GLimp_LogComment( "----------\n" );
}
