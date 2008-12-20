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

// this is only used for visualization tools in cm_ debug functions


#include "cm_local.h"


// counters are only bumped when running single threaded,
// because they are an awful coherence problem
int	c_active_windings;
int	c_peak_windings;
int	c_winding_allocs;
int	c_winding_points;

/*
=============
AllocWinding
=============
*/
winding_t	*AllocWinding (int points)
{
	winding_t	*w;
	int			s;

	c_winding_allocs++;
	c_winding_points += points;
	c_active_windings++;
	if (c_active_windings > c_peak_windings)
		c_peak_windings = c_active_windings;

	/* one extra float for SSE load */
	s = sizeof(vec_t)*3*points + sizeof(int) + sizeof(vec_t);
	w = Z_Malloc (s);
	Com_Memset (w, 0, s); 
	return w;
}

void FreeWinding (winding_t *w)
{
	if (*(unsigned *)w == 0xdeaddead)
		Com_Error (ERR_FATAL, "FreeWinding: freed a freed winding");
	*(unsigned *)w = 0xdeaddead;

	c_active_windings--;
	Z_Free (w);
}

/*
============
RemoveColinearPoints
============
*/
int	c_removed;

#if id386_sse >= 1
void	RemoveColinearPoints_sse (winding_t *w)
{
	int	i, j, k;
	v4f	v1, v2, dotNineNineNine = s4fInit( 0.999 );
	int	nump;
	vec3_t	p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for (i = 0 ; i < w->numpoints ; i++)
	{
		j = (i + 1) % w->numpoints;
		k = (i + w->numpoints - 1) % w->numpoints;
		v1 = v4fSub( v4fLoadU( w->p[j] ), v4fLoadU( w->p[i] ) );
		v2 = v4fSub( v4fLoadU( w->p[i] ), v4fLoadU( w->p[k] ) );
		v1 = v4fVectorNormalize( v1 );
		v2 = v4fVectorNormalize( v2 );
		if (v4fAny( v4fLt( v4fDotProduct(v1, v2), dotNineNineNine ) ) )
		{
			/* this writes 4 floats, the last one will be overwritten again */
			v4fStoreU( p[nump], v4fLoadU( w->p[i] ) );
			nump++;
		}
	}

	if (nump == w->numpoints)
		return;

	c_removed += w->numpoints - nump;
	w->numpoints = nump;
	Com_Memcpy (w->p, p, nump*sizeof(p[0]));
}
#endif
void	RemoveColinearPoints (winding_t *w)
{
	int		i, j, k;
	vec3_t	v1, v2;
	int		nump;
	vec3_t	p[MAX_POINTS_ON_WINDING];

	nump = 0;
	for (i=0 ; i<w->numpoints ; i++)
	{
		j = (i+1)%w->numpoints;
		k = (i+w->numpoints-1)%w->numpoints;
		VectorSubtract (w->p[j], w->p[i], v1);
		VectorSubtract (w->p[i], w->p[k], v2);
		VectorNormalize2(v1,v1);
		VectorNormalize2(v2,v2);
		if (DotProduct(v1, v2) < 0.999)
		{
			VectorCopy (w->p[i], p[nump]);
			nump++;
		}
	}

	if (nump == w->numpoints)
		return;

	c_removed += w->numpoints - nump;
	w->numpoints = nump;
	Com_Memcpy (w->p, p, nump*sizeof(p[0]));
}

/*
============
WindingPlane
============
*/
#if id386_sse >= 1
void WindingPlane_sse (winding_t *w, vec3a_t normal, vec_t *dist)
{
	v4f  p0, p1, p2;
	v4f  v1, v2, normalVec;

	p0 = vec3Load( w->p[0] );
	p1 = vec3Load( w->p[1] );
	p2 = vec3Load( w->p[2] );
	v1 = v4fSub( p1 , p0 );
	v2 = v4fSub( p2 , p0 );
	normalVec = v4fVectorNormalize( v4fCrossProduct( v2, v1 ) );
	vec3aStore( normal, normalVec );
	*dist = s4fToFloat( v4fDotProduct( p0, normalVec ) );

}
#endif
void WindingPlane (winding_t *w, vec3_t normal, vec_t *dist)
{
	vec3_t	v1, v2;

	VectorSubtract (w->p[1], w->p[0], v1);
	VectorSubtract (w->p[2], w->p[0], v2);
	CrossProduct (v2, v1, normal);
	VectorNormalize2(normal, normal);
	*dist = DotProduct (w->p[0], normal);

}

/*
=============
WindingArea
=============
*/
#if id386_sse >= 1
vec_t	WindingArea_sse (winding_t *w)
{
	int	i;
	v4f	p0, pi, pi1, d1, d2, cross;
	s4f	total;

	p0 = vec3Load( w->p[0] );
	pi1 = vec3Load( w->p[1] );
	d1 = v4fSub( pi1, p0 );
	total = v4fZero;
	for (i=2 ; i<w->numpoints ; i++)
	{
		pi = vec3Load( w->p[i] );
		d2 = v4fSub( pi, p0 );
		cross = v4fCrossProduct( d1, d2 );
		total = v4fAdd( total, v4fVectorLength( cross ) );
		pi1 = pi;
		d1 = d2;
	}
	return 0.5 * s4fToFloat( total );
}
#endif
vec_t	WindingArea (winding_t *w)
{
	int	i;
	vec3_t	d1, d2, cross;
	vec_t	total;

	total = 0;
	for (i=2 ; i<w->numpoints ; i++)
	{
		VectorSubtract (w->p[i-1], w->p[0], d1);
		VectorSubtract (w->p[i], w->p[0], d2);
		CrossProduct (d1, d2, cross);
		total += 0.5 * VectorLength ( cross );
	}
	return total;
}

/*
=============
WindingBounds
=============
*/
#if id386_sse >= 1
void	WindingBounds_sse (winding_t *w, vec3a_t mins, vec3a_t maxs)
{
	v4f	minsVec, maxsVec, v;
	int	i;

	minsVec = s4fInit( MAX_MAP_BOUNDS );
	maxsVec = v4fNeg( minsVec );

	for (i = 0 ; i < w->numpoints ; i++)
	{
		v = vec3Load( w->p[i] );
		minsVec = v4fMin( minsVec, v );
		maxsVec = v4fMax( maxsVec, v );
	}
	
	vec3aStore( mins, minsVec );
	vec3aStore( maxs, maxsVec );
}
#endif
void	WindingBounds (winding_t *w, vec3_t mins, vec3_t maxs)
{
	vec_t	v;
	int		i,j;

	mins[0] = mins[1] = mins[2] = MAX_MAP_BOUNDS;
	maxs[0] = maxs[1] = maxs[2] = -MAX_MAP_BOUNDS;

	for (i=0 ; i<w->numpoints ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
}

/*
=============
WindingCenter
=============
*/
#if id386_sse >= 1
void	WindingCenter_sse (winding_t *w, vec3a_t center)
{
	int	i;
	v4f     centerVec;
	s4f	scale;

	centerVec = v4fZero;
	for (i=0 ; i<w->numpoints ; i++)
		centerVec = v4fAdd( centerVec, vec3Load( w->p[i] ) );

	scale = s4fInit( 1.0/w->numpoints );
	vec3aStore( center, v4fScale( scale, centerVec ) );
}
#endif
void	WindingCenter (winding_t *w, vec3_t center)
{
	int		i;
	float	scale;

	VectorCopy (vec3_origin, center);
	for (i=0 ; i<w->numpoints ; i++)
		VectorAdd (w->p[i], center, center);

	scale = 1.0/w->numpoints;
	VectorScale (center, scale, center);
}

/*
=================
BaseWindingForPlane
=================
*/
#if id386_sse >= 1
winding_t *BaseWindingForPlane_sse (v4f plane)
{
	s4f	v, dist;
	v4f	org, vright, vup;
	winding_t	*w;
	
	dist = v4fW( plane );
	plane = v4fXYZ( plane );

// find the major axis
	vup = v4fAbs( plane );
	vup = v4fGt( v4fZ( vup ), v4fMax( v4fX( vup ), v4fY( vup ) ) );
	// vup is all 1 when z is the major axis
	vup = v4fOr( v4fAnd(    vup, s4fToX( v4fOne ) ),
		     v4fAndNot( vup, s4fToZ( v4fOne ) ) );

	v = v4fDotProduct( vup, plane );
	vup = v4fMA( vup, v4fNeg( v ), plane );
	vup = v4fVectorNormalize( vup );
		
	org = v4fScale( plane, dist );
	vright = v4fCrossProduct( vup, plane );
	
	vup = v4fScale( s4fInit( MAX_MAP_BOUNDS ), vup );
	vright = v4fScale ( s4fInit( MAX_MAP_BOUNDS), vright );

// project a really big	axis aligned box onto the plane
	w = AllocWinding (4);
	
	v4fStoreU( w->p[0], v4fAdd( v4fSub( org, vright ), vup ) );
	v4fStoreU( w->p[1], v4fAdd( v4fAdd( org, vright ), vup ) );
	v4fStoreU( w->p[2], v4fSub( v4fAdd( org, vright ), vup ) );
	v4fStoreU( w->p[3], v4fSub( v4fSub( org, vright ), vup ) );
	
	w->numpoints = 4;
	
	return w;	
}
#endif
winding_t *BaseWindingForPlane (vec3_t normal, vec_t dist)
{
	int		i, x;
	vec_t	max, v;
	vec3_t	org, vright, vup;
	winding_t	*w;
	
// find the major axis

	max = -MAX_MAP_BOUNDS;
	x = -1;
	for (i=0 ; i<3; i++)
	{
		v = fabs(normal[i]);
		if (v > max)
		{
			x = i;
			max = v;
		}
	}
	if (x==-1)
		Com_Error (ERR_DROP, "BaseWindingForPlane: no axis found");
		
	VectorCopy (vec3_origin, vup);	
	switch (x)
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;		
	case 2:
		vup[0] = 1;
		break;		
	}

	v = DotProduct (vup, normal);
	VectorMA (vup, -v, normal, vup);
	VectorNormalize2(vup, vup);
		
	VectorScale (normal, dist, org);
	
	CrossProduct (vup, normal, vright);
	
	VectorScale (vup, MAX_MAP_BOUNDS, vup);
	VectorScale (vright, MAX_MAP_BOUNDS, vright);

// project a really big	axis aligned box onto the plane
	w = AllocWinding (4);
	
	VectorSubtract (org, vright, w->p[0]);
	VectorAdd (w->p[0], vup, w->p[0]);
	
	VectorAdd (org, vright, w->p[1]);
	VectorAdd (w->p[1], vup, w->p[1]);
	
	VectorAdd (org, vright, w->p[2]);
	VectorSubtract (w->p[2], vup, w->p[2]);
	
	VectorSubtract (org, vright, w->p[3]);
	VectorSubtract (w->p[3], vup, w->p[3]);
	
	w->numpoints = 4;
	
	return w;	
}

/*
==================
CopyWinding
==================
*/
winding_t	*CopyWinding (winding_t *w)
{
	unsigned long	size;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	size = (long)((winding_t *)0)->p[w->numpoints];
	Com_Memcpy (c, w, size);
	return c;
}

/*
==================
ReverseWinding
==================
*/
#if id386_sse >= 1
winding_t	*ReverseWinding_sse (winding_t *w)
{
	int		i;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	for (i=0 ; i<w->numpoints ; i++)
	{
		v4fStoreU( c->p[i], v4fLoadU( w->p[w->numpoints-1-i] ) );
	}
	c->numpoints = w->numpoints;
	return c;
}
#endif
winding_t	*ReverseWinding (winding_t *w)
{
	int			i;
	winding_t	*c;

	c = AllocWinding (w->numpoints);
	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorCopy (w->p[w->numpoints-1-i], c->p[i]);
	}
	c->numpoints = w->numpoints;
	return c;
}


/*
=============
ClipWindingEpsilon
=============
*/
#if id386_sse >= 1
void	ClipWindingEpsilon_sse (winding_t *in, v4f plane, vec_t epsilon,
				winding_t **front, winding_t **back)
{
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int	sides[MAX_POINTS_ON_WINDING+4];
	int	counts[3];
	s4f	dist;
	int	i;
	v4f	p1, p2;
	v4f	mid;
	winding_t	*f, *b;
	int	maxpts;
	
	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dist = v4fPlaneDist( vec3Load( in->p[i] ), plane );
		dists[i] = s4fToFloat( dist );
		if (dists[i] > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dists[i] < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	*front = *back = NULL;

	if (!counts[SIDE_FRONT])
	{
		*back = CopyWinding (in);
		return;
	}
	if (!counts[SIDE_BACK])
	{
		*front = CopyWinding (in);
		return;
	}

	maxpts = in->numpoints+4;	// cant use counts[0]+2 because
					// of fp grouping errors

	*front = f = AllocWinding (maxpts);
	*back = b = AllocWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = vec3Load( in->p[i] );
		
		if (sides[i] == SIDE_ON)
		{
			v4fStoreU( f->p[f->numpoints++], p1 );
			v4fStoreU( b->p[b->numpoints++], p1 );
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			v4fStoreU( f->p[f->numpoints++], p1 );
		}
		if (sides[i] == SIDE_BACK)
		{
			v4fStoreU( b->p[b->numpoints++], p1 );
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
		// generate a split point
		p2 = vec3Load( in->p[(i+1)%in->numpoints] );
		
		dist = s4fInit( dists[i] / (dists[i]-dists[i+1]) );
		mid = v4fLerp( dist, p1, p2 );
		
		v4fStoreU( f->p[f->numpoints++], mid );
		v4fStoreU( b->p[b->numpoints++], mid );
	}
	
	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Com_Error (ERR_DROP, "ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING || b->numpoints > MAX_POINTS_ON_WINDING)
		Com_Error (ERR_DROP, "ClipWinding: MAX_POINTS_ON_WINDING");
}
#endif
void	ClipWindingEpsilon (winding_t *in, vec3_t normal, vec_t dist, 
				vec_t epsilon, winding_t **front, winding_t **back)
{
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	static	vec_t	dot;		// VC 4.2 optimizer bug if not static
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f, *b;
	int		maxpts;
	
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->p[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	*front = *back = NULL;

	if (!counts[0])
	{
		*back = CopyWinding (in);
		return;
	}
	if (!counts[1])
	{
		*front = CopyWinding (in);
		return;
	}

	maxpts = in->numpoints+4;	// cant use counts[0]+2 because
								// of fp grouping errors

	*front = f = AllocWinding (maxpts);
	*back = b = AllocWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->p[i];
		
		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
			VectorCopy (p1, b->p[b->numpoints]);
			b->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
		}
		if (sides[i] == SIDE_BACK)
		{
			VectorCopy (p1, b->p[b->numpoints]);
			b->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
	// generate a split point
		p2 = in->p[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, f->p[f->numpoints]);
		f->numpoints++;
		VectorCopy (mid, b->p[b->numpoints]);
		b->numpoints++;
	}
	
	if (f->numpoints > maxpts || b->numpoints > maxpts)
		Com_Error (ERR_DROP, "ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING || b->numpoints > MAX_POINTS_ON_WINDING)
		Com_Error (ERR_DROP, "ClipWinding: MAX_POINTS_ON_WINDING");
}


/*
=============
ChopWindingInPlace
=============
*/
#if id386_sse >= 1
void ChopWindingInPlace_sse (winding_t **inout, v4f plane, vec_t epsilon)
{
	winding_t	*in;
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int	sides[MAX_POINTS_ON_WINDING+4];
	int	counts[3];
	v4f	dist;
	int	i;
	v4f	p1, p2;
	v4f	mid;
	winding_t	*f;
	int	maxpts;

	in = *inout;
	counts[SIDE_FRONT] = counts[SIDE_BACK] = counts[SIDE_ON] = 0;
	
// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dist = v4fPlaneDist( vec3Load( in->p[i] ), plane );
		dists[i] = s4fToFloat( dist );
		if (dists[i] > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dists[i] < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	if (!counts[SIDE_FRONT])
	{
		FreeWinding (in);
		*inout = NULL;
		return;
	}
	if (!counts[SIDE_BACK])
		return;		// inout stays the same

	maxpts = in->numpoints+4;	// cant use counts[0]+2 because
					// of fp grouping errors

	f = AllocWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = vec3Load( in->p[i] );
		
		if (sides[i] == SIDE_ON)
		{
			v4fStoreU( f->p[f->numpoints++], p1 );
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			v4fStoreU( f->p[f->numpoints++], p1 );
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
		// generate a split point
		p2 = vec3Load( in->p[(i+1)%in->numpoints] );
		
		dist = s4fInit( dists[i] / (dists[i]-dists[i+1]) );
		mid = v4fLerp( dist, p1, p2 );
			
		v4fStoreU( f->p[f->numpoints++], mid );
	}
	
	if (f->numpoints > maxpts)
		Com_Error (ERR_DROP, "ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Com_Error (ERR_DROP, "ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWinding (in);
	*inout = f;
}
#endif
void ChopWindingInPlace (winding_t **inout, vec3_t normal, vec_t dist, vec_t epsilon)
{
	winding_t	*in;
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	static	vec_t	dot;		// VC 4.2 optimizer bug if not static
	int		i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f;
	int		maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

// determine sides for each point
	for (i=0 ; i<in->numpoints ; i++)
	{
		dot = DotProduct (in->p[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];
	
	if (!counts[0])
	{
		FreeWinding (in);
		*inout = NULL;
		return;
	}
	if (!counts[1])
		return;		// inout stays the same

	maxpts = in->numpoints+4;	// cant use counts[0]+2 because
								// of fp grouping errors

	f = AllocWinding (maxpts);
		
	for (i=0 ; i<in->numpoints ; i++)
	{
		p1 = in->p[i];
		
		if (sides[i] == SIDE_ON)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
			continue;
		}
	
		if (sides[i] == SIDE_FRONT)
		{
			VectorCopy (p1, f->p[f->numpoints]);
			f->numpoints++;
		}

		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;
			
	// generate a split point
		p2 = in->p[(i+1)%in->numpoints];
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	// avoid round off error when possible
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}
			
		VectorCopy (mid, f->p[f->numpoints]);
		f->numpoints++;
	}
	
	if (f->numpoints > maxpts)
		Com_Error (ERR_DROP, "ClipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		Com_Error (ERR_DROP, "ClipWinding: MAX_POINTS_ON_WINDING");

	FreeWinding (in);
	*inout = f;
}


/*
=================
ChopWinding

Returns the fragment of in that is on the front side
of the cliping plane.  The original is freed.
=================
*/
#if id386_sse >= 1
winding_t	*ChopWinding_sse (winding_t *in, v4f plane)
{
	winding_t	*f, *b;

	ClipWindingEpsilon_sse (in, plane, ON_EPSILON, &f, &b);
	FreeWinding (in);
	if (b)
		FreeWinding (b);
	return f;
}
#endif
winding_t	*ChopWinding (winding_t *in, vec3_t normal, vec_t dist)
{
	winding_t	*f, *b;

	ClipWindingEpsilon (in, normal, dist, ON_EPSILON, &f, &b);
	FreeWinding (in);
	if (b)
		FreeWinding (b);
	return f;
}


/*
=================
CheckWinding

=================
*/
#if id386_sse >= 1
void CheckWinding_sse (winding_t *w)
{
	int	i, j;
	v4f	p1, p2, dir, edgenormal, maxBounds = s4fInit( MAX_MAP_BOUNDS );
	vec_t	d, edgedist;
	vec3a_t	facenormal;
	vec_t	area;
	vec_t	facedist;

	if (w->numpoints < 3)
		Com_Error (ERR_DROP, "CheckWinding: %i points",w->numpoints);
	
	area = WindingArea_sse(w);
	if (area < 1)
		Com_Error (ERR_DROP, "CheckWinding: %f area", area);

	WindingPlane_sse (w, facenormal, &facedist);
	
	for (i=0 ; i<w->numpoints ; i++)
	{
		p1 = vec3Load( w->p[i] );
		if( v4fAny( v4fGt( v4fAbs( p1 ), maxBounds ) ) )
			Com_Error (ERR_DROP, "CheckFace: BUGUS_RANGE ");

		j = i+1 == w->numpoints ? 0 : i+1;
		
	// check the point is on the face plane
		d = s4fToFloat( v4fDotProduct (p1, vec3aLoad( facenormal ) ) ) - facedist;
		if (d < -ON_EPSILON || d > ON_EPSILON)
			Com_Error (ERR_DROP, "CheckWinding: point off plane");
	
	// check the edge isnt degenerate
		p2 = vec3aLoad( w->p[j] );
		dir = v4fSub( p2, p1 );
		
		if ( s4fToFloat( v4fVectorLength(dir) ) < ON_EPSILON )
			Com_Error (ERR_DROP, "CheckWinding: degenerate edge");
			
		edgenormal = v4fCrossProduct( vec3aLoad( facenormal ), dir );
		edgenormal = v4fVectorNormalize( edgenormal );
		edgedist = s4fToFloat( v4fDotProduct( p1, edgenormal ) );
		edgedist += ON_EPSILON;
		
	// all other points must be on front side
		for (j=0 ; j<w->numpoints ; j++)
		{
			if (j == i)
				continue;
			d = s4fToFloat( v4fDotProduct( vec3aLoad( w->p[j] ),
						       edgenormal ) );
			if (d > edgedist)
				Com_Error (ERR_DROP, "CheckWinding: non-convex");
		}
	}
}
#endif
void CheckWinding (winding_t *w)
{
	int		i, j;
	vec_t	*p1, *p2;
	vec_t	d, edgedist;
	vec3_t	dir, edgenormal, facenormal;
	vec_t	area;
	vec_t	facedist;

	if (w->numpoints < 3)
		Com_Error (ERR_DROP, "CheckWinding: %i points",w->numpoints);
	
	area = WindingArea(w);
	if (area < 1)
		Com_Error (ERR_DROP, "CheckWinding: %f area", area);

	WindingPlane (w, facenormal, &facedist);
	
	for (i=0 ; i<w->numpoints ; i++)
	{
		p1 = w->p[i];

		for (j=0 ; j<3 ; j++)
			if (p1[j] > MAX_MAP_BOUNDS || p1[j] < -MAX_MAP_BOUNDS)
				Com_Error (ERR_DROP, "CheckFace: BUGUS_RANGE: %f",p1[j]);

		j = i+1 == w->numpoints ? 0 : i+1;
		
	// check the point is on the face plane
		d = DotProduct (p1, facenormal) - facedist;
		if (d < -ON_EPSILON || d > ON_EPSILON)
			Com_Error (ERR_DROP, "CheckWinding: point off plane");
	
	// check the edge isnt degenerate
		p2 = w->p[j];
		VectorSubtract (p2, p1, dir);
		
		if (VectorLength (dir) < ON_EPSILON)
			Com_Error (ERR_DROP, "CheckWinding: degenerate edge");
			
		CrossProduct (facenormal, dir, edgenormal);
		VectorNormalize2 (edgenormal, edgenormal);
		edgedist = DotProduct (p1, edgenormal);
		edgedist += ON_EPSILON;
		
	// all other points must be on front side
		for (j=0 ; j<w->numpoints ; j++)
		{
			if (j == i)
				continue;
			d = DotProduct (w->p[j], edgenormal);
			if (d > edgedist)
				Com_Error (ERR_DROP, "CheckWinding: non-convex");
		}
	}
}


/*
============
WindingOnPlaneSide
============
*/
#if id386_sse >= 1
int	WindingOnPlaneSide_sse (winding_t *w, v4f plane)
{
	qboolean	front, back;
	int		i;
	vec_t		d;

	front = qfalse;
	back = qfalse;
	for (i=0 ; i<w->numpoints ; i++)
	{
		d = s4fToFloat( v4fPlaneDist( vec3Load( w->p[i] ), plane ) );
		if (d < -ON_EPSILON)
		{
			if (front)
				return SIDE_CROSS;
			back = qtrue;
			continue;
		}
		if (d > ON_EPSILON)
		{
			if (back)
				return SIDE_CROSS;
			front = qtrue;
			continue;
		}
	}

	if (back)
		return SIDE_BACK;
	if (front)
		return SIDE_FRONT;
	return SIDE_ON;
}
#endif
int		WindingOnPlaneSide (winding_t *w, vec3_t normal, vec_t dist)
{
	qboolean	front, back;
	int			i;
	vec_t		d;

	front = qfalse;
	back = qfalse;
	for (i=0 ; i<w->numpoints ; i++)
	{
		d = DotProduct (w->p[i], normal) - dist;
		if (d < -ON_EPSILON)
		{
			if (front)
				return SIDE_CROSS;
			back = qtrue;
			continue;
		}
		if (d > ON_EPSILON)
		{
			if (back)
				return SIDE_CROSS;
			front = qtrue;
			continue;
		}
	}

	if (back)
		return SIDE_BACK;
	if (front)
		return SIDE_FRONT;
	return SIDE_ON;
}


/*
=================
AddWindingToConvexHull

Both w and *hull are on the same plane
=================
*/
#define	MAX_HULL_POINTS		128
#if id386_sse >= 1
void	AddWindingToConvexHull_sse( winding_t *w, winding_t **hull, v4f normal ) {
	int		i, j, k;
	v4f             p, copy;
	v4f		dir;
	float		d;
	int		numHullPoints, numNew;
	vec3_t		hullPoints[MAX_HULL_POINTS];
	vec3_t		newHullPoints[MAX_HULL_POINTS];
	vec3_t		hullDirs[MAX_HULL_POINTS];
	qboolean	hullSide[MAX_HULL_POINTS];
	qboolean	outside;

	if ( !*hull ) {
		*hull = CopyWinding( w );
		return;
	}

	numHullPoints = (*hull)->numpoints;
	Com_Memcpy( hullPoints, (*hull)->p, numHullPoints * sizeof(vec3_t) );

	for ( i = 0 ; i < w->numpoints ; i++ ) {
		p = vec3Load( w->p[i] );

		// calculate hull side vectors
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			k = ( j + 1 ) % numHullPoints;

			dir = v4fSub( vec3Load( hullPoints[k] ),
				      vec3Load( hullPoints[j] ) );
			dir = v4fVectorNormalize( dir );
			vec3aStore(hullDirs[j], v4fCrossProduct( normal, dir ) );
		}

		outside = qfalse;
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			dir = v4fSub( p, vec3Load( hullPoints[j] ) );
			d = s4fToFloat( v4fDotProduct( dir, vec3Load( hullDirs[j] ) ) );
			if ( d >= ON_EPSILON ) {
				outside = qtrue;
			}
			if ( d >= -ON_EPSILON ) {
				hullSide[j] = qtrue;
			} else {
				hullSide[j] = qfalse;
			}
		}

		// if the point is effectively inside, do nothing
		if ( !outside ) {
			continue;
		}

		// find the back side to front side transition
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			if ( !hullSide[ j % numHullPoints ] && hullSide[ (j + 1) % numHullPoints ] ) {
				break;
			}
		}
		if ( j == numHullPoints ) {
			continue;
		}

		// insert the point here
		v4fStoreU( newHullPoints[0], p );
		numNew = 1;

		// copy over all points that aren't double fronts
		j = (j+1)%numHullPoints;
		for ( k = 0 ; k < numHullPoints ; k++ ) {
			if ( hullSide[ (j+k) % numHullPoints ] && hullSide[ (j+k+1) % numHullPoints ] ) {
				continue;
			}
			copy = vec3Load( hullPoints[ (j+k+1) % numHullPoints ] );
			v4fStoreU( newHullPoints[numNew++], copy );
		}

		numHullPoints = numNew;
		Com_Memcpy( hullPoints, newHullPoints, numHullPoints * sizeof(vec3_t) );
	}

	FreeWinding( *hull );
	w = AllocWinding( numHullPoints );
	w->numpoints = numHullPoints;
	*hull = w;
	Com_Memcpy( w->p, hullPoints, numHullPoints * sizeof(vec3_t) );
}
#endif
void	AddWindingToConvexHull( winding_t *w, winding_t **hull, vec3_t normal ) {
	int			i, j, k;
	float		*p, *copy;
	vec3_t		dir;
	float		d;
	int			numHullPoints, numNew;
	vec3_t		hullPoints[MAX_HULL_POINTS];
	vec3_t		newHullPoints[MAX_HULL_POINTS];
	vec3_t		hullDirs[MAX_HULL_POINTS];
	qboolean	hullSide[MAX_HULL_POINTS];
	qboolean	outside;

	if ( !*hull ) {
		*hull = CopyWinding( w );
		return;
	}

	numHullPoints = (*hull)->numpoints;
	Com_Memcpy( hullPoints, (*hull)->p, numHullPoints * sizeof(vec3_t) );

	for ( i = 0 ; i < w->numpoints ; i++ ) {
		p = w->p[i];

		// calculate hull side vectors
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			k = ( j + 1 ) % numHullPoints;

			VectorSubtract( hullPoints[k], hullPoints[j], dir );
			VectorNormalize2( dir, dir );
			CrossProduct( normal, dir, hullDirs[j] );
		}

		outside = qfalse;
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			VectorSubtract( p, hullPoints[j], dir );
			d = DotProduct( dir, hullDirs[j] );
			if ( d >= ON_EPSILON ) {
				outside = qtrue;
			}
			if ( d >= -ON_EPSILON ) {
				hullSide[j] = qtrue;
			} else {
				hullSide[j] = qfalse;
			}
		}

		// if the point is effectively inside, do nothing
		if ( !outside ) {
			continue;
		}

		// find the back side to front side transition
		for ( j = 0 ; j < numHullPoints ; j++ ) {
			if ( !hullSide[ j % numHullPoints ] && hullSide[ (j + 1) % numHullPoints ] ) {
				break;
			}
		}
		if ( j == numHullPoints ) {
			continue;
		}

		// insert the point here
		VectorCopy( p, newHullPoints[0] );
		numNew = 1;

		// copy over all points that aren't double fronts
		j = (j+1)%numHullPoints;
		for ( k = 0 ; k < numHullPoints ; k++ ) {
			if ( hullSide[ (j+k) % numHullPoints ] && hullSide[ (j+k+1) % numHullPoints ] ) {
				continue;
			}
			copy = hullPoints[ (j+k+1) % numHullPoints ];
			VectorCopy( copy, newHullPoints[numNew] );
			numNew++;
		}

		numHullPoints = numNew;
		Com_Memcpy( hullPoints, newHullPoints, numHullPoints * sizeof(vec3_t) );
	}

	FreeWinding( *hull );
	w = AllocWinding( numHullPoints );
	w->numpoints = numHullPoints;
	*hull = w;
	Com_Memcpy( w->p, hullPoints, numHullPoints * sizeof(vec3_t) );
}


