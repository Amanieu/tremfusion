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
#if id386_sse >= 1
// qsse.h -- sse1/2 routines for vector math
#include <xmmintrin.h>

#define POINTLESS_PEDANTISM

/* SSE 1 types */
typedef __m128  v4f;   /* vector of 4 floats */
typedef __m128  s4f;   /* scalar of 4 floats (all components are equal) */

extern v4f v4fZero;
extern v4f v4fZeroDotOne;
extern v4f v4fMZeroDotOne;
extern v4f v4fZeroDotTwoFive;
extern v4f v4fZeroDotFive;
extern v4f v4fMZeroDotFive;
extern v4f v4fOne;
extern v4f v4fMOne;
extern v4f v4fXMask;
extern v4f v4fYMask;
extern v4f v4fZMask;
extern v4f v4fWMask;
extern v4f v4fXYMask;
extern v4f v4fXYZMask;
extern v4f v4fSignMask;
extern v4f v4fSignMaskW;
extern v4f v4fTwo;
extern v4f v4fThree;
extern v4f v4fFour;
extern v4f v4fTwoTwentyThree;

static ID_INLINE v4f
v4fInit(float x0, float x1, float x2, float x3) {
	return _mm_set_ps(x3, x2, x1, x0);
}

/* Load/Store v4f */
static ID_INLINE s4f
s4fInit(float x) {
	return _mm_set1_ps(x);
}

static ID_INLINE float
s4fToFloat(s4f vec) {
	return _mm_cvtss_f32( vec );
}

static ID_INLINE v4f
v4fLoadU(const float *adr) {
// workaround for a gcc bug
#ifdef __GNUC__
	__m128 result;
	asm("movups %1, %0" : "=x" (result) : "m" (*adr));
	return result;
#else
	return _mm_loadu_ps(adr);
#endif
}

static ID_INLINE v4f
v4fLoadA(const float *adr) {
	return *(__m128 *)(adr);
}

static ID_INLINE void
v4fStoreU(float *adr, v4f vec) {
	_mm_storeu_ps(adr, vec);
}

static ID_INLINE void
v4fStoreA(float *adr, v4f vec) {
	_mm_store_ps(adr, vec);
}

static ID_INLINE void
v4fStoreLoA(float *adr, v4f vec) {
	_mm_storel_pi((__m64 *)adr, vec);
}

static ID_INLINE void
v4fStoreHiA(float *adr, v4f vec) {
	_mm_storeh_pi((__m64 *)adr, vec);
}


/* MMX types */
typedef __m64   v2i;   /* vector of 2 ints */
typedef __m64   s2i;   /* scalar of 2 ints */

/* Load/Store v2i */
static ID_INLINE v2i
v2iInit(int x0, int x1) {
	return _mm_set_pi32(x1, x0);
}

static ID_INLINE s2i
s2iInit(int x) {
	return _mm_set1_pi32(x);
}

static ID_INLINE v2i
v2iLoadU(int *adr) {
	return v2iInit( *adr, *(adr+1) );
}

static ID_INLINE v2i
v2iLoadA(int *adr) {
	return *(__m64 *)(adr);
}

static ID_INLINE void
v2iStoreU(int *adr, v2i vec) {
	*adr = _mm_cvtsi64_si32( vec );
	*(adr + 1) = _mm_cvtsi64_si32( _mm_unpackhi_pi32( vec, vec ) );
}

static ID_INLINE void
v2iStoreA(int *adr, v2i vec) {
	*(__m64 *)adr = vec;
}


typedef __m64   v4s;   /* vector of 4 shorts */
typedef __m64   s4s;   /* scalar of 4 shorts */

/* Load/Store v4s */
static ID_INLINE v4s
v4sInit(short x0, short x1, short x2, short x3) {
	return _mm_set_pi16(x3, x2, x1, x0);
}

static ID_INLINE s4s
s4sInit(short x) {
	return _mm_set1_pi16(x);
}

static ID_INLINE v4s
v4sLoadU(short *adr) {
	return v4sInit( *adr, *(adr+1), *(adr+2), *(adr+3) );
}

static ID_INLINE v4s
v4sLoadA(short *adr) {
	return *(__m64 *)(adr);
}

static ID_INLINE void
v4sStoreU(short *adr, v4s vec) {
	v2iStoreU((int *)adr, (v2i)vec);
}

static ID_INLINE void
v4sStoreA(short *adr, v4s vec) {
	*(__m64 *)adr = vec;
}

#if id386_sse >= 2
#include <emmintrin.h>

/* SSE 2 types */
typedef __m128i v4i;   /* vector of 4 ints */
typedef __m128i s4i;   /* scalar of 4 ints */

/* Load/Store v4i */
static ID_INLINE v4i
v4iInit(int x0, int x1, int x2, int x3) {
	return _mm_set_epi32(x3, x2, x1, x0);
}

static ID_INLINE s4i
s4iInit(int x) {
	return _mm_set1_epi32(x);
}

static ID_INLINE v4i
v4iLoadU(int *adr) {
	return _mm_loadu_si128((__m128i *)adr);
}

static ID_INLINE v4i
v4iLoadA(int *adr) {
	return *(__m128i *)(adr);
}

static ID_INLINE void
v4iStoreU(int *adr, v4i vec) {
	_mm_storeu_si128((__m128i *)adr, vec);
}

static ID_INLINE void
v4iStoreA(int *adr, v4i vec) {
	_mm_store_si128((__m128i *)adr, vec);
}

static ID_INLINE void
v4iStoreLoA(int *adr, v4i vec) {
	_mm_storel_epi64((__m128i *)adr, vec);
}


typedef __m128i v8s;   /* vector of 8 shorts */
typedef __m128i s8s;   /* scalar of 8 shorts */

/* Load/Store v8s */
static ID_INLINE v8s
v8sInit(short x0, short x1, short x2, short x3,
	short x4, short x5, short x6, short x7) {
	return _mm_set_epi16(x7, x6, x5, x4, x3, x2, x1, x0);
}

static ID_INLINE s8s
s8sInit(short x) {
	return _mm_set1_epi16(x);
}

static ID_INLINE v8s
v8sLoadU(short *adr) {
	return _mm_loadu_si128((__m128i *)adr);
}

static ID_INLINE v8s
v8sLoadA(short *adr) {
	return *(__m128i *)(adr);
}

static ID_INLINE void
v8sStoreU(short *adr, v8s vec) {
	_mm_storeu_si128((__m128i *)adr, vec);
}

static ID_INLINE void
v8sStoreA(short *adr, v8s vec) {
	_mm_store_si128((__m128i *)adr, vec);
}

#endif



/* conversion */

#if id386_sse >= 2
static ID_INLINE v8s
v4i_to_v8s(v4i lo, v4i hi) {
	return _mm_packs_epi32(lo, hi);
}

#define v8s_to_v4i(in, outlo, outhi)                                     \
	do {                                                             \
		v8s signs = v8sShiftRight(in, 15);                       \
		outlo = _mm_unpacklo_epi16(in, signs);                   \
		outhi = _mm_unpackhi_epi16(in, signs);                   \
	} while(0)

static ID_INLINE v4f
v4i_to_v4f(v4i in) {
	return _mm_cvtepi32_ps(in);
}

static ID_INLINE v4i
v4f_to_v4i(v4f in) {
	return _mm_cvtps_epi32(in);
}
#endif

static ID_INLINE v4s
v2i_to_v4s(v2i lo, v2i hi) {
	return _mm_packs_pi16(lo, hi);
}

#define v4s_to_v2i(in, outlo, outhi)                                     \
	do {                                                             \
		v4s signs = v4sShiftRight(in, 15);                       \
		outlo = _mm_unpacklo_pi16(in, signs);                    \
		outhi = _mm_unpackhi_pi16(in, signs);                    \
	} while(0)



/* basic arithmetic functions */
static ID_INLINE v4f
v4fAdd(v4f a, v4f b) {
	return _mm_add_ps(a, b);
}

static ID_INLINE v4f
v4fSub(v4f a, v4f b) {
	return _mm_sub_ps(a, b);
}

static ID_INLINE v4f
v4fXor(v4f a, v4f b) {
	return _mm_xor_ps(a, b);
}

static ID_INLINE v4f
v4fNeg(v4f a) {
	return v4fXor(v4fSignMask, a);
}

static ID_INLINE v4f
v4fMul(v4f a, v4f b) {
	return _mm_mul_ps(a, b);
}

static ID_INLINE v4f
v4fScale(s4f scalar, v4f vec) {
	return _mm_mul_ps(scalar, vec);
}

static ID_INLINE v4f
v4fInverse(v4f vec) {
	v4f r = _mm_rcp_ps(vec);
#ifdef POINTLESS_PEDANTISM
	// Newton-Rhapson step for higher precision
	r = v4fSub(v4fAdd(r, r),
		   v4fMul(v4fMul(r, vec), r));
        //r = v4fSub(v4fAdd(r, r),
        //           v4fMul(v4fMul(r, vec), r));
#endif
	return r;
}

static ID_INLINE v4f
v4fInverseRoot(v4f vec) {
	v4f r = _mm_rsqrt_ps(vec);
#ifdef POINTLESS_PEDANTISM
	// Newton-Rhapson step for higher precision
	r = v4fMul(v4fMul(v4fZeroDotFive, r),
		   v4fSub(v4fThree,
			  v4fMul(v4fMul(vec, r), r)));
        //r = v4fMul(v4fMul(v4fZeroDotFive, r),
        //           v4fSub(v4fThree,
        //                  v4fMul(v4fMul(vec, r), r)));
#endif
	return r;
}

static ID_INLINE v4f
v4fMin(v4f a, v4f b) {
	return _mm_min_ps(a, b);
}

static ID_INLINE v4f
v4fMax(v4f a, v4f b) {
	return _mm_max_ps(a, b);
}

static ID_INLINE v2i
v2iZero(void) {
	return _mm_setzero_si64();
}

static ID_INLINE v2i
v2iAdd(v2i a, v2i b) {
	return _mm_add_pi32(a, b);
}

static ID_INLINE v2i
v2iSub(v2i a, v2i b) {
	return _mm_sub_pi32(a, b);
}

static ID_INLINE v2i
v2iNeg(v2i a) {
	return v2iSub(v2iZero(), a);
}

static ID_INLINE v4s
v4sZero(void) {
	return _mm_setzero_si64();
}

static ID_INLINE v4s
v4sAdd(v4s a, v4s b) {
	return _mm_add_pi16(a, b);
}

static ID_INLINE v4s
v4sSub(v4s a, v4s b) {
	return _mm_sub_pi16(a, b);
}

static ID_INLINE v4s
v4sNeg(v4s a) {
	return v4sSub(v4sZero(), a);
}

#define v4sMul(a, b, l, h)                               \
	do {                                             \
		v4s hiBits = _mm_mulhi_pi16(a, b);      \
		v4s loBits = _mm_mullo_pi16(a, b);      \
		l = _mm_unpacklo_pi16(loBits, hiBits);  \
		h = _mm_unpackhi_pi16(loBits, hiBits);  \
	} while(0)


#if id386_sse >= 2
static ID_INLINE v4i
v4iZero(void) {
	return _mm_setzero_si128();
}

static ID_INLINE v4i
v4iAdd(v4i a, v4i b) {
	return _mm_add_epi32(a, b);
}

static ID_INLINE v4i
v4iSub(v4i a, v4i b) {
	return _mm_sub_epi32(a, b);
}

static ID_INLINE v4i
v4iNeg(v4i a) {
	return v4iSub(v4iZero(), a);
}


static ID_INLINE v8s
v8sZero(void) {
	return _mm_setzero_si128();
}

static ID_INLINE v8s
v8sAdd(v8s a, v8s b) {
	return _mm_add_epi16(a, b);
}

static ID_INLINE v8s
v8sSub(v8s a, v8s b) {
	return _mm_sub_epi16(a, b);
}

static ID_INLINE v8s
v8sNeg(v8s a) {
	return v8sSub(v8sZero(), a);
}

#define v8sMul(a, b, l, h)                               \
	do {                                             \
		v8s hiBits = _mm_mulhi_epi16(a, b);      \
		v8s loBits = _mm_mullo_epi16(a, b);      \
		l = _mm_unpacklo_epi16(loBits, hiBits);  \
		h = _mm_unpackhi_epi16(loBits, hiBits);  \
	} while(0)
#endif



/* comparisons */
static ID_INLINE v4f
v4fLt(v4f a, v4f b) {
	return _mm_cmplt_ps(a, b);
}
static ID_INLINE v4f
v4fGt(v4f a, v4f b) {
	return _mm_cmpgt_ps(a, b);
}
static ID_INLINE v4f
v4fEq(v4f a, v4f b) {
	return _mm_cmpeq_ps(a, b);
}
static ID_INLINE v4f
v4fLe(v4f a, v4f b) {
	return _mm_cmple_ps(a, b);
}
static ID_INLINE v4f
v4fGe(v4f a, v4f b) {
	return _mm_cmpge_ps(a, b);
}
static ID_INLINE v4f
v4fNe(v4f a, v4f b) {
	return _mm_cmpneq_ps(a, b);
}

static ID_INLINE int
v4fSignBits(v4f a) {
	return _mm_movemask_ps(a);
}

static ID_INLINE int
v4fNone(v4f a) {
	return _mm_movemask_ps(a) == 0;
}
static ID_INLINE int
v4fAny(v4f a) {
	return _mm_movemask_ps(a) != 0;
}
static ID_INLINE int
v4fAll(v4f a) {
	return _mm_movemask_ps(a) == 0xf;
}
static ID_INLINE int
v4fNotAll(v4f a) {
	return _mm_movemask_ps(a) != 0xf;
}


/* logic functions */
static ID_INLINE v4f
v4fOr(v4f a, v4f b) {
	return _mm_or_ps(a, b);
}

static ID_INLINE v4f
v4fAnd(v4f a, v4f b) {
	return _mm_and_ps(a, b);
}

static ID_INLINE v4f
v4fAndNot(v4f a, v4f b) {
	return _mm_andnot_ps(a, b);
}

extern v4f mixMask0000, mixMask0001, mixMask0010, mixMask0011,
           mixMask0100, mixMask0101, mixMask0110, mixMask0111,
           mixMask1000, mixMask1001, mixMask1010, mixMask1011,
           mixMask1100, mixMask1101, mixMask1110, mixMask1111;

static ID_INLINE v4f
v4fMix(v4f v0, v4f v1, v4f mask) {
	return _mm_or_ps( _mm_and_ps( mask, v1 ), _mm_andnot_ps( mask, v0 ));
}

static ID_INLINE v2i
v2iOr(v2i a, v2i b) {
	return _mm_or_si64(a, b);
}

static ID_INLINE v2i
v2iAnd(v2i a, v2i b) {
	return _mm_and_si64(a, b);
}

static ID_INLINE v2i
v2iAndNot(v2i a, v2i b) {
	return _mm_andnot_si64(a, b);
}

static ID_INLINE v2i
v2iXor(v2i a, v2i b) {
	return _mm_xor_si64(a, b);
}

static ID_INLINE v2i
v2iShiftRight(v2i vec, int bits) {
	return _mm_srai_pi32(vec, bits);
}

static ID_INLINE v2i
v2iShiftLeft(v2i vec, int bits) {
	return _mm_slli_pi32(vec, bits);
}

static ID_INLINE v4s
v4sOr(v4s a, v4s b) {
	return _mm_or_si64(a, b);
}

static ID_INLINE v4s
v4sAnd(v4s a, v4s b) {
	return _mm_and_si64(a, b);
}

static ID_INLINE v4s
v4sXor(v4s a, v4s b) {
	return _mm_xor_si64(a, b);
}

static ID_INLINE v4s
v4sShiftRight(v4s vec, int bits) {
	return _mm_srai_pi16(vec, bits);
}

static ID_INLINE v4s
v4sShiftLeft(v4s vec, int bits) {
	return _mm_slli_pi16(vec, bits);
}

#if id386_sse >= 2
static ID_INLINE v4i
v4iOr(v4i a, v4i b) {
	return _mm_or_si128(a, b);
}

static ID_INLINE v4i
v4iAnd(v4i a, v4i b) {
	return _mm_and_si128(a, b);
}

static ID_INLINE v4i
v4iAndNot(v4i a, v4i b) {
	return _mm_andnot_si128(a, b);
}

static ID_INLINE v4i
v4iXor(v4i a, v4i b) {
	return _mm_xor_si128(a, b);
}

static ID_INLINE v4i
v4iShiftRight(v4i vec, int bits) {
	return _mm_srai_epi32(vec, bits);
}

static ID_INLINE v4i
v4iShiftLeft(v4i vec, int bits) {
	return _mm_slli_epi32(vec, bits);
}

static ID_INLINE v8s
v8sOr(v8s a, v8s b) {
	return _mm_or_si128(a, b);
}

static ID_INLINE v8s
v8sAnd(v8s a, v8s b) {
	return _mm_and_si128(a, b);
}

static ID_INLINE v8s
v8sXor(v8s a, v8s b) {
	return _mm_xor_si128(a, b);
}

static ID_INLINE v8s
v8sShiftRight(v8s vec, int bits) {
	return _mm_srai_epi16(vec, bits);
}

static ID_INLINE v8s
v8sShiftLeft(v8s vec, int bits) {
	return _mm_slli_epi16(vec, bits);
}
#endif



/* geometric functions */
#define v4fUnpack(vec, x, y, z, w)                    \
	do {                                          \
		v4f xz = _mm_unpacklo_ps(vec, vec);   \
		v4f yw = _mm_unpackhi_ps(vec, vec);   \
		x = _mm_unpacklo_ps(xz, xz);          \
		y = _mm_unpacklo_ps(yw, yw);          \
		z = _mm_unpackhi_ps(xz, xz);          \
		w = _mm_unpackhi_ps(yw, yw);          \
	} while(0)

static ID_INLINE s4f
v4fX(v4f vec) {
	return _mm_shuffle_ps( vec, vec, 0x00 );
}
static ID_INLINE s4f
v4fY(v4f vec) {
	return _mm_shuffle_ps( vec, vec, 0x55 );
}
static ID_INLINE s4f
v4fZ(v4f vec) {
	return _mm_shuffle_ps( vec, vec, 0xaa );
}
static ID_INLINE s4f
v4fW(v4f vec) {
	return _mm_shuffle_ps( vec, vec, 0xff );
}

static ID_INLINE v4f
s4fToX(s4f x) {
	return v4fAnd( x, v4fXMask );
}

static ID_INLINE v4f
s4fToY(s4f x) {
	return v4fAnd( x, v4fYMask );
}

static ID_INLINE v4f
s4fToZ(s4f x) {
	return v4fAnd( x, v4fZMask );
}

static ID_INLINE v4f
s4fToW(s4f x) {
	return v4fAnd( x, v4fWMask );
}

static ID_INLINE v4f
v4fXY(v4f x) {
	return v4fAnd( x, v4fXYMask );
}

static ID_INLINE v4f
v4fXYZ(v4f x) {
	return v4fAnd( x, v4fXYZMask );
}

static ID_INLINE v4f
v4fAbs(v4f a) {
	return v4fAndNot( v4fSignMask, a );
}

static ID_INLINE s4f
v4fDotProduct(v4f a, v4f b) {
	v4f xyz = _mm_mul_ps(a, b);
	// move all components into Y
	v4f xxy = _mm_unpacklo_ps(xyz, xyz);
	v4f zzw = _mm_unpackhi_ps(xyz, xyz);

	return v4fY( v4fAdd( v4fAdd( xyz, xxy ), zzw ) );
}

static ID_INLINE s4f
v4fVectorLengthSquared(v4f a) {
	return v4fDotProduct(a, a);
}

static ID_INLINE s4f
v4fVectorLength(v4f a) {
	s4f squared = v4fVectorLengthSquared(a);
	return v4fMul( v4fInverseRoot( squared ), squared );
}

static ID_INLINE s4f
v4fPlaneDist(v4f vec, v4f plane) {
	return v4fSub( v4fDotProduct( vec, plane ), v4fW( plane ) );
}

static ID_INLINE v4f
v4fVectorNormalize(v4f a) {
	s4f length = v4fVectorLengthSquared(a);
	if( v4fAll( v4fEq( a, v4fZero ) ) )
		return v4fZero;
	length = v4fInverseRoot( length );
	return v4fScale( length, a );
}

static ID_INLINE v4f
v4fMA(v4f v, s4f s, v4f b) {
	return v4fAdd(v, v4fScale(s, b));
}

static ID_INLINE v4f
v4fMid(v4f a, v4f b) {
	return v4fMul( v4fZeroDotFive, v4fAdd( a, b ) );
}

static ID_INLINE v4f
v4fLerp(s4f f, v4f s, v4f e) {
	return v4fMA(s, f, v4fSub(e, s));
}

#define v4fTranspose4(m0, m1, m2, m3)                 \
	do {                                          \
		v4f m02l = _mm_unpacklo_ps(m0, m2);   \
		v4f m02h = _mm_unpackhi_ps(m0, m2);   \
		v4f m13l = _mm_unpacklo_ps(m1, m3);   \
		v4f m13h = _mm_unpackhi_ps(m1, m3);   \
		                                      \
		m0 = _mm_unpacklo_ps(m02l, m13l);     \
		m1 = _mm_unpackhi_ps(m02l, m13l);     \
		m2 = _mm_unpacklo_ps(m02h, m13h);     \
		m3 = _mm_unpackhi_ps(m02h, m13h);     \
	} while(0)

#define v4fTranspose(m0, m1, m2)                      \
	do {                                          \
		v4f m3 = _mm_setzero_ps();            \
		                                      \
		v4f m02l = _mm_unpacklo_ps(m0, m2);   \
		v4f m02h = _mm_unpackhi_ps(m0, m2);   \
		v4f m13l = _mm_unpacklo_ps(m1, m3);   \
		v4f m13h = _mm_unpackhi_ps(m1, m3);   \
		                                      \
		m0 = _mm_unpacklo_ps(m02l, m13l);     \
		m1 = _mm_unpackhi_ps(m02l, m13l);     \
		m2 = _mm_unpacklo_ps(m02h, m13h);     \
	} while(0)

static ID_INLINE v4f
v4fRotatePoint(v4f point, v4f m0, v4f m1, v4f m2) {
	v4f r0 = _mm_mul_ps(point, m0);
	v4f r1 = _mm_mul_ps(point, m1);
	v4f r2 = _mm_mul_ps(point, m2);
	
	v4fTranspose(r0, r1, r2);
	
	return v4fAdd(v4fAdd(r0, r1), r2);
}

/* misc */
void InitSSEMode(void);

static ID_INLINE v4f
v4fRound(v4f vec) {
	return v4fAdd(v4fSub(v4fSub(v4fAdd(vec,
					   v4fTwoTwentyThree),
				    v4fTwoTwentyThree),
			     v4fTwoTwentyThree),
		      v4fTwoTwentyThree);
}

#if id386_sse >= 2
void CopyArrayAndAddConstant_sse2(unsigned *dst, unsigned *src, int add, int count);
#endif
void CopyArrayAndAddConstant_sse1(unsigned *dst, unsigned *src, int add, int count);

static ID_INLINE v4f
vec3_to_v4f(const vec3_t vec) {
#if 0
	/* this doesn't load the 4th float, but is probably slower */
	__m128 v0 = _mm_load_ss(&(vec[0]));
	__m128 v1 = _mm_load_ss(&(vec[1]));
	__m128 v2 = _mm_load_ss(&(vec[2]));
	
	__m128 v02 = _mm_unpacklo_ps(v0, v2);
	__m128 v012 = _mm_unpacklo_ps(v02, v1);
	
	return v012;
#else
	/* could theoretically segfault, because it loads an additional float */
	return v4fAnd( v4fLoadU( vec ), v4fXYZMask );
#endif
}

static ID_INLINE void
v4f_to_vec3(vec3_t trg, v4f src) {
#if 0
	trg[0] = s4fToFloat( v4fX( src ) );
	trg[1] = s4fToFloat( v4fY( src ) );
	trg[2] = s4fToFloat( v4fZ( src ) );
#else
	float tmp[4] ALIGNED(16);
	
	v4fStoreA( tmp, src );
	*(int *)&(trg[0]) = *(int *)&(tmp[0]);
	*(int *)&(trg[1]) = *(int *)&(tmp[1]);
	*(int *)&(trg[2]) = *(int *)&(tmp[2]);
#endif
}

static ID_INLINE v4f
v4fCrossProduct(v4f a, v4f b) {
	v4f a_yzx = _mm_shuffle_ps( a, a, 0xc9 );
	v4f b_yzx = _mm_shuffle_ps( b, b, 0xc9 );
	v4f c_zxy = v4fSub( v4fMul( a, b_yzx ), v4fMul( a_yzx, b ) );
	return v4fXYZ(_mm_shuffle_ps( c_zxy, c_zxy, 0xc9 ));
}

void v4fSinCos(v4f x, v4f *s, v4f *c);
void v4fSinCosDeg(v4f x, v4f *s, v4f *c);

void v4fAngleVectors(v4f angles, v4f *forward, v4f *right, v4f *up);

#define v4fCreateRotationMatrix(angles, m0, m1, m2)              \
	do {                                                     \
		v4fAngleVectors((angles), &(m0), &(m1), &(m2));  \
		m1 = v4fNeg(m1);                                 \
	} while(0)

float v4fDistanceBetweenLineSegmentsSquared(
    v4f sP0, v4f sP1,
    v4f tP0, v4f tP1,
    float *s, float *t );


static ID_INLINE int
v4fVectorCompareEpsilon(v4f v1, v4f v2, s4f epsilon )
{
	v4f d = v4fAbs( v4fSub( v1, v2 ) );

	if( v4fAny( v4fGt( d, epsilon ) ) )
		return 0;

	return 1;
}

static ID_INLINE int
v4fBoxOnPlaneSide(v4f mins, v4f maxs, v4f plane) {
	v4f mask = v4fLt( plane, v4fZero );
	v4f corner0 = v4fOr( v4fAnd( mask, mins ), v4fAndNot( mask, maxs ) );
	v4f corner1 = v4fOr( v4fAnd( mask, maxs ), v4fAndNot( mask, mins ) );
	
	v4f dist1 = v4fNeg( v4fPlaneDist( corner0, plane ) );
	v4f dist2 =         v4fPlaneDist( corner1, plane );
	
	return v4fSignBits( v4fMix( dist1, dist2, mixMask0111 ) ) & 0x03;
}

static ID_INLINE v4f
v4fQuatMul(v4f a, v4f b) {
	/*
	  from matrix and quaternion faq
	  x = w1x2 + x1w2 + y1z2 - z1y2
	  y = w1y2 + y1w2 + z1x2 - x1z2
	  z = w1z2 + z1w2 + x1y2 - y1x2
	  
	  w = w1w2 - x1x2 - y1y2 - z1z2
	*/
	
	v4f a1 = v4fW(a);
	v4f c1 = v4fMul(a1, b);
	
	v4f a2 = _mm_shuffle_ps(a, a, 0x24);
	v4f b2 = _mm_shuffle_ps(b, b, 0x3f);
	v4f c2 = v4fXor(v4fMul(a2, b2), v4fSignMaskW);

	v4f a3 = _mm_shuffle_ps(a, a, 0x49);
	v4f b3 = _mm_shuffle_ps(b, b, 0x53);
	v4f c3 = v4fXor(v4fMul(a3, b3), v4fSignMaskW);

	v4f a4 = _mm_shuffle_ps(a, a, 0x92);
	v4f b4 = _mm_shuffle_ps(b, b, 0x45);
	v4f c4 = v4fMul(a4, b4);

	return v4fAdd(v4fAdd(c1, c2), v4fSub(c3, c4));
}
#endif
