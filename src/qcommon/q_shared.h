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
//
#ifndef __Q_SHARED_H
#define __Q_SHARED_H

// q_shared.h -- included first by ALL program modules.
// A user mod should never modify this file

#define PRODUCT_NAME            "tremfusion"

#ifdef _MSC_VER
# define PRODUCT_VERSION          "0.99r2"
#endif

#define CLIENT_WINDOW_TITLE       "Tremfusion " PRODUCT_VERSION
#define CLIENT_WINDOW_MIN_TITLE   "Tremfusion"
#define Q3_VERSION                 PRODUCT_NAME " " PRODUCT_VERSION

#define MAX_TEAMNAME 32

#define BASEGAME "base"

#define GAMENAME_FOR_MASTER "tremfusion"

#ifdef _MSC_VER
#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4032)
#pragma warning(disable : 4051)
#pragma warning(disable : 4055)		// 'type cast' : from data pointer 'void *' to function pointer
#pragma warning(disable : 4057)		// slightly different base types
#pragma warning(disable : 4100)		// unreferenced formal parameter
#pragma warning(disable : 4115)
#pragma warning(disable : 4125)		// decimal digit terminates octal escape sequence
#pragma warning(disable : 4127)		// conditional expression is constant
#pragma warning(disable : 4136)
#pragma warning(disable : 4152)		// nonstandard extension, function/data pointer conversion in expression
//#pragma warning(disable : 4201)
//#pragma warning(disable : 4214)
#pragma warning(disable : 4244)
#pragma warning(disable : 4201)		// nonstandard extension used
#pragma warning(disable : 4142)		// benign redefinition
//#pragma warning(disable : 4305)		// truncation from const double to float
//#pragma warning(disable : 4310)		// cast truncates constant value
//#pragma warning(disable:  4505) 	// unreferenced local function has been removed
#pragma warning(disable : 4514)
#pragma warning(disable : 4702)		// unreachable code
#pragma warning(disable : 4706)		// assignment within conditional expression
#pragma warning(disable : 4711)		// selected for automatic inline expansion
#pragma warning(disable : 4220)		// varargs matches remaining parameters
#pragma warning(disable : 4996)		// deprecated functions
//#pragma intrinsic( memset, memcpy )
#endif

//Ignore __attribute__ on non-gcc platforms
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>				// Tr3B - for DBL_MAX and FLT_MAX

// vsnprintf is ISO/IEC 9899:1999
// abstracting this to make it portable
#ifdef _WIN32
  #define Q_vsnprintf _vsnprintf
  #define Q_snprintf _snprintf
#else
  #define Q_vsnprintf vsnprintf
  #define Q_snprintf snprintf
#endif

#ifdef _MSC_VER
  #include <io.h>

  typedef __int64 int64_t;
  typedef __int32 int32_t;
  typedef __int16 int16_t;
  typedef __int8 int8_t;
  typedef unsigned __int64 uint64_t;
  typedef unsigned __int32 uint32_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int8 uint8_t;
#else
  #include <stdint.h>
#endif


#include "q_platform.h"

//=============================================================

typedef unsigned char 		byte;

typedef enum {qfalse, qtrue}	qboolean;

typedef union {
	float f;
	int i;
	unsigned int ui;
} floatint_t;

typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;

#define PAD(x,y) (((x)+(y)-1) & ~((y)-1))

#ifdef __GNUC__
#define ALIGNED(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
#define ALIGNED(x) __declspec(align(x));
#else
#define ALIGNED(x)
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef BIT
#define BIT(x)				(1 << x)
#endif

#define	MAX_QINT			0x7fffffff
#define	MIN_QINT			(-MAX_QINT-1)

#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#ifndef max
#define max(x,y) MAX(x,y)
#define min(x,y) MIN(x,y)
#endif

#ifndef sign
#define sign( f )	( ( f > 0 ) ? 1 : ( ( f < 0 ) ? -1 : 0 ) )
#endif

// the game guarantees that no string from the network will ever
// exceed MAX_STRING_CHARS
#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	1024	// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		1024	// max length of an individual token

#define	MAX_INFO_STRING		1024
#define	MAX_INFO_KEY		  1024
#define	MAX_INFO_VALUE		1024

#define	BIG_INFO_STRING		8192  // used for system info key only
#define	BIG_INFO_KEY		  8192
#define	BIG_INFO_VALUE		8192


#define	MAX_QPATH			256		// max length of a quake game pathname
#ifdef PATH_MAX
#define MAX_OSPATH			PATH_MAX
#else
#define	MAX_OSPATH			256		// max length of a filesystem pathname
#endif

#define	MAX_NAME_LENGTH			32		// max length of a client name
#define	MAX_HOSTNAME_LENGTH	80		// max length of a host name

#define	MAX_SAY_TEXT	150

// paramters for command buffer stuffing
typedef enum {
	EXEC_NOW,			// don't return until completed, a VM should NEVER use this,
						// because some commands might cause the VM to be unloaded...
	EXEC_INSERT,		// insert at current position, but don't run yet
	EXEC_APPEND			// add to end of the command buffer (normal case)
} cbufExec_t;


//
// these aren't needed by any of the VMs.  put in another header?
//
#define	MAX_MAP_AREA_BYTES		32		// bit vector of area visibility


// print levels from renderer (FIXME: set up for game / cgame?)
typedef enum {
	PRINT_ALL,
	PRINT_DEVELOPER,		// only print when "developer 1"
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;


#ifdef ERR_FATAL
#undef ERR_FATAL			// this is be defined in malloc.h
#endif

// parameters to the main Error routine
typedef enum {
	ERR_FATAL,					// exit the entire game with a popup window
	ERR_DROP,					// print to console and disconnect from game
	ERR_SERVERDISCONNECT,		// don't kill server
	ERR_DISCONNECT,				// client disconnected from the server
} errorParm_t;


// font rendering values used by ui and cgame

#define PROP_GAP_WIDTH			3
#define PROP_SPACE_WIDTH		8
#define PROP_HEIGHT				27
#define PROP_SMALL_SIZE_SCALE	0.75

#define BLINK_DIVISOR			200
#define PULSE_DIVISOR			75

#define UI_LEFT			0x00000000	// default
#define UI_CENTER		0x00000001
#define UI_RIGHT		0x00000002
#define UI_FORMATMASK	0x00000007
#define UI_SMALLFONT	0x00000010
#define UI_BIGFONT		0x00000020	// default
#define UI_GIANTFONT	0x00000040
#define UI_DROPSHADOW	0x00000800
#define UI_BLINK		0x00001000
#define UI_INVERSE		0x00002000
#define UI_PULSE		0x00004000

#if defined(_DEBUG) && !defined(BSPC)
	#define HUNK_DEBUG
#endif

typedef enum {
	h_high,
	h_low,
	h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
#define Hunk_Alloc( size, preference )				Hunk_AllocDebug(size, preference, #size, __FILE__, __LINE__)
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line );
#else
void *Hunk_Alloc( int size, ha_pref preference );
#endif

#define Com_Memset memset
#define Com_Memcpy memcpy

#define Com_Allocate malloc
#define Com_Dealloc free

#define CIN_system	1
#define CIN_loop	2
#define	CIN_hold	4
#define CIN_silent	8
#define CIN_shader	16

/*
==============================================================

MATHLIB

==============================================================
*/


typedef float vec_t;
typedef vec_t vec2_t[2];

// Align all vectors to 16 bytes to allow SSE optimisations
typedef vec_t vec4_t[4] ALIGNED(16);
typedef vec4_t vec3a_t ALIGNED(16);
typedef vec_t vec3_t[3];

#if id386_sse >= 1
#define vec3aLoad(vec3a)       v4fLoadA(vec3a)
#define vec3Load(vec3)         vec3_to_v4f(vec3)
#define vec3aStore(vec3a, v4f) v4fStoreA(vec3a, v4f)
#define vec3Store(vec3, v4f)   v4f_to_vec3(vec3, v4f)
#endif

typedef vec3_t  axis_t[3] ALIGNED(16);
typedef vec_t   matrix3x3_t[9] ALIGNED(16);
typedef vec_t   matrix_t[16] ALIGNED(16);
typedef vec_t   quat_t[4] ALIGNED(16);		// | x y z w |

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

static ID_INLINE long Q_ftol(float f)
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

static ID_INLINE float Q_rsqrt(float number)
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

static ID_INLINE float Q_fabs(float x)
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

static ID_INLINE vec_t Q_recip(vec_t in)
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
static ID_INLINE void VectorClear(vec3_t v)
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
static ID_INLINE void VectorCopy(const vec3_t in, vec3_t out)
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
static ID_INLINE void VectorAdd(const vec3_t a, const vec3_t b, vec3_t out)
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
static ID_INLINE void VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out)
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
static ID_INLINE void VectorMA(const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc)
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
static ID_INLINE void VectorScale(const vec3_t in, vec_t scale, vec3_t out)
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
static ID_INLINE vec_t DotProduct(const vec3_t a, const vec3_t b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
#endif

static ID_INLINE void SnapVector(vec3_t v)
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

static ID_INLINE void BoundsToCorners(const vec3_t mins, const vec3_t maxs, vec3_t corners[8])
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

static ID_INLINE int VectorCompare( const vec3_t v1, const vec3_t v2 ) {
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
		return 0;
	}			
	return 1;
}

static ID_INLINE int VectorCompare4(const vec4_t v1, const vec4_t v2)
{
	if(v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2] || v1[3] != v2[3])
	{
		return 0;
	}
	return 1;
}

static ID_INLINE int VectorCompareEpsilon(
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

static ID_INLINE vec_t VectorLength( const vec3_t v ) {
	return (vec_t)sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

static ID_INLINE vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static ID_INLINE void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

static ID_INLINE void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static ID_INLINE void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

static ID_INLINE void VectorLerp(const vec3_t from, const vec3_t to, float frac, vec3_t out)
{
	out[0] = from[0] + ((to[0] - from[0]) * frac);
	out[1] = from[1] + ((to[1] - from[1]) * frac);
	out[2] = from[2] + ((to[2] - from[2]) * frac);
}

static ID_INLINE void VectorReflect(const vec3_t v, const vec3_t normal, vec3_t out)
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

static ID_INLINE void AnglesToVector(const vec3_t angles, vec3_t out)
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

static ID_INLINE void AnglesToMatrix(const vec3_t angles, matrix_t m)
{
	MatrixFromAngles(m, angles[PITCH], angles[YAW], angles[ROLL]);
}


#define QuatSet(q,x,y,z,w)	((q)[0]=(x),(q)[1]=(y),(q)[2]=(z),(q)[3]=(w))
#define QuatCopy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define QuatCompare(a,b)	((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3])

static ID_INLINE void QuatClear(quat_t q)
{
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}

/*
static ID_INLINE int QuatCompare(const quat_t a, const quat_t b)
{
	if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
	{
		return 0;
	}
	return 1;
}
*/

static ID_INLINE void QuatCalcW(quat_t q)
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

static ID_INLINE void QuatInverse(quat_t q)
{
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
}

static ID_INLINE void QuatAntipodal(quat_t q)
{
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
	q[3] = -q[3];
}

static ID_INLINE vec_t QuatLength(const quat_t q)
{
	return (vec_t) sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

vec_t           QuatNormalize(quat_t q);

void            QuatFromAngles(quat_t q, vec_t pitch, vec_t yaw, vec_t roll);

static ID_INLINE void AnglesToQuat(const vec3_t angles, quat_t q)
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

//=============================================

typedef struct
{
	qboolean        frameMemory;
	int             currentElements;
	int             maxElements;	// will reallocate and move when exceeded
	void          **elements;
} growList_t;

// you don't need to init the growlist if you don't mind it growing and moving
// the list as it expands
void            Com_InitGrowList(growList_t * list, int maxElements);
void            Com_DestroyGrowList(growList_t * list);
int             Com_AddToGrowList(growList_t * list, void *data);
void           *Com_GrowListElement(const growList_t * list, int index);
int             Com_IndexForGrowListElement(const growList_t * list, const void *element);


//=============================================================================

enum
{
	MEMSTREAM_SEEK_SET,
	MEMSTREAM_SEEK_CUR,
	MEMSTREAM_SEEK_END
};

enum
{
	MEMSTREAM_FLAGS_EOF = BIT(0),
	MEMSTREAM_FLAGS_ERR = BIT(1),
};

// helper struct for reading binary file formats
typedef struct memStream_s
{
	byte           *buffer;
	int				bufSize;
	byte           *curPos;
	int             flags;
}
memStream_t;

memStream_t    *AllocMemStream(byte *buffer, int bufSize);
void			FreeMemStream(memStream_t * s);
int				MemStreamRead(memStream_t *s, void *buffer, int len);
int				MemStreamGetC(memStream_t *s);
int				MemStreamGetLong(memStream_t * s);
int				MemStreamGetShort(memStream_t * s);
float			MemStreamGetFloat(memStream_t * s);

//=============================================

float Com_Clamp( float min, float max, float value );

char	*Com_SkipPath( char *pathname );
const char	*Com_GetExtension( const char *name );
void	Com_StripExtension(const char *in, char *out, int destsize);
void	Com_DefaultExtension( char *path, int maxSize, const char *extension );

void	Com_BeginParseSession( const char *name );
int		Com_GetCurrentParseLine( void );
char	*Com_Parse( char **data_p );
char	*Com_ParseExt( char **data_p, qboolean allowLineBreak );
int		Com_Compress( char *data_p );
void	Com_ParseError( char *format, ... ) __attribute__ ((format (printf, 1, 2)));
void	Com_ParseWarning( char *format, ... ) __attribute__ ((format (printf, 1, 2)));

#define MAX_TOKENLENGTH		1024

#ifndef TT_STRING
//token types
#define TT_STRING					1			// string
#define TT_LITERAL					2			// literal
#define TT_NUMBER					3			// number
#define TT_NAME						4			// name
#define TT_PUNCTUATION				5			// punctuation
#endif

typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
} pc_token_t;

// data is an in/out parm, returns a parsed out token

void	Com_MatchToken( char**buf_p, char *match );

void Com_SkipBracedSection (char **program);
void Com_SkipRestOfLine ( char **data );

void Com_Parse1DMatrix (char **buf_p, int x, float *m);
void Com_Parse2DMatrix (char **buf_p, int y, int x, float *m);
void Com_Parse3DMatrix (char **buf_p, int z, int y, int x, float *m);
int Com_HexStrToInt( const char *str );

void	QDECL Com_sprintf (char *dest, int size, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

char *Com_SkipTokens( char *s, int numTokens, char *sep );
char *Com_SkipCharset( char *s, char *sep );

void Com_RandomBytes( byte *string, int len );
qboolean Com_CheckColorCodes(const char *s);

// mode parm for FS_FOpenFile
typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

//=============================================

int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );
qboolean Q_isanumber( const char *s );
qboolean Q_isintegral( float f );

// portable case insensitive compare
int		Q_stricmp (const char *s1, const char *s2);
int		Q_strncmp (const char *s1, const char *s2, int n);
int		Q_stricmpn (const char *s1, const char *s2, int n);
char	*Q_strlwr( char *s1 );
char	*Q_strupr( char *s1 );
char	*Q_strrchr( const char* string, int c );
const char	*Q_stristr( const char *s, const char *find);

// buffer size safe library replacements
void	Q_strncpyz( char *dest, const char *src, int destsize );
void	Q_strcat( char *dest, int size, const char *src );
qboolean	Q_strreplace(char *dest, int destsize, const char *find, const char *replace);

// strlen that discounts Quake color sequences
int Q_PrintStrlen( const char *string );
// removes color sequences from string
char *Q_CleanStr( char *string );
// Count the number of char tocount encountered in string
int Q_CountChar(const char *string, char tocount);

//=============================================

// 64-bit integers for global rankings interface
// implemented as a struct for qvm compatibility
typedef struct
{
	byte	b0;
	byte	b1;
	byte	b2;
	byte	b3;
	byte	b4;
	byte	b5;
	byte	b6;
	byte	b7;
} qint64;

//=============================================
/*
short	BigShort(short l);
short	LittleShort(short l);
int		BigLong (int l);
int		LittleLong (int l);
qint64  BigLong64 (qint64 l);
qint64  LittleLong64 (qint64 l);
float	BigFloat (const float *l);
float	LittleFloat (const float *l);

void	Swap_Init (void);
*/
char	* QDECL va(char *format, ...) __attribute__ ((format (printf, 1, 2)));

#define TRUNCATE_LENGTH	64
void Com_TruncateLongString( char *buffer, const char *s );

//=============================================

//
// key / value info strings
//
char *Info_ValueForKey( const char *s, const char *key );
void Info_RemoveKey( char *s, const char *key );
void Info_RemoveKey_big( char *s, const char *key );
void Info_SetValueForKey( char *s, const char *key, const char *value );
void Info_SetValueForKey_Big( char *s, const char *key, const char *value );
qboolean Info_Validate( const char *s );
void Info_NextPair( const char **s, char *key, char *value );

// this is only here so the functions in q_shared.c and bg_*.c can link
void	QDECL Com_Error( int level, const char *error, ... ) __attribute__ ((format (printf, 2, 3)));
void	QDECL Com_Printf( const char *msg, ... ) __attribute__ ((format (printf, 1, 2)));


/*
==========================================================

CVARS (console variables)

Many variables can be used for cheating purposes, so when
cheats is zero, force all unspecified variables to their
default values.
==========================================================
*/

#define	CVAR_ARCHIVE		1	// set to cause it to be saved to vars.rc
								// used for system variables, not for player
								// specific configurations
#define	CVAR_USERINFO		2	// sent to server on connect or change
#define	CVAR_SERVERINFO		4	// sent in response to front end requests
#define	CVAR_SYSTEMINFO		8	// these cvars will be duplicated on all clients
#define	CVAR_INIT			16	// don't allow change from console at all,
								// but can be set from the command line
#define	CVAR_LATCH			32	// will only change when C code next does
								// a Cvar_Get(), so it can't be changed
								// without proper initialization.  modified
								// will be set, even though the value hasn't
								// changed yet
#define	CVAR_ROM			64	// display only, cannot be set by user at all
#define	CVAR_USER_CREATED	128	// created by a set command
#define	CVAR_TEMP			256	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT			512	// can not be changed if cheats are disabled
#define CVAR_NORESTART		1024	// do not clear when a cvar_restart is issued

#define CVAR_SERVER_CREATED	2048	// cvar was created by a server the client connected to.
#define CVAR_VM_CREATED		4096	// cvar was created by a qvm
#define CVAR_PROTECTED		8192	// prevent modifying this var from VMs or the server
#define CVAR_NONEXISTENT	0xFFFFFFFF	// Cvar doesn't exist.

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s {
	char			*name;
	char			*string;
	char			*resetString;		// cvar_restart will reset to this value
	char			*latchedString;		// for CVAR_LATCH vars
	int				flags;
	qboolean	modified;			// set each time the cvar is changed
	int				modificationCount;	// incremented each time the cvar is changed
	float			value;				// atof( string )
	int				integer;			// atoi( string )
	qboolean	validate;
	qboolean	integral;
	float			min;
	float			max;
	struct cvar_s *next;
	struct cvar_s *hashNext;
} cvar_t;

#define	MAX_CVAR_VALUE_STRING	256

typedef int	cvarHandle_t;

// the modules that run in the virtual machine can't access the cvar_t directly,
// so they must ask for structured updates
typedef struct {
	cvarHandle_t	handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

#include "surfaceflags.h"			// shared with the xmap utility

// plane types are used to speed some tests
// 0-2 are axial planes
typedef enum
{
	PLANE_X = 0,
	PLANE_Y = 1,
	PLANE_Z = 2,
	PLANE_NON_AXIAL = 3
} planeType_t;

/*
=================
PlaneTypeForNormal
=================
*/
//#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL) ) )
static ID_INLINE int PlaneTypeForNormal(vec3_t normal)
{
	if(normal[0] == 1.0)
		return PLANE_X;

	if(normal[1] == 1.0)
		return PLANE_Y;

	if(normal[2] == 1.0)
		return PLANE_Z;

	return PLANE_NON_AXIAL;
}

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte	signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte	pad[2];
} cplane_t;

typedef enum {
	TT_NONE,

	TT_AABB,
	TT_CAPSULE,
	TT_BISPHERE,

	TT_NUM_TRACE_TYPES
} traceType_t;

// a trace is returned when a box is swept through the world
typedef struct {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact, transformed to world space
	int			surfaceFlags;	// surface hit
	int			contents;	// contents on other side of surface hit
	int			entityNum;	// entity the contacted sirface is a part of
	float		lateralFraction; // fraction of collision tangetially to the trace direction
} trace_t;

// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD


// markfragments are returned by CM_MarkFragments()
typedef struct {
	int		firstPoint;
	int		numPoints;
} markFragment_t;



typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;

//=====================================================================


// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE		0x0001
#define	KEYCATCH_UI					0x0002
#define	KEYCATCH_CGAME			0x0004


// sound channels
// channel 0 never willingly overrides
// other channels will allways override a playing sound on that channel
typedef enum {
	CHAN_AUTO,
	CHAN_LOCAL,		// menu sounds, etc
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_LOCAL_SOUND,	// chat messages, etc
	CHAN_ANNOUNCER		// announcer voices, etc
} soundChannel_t;


/*
========================================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

========================================================================
*/

#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

#define	SNAPFLAG_RATE_DELAYED	1
#define	SNAPFLAG_NOT_ACTIVE		2	// snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT	4	// toggled every map_restart so transitions can be detected

//
// per-level limits
//
#define CLIENTNUM_BITS		6
#define	MAX_CLIENTS			(1<<CLIENTNUM_BITS)		// absolute limit
#define MAX_LOCATIONS		64

#define	GENTITYNUM_BITS		10		// don't need to send any more
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)
#define GENTITYNUM_MASK		(MAX_GENTITIES - 1)
#define MAX_GENTITYNUM_PACK	10

// entitynums are communicated with GENTITY_BITS, so any reserved
// values that are going to be communcated over the net need to
// also be in this range
#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)


#define	MAX_MODELS									256		// these are sent over the net as 8 bits
#define	MAX_SOUNDS									256		// so they cannot be blindly increased
#define	MAX_GAME_SHADERS						64
#define	MAX_GAME_PARTICLE_SYSTEMS		64


#define	MAX_CONFIGSTRINGS	1024

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define	CS_SERVERINFO		0		// an info string with all the serverinfo cvars
#define	CS_SYSTEMINFO		1		// an info string for server system to client system configuration (timescale, etc)

#define	RESERVED_CONFIGSTRINGS	2	// game can't modify below this, only the system can

#define	MAX_GAMESTATE_CHARS	16000
typedef struct {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;

//=========================================================

// bit field limits
#define	MAX_STATS				16
#define	MAX_PERSISTANT			16
#define	MAX_MISC    			16

#define	MAX_PS_EVENTS			2

#define PS_PMOVEFRAMECOUNTBITS	6

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur

// you can't add anything to this without modifying the code in msg.c

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
typedef struct playerState_s {
	int			commandTime;	// cmd->serverTime of last executed command
	int			pm_type;
	int			bobCycle;		// for view bobbing and footstep generation
	int			pm_flags;		// ducked, jump_held, etc
	int			pm_time;

	vec3_t		origin;
	vec3_t		velocity;
	int			weaponTime;
	int			gravity;
	int			speed;
	int			delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters

	int			groundEntityNum;// ENTITYNUM_NONE = in air

	int			legsTimer;		// don't change low priority animations until this runs out
	int			legsAnim;		// mask off ANIM_TOGGLEBIT

	int			torsoTimer;		// don't change low priority animations until this runs out
	int			torsoAnim;		// mask off ANIM_TOGGLEBIT

	int			weaponAnim;		// mask off ANIM_TOGGLEBIT

	int			movementDir;	// a number 0 to 7 that represents the reletive angle
								// of movement to the view angle (axial and diagonals)
								// when at rest, the value will remain unchanged
								// used to twist the legs during strafing

	vec3_t		grapplePoint;	// location of grapple to pull towards if PMF_GRAPPLE_PULL

	int			eFlags;			// copied to entityState_t->eFlags

	int			eventSequence;	// pmove generated events
	int			events[MAX_PS_EVENTS];
	int			eventParms[MAX_PS_EVENTS];

	int			externalEvent;	// events set on player from another source
	int			externalEventParm;
	int			externalEventTime;

	int			clientNum;		// ranges from 0 to MAX_CLIENTS-1
	int			weapon;			// copied to entityState_t->weapon
	int			weaponstate;

	vec3_t		viewangles;		// for fixed views
	int			viewheight;

	// damage feedback
	int			damageEvent;	// when it changes, latch the other parms
	int			damageYaw;
	int			damagePitch;
	int			damageCount;

	int			stats[MAX_STATS];
	int			persistant[MAX_PERSISTANT];	// stats that aren't cleared on death
	int			misc[MAX_MISC];	// misc data
	int			ammo;			// ammo held
	int			clips;			// clips held

	int			generic1;
	int			loopSound;
	int			otherEntityNum;

	// not communicated over the net at all
	int			ping;			// server to game info for scoreboard
	int			pmove_framecount;	// FIXME: don't transmit over the network
	int			jumppad_frame;
	int			entityEventSequence;
} playerState_t;


//====================================================================


//
// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
//
#define	BUTTON_ATTACK		1
#define	BUTTON_TALK			2			// displays talk balloon and disables actions
#define	BUTTON_USE_HOLDABLE	4
#define	BUTTON_GESTURE		8
#define	BUTTON_WALKING		16			// walking can't just be infered from MOVE_RUN
										// because a key pressed late in the frame will
										// only generate a small move value for that frame
										// walking will use different animations and
										// won't generate footsteps
#define BUTTON_ATTACK2	32
#define BUTTON_DODGE        64          // start a dodge or sprint motion
#define BUTTON_USE_EVOLVE   128         // use target or open evolve menu

#define	BUTTON_ANY			2048			// any key whatsoever

#define	MOVE_RUN			120			// if forwardmove or rightmove are >= MOVE_RUN,
										// then BUTTON_WALKING should be set

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	int				serverTime;
	int				angles[3];
	int 			buttons;
	byte			weapon;           // weapon 
	signed char	forwardmove, rightmove, upmove;
} usercmd_t;

//===================================================================

// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define	SOLID_BMODEL	0xffffff

typedef enum {
	TR_STATIONARY,
	TR_INTERPOLATE,				// non-parametric, but interpolate between snapshots
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_SINE,					// value = base + sin( time / duration ) * delta
	TR_GRAVITY,
	TR_BUOYANCY
} trType_t;

typedef struct {
	trType_t	trType;
	int		trTime;
	int		trDuration;			// if non 0, trTime + trDuration = stop time
	vec3_t	trBase;
	vec3_t	trDelta;			// velocity, etc
} trajectory_t;

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large

typedef struct entityState_s {
	int		number;			// entity index
	int		eType;			// entityType_t
	int		eFlags;

	trajectory_t	pos;	// for calculating position
	trajectory_t	apos;	// for calculating angles

	int		time;
	int		time2;

	vec3_t	origin;
	vec3_t	origin2;

	vec3_t	angles;
	vec3_t	angles2;

	int		otherEntityNum;	// shotgun sources, etc
	int		otherEntityNum2;

	int		groundEntityNum;	// -1 = in air

	int		constantLight;	// r + (g<<8) + (b<<16) + (intensity<<24)
	int		loopSound;		// constantly loop this sound

	int		modelindex;
	int		modelindex2;
	int		clientNum;		// 0 to (MAX_CLIENTS - 1), for players and corpses
	int		frame;

	int		solid;			// for client side prediction, trap_linkentity sets this properly

	int		event;			// impulse events -- muzzle flashes, footsteps, etc
	int		eventParm;

	// for players
	int		misc;			// bit flags
	int		weapon;			// determines weapon and flash model, etc
	int		legsAnim;		// mask off ANIM_TOGGLEBIT
	int		torsoAnim;		// mask off ANIM_TOGGLEBIT
	int		weaponAnim;		// mask off ANIM_TOGGLEBIT

	int		generic1;
} entityState_t;

typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED, 	// not talking to a server
	CA_AUTHORIZING,		// not used any more, was checking cd key 
	CA_CONNECTING,		// sending request packets to the server
	CA_CHALLENGING,		// sending challenge packets to the server
	CA_CONNECTED,		// netchan_t established, getting gamestate
	CA_LOADING,			// only during cgame initialization, never during main loop
	CA_PRIMED,			// got gamestate, waiting for first frame
	CA_ACTIVE,			// game views should be displayed
	CA_CINEMATIC		// playing a cinematic or a static pic, not connected to a server
} connstate_t;

// font support 

#define GLYPH_START 0
#define GLYPH_END 255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND 127
#define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1
typedef struct {
  int height;       // number of scan lines
  int top;          // top of glyph in buffer
  int bottom;       // bottom of glyph in buffer
  int pitch;        // width for copying
  int xSkip;        // x adjustment
  int imageWidth;   // width of actual image
  int imageHeight;  // height of actual image
  float s;          // x offset in image where glyph starts
  float t;          // y offset in image where glyph starts
  float s2;
  float t2;
  qhandle_t glyph;  // handle to the shader with the glyph
  char shaderName[32];
} glyphInfo_t;

typedef struct {
  glyphInfo_t glyphs [GLYPHS_PER_FONT];
  float glyphScale;
  char name[MAX_QPATH];
} fontInfo_t;

#define Square(x) ((x)*(x))

// real time
//=============================================


typedef struct qtime_s {
	int tm_sec;     /* seconds after the minute - [0,59] */
	int tm_min;     /* minutes after the hour - [0,59] */
	int tm_hour;    /* hours since midnight - [0,23] */
	int tm_mday;    /* day of the month - [1,31] */
	int tm_mon;     /* months since January - [0,11] */
	int tm_year;    /* years since 1900 */
	int tm_wday;    /* days since Sunday - [0,6] */
	int tm_yday;    /* days since January 1 - [0,365] */
	int tm_isdst;   /* daylight savings time flag */
} qtime_t;


// server browser sources
// AS_MPLAYER is no longer used
#define AS_GLOBAL			0
#define AS_MPLAYER		1
#define AS_LOCAL			2
#define AS_FAVORITES	3


// cinematic states
typedef enum {
	FMV_IDLE,
	FMV_PLAY,		// play
	FMV_EOF,		// all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

typedef enum _flag_status {
	FLAG_ATBASE = 0,
	FLAG_TAKEN,			// CTF
	FLAG_TAKEN_RED,		// One Flag CTF
	FLAG_TAKEN_BLUE,	// One Flag CTF
	FLAG_DROPPED
} flagStatus_t;

typedef enum {
	DS_NONE,

	DS_PLAYBACK,
	DS_RECORDING,

	DS_NUM_DEMO_STATES
} demoState_t;


#define	MAX_GLOBAL_SERVERS				4096
#define	MAX_OTHER_SERVERS					128
#define MAX_PINGREQUESTS					32
#define MAX_SERVERSTATUSREQUESTS	16

#define SAY_ALL		0
#define SAY_TEAM	1
#define SAY_TELL	2

#define MAX_EMOTICON_NAME_LEN 16
#define MAX_EMOTICONS 64

// flags for cl_downloadPrompt
#define DLP_TYPE_MASK 0x0f
#define DLP_IGNORE    0x01 // don't download anything
#define DLP_CURL      0x02 // download via HTTP redirect
#define DLP_UDP       0x04 // download from server
#define DLP_SHOW      0x10 // prompt needs to be shown
#define DLP_PROMPTED  0x20 // prompt has been processed by client
#define DLP_STALE     0x40 // prompt is not being shown by UI VM

#endif	// __Q_SHARED_H
