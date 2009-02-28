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
#include "q_shared.h"

#if id386_sse

#include "qsse.h"

v4f v4fZero;
v4f v4fZeroDotOne;
v4f v4fMZeroDotOne;
v4f v4fZeroDotTwoFive;
v4f v4fZeroDotFive;
v4f v4fMZeroDotFive;
v4f v4fOne;
v4f v4fMOne;
v4f v4fXMask;
v4f v4fYMask;
v4f v4fZMask;
v4f v4fWMask;
v4f v4fXYMask;
v4f v4fXYZMask;
v4f v4fSignMask;
v4f v4fSignMaskW;
v4f v4fTwo;
v4f v4fThree;
v4f v4fFour;
v4f v4fTwoTwentyThree;  // = 2^23

v4f mixMask0000, mixMask0001, mixMask0010, mixMask0011,
    mixMask0100, mixMask0101, mixMask0110, mixMask0111,
    mixMask1000, mixMask1001, mixMask1010, mixMask1011,
    mixMask1100, mixMask1101, mixMask1110, mixMask1111;

static v4f loadInts(int a, int b, int c, int d) {
	floatint_t x[4] ALIGNED(16);
	
	x[0].i = a;
	x[1].i = b;
	x[2].i = c;
	x[3].i = d;
	return _mm_load_ps( &(x[0].f) );
}

void InitSSEMode(void) {
	/* round down, denormals are zero and flush to zero */
	_mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) | 0x8040 | _MM_ROUND_NEAREST);
	v4fZero = _mm_setzero_ps();
	v4fZeroDotOne = _mm_set1_ps( 0.1f );
	v4fMZeroDotOne = _mm_set1_ps( 0.1f );
	v4fZeroDotTwoFive = _mm_set1_ps( 0.25f );
	v4fZeroDotFive = _mm_set1_ps( 0.5f );
	v4fMZeroDotFive = _mm_set1_ps( -0.5f );
	v4fOne = _mm_set1_ps( 1.0f );
	v4fMOne = _mm_set1_ps( -1.0f );
	v4fXMask = loadInts( -1, 0, 0, 0 );
	v4fYMask = loadInts( 0, -1, 0, 0 );
	v4fZMask = loadInts( 0, 0, -1, 0 );
	v4fWMask = loadInts( 0, 0, 0, -1 );
	v4fXYMask = loadInts( -1, -1, 0, 0 );
	v4fXYZMask = loadInts( -1, -1, -1, 0 );
	v4fSignMask = loadInts( 0x80000000, 0x80000000, 0x80000000, 0x80000000 );
	v4fSignMaskW = loadInts( 0, 0, 0, 0x80000000 );
	v4fTwo = _mm_set1_ps( 2.0f );
	v4fThree = _mm_set1_ps( 3.0f );
	v4fFour = _mm_set1_ps( 4.0f );
	v4fTwoTwentyThree = _mm_set1_ps( 8388608.0f );

	mixMask0000 = loadInts(  0,  0,  0,  0 );
	mixMask0001 = loadInts(  0,  0,  0, -1 );
	mixMask0010 = loadInts(  0,  0, -1,  0 );
	mixMask0011 = loadInts(  0,  0, -1, -1 );
	mixMask0100 = loadInts(  0, -1,  0,  0 );
	mixMask0101 = loadInts(  0, -1,  0, -1 );
	mixMask0110 = loadInts(  0, -1, -1,  0 );
	mixMask0111 = loadInts(  0, -1, -1, -1 );
	mixMask1000 = loadInts( -1,  0,  0,  0 );
	mixMask1001 = loadInts( -1,  0,  0, -1 );
	mixMask1010 = loadInts( -1,  0, -1,  0 );
	mixMask1011 = loadInts( -1,  0, -1, -1 );
	mixMask1100 = loadInts( -1, -1,  0,  0 );
	mixMask1101 = loadInts( -1, -1,  0, -1 );
	mixMask1110 = loadInts( -1, -1, -1,  0 );
	mixMask1111 = loadInts( -1, -1, -1, -1 );
}

#if id386_sse >= 2
void CopyArrayAndAddConstant_sse2(unsigned *dst, unsigned *src, int add, int count)
{
	v4i            addVec, mask, dataVec, nextVec;
	
	addVec = s4iInit(add);
	
	/* make dst is 8-byte aligned */
	if ( ((int)dst & 0x04) && count > 0 ) {
		*dst++ = *src++ + add;
		count--;
	}
	
	/* make dst is 16-byte aligned */
	if ( ((int)dst & 0x08) && count > 1 ) {
		*dst++ = *src++ + add;
		*dst++ = *src++ + add;
		count -= 2;
	}
	
	/* fast SSE2 loop */
	switch ( 0x0c & (int)src ){
	case 0x0:
		while( count > 3 ) {
			count -= 4;
			dataVec = v4iLoadA( (int *)src );
			src += 4;
			dataVec = v4iAdd( dataVec, addVec );
			v4iStoreA( (int *)dst, dataVec );
			dst += 4;
		}
		break;
	case 0x4:
		src += 3;
		dataVec = v4iLoadA( (int *)(src - 4) );
		mask = v4iInit( 0, -1, -1, -1 );
		while( count > 3 ) {
			count -= 4;

			nextVec = v4iLoadA( (int *)src );
			src += 4;
			dataVec = v4iOr( v4iAnd( mask, dataVec ),
					 v4iAndNot( mask, nextVec ) );
			dataVec = _mm_shuffle_epi32( dataVec, 0x39 );
			dataVec = v4iAdd( dataVec, addVec );
			v4iStoreA( (int *)dst, dataVec );
			dst += 4;
			
			dataVec = nextVec;
		}
		src -= 3;
		break;
	case 0x8:
		src += 2;
		dataVec = v4iLoadA( (int *)(src - 4) );
		mask = v4iInit( 0, 0, -1, -1 );
		while( count > 3 ) {
			count -= 4;

			nextVec = v4iLoadA( (int *)src );
			src += 4;
			dataVec = v4iOr( v4iAnd( mask, dataVec ),
					 v4iAndNot( mask, nextVec ) );
			dataVec = _mm_shuffle_epi32( dataVec, 0x4e );
			dataVec = v4iAdd( dataVec, addVec );
			v4iStoreA( (int *)dst, dataVec );
			dst += 4;
			
			dataVec = nextVec;
		}
		src -= 2;
		break;
	case 0xc:
		src += 1;
		dataVec = v4iLoadA( (int *)(src - 4) );
		mask = v4iInit( 0, 0, 0, -1 );
		while( count > 3 ) {
			count -= 4;
			
			nextVec = v4iLoadA( (int *)src );
			src += 4;
			dataVec = v4iOr( v4iAnd( mask, dataVec ),
					 v4iAndNot( mask, nextVec ) );
			dataVec = _mm_shuffle_epi32( dataVec, 0x93 );
			dataVec = v4iAdd( dataVec, addVec );
			v4iStoreA( (int *)dst, dataVec );
			dst += 4;
			
			dataVec = nextVec;
		}
		src -= 1;
		break;
    	}
	/* copy any remaining data */
	while( count-- > 0 ) {
		*dst++ = *src++ + add;
	}
}
#endif

void CopyArrayAndAddConstant_sse1(unsigned *dst, unsigned *src, int add, int count)
{
	v2i            addVec, mask, dataVec, nextVec;
	
	addVec = s2iInit(add);
	
	/* make dst is 8-byte aligned */
	if ( 0x04 & (int)dst && count > 0 ) {
		*dst++ = *src++ + add;
		count--;
	}
	
	/* fast MMX loop */
	switch ( 0x04 & (int)src ){
	case 0x0:
		while( count > 1 ) {
			count -= 2;
			dataVec = v2iLoadA( (int *)src );
			src += 2;
			dataVec = v2iAdd( dataVec, addVec );
			v2iStoreA( (int *)dst, dataVec );
			dst += 2;
		}
		break;
	case 0x4:
		src += 1;
		dataVec = v2iLoadA( (int *)(src - 2) );
		mask = v2iInit( 0, -1 );
		while( count > 1 ) {
			nextVec = v2iLoadA( (int *)src );
			src += 2;
			dataVec = v2iOr( v2iAnd( mask, dataVec ),
					 v2iAndNot( mask, nextVec ) );
			dataVec = _mm_unpacklo_pi32( _mm_unpackhi_pi32( dataVec, dataVec), dataVec );
			dataVec = v2iAdd( dataVec, addVec );
			v2iStoreA( (int *)dst, dataVec );
			dst += 2;
			
			dataVec = nextVec;
		}
		src -= 1;
		break;
	}
	/* copy any remaining data */
	while( count-- > 0 ) {
		*dst++ = *src++ + add;
	}
}


/* sincos function */
/* algorithm adapted from Marcus H. Mendenhall */
/* "A Fast, Vectorizable Algorithm for Producing Single-Precision
    Sine-Cosine Pairs.
*/
static void
v4fSinCos1(v4f x, v4f *s, v4f *c) {
	/* modulo 1 to range -0.5 -> 0.5 */
	v4f q1 = v4fSub(x, v4fRound(x));
	v4f q2 = v4fMul(q1, q1);
	
	v4f s1, s2, c1, c2, fixmag;

	s1 = v4fMul(q2, s4fInit(-0.0046075748));
	s1 = v4fAdd(s1, s4fInit( 0.0796819754));
	s1 = v4fMul(s1, q2);
	s1 = v4fAdd(s1, s4fInit(-0.645963615));
	s1 = v4fMul(s1, q2);
	s1 = v4fAdd(s1, s4fInit( 1.5707963235));
	s1 = v4fMul(s1, q1);
	
	c1 = v4fMul(q2, s4fInit(-0.0204391631));
	c1 = v4fAdd(c1, s4fInit( 0.2536086171));
	c1 = v4fMul(c1, q2);
	c1 = v4fAdd(c1, s4fInit(-1.2336977925));
	c1 = v4fMul(c1, q2);
	c1 = v4fAdd(c1, s4fInit( 1.0000000000));

	c2 = v4fSub(v4fMul(c1, c1), v4fMul(s1, s1));
	s2 = v4fMul(s1, c1);
	s2 = v4fAdd(s2, s2);

	fixmag = s4fInit(2.0);
	fixmag = v4fSub(fixmag, v4fMul(c2, c2));
	fixmag = v4fSub(fixmag, v4fMul(s2, s2));
	
	c1 = v4fSub(v4fMul(c2, c2), v4fMul(s2, s2));
	s1 = v4fMul(v4fMul(s4fInit(2.0), s2), c2);
	
	*s = v4fMul(s1, fixmag);
	*c = v4fMul(c1, fixmag);
}

void v4fSinCos(v4f x, v4f *s, v4f *c) {
	/* scale 0->2pi to 0->1 */
	v4f x1 = v4fMul(x, s4fInit(1.0 / (2.0 * M_PI)));
	v4fSinCos1(x1, s, c);
}

void v4fSinCosDeg(v4f x, v4f *s, v4f *c) {
	/* scale 0->360 to 0->1 */
	v4f x1 = v4fMul(x, s4fInit(1.0 / 360.0));
	v4fSinCos1(x1, s, c);
}

void v4fAngleVectors(v4f angles, v4f *forward, v4f *right, v4f *up) {
	v4f sin, cos, sp, cp, sy, cy, sr, cr;
	
	/* compute sine and cosine of all angles */
	v4fSinCosDeg(angles, &sin, &cos);
	
	sp = _mm_unpacklo_ps(sin, sin);
	cp = _mm_unpacklo_ps(cos, cos);

	sr = _mm_movehl_ps(sp, sp);
	cr = _mm_movehl_ps(cp, cp);

	sp = _mm_movelh_ps(sp, cp);
	cp = _mm_movelh_ps(cp, sp);

	cy = _mm_unpacklo_ps(cr, sr);
	sy = _mm_unpacklo_ps(v4fNeg(sr), cr);
	cy = _mm_movelh_ps(cy, _mm_set_ss(-1.0));
	sy = _mm_movelh_ps(sy, v4fZero);

	sr = _mm_unpackhi_ps(sin, sin);
	cr = _mm_unpackhi_ps(cos, cos);
	sr = v4fNeg(_mm_unpacklo_ps(sr, sr));
	cr = _mm_unpacklo_ps(cr, cr);
}

#define LINE_DISTANCE_EPSILON 1e-05f

float v4fDistanceBetweenLineSegmentsSquared(
    v4f sP0, v4f sP1,
    v4f tP0, v4f tP1,
    float *s, float *t )
{
	v4f     sMag, tMag, diff;
	float   a, b, c, d, e;
	float   D;
	float   sN, sD;
	float   tN, tD;
	v4f     separation;
	
	sMag = v4fSub( sP1, sP0 );
	tMag = v4fSub( tP1, tP0 );
	diff = v4fSub( sP0, tP0 );
	
	a = s4fToFloat( v4fDotProduct( sMag, sMag ) );
	b = s4fToFloat( v4fDotProduct( sMag, tMag ) );
	c = s4fToFloat( v4fDotProduct( tMag, tMag ) );
	d = s4fToFloat( v4fDotProduct( sMag, diff ) );
	e = s4fToFloat( v4fDotProduct( tMag, diff ) );

	sD = tD = D = a * c - b * b;
	
	if( D < LINE_DISTANCE_EPSILON )
	{
		// the lines are almost parallel
		sN = 0.0;   // force using point P0 on segment S1
		sD = 1.0;   // to prevent possible division by 0.0 later
		tN = e;
		tD = c;
	}
	else
	{
		// get the closest points on the infinite lines
		sN = ( b * e - c * d );
		tN = ( a * e - b * d );
		
		if( sN < 0.0 )
		{
			// sN < 0 => the s=0 edge is visible
			sN = 0.0;
			tN = e;
			tD = c;
		}
		else if( sN > sD )
		{
			// sN > sD => the s=1 edge is visible
			sN = sD;
			tN = e + b;
			tD = c;
		}
	}
	
	if( tN < 0.0 )
	{
		// tN < 0 => the t=0 edge is visible
		tN = 0.0;
		
		// recompute sN for this edge
		if( -d < 0.0 )
			sN = 0.0;
		else if( -d > a )
			sN = sD;
		else
		{
			sN = -d;
			sD = a;
		}
	}
	else if( tN > tD )
	{
		// tN > tD => the t=1 edge is visible
		tN = tD;
		
		// recompute sN for this edge
		if( ( -d + b ) < 0.0 )
			sN = 0;
		else if( ( -d + b ) > a )
			sN = sD;
		else
		{
			sN = ( -d + b );
			sD = a;
		}
	}
	
	// finally do the division to get *s and *t
	*s = ( fabs( sN ) < LINE_DISTANCE_EPSILON ? 0.0 : sN / sD );
	*t = ( fabs( tN ) < LINE_DISTANCE_EPSILON ? 0.0 : tN / tD );
	
	// get the difference of the two closest points
	sMag = v4fScale( s4fInit(*s), sMag );
	tMag = v4fScale( s4fInit(*t), tMag );
	separation = v4fSub( v4fAdd( diff, sMag ), tMag );
	
	return s4fToFloat( v4fVectorLengthSquared( separation ) );
}

#endif
