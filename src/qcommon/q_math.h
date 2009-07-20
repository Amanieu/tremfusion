/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2009 Amanieu d'Antras

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

// q_math.h -- stateless support routines that are included in each code module

#ifndef __Q_MATH_H
#define __Q_MATH_H

typedef float vec_t;
typedef vec_t vec2_t[2];

// Align all vectors to 16 bytes to allow SSE optimisations
typedef vec_t vec4_t[4] __aligned(16);
typedef vec4_t vec3a_t __aligned(16);
typedef vec_t vec3_t[3];

#if id386_sse >= 1
#define vec3aLoad(vec3a)       v4fLoadA(vec3a)
#define vec3Load(vec3)         vec3_to_v4f(vec3)
#define vec3aStore(vec3a, v4f) v4fStoreA(vec3a, v4f)
#define vec3Store(vec3, v4f)   v4f_to_vec3(vec3, v4f)
#endif

typedef vec3_t  axis_t[3] __aligned(16);
typedef vec_t   matrix3x3_t[9] __aligned(16);
typedef vec_t   matrix_t[16] __aligned(16);
typedef vec_t   quat_t[4] __aligned(16);		// | x y z w |

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.414213562f
#endif

#ifndef M_ROOT3
#define M_ROOT3 1.732050808f
#endif

// angle indexes
#define	PITCH				0	// up / down
#define	YAW					1	// left / right
#define	ROLL				2	// fall over

// plane sides
typedef enum
{
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2,
	SIDE_CROSS = 3
} planeSide_t;

#define NUMVERTEXNORMALS	162
extern	vec3_t	bytedirs[NUMVERTEXNORMALS];

// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define	SCREEN_WIDTH		640
#define	SCREEN_HEIGHT		480

#define TINYCHAR_WIDTH		(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT		(SMALLCHAR_HEIGHT/2)

#define SMALLCHAR_WIDTH		8
#define SMALLCHAR_HEIGHT	16

#define BIGCHAR_WIDTH		16
#define BIGCHAR_HEIGHT		16

#define	GIANTCHAR_WIDTH		32
#define	GIANTCHAR_HEIGHT	48

extern	vec4_t		colorBlack;
extern	vec4_t		colorRed;
extern	vec4_t		colorGreen;
extern	vec4_t		colorBlue;
extern	vec4_t		colorYellow;
extern	vec4_t		colorMagenta;
extern	vec4_t		colorCyan;
extern	vec4_t		colorWhite;
extern	vec4_t		colorLtGrey;
extern	vec4_t		colorMdGrey;
extern	vec4_t		colorDkGrey;

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && isalnum(*((p)+1)) )

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'

#define MAX_CCODES	62
int ColorIndex(char ccode);

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"

extern const vec4_t g_color_table[MAX_CCODES];

#define	MAKERGB( v, r, g, b ) v[0]=r;v[1]=g;v[2]=b
#define	MAKERGBA( v, r, g, b, a ) v[0]=r;v[1]=g;v[2]=b;v[3]=a

#define DEG2RAD( a ) ( ( (a) * M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )

#define Q_max(a, b)      ((a) > (b) ? (a) : (b))
#define Q_min(a, b)      ((a) < (b) ? (a) : (b))
#define Q_bound(a, b, c) (Q_max(a, Q_min(b, c)))
#define Q_clamp(a, b, c) ((b) >= (c) ? (a)=(b) : (a) < (b) ? (a)=(b) : (a) > (c) ? (a)=(c) : (a))
#define Q_lerp(from, to, frac) (from + ((to - from) * frac))

struct cplane_s;

extern	vec3_t	vec3_origin;
extern	vec3_t	axisDefault[3];
extern	matrix_t matrixIdentity;
extern	quat_t   quatIdentity;

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

static inline long Q_ftol(float f)
{
#if id386_sse && defined(_MSC_VER)
	static int      tmp;
	__asm fld f
	__asm fistp tmp
	__asm mov eax, tmp
#else
	return (long)f;
#endif
}

static inline float Q_rsqrt(float number)
{
	float           y;

#if idppc
	float           x = 0.5f * number;

#ifdef __GNUC__
	asm("frsqrte %0, %1": "=f" (y) : "f" (number));
#else
	y = __frsqrte(number);
#endif
	return y * (1.5f - (x * y * y));

#elif id386_sse && defined __GNUC__
	asm volatile("rsqrtss %0, %1" : "=x" (y) : "x" (number));
#elif id386_sse && defined _MSC_VER
	__asm
	{
		rsqrtss xmm0, number
		movss y, xmm0
	}
#else
	floatint_t      t;
	float           x2;
	const float     threehalfs = 1.5F;

	x2 = number * 0.5F;
	t.f = number;
	t.i = 0x5f3759df - (t.i >> 1); // what the fuck?
	y = t.f;
	y = y * (threehalfs - (x2 * y * y)); // 1st iteration
#endif
	return y;
}

static inline float Q_fabs(float x)
{
#if idppc && defined __GNUC__
	float           abs_x;

 	asm("fabs %0, %1" : "=f" (abs_x) : "f" (x));
	return abs_x;
#else
	floatint_t      tmp;

	tmp.f = x;
	tmp.i &= 0x7FFFFFFF;
	return tmp.f;
#endif
}

static inline vec_t Q_recip(vec_t in)
{
	return ((float)(1.0f / (in)));
}

#define SQRTFAST( x ) ( (x) * Q_rsqrt( x ) )

byte ClampByte(int i);
signed char ClampChar( int i );
signed short ClampShort( int i );

// this isn't a real cheap function to call!
int DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );

#define Vector2Set(v, x, y) ((v)[0]=(x), (v)[1]=(y))
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define Vector2Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1])
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define Vector4Add(a,b,c)    ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2],(c)[3]=(a)[3]+(b)[3])
#define Vector4Lerp( f, s, e, r ) ((r)[0]=(s)[0]+(f)*((e)[0]-(s)[0]),\
  (r)[1]=(s)[1]+(f)*((e)[1]-(s)[1]),\
  (r)[2]=(s)[2]+(f)*((e)[2]-(s)[2]),\
  (r)[3]=(s)[3]+(f)*((e)[3]-(s)[3]))


#define DotProduct4(x,y)		((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]+(x)[3]*(y)[3])
#define VectorSubtract4(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#define VectorAdd4(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2],(c)[3]=(a)[3]+(b)[3])
#define VectorCopy4(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define	VectorScale4(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s),(o)[3]=(v)[3]*(s))
#define	VectorMA4(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s),(o)[3]=(v)[3]+(b)[3]*(s))
#define VectorClear4(a)			((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#define VectorNegate4(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2],(b)[3]=-(a)[3])
#define VectorSet4(v,x,y,z,w)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z),(v)[3]=(w))

#if 1
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#else
static inline void VectorClear(vec3_t v)
{
#if defined(SSEVEC3_T)
	__m128          _tmp = _mm_setzero_ps();

	_mm_storeu_ps(v, _tmp);
#else
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
#endif
}
#endif

#if 1
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#else
static inline void VectorCopy(const vec3_t in, vec3_t out)
{
#if defined(SSEVEC3_T)
	__m128          _tmp;

	_tmp = _mm_loadu_ps(in);
	_mm_storeu_ps(out, _tmp);
#else
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
#endif
}
#endif

#if 1
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#else
static inline void VectorAdd(const vec3_t a, const vec3_t b, vec3_t out)
{
#if defined(SSEVEC3_T)
	__m128          _a, _b, _out;

	_a = _mm_loadu_ps(a);
	_b = _mm_loadu_ps(b);

	_out = _mm_add_ps(_a, _b);

	_mm_storeu_ps(out, _out);

#else
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
#endif
}
#endif

#if 1
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#else
static inline void VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out)
{
#if defined(SSEVEC3_T)
	__m128          _a, _b, _out;

	_a = _mm_loadu_ps(a);
	_b = _mm_loadu_ps(b);

	_out = _mm_sub_ps(_a, _b);

	_mm_storeu_ps(out, _out);
#else
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
#endif
}
#endif

#if 1
#define	VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))
#else
static inline void VectorMA(const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
{
#if defined(SSEVEC3_T)
	__m128          _a, _b, _s, _c;

	_a = _mm_loadu_ps(veca);
	_b = _mm_loadu_ps(vecb);
	_s = _mm_set1_ps(scale);

	_c = _mm_mul_ps(_s, _b);
	_c = _mm_add_ps(_a, _c);

	_mm_storeu_ps(vecc, _c);
#else
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
#endif
}
#endif

#if 1
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#else
static inline void VectorScale(const vec3_t in, vec_t scale, vec3_t out)
{
#if defined(SSEVEC3_T)
	__m128          _in, _scale, _out;

	_in = _mm_loadu_ps(in);
	_scale = _mm_set1_ps(scale);

	_out = _mm_mul_ps(_in, _scale);

	_mm_storeu_ps(out, _out);
#else
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
#endif
}
#endif

#if 1
#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#else
static inline vec_t DotProduct(const vec3_t a, const vec3_t b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
#endif

static inline void SnapVector(vec3_t v)
{
#if id386 && defined(_MSC_VER)
	int             i;
	float           f;

	f = *v;
	__asm fld       f;
	__asm fistp     i;

	*v = i;
	v++;
	f = *v;
	__asm fld       f;
	__asm fistp     i;

	*v = i;
	v++;
	f = *v;
	__asm fld       f;
	__asm fistp     i;

	*v = i;
#else
	v[0] = (int)v[0];
	v[1] = (int)v[1];
	v[2] = (int)v[2];
#endif
}

void SnapVectorTowards(vec3_t v, vec3_t to);

unsigned ColorBytes3 (float r, float g, float b);
unsigned ColorBytes4 (float r, float g, float b, float a);

float NormalizeColor( const vec3_t in, vec3_t out );
void ClampColor(vec4_t color);

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ClearBounds( vec3_t mins, vec3_t maxs );
void ZeroBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
void BoundsAdd(vec3_t mins, vec3_t maxs, const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsIntersect(const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsIntersectSphere(const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec_t radius);
qboolean BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs, const vec3_t origin);

static inline void BoundsToCorners(const vec3_t mins, const vec3_t maxs, vec3_t corners[8])
{
	VectorSet(corners[0], mins[0], maxs[1], maxs[2]);
	VectorSet(corners[1], maxs[0], maxs[1], maxs[2]);
	VectorSet(corners[2], maxs[0], mins[1], maxs[2]);
	VectorSet(corners[3], mins[0], mins[1], maxs[2]);
	VectorSet(corners[4], mins[0], maxs[1], mins[2]);
	VectorSet(corners[5], maxs[0], maxs[1], mins[2]);
	VectorSet(corners[6], maxs[0], mins[1], mins[2]);
	VectorSet(corners[7], mins[0], mins[1], mins[2]);
}

static inline int VectorCompare( const vec3_t v1, const vec3_t v2 ) {
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
		return 0;
	}			
	return 1;
}

static inline int VectorCompare4(const vec4_t v1, const vec4_t v2)
{
	if(v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2] || v1[3] != v2[3])
	{
		return 0;
	}
	return 1;
}

static inline int VectorCompareEpsilon(
		const vec3_t v1, const vec3_t v2, float epsilon )
{
	vec3_t d;

	VectorSubtract( v1, v2, d );
	d[ 0 ] = fabs( d[ 0 ] );
	d[ 1 ] = fabs( d[ 1 ] );
	d[ 2 ] = fabs( d[ 2 ] );

	if( d[ 0 ] > epsilon || d[ 1 ] > epsilon || d[ 2 ] > epsilon )
		return 0;

	return 1;
}

static inline vec_t VectorLength( const vec3_t v ) {
	return (vec_t)sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

static inline vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static inline void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

static inline void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static inline void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

static inline void VectorLerp(const vec3_t from, const vec3_t to, float frac, vec3_t out)
{
	out[0] = from[0] + ((to[0] - from[0]) * frac);
	out[1] = from[1] + ((to[1] - from[1]) * frac);
	out[2] = from[2] + ((to[2] - from[2]) * frac);
}

static inline void VectorReflect(const vec3_t v, const vec3_t normal, vec3_t out)
{
	float           d;

	d = 2.0 * (v[0] * normal[0] + v[1] * normal[1] + v[2] * normal[2]);

	out[0] = v[0] - normal[0] * d;
	out[1] = v[1] - normal[1] * d;
	out[2] = v[2] - normal[2] * d;
}

vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out );
int NearestPowerOfTwo( int val );
int Q_log2(int val);

float Q_acos(float c);

int Q_isnan(float x);
int		Q_rand( int *seed );
float	Q_random( int *seed );
float	Q_crandom( int *seed );

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

void AnglesToAxis( const vec3_t angles, vec3_t axis[3] );
void AxisToAngles( vec3_t axis[3], vec3_t angles );

void AxisClear( vec3_t axis[3] );
void AxisCopy( vec3_t in[3], vec3_t out[3] );

void SetPlaneSignbits( struct cplane_s *out );
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);

int BoxOnPlaneSide2(vec3_t mins, vec3_t maxs, vec4_t plane);

float	AngleMod(float a);
float	LerpAngle (float from, float to, float frac);
float	AngleSubtract( float a1, float a2 );
void	AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 );

float AngleNormalize360 ( float angle );
float AngleNormalize180 ( float angle );
float AngleDelta ( float angle1, float angle2 );
float AngleBetweenVectors(const vec3_t a, const vec3_t b);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

static inline void AnglesToVector(const vec3_t angles, vec3_t out)
{
	AngleVectors(angles, out, NULL, NULL);
}

void VectorToAngles(const vec3_t value1, vec3_t angles);

vec_t PlaneNormalize(vec4_t plane);	// returns normal length
qboolean PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c, qboolean cw);
/* greebo: This calculates the intersection point of three planes.
 * Returns <0,0,0> if no intersection point could be found, otherwise returns the coordinates of the intersection point
 * (this may also be 0,0,0) */
qboolean PlanesGetIntersectionPoint(const vec4_t plane1, const vec4_t plane2, const vec4_t plane3, vec3_t out);

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void RotateAroundDirection( vec3_t axis[3], vec_t angle );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );
// perpendicular vector could be replaced by this

//int	PlaneTypeForNormal (vec3_t normal);

void AxisMultiply(axis_t in1, axis_t in2, axis_t out);
void VectorAxisMultiply(const vec3_t p, vec3_t m[3], vec3_t out);
void PerpendicularVector( vec3_t dst, const vec3_t src );

void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1,
		const vec3_t p2, vec3_t up );
void ProjectPointOntoVector( vec3_t point, vec3_t vStart,
		vec3_t vEnd, vec3_t vProj );
float VectorDistance( vec3_t v1, vec3_t v2 );

float pointToLineDistance( const vec3_t point, const vec3_t p1, const vec3_t p2 );
float VectorMinComponent( vec3_t v );
float VectorMaxComponent( vec3_t v );

vec_t DistanceBetweenLineSegmentsSquared(
    const vec3_t sP0, const vec3_t sP1,
    const vec3_t tP0, const vec3_t tP1,
    float *s, float *t );
vec_t DistanceBetweenLineSegments(
    const vec3_t sP0, const vec3_t sP1,
    const vec3_t tP0, const vec3_t tP1,
    float *s, float *t );

#ifdef _MSC_VER
float rint( float v );
#endif

//=============================================

void            MatrixIdentity(matrix_t m);
void            MatrixClear(matrix_t m);
void            MatrixCopy(const matrix_t in, matrix_t out);
qboolean        MatrixCompare(const matrix_t a, const matrix_t b);
void            MatrixTransposeIntoXMM(const matrix_t m);
void            MatrixTranspose(const matrix_t in, matrix_t out);

// invert any m4x4 using Kramer's rule.. return qtrue if matrix is singular, else return qfalse
qboolean        MatrixInverse(matrix_t m);
void            MatrixSetupXRotation(matrix_t m, vec_t degrees);
void            MatrixSetupYRotation(matrix_t m, vec_t degrees);
void            MatrixSetupZRotation(matrix_t m, vec_t degrees);
void            MatrixSetupTranslation(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixSetupScale(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixSetupShear(matrix_t m, vec_t x, vec_t y);
void            MatrixMultiply(const matrix_t a, const matrix_t b, matrix_t out);
void            MatrixMultiply2(matrix_t m, const matrix_t m2);
void            MatrixMultiplyRotation(matrix_t m, vec_t pitch, vec_t yaw, vec_t roll);
void            MatrixMultiplyZRotation(matrix_t m, vec_t degrees);
void            MatrixMultiplyTranslation(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixMultiplyScale(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixMultiplyShear(matrix_t m, vec_t x, vec_t y);
void            MatrixToAngles(const matrix_t m, vec3_t angles);
void            MatrixFromAngles(matrix_t m, vec_t pitch, vec_t yaw, vec_t roll);
void            MatrixFromVectorsFLU(matrix_t m, const vec3_t forward, const vec3_t left, const vec3_t up);
void            MatrixFromVectorsFRU(matrix_t m, const vec3_t forward, const vec3_t right, const vec3_t up);
void            MatrixFromQuat(matrix_t m, const quat_t q);
void            MatrixFromPlanes(matrix_t m, const vec4_t left, const vec4_t right, const vec4_t bottom, const vec4_t top,
								 const vec4_t front, const vec4_t back);
void            MatrixToVectorsFLU(const matrix_t m, vec3_t forward, vec3_t left, vec3_t up);
void            MatrixToVectorsFRU(const matrix_t m, vec3_t forward, vec3_t right, vec3_t up);
void            MatrixSetupTransform(matrix_t m, const vec3_t forward, const vec3_t left, const vec3_t up, const vec3_t origin);
void            MatrixSetupTransformFromRotation(matrix_t m, const matrix_t rot, const vec3_t origin);
void            MatrixSetupTransformFromQuat(matrix_t m, const quat_t quat, const vec3_t origin);
void            MatrixAffineInverse(const matrix_t in, matrix_t out);
void            MatrixTransformNormal(const matrix_t m, const vec3_t in, vec3_t out);
void            MatrixTransformNormal2(const matrix_t m, vec3_t inout);
void            MatrixTransformPoint(const matrix_t m, const vec3_t in, vec3_t out);
void            MatrixTransformPoint2(const matrix_t m, vec3_t inout);
void            MatrixTransform4(const matrix_t m, const vec4_t in, vec4_t out);
void            MatrixSetupPerspectiveProjection(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near,
												 vec_t far);
void            MatrixSetupOrthogonalProjection(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near,
												vec_t far);

static inline void AnglesToMatrix(const vec3_t angles, matrix_t m)
{
	MatrixFromAngles(m, angles[PITCH], angles[YAW], angles[ROLL]);
}


#define QuatSet(q,x,y,z,w)	((q)[0]=(x),(q)[1]=(y),(q)[2]=(z),(q)[3]=(w))
#define QuatCopy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define QuatCompare(a,b)	((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3])

static inline void QuatClear(quat_t q)
{
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}

/*
static inline int QuatCompare(const quat_t a, const quat_t b)
{
	if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
	{
		return 0;
	}
	return 1;
}
*/

static inline void QuatCalcW(quat_t q)
{
#if 1
	vec_t           term = 1.0f - (q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);

	if(term < 0.0)
		q[3] = 0.0;
	else
		q[3] = -sqrt(term);
#else
	q[3] = sqrt(fabs(1.0f - (q[0] * q[0] + q[1] * q[1] + q[2] * q[2])));
#endif
}

static inline void QuatInverse(quat_t q)
{
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
}

static inline void QuatAntipodal(quat_t q)
{
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
	q[3] = -q[3];
}

static inline vec_t QuatLength(const quat_t q)
{
	return (vec_t) sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

vec_t           QuatNormalize(quat_t q);

void            QuatFromAngles(quat_t q, vec_t pitch, vec_t yaw, vec_t roll);

static inline void AnglesToQuat(const vec3_t angles, quat_t q)
{
	QuatFromAngles(q, angles[PITCH], angles[YAW], angles[ROLL]);
}

void            QuatFromMatrix(quat_t q, const matrix_t m);
void            QuatToVectorsFLU(const quat_t quat, vec3_t forward, vec3_t left, vec3_t up);
void            QuatToVectorsFRU(const quat_t quat, vec3_t forward, vec3_t right, vec3_t up);
void            QuatToAxis(const quat_t q, vec3_t axis[3]);
void            QuatToAngles(const quat_t q, vec3_t angles);

// Quaternion multiplication, analogous to the matrix multiplication routines.

// qa = rotate by qa, then qb
void            QuatMultiply0(quat_t qa, const quat_t qb);

// qc = rotate by qa, then qb
void            QuatMultiply1(const quat_t qa, const quat_t qb, quat_t qc);

// qc = rotate by qa, then by inverse of qb
void            QuatMultiply2(const quat_t qa, const quat_t qb, quat_t qc);

// qc = rotate by inverse of qa, then by qb
void            QuatMultiply3(const quat_t qa, const quat_t qb, quat_t qc);

// qc = rotate by inverse of qa, then by inverse of qb
void            QuatMultiply4(const quat_t qa, const quat_t qb, quat_t qc);

void            QuatSlerp(const quat_t from, const quat_t to, float frac, quat_t out);
void            QuatTransformVector(const quat_t q, const vec3_t in, vec3_t out);

#endif	// __Q_MATH_H
