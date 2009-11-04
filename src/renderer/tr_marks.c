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
// tr_marks.c -- polygon projection on the world polygons

#include "tr_local.h"
//#include "assert.h"
#if id386_sse >= 1
#include "../qcommon/qsse.h"
#endif

#define MAX_VERTS_ON_POLY		64

#define MARKER_OFFSET			0	// 1

/*
=============
R_ChopPolyBehindPlane

Out must have space for two more vertexes than in
=============
*/
#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
#if id386_sse >= 1
static void R_ChopPolyBehindPlane_sse( int numInPoints, v4f inPoints[MAX_VERTS_ON_POLY],
				       int *numOutPoints, v4f outPoints[MAX_VERTS_ON_POLY], 
				       v4f plane, vec_t epsilon) {
	float		dists[MAX_VERTS_ON_POLY+4];
	int			sides[MAX_VERTS_ON_POLY+4];
	int			counts[3];
	v4f		p1, p2, dot;
	int			i;
	v4f		*clip;
	float		d;

	// don't clip if it might overflow
	if ( numInPoints >= MAX_VERTS_ON_POLY - 2 ) {
		*numOutPoints = 0;
		return;
	}

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0 ; i < numInPoints ; i++ ) {
		dot = v4fPlaneDist( inPoints[i], plane );
		dists[i] = s4fToFloat(dot);
		if ( dists[i] > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dists[i] < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*numOutPoints = 0;

	if ( !counts[0] ) {
		return;
	}
	if ( !counts[1] ) {
		*numOutPoints = numInPoints;
		Com_Memcpy( outPoints, inPoints, numInPoints * sizeof(v4f) );
		return;
	}

	for ( i = 0 ; i < numInPoints ; i++ ) {
		p1 = inPoints[i];
		clip = &outPoints[ *numOutPoints ];
		
		if ( sides[i] == SIDE_ON ) {
			*clip = p1;
			(*numOutPoints)++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			*clip = p1;
			(*numOutPoints)++;
			clip = &outPoints[ *numOutPoints ];
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		// generate a split point
		p2 = inPoints[ (i+1) % numInPoints ];

		d = dists[i] - dists[i+1];
		if ( d == 0 ) {
			dot = v4fZero;
		} else {
			dot = s4fInit(dists[i] / d);
		}

		// clip xyz
		*clip = v4fLerp( dot, p1, p2 );
		(*numOutPoints)++;
	}
}
#endif
static void R_ChopPolyBehindPlane( int numInPoints, vec3_t inPoints[MAX_VERTS_ON_POLY],
								   int *numOutPoints, vec3_t outPoints[MAX_VERTS_ON_POLY], 
							vec3_t normal, vec_t dist, vec_t epsilon) {
	float		dists[MAX_VERTS_ON_POLY+4];
	int			sides[MAX_VERTS_ON_POLY+4];
	int			counts[3];
	float		dot;
	int			i, j;
	float		*p1, *p2, *clip;
	float		d;

	// don't clip if it might overflow
	if ( numInPoints >= MAX_VERTS_ON_POLY - 2 ) {
		*numOutPoints = 0;
		return;
	}

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0 ; i < numInPoints ; i++ ) {
		dot = DotProduct( inPoints[i], normal );
		dot -= dist;
		dists[i] = dot;
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*numOutPoints = 0;

	if ( !counts[0] ) {
		return;
	}
	if ( !counts[1] ) {
		*numOutPoints = numInPoints;
		Com_Memcpy( outPoints, inPoints, numInPoints * sizeof(vec3_t) );
		return;
	}

	for ( i = 0 ; i < numInPoints ; i++ ) {
		p1 = inPoints[i];
		clip = outPoints[ *numOutPoints ];
		
		if ( sides[i] == SIDE_ON ) {
			VectorCopy( p1, clip );
			(*numOutPoints)++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, clip );
			(*numOutPoints)++;
			clip = outPoints[ *numOutPoints ];
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		// generate a split point
		p2 = inPoints[ (i+1) % numInPoints ];

		d = dists[i] - dists[i+1];
		if ( d == 0 ) {
			dot = 0;
		} else {
			dot = dists[i] / d;
		}

		// clip xyz

		for (j=0 ; j<3 ; j++) {
			clip[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		(*numOutPoints)++;
	}
}

/*
=================
R_BoxSurfaces_r

=================
*/
#if id386_sse >= 1
void R_BoxSurfaces_r_sse(mnode_t *node, v4f mins, v4f maxs, surfaceType_t **list, int listsize, int *listlength, v4f dir) {

	int			s, c;
	msurface_t	*surf, **mark;

	// do the tail recursion in a loop
	while ( node->contents == -1 ) {
		s = v4fBoxOnPlaneSide( mins, maxs, v4fLoadU(node->plane->normal) );
		if (s == 1) {
			node = node->children[0];
		} else if (s == 2) {
			node = node->children[1];
		} else {
			R_BoxSurfaces_r_sse(node->children[0], mins, maxs, list, listsize, listlength, dir);
			node = node->children[1];
		}
	}

	// add the individual surfaces
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;
	while (c--) {
		//
		if (*listlength >= listsize) break;
		//
		surf = *mark;
		// check if the surface has NOIMPACT or NOMARKS set
		if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) )
			|| ( surf->shader->contentFlags & CONTENTS_FOG ) ) {
			surf->viewCount = tr.viewCount;
		}
		// extra check for surfaces to avoid list overflows
		else if (*(surf->data) == SF_FACE) {
			// the face plane should go through the box
			s = v4fBoxOnPlaneSide( mins, maxs,
					       v4fLoadU((( srfSurfaceFace_t * ) surf->data)->plane.normal) );
			if (s == 1 || s == 2) {
				surf->viewCount = tr.viewCount;
			} else if (v4fAny(v4fGt(v4fDotProduct(vec3Load((( srfSurfaceFace_t * ) surf->data)->plane.normal), dir),v4fMZeroDotFive))) {
			// don't add faces that make sharp angles with the projection direction
				surf->viewCount = tr.viewCount;
			}
		}
		else if (*(surfaceType_t *) (surf->data) != SF_GRID) surf->viewCount = tr.viewCount;
		// check the viewCount because the surface may have
		// already been added if it spans multiple leafs
		if (surf->viewCount != tr.viewCount) {
			surf->viewCount = tr.viewCount;
			list[*listlength] = (surfaceType_t *) surf->data;
			(*listlength)++;
		}
		mark++;
	}
}
#endif
void R_BoxSurfaces_r(mnode_t *node, vec3_t mins, vec3_t maxs, surfaceType_t **list, int listsize, int *listlength, vec3_t dir) {

	int			s, c;
	msurface_t	*surf, **mark;

	// do the tail recursion in a loop
	while ( node->contents == -1 ) {
		s = BoxOnPlaneSide( mins, maxs, node->plane );
		if (s == 1) {
			node = node->children[0];
		} else if (s == 2) {
			node = node->children[1];
		} else {
			R_BoxSurfaces_r(node->children[0], mins, maxs, list, listsize, listlength, dir);
			node = node->children[1];
		}
	}

	// add the individual surfaces
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;
	while (c--) {
		//
		if (*listlength >= listsize) break;
		//
		surf = *mark;
		// check if the surface has NOIMPACT or NOMARKS set
		if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) )
			|| ( surf->shader->contentFlags & CONTENTS_FOG ) ) {
			surf->viewCount = tr.viewCount;
		}
		// extra check for surfaces to avoid list overflows
		else if (*(surf->data) == SF_FACE) {
			// the face plane should go through the box
			s = BoxOnPlaneSide( mins, maxs, &(( srfSurfaceFace_t * ) surf->data)->plane );
			if (s == 1 || s == 2) {
				surf->viewCount = tr.viewCount;
			} else if (DotProduct((( srfSurfaceFace_t * ) surf->data)->plane.normal, dir) > -0.5) {
			// don't add faces that make sharp angles with the projection direction
				surf->viewCount = tr.viewCount;
			}
		}
		else if (*(surfaceType_t *) (surf->data) != SF_GRID) surf->viewCount = tr.viewCount;
		// check the viewCount because the surface may have
		// already been added if it spans multiple leafs
		if (surf->viewCount != tr.viewCount) {
			surf->viewCount = tr.viewCount;
			list[*listlength] = (surfaceType_t *) surf->data;
			(*listlength)++;
		}
		mark++;
	}
}

/*
=================
R_AddMarkFragments

=================
*/
#if id386_sse >= 1
void R_AddMarkFragments_sse(int numClipPoints, v4f clipPoints[2][MAX_VERTS_ON_POLY],
			    int numPlanes, v4f *planes,
			    int maxPoints, vec3_t pointBuffer,
			    int maxFragments, markFragment_t *fragmentBuffer,
			    int *returnedPoints, int *returnedFragments,
			    v4f mins, v4f maxs) {
	int pingPong, i;
	markFragment_t	*mf;

	// chop the surface by all the bounding planes of the to be projected polygon
	pingPong = 0;

	for ( i = 0 ; i < numPlanes ; i++ ) {

		R_ChopPolyBehindPlane_sse( numClipPoints, clipPoints[pingPong],
					   &numClipPoints, clipPoints[!pingPong],
					   planes[i], 0.5 );
		pingPong ^= 1;
		if ( numClipPoints == 0 ) {
			break;
		}
	}
	// completely clipped away?
	if ( numClipPoints == 0 ) {
		return;
	}

	// add this fragment to the returned list
	if ( numClipPoints + (*returnedPoints) > maxPoints ) {
		return;	// not enough space for this polygon
	}
	/*
	// all the clip points should be within the bounding box
	for ( i = 0 ; i < numClipPoints ; i++ ) {
		int j;
		for ( j = 0 ; j < 3 ; j++ ) {
			if (clipPoints[pingPong][i][j] < mins[j] - 0.5) break;
			if (clipPoints[pingPong][i][j] > maxs[j] + 0.5) break;
		}
		if (j < 3) break;
	}
	if (i < numClipPoints) return;
	*/

	mf = fragmentBuffer + (*returnedFragments);
	mf->firstPoint = (*returnedPoints);
	mf->numPoints = numClipPoints;
	for ( i = 0; i < numClipPoints; i++ ) {
		Com_Memcpy( pointBuffer + (*returnedPoints) * 3, &clipPoints[pingPong][i], sizeof(vec3_t) );
		(*returnedPoints) ++;
	}
	(*returnedFragments)++;
}
#endif
void R_AddMarkFragments(int numClipPoints, vec3_t clipPoints[2][MAX_VERTS_ON_POLY],
				   int numPlanes, vec3_t *normals, float *dists,
				   int maxPoints, vec3_t pointBuffer,
				   int maxFragments, markFragment_t *fragmentBuffer,
				   int *returnedPoints, int *returnedFragments,
				   vec3_t mins, vec3_t maxs) {
	int pingPong, i;
	markFragment_t	*mf;

	// chop the surface by all the bounding planes of the to be projected polygon
	pingPong = 0;

	for ( i = 0 ; i < numPlanes ; i++ ) {

		R_ChopPolyBehindPlane( numClipPoints, clipPoints[pingPong],
						   &numClipPoints, clipPoints[!pingPong],
							normals[i], dists[i], 0.5 );
		pingPong ^= 1;
		if ( numClipPoints == 0 ) {
			break;
		}
	}
	// completely clipped away?
	if ( numClipPoints == 0 ) {
		return;
	}

	// add this fragment to the returned list
	if ( numClipPoints + (*returnedPoints) > maxPoints ) {
		return;	// not enough space for this polygon
	}
	/*
	// all the clip points should be within the bounding box
	for ( i = 0 ; i < numClipPoints ; i++ ) {
		int j;
		for ( j = 0 ; j < 3 ; j++ ) {
			if (clipPoints[pingPong][i][j] < mins[j] - 0.5) break;
			if (clipPoints[pingPong][i][j] > maxs[j] + 0.5) break;
		}
		if (j < 3) break;
	}
	if (i < numClipPoints) return;
	*/

	mf = fragmentBuffer + (*returnedFragments);
	mf->firstPoint = (*returnedPoints);
	mf->numPoints = numClipPoints;
	Com_Memcpy( pointBuffer + (*returnedPoints) * 3, clipPoints[pingPong], numClipPoints * sizeof(vec3_t) );

	(*returnedPoints) += numClipPoints;
	(*returnedFragments)++;
}

/*
=================
R_MarkFragments

=================
*/
#if id386_sse >= 1
int R_MarkFragments_sse( int numPoints, const vec3_t *points, const vec3_t projection,
			 int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {
	int				numsurfaces, numPlanes;
	int				i, j, k, m, n;
	surfaceType_t	*surfaces[64];
	v4f			mins, maxs;
	int				returnedFragments;
	int				returnedPoints;
	v4f		planes[MAX_VERTS_ON_POLY+2];
	v4f			clipPoints[2][MAX_VERTS_ON_POLY];
	int				numClipPoints;
	float			*v;
	srfSurfaceFace_t *surf;
	srfGridMesh_t	*cv;
	drawVert_t		*dv;
	v4f			normal;
	v4f			projectionVec, projectionDir;
	v4f			v1, v2;
	s4f                     MO;
	int				*indexes;

	//increment view count for double check prevention
	tr.viewCount++;

	//
	projectionVec = vec3Load( projection );
	projectionDir = v4fVectorNormalize( projectionVec );
	MO = s4fInit( MARKER_OFFSET );
	
	// find all the brushes that are to be considered
	mins = s4fInit( 99999 );
	maxs = s4fInit( -99999 );
	for ( i = 0 ; i < numPoints ; i++ ) {
		v4f	point, temp;

		point = vec3Load( points[i] );
		temp = v4fAdd( point, projectionVec );
		mins = v4fMin( mins, temp ); maxs = v4fMax( maxs, temp );
		// make sure we get all the leafs (also the one(s) in front of the hit surface)
		temp = v4fMA( point, s4fInit(-20), projectionDir );
		mins = v4fMin( mins, temp ); maxs = v4fMax( maxs, temp );
	}

	if (numPoints > MAX_VERTS_ON_POLY) numPoints = MAX_VERTS_ON_POLY;
	// create the bounding planes for the to be projected polygon
	for ( i = 0 ; i < numPoints ; i++ ) {
		v4f	point, nextPoint;

		point = vec3Load(points[i]);
		nextPoint = vec3Load(points[(i+1)%numPoints]);
		v1 = v4fSub( nextPoint, point );
		v2 = v4fAdd( point, projectionVec );
		v2 = v4fSub( point, v2 );
		v2 = v4fCrossProduct( v1, v2 );
		v2 = v4fVectorNormalize( v2 );
		planes[i] = v4fMix( v2, v4fDotProduct( v2, point ), mixMask0001 );
	}
	// add near and far clipping planes for projection
	v1 = vec3Load( points[0] );
	v2 = projectionDir;
	planes[numPoints] = v4fMix( v2, v4fSub(v4fDotProduct( v2, v1 ), s4fInit(32) ), mixMask0001 );
	v2 = v4fNeg( projectionDir );
	planes[numPoints + 1] =  v4fMix( v2, v4fSub(v4fDotProduct( v2, v1 ), s4fInit(20) ), mixMask0001 );
	numPlanes = numPoints + 2;

	numsurfaces = 0;
	R_BoxSurfaces_r_sse(tr.world->nodes, mins, maxs, surfaces, 64, &numsurfaces, projectionDir);
	//assert(numsurfaces <= 64);
	//assert(numsurfaces != 64);

	returnedPoints = 0;
	returnedFragments = 0;

	for ( i = 0 ; i < numsurfaces ; i++ ) {

		if (*surfaces[i] == SF_GRID) {

			cv = (srfGridMesh_t *) surfaces[i];
			for ( m = 0 ; m < cv->height - 1 ; m++ ) {
				for ( n = 0 ; n < cv->width - 1 ; n++ ) {
					// We triangulate the grid and chop all triangles within
					// the bounding planes of the to be projected polygon.
					// LOD is not taken into account, not such a big deal though.
					//
					// It's probably much nicer to chop the grid itself and deal
					// with this grid as a normal SF_GRID surface so LOD will
					// be applied. However the LOD of that chopped grid must
					// be synced with the LOD of the original curve.
					// One way to do this; the chopped grid shares vertices with
					// the original curve. When LOD is applied to the original
					// curve the unused vertices are flagged. Now the chopped curve
					// should skip the flagged vertices. This still leaves the
					// problems with the vertices at the chopped grid edges.
					//
					// To avoid issues when LOD applied to "hollow curves" (like
					// the ones around many jump pads) we now just add a 2 unit
					// offset to the triangle vertices.
					// The offset is added in the vertex normal vector direction
					// so all triangles will still fit together.
					// The 2 unit offset should avoid pretty much all LOD problems.

					numClipPoints = 3;

					dv = cv->verts + m * cv->width + n;

					clipPoints[0][0] = v4fMA( vec3Load(dv[0].xyz), MO, vec3Load(dv[0].normal) );
					clipPoints[0][1] = v4fMA( vec3Load(dv[cv->width].xyz), MO, vec3Load(dv[cv->width].normal) );
					clipPoints[0][2] = v4fMA( vec3Load(dv[1].xyz), MO, vec3Load(dv[1].normal) );
					// check the normal of this triangle
					v1 = v4fSub(clipPoints[0][0], clipPoints[0][1]);
					v2 = v4fSub(clipPoints[0][2], clipPoints[0][1]);
					normal = v4fCrossProduct(v1, v2);
					normal = v4fVectorNormalize(normal);
					if (v4fAny(v4fLt(v4fDotProduct(normal, projectionDir),v4fMZeroDotOne)) ) {
						// add the fragments of this triangle
						R_AddMarkFragments_sse(numClipPoints, clipPoints,
								       numPlanes, planes,
								       maxPoints, pointBuffer,
								       maxFragments, fragmentBuffer,
								       &returnedPoints, &returnedFragments, mins, maxs);

						if ( returnedFragments == maxFragments ) {
							return returnedFragments;	// not enough space for more fragments
						}
					}

					clipPoints[0][0] = v4fMA( vec3Load(dv[1].xyz), MO, vec3Load(dv[1].normal) );
					clipPoints[0][1] = v4fMA( vec3Load(dv[cv->width].xyz), MO, vec3Load(dv[cv->width].normal) );
					clipPoints[0][2] = v4fMA( vec3Load(dv[cv->width+1].xyz), MO, vec3Load(dv[cv->width+1].normal) );

					// check the normal of this triangle
					v1 = v4fSub(clipPoints[0][0], clipPoints[0][1]);
					v2 = v4fSub(clipPoints[0][2], clipPoints[0][1]);
					normal = v4fCrossProduct(v1, v2);
					normal = v4fVectorNormalize(normal);
					if (v4fAny(v4fLt(v4fDotProduct(normal, projectionDir), s4fInit(-0.05))) ) {
						// add the fragments of this triangle
						R_AddMarkFragments_sse(numClipPoints, clipPoints,
								       numPlanes, planes,
								       maxPoints, pointBuffer,
								       maxFragments, fragmentBuffer,
								       &returnedPoints, &returnedFragments, mins, maxs);

						if ( returnedFragments == maxFragments ) {
							return returnedFragments;	// not enough space for more fragments
						}
					}
				}
			}
		}
		else if (*surfaces[i] == SF_FACE) {
			v4f normal;

			surf = ( srfSurfaceFace_t * ) surfaces[i];
			normal = vec3Load(surf->plane.normal);

			// check the normal of this face
			if (v4fAny(v4fGt(v4fDotProduct(normal, projectionDir), v4fMZeroDotFive)) ) {
				continue;
			}

			/*
			VectorSubtract(clipPoints[0][0], clipPoints[0][1], v1);
			VectorSubtract(clipPoints[0][2], clipPoints[0][1], v2);
			CrossProduct(v1, v2, normal);
			VectorNormalize(normal);
			if (DotProduct(normal, projectionDir) > -0.5) continue;
			*/
			indexes = (int *)( (byte *)surf + surf->ofsIndices );
			for ( k = 0 ; k < surf->numIndices ; k += 3 ) {
				for ( j = 0 ; j < 3 ; j++ ) {
					v = surf->points[0] + VERTEXSIZE * indexes[k+j];;
					clipPoints[0][j] = v4fMA( vec3Load(v), MO, normal );
				}
				// add the fragments of this face
				R_AddMarkFragments_sse( 3 , clipPoints,
							numPlanes, planes,
							maxPoints, pointBuffer,
							maxFragments, fragmentBuffer,
							&returnedPoints, &returnedFragments, mins, maxs);
				if ( returnedFragments == maxFragments ) {
					return returnedFragments;	// not enough space for more fragments
				}
			}
			continue;
		}
		else {
			// ignore all other world surfaces
			// might be cool to also project polygons on a triangle soup
			// however this will probably create huge amounts of extra polys
			// even more than the projection onto curves
			continue;
		}
	}
	return returnedFragments;
}

#endif
int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ) {
	int				numsurfaces, numPlanes;
	int				i, j, k, m, n;
	surfaceType_t	*surfaces[64];
	vec3_t			mins, maxs;
	int				returnedFragments;
	int				returnedPoints;
	vec3_t			normals[MAX_VERTS_ON_POLY+2];
	float			dists[MAX_VERTS_ON_POLY+2];
	vec3_t			clipPoints[2][MAX_VERTS_ON_POLY];
	int				numClipPoints;
	float			*v;
	srfSurfaceFace_t *surf;
	srfGridMesh_t	*cv;
	drawVert_t		*dv;
	vec3_t			normal;
	vec3_t			projectionDir;
	vec3_t			v1, v2;
	int				*indexes;

#if id386_sse >= 1
	if ( com_sse->integer >= 1 )
		return R_MarkFragments_sse( numPoints, points, projection,
					    maxPoints, pointBuffer, maxFragments, fragmentBuffer );
#endif

	//increment view count for double check prevention
	tr.viewCount++;

	//
	VectorNormalize2( projection, projectionDir );
	// find all the brushes that are to be considered
	ClearBounds( mins, maxs );
	for ( i = 0 ; i < numPoints ; i++ ) {
		vec3_t	temp;

		AddPointToBounds( points[i], mins, maxs );
		VectorAdd( points[i], projection, temp );
		AddPointToBounds( temp, mins, maxs );
		// make sure we get all the leafs (also the one(s) in front of the hit surface)
		VectorMA( points[i], -20, projectionDir, temp );
		AddPointToBounds( temp, mins, maxs );
	}

	if (numPoints > MAX_VERTS_ON_POLY) numPoints = MAX_VERTS_ON_POLY;
	// create the bounding planes for the to be projected polygon
	for ( i = 0 ; i < numPoints ; i++ ) {
		VectorSubtract(points[(i+1)%numPoints], points[i], v1);
		VectorAdd(points[i], projection, v2);
		VectorSubtract(points[i], v2, v2);
		CrossProduct(v1, v2, normals[i]);
		VectorNormalizeFast(normals[i]);
		dists[i] = DotProduct(normals[i], points[i]);
	}
	// add near and far clipping planes for projection
	VectorCopy(projectionDir, normals[numPoints]);
	dists[numPoints] = DotProduct(normals[numPoints], points[0]) - 32;
	VectorCopy(projectionDir, normals[numPoints+1]);
	VectorInverse(normals[numPoints+1]);
	dists[numPoints+1] = DotProduct(normals[numPoints+1], points[0]) - 20;
	numPlanes = numPoints + 2;

	numsurfaces = 0;
	R_BoxSurfaces_r(tr.world->nodes, mins, maxs, surfaces, 64, &numsurfaces, projectionDir);
	//assert(numsurfaces <= 64);
	//assert(numsurfaces != 64);

	returnedPoints = 0;
	returnedFragments = 0;

	for ( i = 0 ; i < numsurfaces ; i++ ) {

		if (*surfaces[i] == SF_GRID) {

			cv = (srfGridMesh_t *) surfaces[i];
			for ( m = 0 ; m < cv->height - 1 ; m++ ) {
				for ( n = 0 ; n < cv->width - 1 ; n++ ) {
					// We triangulate the grid and chop all triangles within
					// the bounding planes of the to be projected polygon.
					// LOD is not taken into account, not such a big deal though.
					//
					// It's probably much nicer to chop the grid itself and deal
					// with this grid as a normal SF_GRID surface so LOD will
					// be applied. However the LOD of that chopped grid must
					// be synced with the LOD of the original curve.
					// One way to do this; the chopped grid shares vertices with
					// the original curve. When LOD is applied to the original
					// curve the unused vertices are flagged. Now the chopped curve
					// should skip the flagged vertices. This still leaves the
					// problems with the vertices at the chopped grid edges.
					//
					// To avoid issues when LOD applied to "hollow curves" (like
					// the ones around many jump pads) we now just add a 2 unit
					// offset to the triangle vertices.
					// The offset is added in the vertex normal vector direction
					// so all triangles will still fit together.
					// The 2 unit offset should avoid pretty much all LOD problems.

					numClipPoints = 3;

					dv = cv->verts + m * cv->width + n;

					VectorCopy(dv[0].xyz, clipPoints[0][0]);
					VectorMA(clipPoints[0][0], MARKER_OFFSET, dv[0].normal, clipPoints[0][0]);
					VectorCopy(dv[cv->width].xyz, clipPoints[0][1]);
					VectorMA(clipPoints[0][1], MARKER_OFFSET, dv[cv->width].normal, clipPoints[0][1]);
					VectorCopy(dv[1].xyz, clipPoints[0][2]);
					VectorMA(clipPoints[0][2], MARKER_OFFSET, dv[1].normal, clipPoints[0][2]);
					// check the normal of this triangle
					VectorSubtract(clipPoints[0][0], clipPoints[0][1], v1);
					VectorSubtract(clipPoints[0][2], clipPoints[0][1], v2);
					CrossProduct(v1, v2, normal);
					VectorNormalizeFast(normal);
					if (DotProduct(normal, projectionDir) < -0.1) {
						// add the fragments of this triangle
						R_AddMarkFragments(numClipPoints, clipPoints,
										   numPlanes, normals, dists,
										   maxPoints, pointBuffer,
										   maxFragments, fragmentBuffer,
										   &returnedPoints, &returnedFragments, mins, maxs);

						if ( returnedFragments == maxFragments ) {
							return returnedFragments;	// not enough space for more fragments
						}
					}

					VectorCopy(dv[1].xyz, clipPoints[0][0]);
					VectorMA(clipPoints[0][0], MARKER_OFFSET, dv[1].normal, clipPoints[0][0]);
					VectorCopy(dv[cv->width].xyz, clipPoints[0][1]);
					VectorMA(clipPoints[0][1], MARKER_OFFSET, dv[cv->width].normal, clipPoints[0][1]);
					VectorCopy(dv[cv->width+1].xyz, clipPoints[0][2]);
					VectorMA(clipPoints[0][2], MARKER_OFFSET, dv[cv->width+1].normal, clipPoints[0][2]);
					// check the normal of this triangle
					VectorSubtract(clipPoints[0][0], clipPoints[0][1], v1);
					VectorSubtract(clipPoints[0][2], clipPoints[0][1], v2);
					CrossProduct(v1, v2, normal);
					VectorNormalizeFast(normal);
					if (DotProduct(normal, projectionDir) < -0.05) {
						// add the fragments of this triangle
						R_AddMarkFragments(numClipPoints, clipPoints,
										   numPlanes, normals, dists,
										   maxPoints, pointBuffer,
										   maxFragments, fragmentBuffer,
										   &returnedPoints, &returnedFragments, mins, maxs);

						if ( returnedFragments == maxFragments ) {
							return returnedFragments;	// not enough space for more fragments
						}
					}
				}
			}
		}
		else if (*surfaces[i] == SF_FACE) {

			surf = ( srfSurfaceFace_t * ) surfaces[i];
			// check the normal of this face
			if (DotProduct(surf->plane.normal, projectionDir) > -0.5) {
				continue;
			}

			/*
			VectorSubtract(clipPoints[0][0], clipPoints[0][1], v1);
			VectorSubtract(clipPoints[0][2], clipPoints[0][1], v2);
			CrossProduct(v1, v2, normal);
			VectorNormalize(normal);
			if (DotProduct(normal, projectionDir) > -0.5) continue;
			*/
			indexes = (int *)( (byte *)surf + surf->ofsIndices );
			for ( k = 0 ; k < surf->numIndices ; k += 3 ) {
				for ( j = 0 ; j < 3 ; j++ ) {
					v = surf->points[0] + VERTEXSIZE * indexes[k+j];;
					VectorMA( v, MARKER_OFFSET, surf->plane.normal, clipPoints[0][j] );
				}
				// add the fragments of this face
				R_AddMarkFragments( 3 , clipPoints,
								   numPlanes, normals, dists,
								   maxPoints, pointBuffer,
								   maxFragments, fragmentBuffer,
								   &returnedPoints, &returnedFragments, mins, maxs);
				if ( returnedFragments == maxFragments ) {
					return returnedFragments;	// not enough space for more fragments
				}
			}
			continue;
		}
		else {
			// ignore all other world surfaces
			// might be cool to also project polygons on a triangle soup
			// however this will probably create huge amounts of extra polys
			// even more than the projection onto curves
			continue;
		}
	}
	return returnedFragments;
}

