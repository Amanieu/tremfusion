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
// tr_surf.c
#include "tr_local.h"
#if idppc_altivec && !defined(MACOS_X)
#include <altivec.h>
#endif
#if id386_sse >= 1
#include "../qcommon/qsse.h"
#endif

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================
vec4_t vec4Scratch ALIGNED((16));


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			ndx;

	if ( tess.vertexPtr ) {
		ndx = tess.numVertexes;
		
		tess.vertexPtr[ndx].xyz[0] = origin[0] + left[0] + up[0];
		tess.vertexPtr[ndx].xyz[1] = origin[1] + left[1] + up[1];
		tess.vertexPtr[ndx].xyz[2] = origin[2] + left[2] + up[2];
		
		tess.vertexPtr[ndx+1].xyz[0] = origin[0] - left[0] + up[0];
		tess.vertexPtr[ndx+1].xyz[1] = origin[1] - left[1] + up[1];
		tess.vertexPtr[ndx+1].xyz[2] = origin[2] - left[2] + up[2];
		
		tess.vertexPtr[ndx+2].xyz[0] = origin[0] - left[0] - up[0];
		tess.vertexPtr[ndx+2].xyz[1] = origin[1] - left[1] - up[1];
		tess.vertexPtr[ndx+2].xyz[2] = origin[2] - left[2] - up[2];
		
		tess.vertexPtr[ndx+3].xyz[0] = origin[0] + left[0] - up[0];
		tess.vertexPtr[ndx+3].xyz[1] = origin[1] + left[1] - up[1];
		tess.vertexPtr[ndx+3].xyz[2] = origin[2] + left[2] - up[2];
		
		// constant normal all the way around
		VectorSubtract( vec3_origin, backEnd.viewParms.or.axis[0], normal );
		
		VectorCopy( normal, tess.vertexPtr[ndx].normal );
		VectorCopy( normal, tess.vertexPtr[ndx+1].normal );
		VectorCopy( normal, tess.vertexPtr[ndx+2].normal );
		VectorCopy( normal, tess.vertexPtr[ndx+3].normal );
		
		// standard square texture coordinates
		tess.vertexPtr[ndx].tc1[0] = tess.vertexPtr[ndx].tc2[0] = s1;
		tess.vertexPtr[ndx].tc1[1] = tess.vertexPtr[ndx].tc2[1] = t1;
		
		tess.vertexPtr[ndx+1].tc1[0] = tess.vertexPtr[ndx+1].tc2[0] = s2;
		tess.vertexPtr[ndx+1].tc1[1] = tess.vertexPtr[ndx+1].tc2[1] = t1;
		
		tess.vertexPtr[ndx+2].tc1[0] = tess.vertexPtr[ndx+2].tc2[0] = s2;
		tess.vertexPtr[ndx+2].tc1[1] = tess.vertexPtr[ndx+2].tc2[1] = t2;
		
		tess.vertexPtr[ndx+3].tc1[0] = tess.vertexPtr[ndx+3].tc2[0] = s1;
		tess.vertexPtr[ndx+3].tc1[1] = tess.vertexPtr[ndx+3].tc2[1] = t2;
		
		// constant color all the way around
		// should this be identity and let the shader specify from entity?
		tess.vertexPtr[ndx].color[0] = color[0];
		tess.vertexPtr[ndx].color[1] = color[1];
		tess.vertexPtr[ndx].color[2] = color[2];
		tess.vertexPtr[ndx].color[3] = color[3];
		tess.vertexPtr[ndx+1].color[0] = color[0];
		tess.vertexPtr[ndx+1].color[1] = color[1];
		tess.vertexPtr[ndx+1].color[2] = color[2];
		tess.vertexPtr[ndx+1].color[3] = color[3];
		tess.vertexPtr[ndx+2].color[0] = color[0];
		tess.vertexPtr[ndx+2].color[1] = color[1];
		tess.vertexPtr[ndx+2].color[2] = color[2];
		tess.vertexPtr[ndx+2].color[3] = color[3];
		tess.vertexPtr[ndx+3].color[0] = color[0];
		tess.vertexPtr[ndx+3].color[1] = color[1];
		tess.vertexPtr[ndx+3].color[2] = color[2];
		tess.vertexPtr[ndx+3].color[3] = color[3];
	} else {
		ndx = 0; // should never be in a VBO
	}

	if ( tess.indexPtr ) {
		if ( tess.minIndex > ndx )
			tess.minIndex = ndx;
		if ( tess.maxIndex < ndx + 5 )
			tess.maxIndex = ndx + 5;
		
		// triangle indexes for a simple quad
		tess.indexPtr[ tess.numIndexes ] = ndx;
		tess.indexPtr[ tess.numIndexes + 1 ] = ndx + 1;
		tess.indexPtr[ tess.numIndexes + 2 ] = ndx + 3;
		
		tess.indexPtr[ tess.numIndexes + 3 ] = ndx + 3;
		tess.indexPtr[ tess.numIndexes + 4 ] = ndx + 1;
		tess.indexPtr[ tess.numIndexes + 5 ] = ndx + 2;
	}
	
	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.or.axis[1], radius, left );
		VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;
		
		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.or.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.or.axis[2], left );

		VectorScale( backEnd.viewParms.or.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.or.axis[1], up );
	}
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	}

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}


/*
=============
RB_SurfacePolychain
=============
*/
static void RB_SurfacePolychain( surfaceType_t *surface ) {
	srfPoly_t *p = (srfPoly_t *)surface;
	int		i;
	int		numv;
	GLushort	*indexPtr;
	GLuint		*indexPtr32;
	vboVertex_t	*vertexPtr;

	if ( tess.vertexPtr ) {
		vertexPtr = tess.vertexPtr + tess.numVertexes;
		
		// fan triangles into the tess array
		numv = tess.numVertexes;
		for ( i = 0; i < p->numVerts; i++ ) {
			VectorCopy ( p->verts[i].xyz, *(vec3_t *)vertexPtr->xyz );
			Vector2Copy( p->verts[i].st,  vertexPtr->tc1 );
			*(int *)(&vertexPtr->color) = *(int *)p->verts[ i ].modulate;
			
			vertexPtr++;
		}
	} else {
		numv = 0; //ERROR, should never have coordinates in VBO
	}

	if ( tess.indexPtr ) {
		indexPtr = ptrPlusOffset(tess.indexPtr, tess.indexInc * tess.numIndexes);

		if ( tess.minIndex > numv )
			tess.minIndex = numv;
		if ( tess.maxIndex < numv + p->numVerts - 1 )
			tess.maxIndex = numv + p->numVerts - 1;
		
		// generate fan indexes into the tess array
		if ( tess.indexInc == sizeof(GLushort) ) {
			for ( i = 0; i < p->numVerts-2; i++ ) {
				indexPtr[0] = (GLushort)numv;
				indexPtr[1] = (GLushort)(numv + i + 1);
				indexPtr[2] = (GLushort)(numv + i + 2);
				indexPtr += 3;
			}
		} else {
			indexPtr32 = (GLuint *)indexPtr;
			for ( i = 0; i < p->numVerts-2; i++ ) {
				indexPtr32[0] = numv;
				indexPtr32[1] = numv + i + 1;
				indexPtr32[2] = numv+ i + 2;
				indexPtr32 += 3;
			}
		}
	}
	
	tess.numVertexes += p->numVerts;
	tess.numIndexes  += 3*(p->numVerts - 2);
}


/*
=============
RB_SurfaceTriangles
=============
*/
static void RB_SurfaceTriangles( surfaceType_t *surface ) {
	srfTriangles_t *srf = (srfTriangles_t *)surface;
	int			i;
	drawVert_t	*dv;
	int			dlightBits;
	GLushort	*indexPtr;
	GLuint		*indexPtr32;
	vboVertex_t	*vertexPtr;
	int             numv;

	dlightBits = srf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	if ( tess.vertexPtr ) {
		numv = tess.numVertexes;
		
		vertexPtr = tess.vertexPtr + numv;
		dv = srf->verts;
		
		for ( i = 0 ; i < srf->numVerts ; i++, dv++ ) {
			VectorCopy( dv->xyz, *(vec3_t *)vertexPtr->xyz );
			VectorCopy( dv->normal, *(vec3_t *)vertexPtr->normal );
			Vector2Copy( dv->st,       vertexPtr->tc1 );
			Vector2Copy( dv->lightmap, vertexPtr->tc2 );
			*(int *)&vertexPtr->color = *(int *)dv->color;
			vertexPtr->dlights = dlightBits;
			vertexPtr++;
		}
	} else {
		numv = srf->vboStart;
	}

	if ( tess.indexPtr ) {
		if ( tess.minIndex > numv )
			tess.minIndex = numv;
		if ( tess.maxIndex < numv + srf->numVerts - 1 )
			tess.maxIndex = numv + srf->numVerts - 1;
		
		if ( tess.indexInc == sizeof(GLushort) ) {
			indexPtr = tess.indexPtr + tess.numIndexes;
			
			for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
				indexPtr[ i + 0 ] = (GLushort)(numv + srf->indexes[ i + 0 ]);
				indexPtr[ i + 1 ] = (GLushort)(numv + srf->indexes[ i + 1 ]);
				indexPtr[ i + 2 ] = (GLushort)(numv + srf->indexes[ i + 2 ]);
			}
		} else {
			indexPtr32 = ((GLuint *)tess.indexPtr) + tess.numIndexes;
			for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
				indexPtr32[ i + 0 ] = numv + srf->indexes[ i + 0 ];
				indexPtr32[ i + 1 ] = numv + srf->indexes[ i + 1 ];
				indexPtr32[ i + 2 ] = numv + srf->indexes[ i + 2 ];
			}
		}
	}

	tess.numIndexes += srf->numIndexes;
	tess.numVertexes += srf->numVerts;
}



/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam( void ) 
{
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS ; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
//		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	GL_Bind( tr.whiteImage );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	qglColor3f( 1, 0, 0 );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i <= NUM_BEAM_SEGS; i++ ) {
		qglVertex3fv( start_points[ i % NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[ i % NUM_BEAM_SEGS] );
	}
	qglEnd();
}

//================================================================================

static void DoRailCore( const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth )
{
	float		spanWidth2;
	int			vbase;
	float		t = len / 256.0f;

	spanWidth2 = -spanWidth;


	if ( tess.vertexPtr ) {
		vbase = tess.numVertexes;
		
		// FIXME: use quad stamp?
		VectorMA( start, spanWidth, up, tess.vertexPtr[vbase].xyz );
		tess.vertexPtr[vbase].tc1[0] = 0;
		tess.vertexPtr[vbase].tc1[1] = 0;
		tess.vertexPtr[vbase].color[0] = backEnd.currentEntity->e.shaderRGBA[0] * 0.25;
		tess.vertexPtr[vbase].color[1] = backEnd.currentEntity->e.shaderRGBA[1] * 0.25;
		tess.vertexPtr[vbase].color[2] = backEnd.currentEntity->e.shaderRGBA[2] * 0.25;

		VectorMA( start, spanWidth2, up, tess.vertexPtr[vbase+1].xyz );
		tess.vertexPtr[vbase+1].tc1[0] = 0;
		tess.vertexPtr[vbase+1].tc1[1] = 1;
		tess.vertexPtr[vbase+1].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr[vbase+1].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr[vbase+1].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
		
		VectorMA( start, spanWidth, up, tess.vertexPtr[vbase+2].xyz );
		tess.vertexPtr[vbase+2].tc1[0] = t;
		tess.vertexPtr[vbase+2].tc1[1] = 0;
		tess.vertexPtr[vbase+2].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr[vbase+2].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr[vbase+2].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
		
		VectorMA( start, spanWidth, up, tess.vertexPtr[vbase+3].xyz );
		tess.vertexPtr[vbase+3].tc1[0] = t;
		tess.vertexPtr[vbase+3].tc1[1] = 1;
		tess.vertexPtr[vbase+3].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
		tess.vertexPtr[vbase+3].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
		tess.vertexPtr[vbase+3].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
	} else {
		vbase = 0;
	}
	
	if ( tess.indexPtr ) {
		tess.indexPtr[tess.numIndexes] = vbase;
		tess.indexPtr[tess.numIndexes+1] = vbase + 1;
		tess.indexPtr[tess.numIndexes+2] = vbase + 2;
		
		tess.indexPtr[tess.numIndexes+3] = vbase + 2;
		tess.indexPtr[tess.numIndexes+4] = vbase + 1;
		tess.indexPtr[tess.numIndexes+5] = vbase + 3;
	}

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

static void DoRailDiscs( int numSegs, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up )
{
	int i;
	vec3_t	pos[4];
	vec3_t	v;
	int		spanWidth = r_railWidth->integer;
	int		vbase;
	float c, s;
	float		scale = 0.25;

	if ( numSegs > 1 )
		numSegs--;
	if ( !numSegs )
		return;

	for ( i = 0; i < 4; i++ )
	{
		c = cos( DEG2RAD( 45 + i * 90 ) );
		s = sin( DEG2RAD( 45 + i * 90 ) );
		v[0] = ( right[0] * c + up[0] * s ) * scale * spanWidth;
		v[1] = ( right[1] * c + up[1] * s ) * scale * spanWidth;
		v[2] = ( right[2] * c + up[2] * s ) * scale * spanWidth;
		VectorAdd( start, v, pos[i] );

		if ( numSegs > 1 )
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd( pos[i], dir, pos[i] );
		}
	}

	if ( tess.vertexPtr ) {
		vbase = tess.numVertexes;
		for ( i = 0; i < numSegs; i++ )
		{
			int j;
			
			for ( j = 0; j < 4; j++ )
			{
				VectorCopy( pos[j], tess.vertexPtr[vbase].xyz );
				tess.vertexPtr[vbase].tc1[0] = ( j < 2 );
				tess.vertexPtr[vbase].tc1[1] = ( j && j != 3 );
				tess.vertexPtr[vbase].color[0] = backEnd.currentEntity->e.shaderRGBA[0];
				tess.vertexPtr[vbase].color[1] = backEnd.currentEntity->e.shaderRGBA[1];
				tess.vertexPtr[vbase].color[2] = backEnd.currentEntity->e.shaderRGBA[2];
				vbase++;
				
				VectorAdd( pos[j], dir, pos[j] );
			}
		}
		vbase = tess.numVertexes;
	} else {
		vbase = 0;
	}

	for ( i = 0; i < numSegs; i++ )
	{
		int iwrite = tess.numIndexes;
		tess.indexPtr[iwrite++] = vbase + 0;
		tess.indexPtr[iwrite++] = vbase + 1;
		tess.indexPtr[iwrite++] = vbase + 3;
		tess.indexPtr[iwrite++] = vbase + 3;
		tess.indexPtr[iwrite++] = vbase + 1;
		tess.indexPtr[iwrite++] = vbase + 2;
		vbase += 4;
	}

	tess.numVertexes += numSegs * 4;
	tess.numIndexes += numSegs * 6;
}

/*
** RB_SurfaceRailRinges
*/
static void RB_SurfaceRailRings( void ) {
	refEntity_t *e;
	int			numSegs;
	int			len;
	vec3_t		vec;
	vec3_t		right, up;
	vec3_t		start, end;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	MakeNormalVectors( vec, right, up );
	numSegs = ( len ) / r_railSegmentLength->value;
	if ( numSegs <= 0 ) {
		numSegs = 1;
	}

	VectorScale( vec, r_railSegmentLength->value, vec );

	DoRailDiscs( numSegs, start, vec, right, up );
}

/*
** RB_SurfaceRailCore
*/
static void RB_SurfaceRailCore( void ) {
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, start );
	VectorCopy( e->origin, end );

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoRailCore( start, end, right, len, r_railCoreWidth->integer );
}

/*
** RB_SurfaceLightningBolt
*/
static void RB_SurfaceLightningBolt( void ) {
	refEntity_t *e;
	int			len;
	vec3_t		right;
	vec3_t		vec;
	vec3_t		start, end;
	vec3_t		v1, v2;
	int			i;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute variables
	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorNormalize( v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	VectorNormalize( v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	for ( i = 0 ; i < 4 ; i++ ) {
		vec3_t	temp;

		DoRailCore( start, end, right, len, 8 );
		RotatePointAroundVector( temp, vec, right, 45 );
		VectorCopy( temp, right );
	}
}

/*
** VectorArrayNormalize
*
* The inputs to this routing seem to always be close to length = 1.0 (about 0.6 to 2.0)
* This means that we don't have to worry about zero length or enormously long vectors.
*/
static void VectorArrayNormalize(vec4_t *normals, unsigned int count)
{
//    assert(count);
        
#if idppc
    {
        register float half = 0.5;
        register float one  = 1.0;
        float *components = (float *)normals;
        
        // Vanilla PPC code, but since PPC has a reciprocal square root estimate instruction,
        // runs *much* faster than calling sqrt().  We'll use a single Newton-Raphson
        // refinement step to get a little more precision.  This seems to yeild results
        // that are correct to 3 decimal places and usually correct to at least 4 (sometimes 5).
        // (That is, for the given input range of about 0.6 to 2.0).
        do {
            float x, y, z;
            float B, y0, y1;
            
            x = components[0];
            y = components[1];
            z = components[2];
            components += 4;
            B = x*x + y*y + z*z;

#ifdef __GNUC__            
            asm("frsqrte %0,%1" : "=f" (y0) : "f" (B));
#else
			y0 = __frsqrte(B);
#endif
            y1 = y0 + half*y0*(one - B*y0*y0);

            x = x * y1;
            y = y * y1;
            components[-4] = x;
            z = z * y1;
            components[-3] = y;
            components[-2] = z;
        } while(count--);
    }
#elif id386_sse >= 1
    if (com_sse->integer >= 1) {
	while (count--) {
	    v4f normalVec = v4fLoadU((float *)normals);
	    s4f dotProd = v4fDotProduct(normalVec, normalVec);
	    s4f factor = v4fInverseRoot(dotProd);

	    normalVec = v4fScale(factor, normalVec);
	    v4fStoreU((float *)normals, normalVec);

	    normals++;
	}
    }
    else {
    	while (count--) {
	    VectorNormalizeFast(normals[0]);
	    normals++;
	}
    }
#else // No assembly version for this architecture, or C_ONLY defined
	// given the input, it's safe to call VectorNormalizeFast
    while (count--) {
        VectorNormalizeFast(normals[0]);
        normals++;
    }
#endif

}



/*
** LerpMeshVertexes
*/
#if idppc_altivec
static void LerpMeshVertexes_altivec(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	vboVertex_t	*vertexPtr;
	float	oldXyzScale ALIGNED(16);
	float   newXyzScale ALIGNED(16);
	float	oldNormalScale ALIGNED(16);
	float newNormalScale ALIGNED(16);
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	vertexPtr = tess.vertexPtr + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		vector signed short newNormalsVec0;
		vector signed short newNormalsVec1;
		vector signed int newNormalsIntVec;
		vector float newNormalsFloatVec;
		vector float newXyzScaleVec;
		vector unsigned char newNormalsLoadPermute;
		vector unsigned char newNormalsStorePermute;
		vector float zero;
		
		newNormalsStorePermute = vec_lvsl(0,(float *)&newXyzScaleVec);
		newXyzScaleVec = *(vector float *)&newXyzScale;
		newXyzScaleVec = vec_perm(newXyzScaleVec,newXyzScaleVec,newNormalsStorePermute);
		newXyzScaleVec = vec_splat(newXyzScaleVec,0);		
		newNormalsLoadPermute = vec_lvsl(0,newXyz);
		newNormalsStorePermute = vec_lvsr(0,outXyz);
		zero = (vector float)vec_splat_s8(0);
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4, vertexPtr++ )
		{
			newNormalsLoadPermute = vec_lvsl(0,newXyz);
			newNormalsStorePermute = vec_lvsr(0,outXyz);
			newNormalsVec0 = vec_ld(0,newXyz);
			newNormalsVec1 = vec_ld(16,newXyz);
			newNormalsVec0 = vec_perm(newNormalsVec0,newNormalsVec1,newNormalsLoadPermute);
			newNormalsIntVec = vec_unpackh(newNormalsVec0);
			newNormalsFloatVec = vec_ctf(newNormalsIntVec,0);
			newNormalsFloatVec = vec_madd(newNormalsFloatVec,newXyzScaleVec,zero);
			newNormalsFloatVec = vec_perm(newNormalsFloatVec,newNormalsFloatVec,newNormalsStorePermute);
			//outXyz[0] = newXyz[0] * newXyzScale;
			//outXyz[1] = newXyz[1] * newXyzScale;
			//outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vec_ste(newNormalsFloatVec,0,vertexPtr->xyz);
			vec_ste(newNormalsFloatVec,4,vertexPtr->xyz);
			vec_ste(newNormalsFloatVec,8,vertexPtr->xyz);
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
		       vertexPtr++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr->normal);
		}
   	}
}
#endif

#if id386_sse >= 2
static void LerpMeshVertexes_sse2(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	vboVertex_t	*vertexPtr;
	float    oldXyzScale;
	float    newXyzScale;
	float    oldNormalScale;
	float    newNormalScale;
	int	 vertNum;
	unsigned lat, lng;
	int	 numVerts;

	vertexPtr = tess.vertexPtr + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		v8s     newNormalsVec;
		v4i     newNormalsIntVec, dummy;
		v4f     newNormalsFloatVec;
		s4f     newXyzScaleVec = s4fInit(newXyzScale);
		
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			     newXyz += 4, newNormals += 4, vertexPtr++ )
		{
			newNormalsVec = v8sLoadU(newXyz);
			v8s_to_v4i(newNormalsVec, newNormalsIntVec, dummy);
			newNormalsFloatVec = v4i_to_v4f(newNormalsIntVec);
			newNormalsFloatVec = v4fScale(newXyzScaleVec, newNormalsFloatVec);
			//outXyz[0] = newXyz[0] * newXyzScale;
			//outXyz[1] = newXyz[1] * newXyzScale;
			//outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
			
			v4fStoreA(vertexPtr->xyz, newNormalsFloatVec);
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
		       vertexPtr++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr->normal);
		}
		//if ( tess.normalPtr )
		//	VectorArrayNormalize(tess.normalPtr + tess.numVertexes, numVerts);
   	}
}
#endif

static void LerpMeshVertexes_scalar(md3Surface_t *surf, float backlerp)
{
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	vboVertex_t	*vertexPtr;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	vertexPtr = tess.vertexPtr + tess.numVertexes;

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4, vertexPtr ++ )
		{

			vertexPtr->xyz[0] = newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )

			vertexPtr->normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			vertexPtr->normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			vertexPtr->normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
		       vertexPtr++ )
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			vertexPtr->xyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			vertexPtr->xyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			vertexPtr->xyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			vertexPtr->normal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			vertexPtr->normal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			vertexPtr->normal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (vertexPtr->normal);
		}
		//if ( tess.normalPtr )
		//	VectorArrayNormalize(tess.normalPtr + tess.numVertexes, numVerts);
   	}
}


/*
=============
RB_SurfaceMesh
=============
*/
#if idppc_altivec
static void RB_SurfaceMesh_altivec(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Doug;
	int				numVerts;
	vboVertex_t		*vertexPtr;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	if ( tess.vertexPtr ) {
		LerpMeshVertexes_altivec (surface, backlerp);
		
		Doug = tess.numVertexes;
		texCoords = (float *) ((byte *)surface + surface->ofsSt);
		vertexPtr = tess.vertexPtr + Doug;

		numVerts = surface->numVerts;
		for ( j = 0; j < numVerts; j++, vertexPtr++ ) {
			vertexPtr->tc1[0] = texCoords[j*2+0];
			vertexPtr->tc1[1] = texCoords[j*2+1];
			// FIXME: fill in lightmapST for completeness?
		}
	} else {
		Doug = 0;
	}
	
	if ( tess.indexPtr ) {
		if ( tess.minIndex > Doug )
			tess.minIndex = Doug;
		if ( tess.maxIndex < Doug + surface->numVerts - 1 )
			tess.maxIndex = Doug + surface->numVerts - 1;

		triangles = (int *) ((byte *)surface + surface->ofsTriangles);
		indexes = surface->numTriangles * 3;

		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexes + tess.numIndexes;
			for (j = 0 ; j < indexes ; j++) {
				*indexPtr++ = Doug + triangles[j];
			}
		} else {
			GLushort *indexPtr32 = ((GLuint *)tess.indexes) + tess.numIndexes;
			for (j = 0 ; j < indexes ; j++) {
				*indexPtr32++ = Doug + triangles[j];
			}
		}
	}
	tess.numVertexes += surface->numVerts;
	tess.numIndexes  += 3*surface->numTriangles;
}
#endif

#if id386_sse >= 2
static void RB_SurfaceMesh_sse2(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	unsigned			*triangles;
	float			*texCoords;
	int				indexes;
	int				Doug;
	int				numVerts;
	vboVertex_t		*vertexPtr;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	if ( tess.vertexPtr ) {
		LerpMeshVertexes_sse2 (surface, backlerp);
		
		Doug = tess.numVertexes;
		texCoords = (float *) ((byte *)surface + surface->ofsSt);
		vertexPtr = tess.vertexPtr + Doug;
		
		numVerts = surface->numVerts;
		for ( j = 0; j < numVerts; j++ ) {
			vertexPtr->tc1[0] = texCoords[j*2+0];
			vertexPtr->tc1[1] = texCoords[j*2+1];
			vertexPtr++;
			// FIXME: fill in lightmapST for completeness?
		}
	} else {
		Doug = 0;
	}

	if ( tess.indexPtr ) {
		if ( tess.minIndex > Doug )
			tess.minIndex = Doug;
		if ( tess.maxIndex < Doug + surface->numVerts - 1 )
			tess.maxIndex = Doug + surface->numVerts - 1;
		
		triangles = (unsigned *) ((byte *)surface + surface->ofsTriangles);
		indexes = surface->numTriangles * 3;
		
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr + tess.numIndexes;
			CopyArrayAndAddConstantShort_sse2(indexPtr, triangles,
							  Doug, indexes);
		} else {
			GLuint *indexPtr32 = ((GLuint *)tess.indexPtr) + tess.numIndexes;
			CopyArrayAndAddConstant_sse2(indexPtr32, triangles,
						     Doug, indexes);
		}
	}

	tess.numVertexes += surface->numVerts;
	tess.numIndexes  += 3*surface->numTriangles;
}
#endif

static void RB_SurfaceMesh_scalar(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Doug;
	int				numVerts;
	vboVertex_t		*vertexPtr;

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	}

	if ( tess.vertexPtr ) {
		LerpMeshVertexes_scalar (surface, backlerp);

		Doug = tess.numVertexes;
		
		texCoords = (float *) ((byte *)surface + surface->ofsSt);
		vertexPtr = tess.vertexPtr + Doug;
		
		numVerts = surface->numVerts;
		for ( j = 0; j < numVerts; j++ ) {
			vertexPtr->tc1[0] = texCoords[j*2+0];
			vertexPtr->tc1[1] = texCoords[j*2+1];
			vertexPtr++;
			// FIXME: fill in lightmapST for completeness?
		}
	} else {
		Doug = 0;
	}

	if ( tess.indexPtr ) {
		if ( tess.minIndex > Doug )
			tess.minIndex = Doug;
		if ( tess.maxIndex < Doug + surface->numVerts - 1 )
			tess.maxIndex = Doug + surface->numVerts - 1;
		
		triangles = (int *) ((byte *)surface + surface->ofsTriangles);
		indexes = surface->numTriangles * 3;
		
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr + tess.numIndexes;
			for (j = 0 ; j < indexes ; j++) {
				*indexPtr++ = (GLushort)(Doug + triangles[j]);
			}
		} else {
			GLuint *indexPtr32 = ((GLuint *)tess.indexPtr) + tess.numIndexes;
			for (j = 0 ; j < indexes ; j++) {
				*indexPtr32++ = Doug + triangles[j];
			}
		}
	}
	tess.numVertexes += surface->numVerts;
	tess.numIndexes  += 3*surface->numTriangles;
}

void RB_SurfaceMesh( surfaceType_t *surf) {
	md3Surface_t *surface = (md3Surface_t *)surf;
#if idppc_altivec
  if (com_altivec->integer) {
    RB_SurfaceMesh_altivec( surface );
    return;
  }
#endif // idppc_altivec
#if id386_sse >= 2
  if (com_sse->integer >= 2) {
    RB_SurfaceMesh_sse2( surface );
    return;
  }
#endif // id386_sse
  RB_SurfaceMesh_scalar( surface );
}


/*
==============
RB_SurfaceFace
==============
*/
#if id386_sse >= 2
static void RB_SurfaceFace_sse2( srfSurfaceFace_t *surf ) {
	int		 i;
	unsigned	*indices;
	float		*v;
	int		 Bob;
	v4i              Mask1, Mask2, dataVec, nextVec;
	int		 numPoints;
	int		 dlightBits;
	vboVertex_t	*vertexPtr;
	
	dlightBits = surf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;
	
	if ( tess.vertexPtr ) {
		Bob = tess.numVertexes;
		v = surf->points[0];
		
		numPoints = surf->numPoints;
		
		vertexPtr = tess.vertexPtr + tess.numVertexes;
		
		v4f normalVec = v4fLoadU(surf->plane.normal);
		
		Mask1 = v4iInit(0, 0, 0, -1);
		Mask2 = v4iInit(-1, -1, -1, 0);
		for ( i = 0, v = surf->points[0]; i < numPoints; i++, v += VERTEXSIZE,
			      vertexPtr++ ) {
			v4fStoreA(vertexPtr->normal, normalVec);
			
			dataVec = v4iLoadU((int *)v);
			nextVec = v4iLoadU((int *)(v + 4));
			
			v4iStoreA((int *)&vertexPtr->xyz, dataVec);
			dataVec = v4iOr(v4iAnd(Mask1, dataVec), v4iAnd(Mask2, nextVec));
			dataVec = _mm_shuffle_epi32(dataVec, 0x93);
			v4iStoreA((int *)&vertexPtr->tc1, dataVec);
			
			nextVec = _mm_shuffle_epi32(nextVec, 0xff);
			*(unsigned int *)&vertexPtr->color = _mm_cvtsi128_si32(nextVec);
			
			vertexPtr->dlights = dlightBits;
		}
	} else {
		Bob = surf->vboStart;
	}
	if ( tess.indexPtr ) {
		if ( tess.minIndex > Bob )
			tess.minIndex = Bob;
		if ( tess.maxIndex < Bob + surf->numPoints - 1 )
			tess.maxIndex = Bob + surf->numPoints - 1;
		
		indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr + tess.numIndexes;
			CopyArrayAndAddConstantShort_sse2(tessIndexes, indices, Bob, surf->numIndices);
		} else {
			GLuint *indexPtr32 = ((GLuint *)tess.Indexes) + tess.numIndexes;
			CopyArrayAndAddConstant_sse2(indexPtr32, indices, Bob, surf->numIndices);
		}
	}
	
	tess.numVertexes += surf->numPoints;
	tess.numIndexes  += surf->numIndices;
}
#endif
static ID_INLINE void RB_SurfaceFace_scalar( srfSurfaceFace_t *surf ) {
	int			i;
	unsigned	*indices;
	float		*v;
	float		*normal;
	int			Bob;
	int			numPoints;
	int			dlightBits;
	vboVertex_t	*vertexPtr;

	dlightBits = surf->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	if ( tess.vertexPtr ) {
		Bob = tess.numVertexes;

		v = surf->points[0];
		
		numPoints = surf->numPoints;
		
		vertexPtr = tess.vertexPtr + tess.numVertexes;
		
		normal = surf->plane.normal;
		
		for ( i = 0, v = surf->points[0]; i < numPoints; i++, v += VERTEXSIZE,
			      vertexPtr++ ) {
			VectorCopy( normal, *(vec3_t *)&vertexPtr->normal );
			
			VectorCopy ( v, *(vec3_t *)&vertexPtr->xyz);
			Vector2Copy( v+3, vertexPtr->tc1 );
			Vector2Copy( v+5, vertexPtr->tc2 );
			*(unsigned int *)&vertexPtr->color = *(unsigned int *)&v[7];
			vertexPtr->dlights = dlightBits;
		}
	} else {
		Bob = surf->vboStart;
	}

	if ( tess.indexPtr ) {
		if ( tess.minIndex > Bob )
			tess.minIndex = Bob;
		if ( tess.maxIndex < Bob + surf->numPoints - 1 )
			tess.maxIndex = Bob + surf->numPoints - 1;
		
		indices = ( unsigned * ) ( ( ( char  * ) surf ) + surf->ofsIndices );
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr + tess.numIndexes;
			for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
				indexPtr[i] = (GLushort)(indices[i] + Bob);
			}
		} else {
			GLuint *indexPtr32 = ((GLuint *)tess.indexPtr) + tess.numIndexes;
			for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
				indexPtr32[i] = indices[i] + Bob;
			}
		}
	}

	tess.numVertexes += surf->numPoints;
	tess.numIndexes  += surf->numIndices;
}

static void RB_SurfaceFace( surfaceType_t *surface ) {
	srfSurfaceFace_t *surf = (srfSurfaceFace_t *)surface;
#if id386_sse >= 2
	if (com_sse->integer >= 2) {
		RB_SurfaceFace_sse2( surf );
		return;
	}
#endif // id386_sse
	RB_SurfaceFace_scalar ( surf );
}


static float	LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	// never let it go negative
	if ( r_lodCurveError->value < 0 ) {
		return 0;
	}

	world[0] = local[0] * backEnd.or.axis[0][0] + local[1] * backEnd.or.axis[1][0] + 
		local[2] * backEnd.or.axis[2][0] + backEnd.or.origin[0];
	world[1] = local[0] * backEnd.or.axis[0][1] + local[1] * backEnd.or.axis[1][1] + 
		local[2] * backEnd.or.axis[2][1] + backEnd.or.origin[1];
	world[2] = local[0] * backEnd.or.axis[0][2] + local[1] * backEnd.or.axis[1][2] + 
		local[2] * backEnd.or.axis[2][2] + backEnd.or.origin[2];

	VectorSubtract( world, backEnd.viewParms.or.origin, world );
	d = DotProduct( world, backEnd.viewParms.or.axis[0] );

	if ( d < 0 ) {
		d = -d;
	}
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	}

	return r_lodCurveError->value / d;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
static void RB_SurfaceGrid( surfaceType_t *surface ) {
	srfGridMesh_t *cv = (srfGridMesh_t *)surface;
	int		i, j;
	vboVertex_t	*vertexPtr;
	drawVert_t	*dv;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		baseVertex;
	int		dlightBits;

	dlightBits = cv->dlightBits[backEnd.smpFrame];
	tess.dlightBits |= dlightBits;

	// determine the allowable discrepance
	if ( r_ext_vertex_buffer_object->integer ) {
		// always render max res for VBOs
		lodError = r_lodCurveError->value;
	} else {
		lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );
	}

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	}
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	}
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;

	if ( tess.vertexPtr ) {
		baseVertex = tess.numVertexes;
		vertexPtr = tess.vertexPtr + baseVertex;
		
		for ( i = 0 ; i < lodHeight ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++, vertexPtr++ ) {
				dv = cv->verts + heightTable[ i ] * cv->width
					+ widthTable[ j ];
				
				VectorCopy( dv->xyz, *(vec3_t *)&vertexPtr->xyz );
				Vector2Copy ( dv->st, vertexPtr->tc1 );
				Vector2Copy ( dv->lightmap, vertexPtr->tc2 );
				VectorCopy( dv->normal, *(vec3_t *)&vertexPtr->normal );
				*(unsigned int *)&vertexPtr->color = *(unsigned int *)dv->color;
				vertexPtr->dlights = dlightBits;
			}
		}
	} else {
		baseVertex = cv->vboStart;
	}
	
	if ( tess.indexPtr ) {
		if ( tess.minIndex > baseVertex )
			tess.minIndex = baseVertex;
		if ( tess.maxIndex < baseVertex + lodWidth*lodHeight - 1 )
			tess.maxIndex = baseVertex + lodWidth*lodHeight - 1;

		// add the indexes
		int		w, h;
		
		h = lodHeight - 1;
		w = lodWidth - 1;
		
		if ( tess.indexInc == sizeof(GLushort) ) {
			GLushort *indexPtr = tess.indexPtr + tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
					
					// vertex order to be reckognized as tristrips
					v1 = baseVertex + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;
					
					*indexPtr++ = v2;
					*indexPtr++ = v3;
					*indexPtr++ = v1;
					
					*indexPtr++ = v1;
					*indexPtr++ = v3;
					*indexPtr++ = v4;
				}
			}
		} else {
			GLuint *indexPtr32 = ((GLuint *)tess.indexPtr) + tess.numIndexes;
			for (i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;
					
					// vertex order to be reckognized as tristrips
					v1 = baseVertex + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;
					
					*indexPtr32++ = v2;
					*indexPtr32++ = v3;
					*indexPtr32++ = v1;
					
					*indexPtr32++ = v1;
					*indexPtr32++ = v3;
					*indexPtr32++ = v4;
				}
			}
		}
	}
	
	tess.numVertexes += lodWidth * lodHeight;
	tess.numIndexes  += 6 * (lodWidth - 1) * (lodHeight - 1);
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( void ) {
	GL_Bind( tr.whiteImage );
	qglLineWidth( 3 );
	qglBegin( GL_LINES );
	qglColor3f( 1,0,0 );
	qglVertex3f( 0,0,0 );
	qglVertex3f( 16,0,0 );
	qglColor3f( 0,1,0 );
	qglVertex3f( 0,0,0 );
	qglVertex3f( 0,16,0 );
	qglColor3f( 0,0,1 );
	qglVertex3f( 0,0,0 );
	qglVertex3f( 0,0,16 );
	qglEnd();
	qglLineWidth( 1 );
}

//===========================================================================

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
static void RB_SurfaceEntity( surfaceType_t *surfType ) {
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break;
	case RT_BEAM:
		RB_SurfaceBeam();
		break;
	case RT_RAIL_CORE:
		RB_SurfaceRailCore();
		break;
	case RT_RAIL_RINGS:
		RB_SurfaceRailRings();
		break;
	case RT_LIGHTNING:
		RB_SurfaceLightningBolt();
		break;
	default:
		RB_SurfaceAxis();
		break;
	}
	return;
}

static void RB_SurfaceBad( surfaceType_t *surfType ) {
	ri.Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}

static void RB_SurfaceFlare( surfaceType_t *surface )
{
	srfFlare_t *surf = (srfFlare_t *)surface;

	if (r_flares->integer)
		RB_AddFlare(surf, tess.fogNum, surf->origin, surf->color, surf->normal);
}

static void RB_SurfaceDisplayList( surfaceType_t *surface ) {
	srfDisplayList_t *surf = (srfDisplayList_t *)surface;
	// all apropriate state must be set in RB_BeginSurface
	// this isn't implemented yet...
	qglCallList( surf->listNum );
}

static void RB_SurfaceSkip( surfaceType_t *surf ) {
}


void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( surfaceType_t * ) = {
	RB_SurfaceBad,			// SF_BAD, 
	RB_SurfaceSkip,			// SF_SKIP, 
	RB_SurfaceFace,			// SF_FACE,
	RB_SurfaceGrid,			// SF_GRID,
	RB_SurfaceTriangles,		// SF_TRIANGLES,
	RB_SurfacePolychain,		// SF_POLY,
	RB_SurfaceMesh,			// SF_MD3,
	RB_SurfaceAnim,			// SF_MD4,
#ifdef RAVENMD4
	RB_MDRSurfaceAnim,		// SF_MDR,
#endif
	RB_SurfaceFlare,		// SF_FLARE,
	RB_SurfaceEntity,		// SF_ENTITY
	RB_SurfaceDisplayList		// SF_DISPLAY_LIST
};
