/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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
// tr_main.c -- main control flow for each frame
#include "tr_local.h"

trGlobals_t     tr;

// convert from our coordinate system (looking down X)
// to OpenGL's coordinate system (looking down -Z)
const matrix_t  quakeToOpenGLMatrix = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

// inverse of QuakeToOpenGL matrix
const matrix_t  openGLToQuakeMatrix = {
	0, -1, 0, 0,
	0, 0, 1, 0,
	-1, 0, 0, 0,
	0, 0, 0, 1
};

int             shadowMapResolutions[5] = { 2048, 1024, 512, 256, 128 };

refimport_t     ri;

// entities that will have procedurally generated surfaces will just
// point at this for their sorting surface
surfaceType_t   entitySurface = SF_ENTITY;



/*
================
R_CompareVert
================
*/
qboolean R_CompareVert(srfVert_t * v1, srfVert_t * v2, qboolean checkST)
{
	int             i;

	for(i = 0; i < 3; i++)
	{
		if(floor(v1->xyz[i] + 0.1) != floor(v2->xyz[i] + 0.1))
		{
			return qfalse;
		}

		if(checkST && ((v1->st[0] != v2->st[0]) || (v1->st[1] != v2->st[1])))
		{
			return qfalse;
		}
	}

	return qtrue;
}

/*
=============
R_CalcNormalForTriangle
=============
*/
void R_CalcNormalForTriangle(vec3_t normal, const vec3_t v0, const vec3_t v1, const vec3_t v2)
{
	vec3_t          udir, vdir;

	// compute the face normal based on vertex points
	VectorSubtract(v2, v0, udir);
	VectorSubtract(v1, v0, vdir);
	CrossProduct(udir, vdir, normal);

	VectorNormalizeFast(normal);
}

/*
=============
R_CalcTangentsForTriangle
http://members.rogers.com/deseric/tangentspace.htm
=============
*/
/*
void R_CalcTangentsForTriangle(vec3_t tangent, vec3_t binormal,
							   const vec3_t v0, const vec3_t v1, const vec3_t v2,
							   const vec2_t t0, const vec2_t t1, const vec2_t t2)
{
	int             i;
	vec3_t          planes[3];
	vec3_t          e0;
	vec3_t          e1;

	for(i = 0; i < 3; i++)
	{
		VectorSet(e0, v1[i] - v0[i], t1[0] - t0[0], t1[1] - t0[1]);
		VectorSet(e1, v2[i] - v0[i], t2[0] - t0[0], t2[1] - t0[1]);

		VectorNormalizeFast(e0);
		VectorNormalizeFast(e1);

		CrossProduct(e0, e1, planes[i]);
	}

	//So your tangent space will be defined by this :
	//Normal = Normal of the triangle or Tangent X Binormal (careful with the cross product,
	// you have to make sure the normal points in the right direction)
	//Tangent = ( dp(Fx(s,t)) / ds,  dp(Fy(s,t)) / ds, dp(Fz(s,t)) / ds )   or     ( -Bx/Ax, -By/Ay, - Bz/Az )
	//Binormal =  ( dp(Fx(s,t)) / dt,  dp(Fy(s,t)) / dt, dp(Fz(s,t)) / dt )  or     ( -Cx/Ax, -Cy/Ay, -Cz/Az )

	// tangent...
	tangent[0] = -planes[0][1] / planes[0][0];
	tangent[1] = -planes[1][1] / planes[1][0];
	tangent[2] = -planes[2][1] / planes[2][0];
	VectorNormalizeFast(tangent);

	// binormal...
	binormal[0] = -planes[0][2] / planes[0][0];
	binormal[1] = -planes[1][2] / planes[1][0];
	binormal[2] = -planes[2][2] / planes[2][0];
	VectorNormalizeFast(binormal);
}
*/

/*
=============
R_CalcTangentSpace
Tr3B - recoded from Nvidia's SDK
=============
*/
void R_CalcTangentSpace(vec3_t tangent, vec3_t binormal, vec3_t normal,
						const vec3_t v0, const vec3_t v1, const vec3_t v2, const vec2_t t0, const vec2_t t1, const vec2_t t2)
{
	vec3_t          cp, e0, e1;
	vec3_t          faceNormal;

	VectorSet(e0, v1[0] - v0[0], t1[0] - t0[0], t1[1] - t0[1]);
	VectorSet(e1, v2[0] - v0[0], t2[0] - t0[0], t2[1] - t0[1]);

	CrossProduct(e0, e1, cp);
	if(fabs(cp[0]) > 10e-6)
	{
		tangent[0] = -cp[1] / cp[0];
		binormal[0] = -cp[2] / cp[0];
	}

	e0[0] = v1[1] - v0[1];
	e1[0] = v2[1] - v0[1];

	CrossProduct(e0, e1, cp);
	if(fabs(cp[0]) > 10e-6)
	{
		tangent[1] = -cp[1] / cp[0];
		binormal[1] = -cp[2] / cp[0];
	}

	e0[0] = v1[2] - v0[2];
	e1[0] = v2[2] - v0[2];

	CrossProduct(e0, e1, cp);
	if(fabs(cp[0]) > 10e-6)
	{
		tangent[2] = -cp[1] / cp[0];
		binormal[2] = -cp[2] / cp[0];
	}

	VectorNormalizeFast(tangent);
	VectorNormalizeFast(binormal);

	// normal...
	// compute the cross product TxB
	CrossProduct(tangent, binormal, normal);
	VectorNormalizeFast(normal);

	// Gram-Schmidt orthogonalization process for B
	// compute the cross product B=NxT to obtain
	// an orthogonal basis
	CrossProduct(normal, tangent, binormal);

	// compute the face normal based on vertex points
	VectorSubtract(v2, v0, e0);
	VectorSubtract(v1, v0, e1);
	CrossProduct(e0, e1, faceNormal);

	VectorNormalizeFast(faceNormal);

	if(DotProduct(normal, faceNormal) < 0)
	{
		VectorInverse(normal);
		//VectorInverse(tangent);
		//VectorInverse(binormal);
	}
}

/*
=============
R_CalcTangentSpace2
Tr3B - recoded from OverDose
fixes some strange lighting bugs on curves but may mess with negative tangents
=============
*/
void R_CalcTangentSpace2(vec3_t tangent, vec3_t binormal, vec3_t normal,
						 const vec3_t v0, const vec3_t v1, const vec3_t v2, const vec2_t t0, const vec2_t t1, const vec2_t t2)
{
	vec3_t          cross;
	vec_t           e0[5];
	vec_t           e1[5];

	// find edges
	e0[0] = v1[0] - v0[0];
	e0[1] = v1[1] - v0[1];
	e0[2] = v1[2] - v0[2];

	e0[3] = t1[0] - t0[0];
	e0[4] = t1[1] - t0[1];

	e1[0] = v2[0] - v0[0];
	e1[1] = v2[1] - v0[1];
	e1[2] = v2[2] - v0[2];

	e1[3] = t2[0] - t0[0];
	e1[4] = t2[1] - t0[1];

	// compute normal vector
	normal[0] = e1[1] * e0[2] - e1[2] * e0[1];
	normal[1] = e1[2] * e0[0] - e1[0] * e0[2];
	normal[2] = e1[0] * e0[1] - e1[1] * e0[0];

	VectorNormalize(normal);

	// compute tangent vector
	binormal[0] = e1[4] * e0[0] - e1[0] * e0[4];
	binormal[1] = e1[4] * e0[1] - e1[1] * e0[4];
	binormal[2] = e1[4] * e0[2] - e1[2] * e0[4];

	VectorNormalize(binormal);

	// compute binormal vector
	tangent[0] = e1[0] * e0[3] - e1[3] * e0[0];
	tangent[1] = e1[1] * e0[3] - e1[3] * e0[1];
	tangent[2] = e1[2] * e0[3] - e1[3] * e0[2];

	VectorNormalize(tangent);

	// inverse tangent vectors if needed
	CrossProduct(tangent, binormal, cross);
	if(DotProduct(cross, normal) < 0.0f)
	{
		VectorInverse(tangent);
		VectorInverse(binormal);
	}
}


qboolean R_CalcTangentVectors(srfVert_t * dv[3])
{
	int             i;
	float           bb, s, t;
	vec3_t          bary;


	/* calculate barycentric basis for the triangle */
	bb = (dv[1]->st[0] - dv[0]->st[0]) * (dv[2]->st[1] - dv[0]->st[1]) - (dv[2]->st[0] - dv[0]->st[0]) * (dv[1]->st[1] -
																										  dv[0]->st[1]);
	if(fabs(bb) < 0.00000001f)
		return qfalse;

	/* do each vertex */
	for(i = 0; i < 3; i++)
	{
		// calculate s tangent vector
		s = dv[i]->st[0] + 10.0f;
		t = dv[i]->st[1];
		bary[0] = ((dv[1]->st[0] - s) * (dv[2]->st[1] - t) - (dv[2]->st[0] - s) * (dv[1]->st[1] - t)) / bb;
		bary[1] = ((dv[2]->st[0] - s) * (dv[0]->st[1] - t) - (dv[0]->st[0] - s) * (dv[2]->st[1] - t)) / bb;
		bary[2] = ((dv[0]->st[0] - s) * (dv[1]->st[1] - t) - (dv[1]->st[0] - s) * (dv[0]->st[1] - t)) / bb;

		dv[i]->tangent[0] = bary[0] * dv[0]->xyz[0] + bary[1] * dv[1]->xyz[0] + bary[2] * dv[2]->xyz[0];
		dv[i]->tangent[1] = bary[0] * dv[0]->xyz[1] + bary[1] * dv[1]->xyz[1] + bary[2] * dv[2]->xyz[1];
		dv[i]->tangent[2] = bary[0] * dv[0]->xyz[2] + bary[1] * dv[1]->xyz[2] + bary[2] * dv[2]->xyz[2];

		VectorSubtract(dv[i]->tangent, dv[i]->xyz, dv[i]->tangent);
		VectorNormalize(dv[i]->tangent);

		// calculate t tangent vector
		s = dv[i]->st[0];
		t = dv[i]->st[1] + 10.0f;
		bary[0] = ((dv[1]->st[0] - s) * (dv[2]->st[1] - t) - (dv[2]->st[0] - s) * (dv[1]->st[1] - t)) / bb;
		bary[1] = ((dv[2]->st[0] - s) * (dv[0]->st[1] - t) - (dv[0]->st[0] - s) * (dv[2]->st[1] - t)) / bb;
		bary[2] = ((dv[0]->st[0] - s) * (dv[1]->st[1] - t) - (dv[1]->st[0] - s) * (dv[0]->st[1] - t)) / bb;

		dv[i]->binormal[0] = bary[0] * dv[0]->xyz[0] + bary[1] * dv[1]->xyz[0] + bary[2] * dv[2]->xyz[0];
		dv[i]->binormal[1] = bary[0] * dv[0]->xyz[1] + bary[1] * dv[1]->xyz[1] + bary[2] * dv[2]->xyz[1];
		dv[i]->binormal[2] = bary[0] * dv[0]->xyz[2] + bary[1] * dv[1]->xyz[2] + bary[2] * dv[2]->xyz[2];

		VectorSubtract(dv[i]->binormal, dv[i]->xyz, dv[i]->binormal);
		VectorNormalize(dv[i]->binormal);

		// debug code
		//% Sys_FPrintf( SYS_VRB, "%d S: (%f %f %f) T: (%f %f %f)\n", i,
		//%     stv[ i ][ 0 ], stv[ i ][ 1 ], stv[ i ][ 2 ], ttv[ i ][ 0 ], ttv[ i ][ 1 ], ttv[ i ][ 2 ] );
	}

	return qtrue;
}


/*
=================
R_FindSurfaceTriangleWithEdge
Tr3B - recoded from Q2E
=================
*/
static int R_FindSurfaceTriangleWithEdge(int numTriangles, srfTriangle_t * triangles, int start, int end, int ignore)
{
	srfTriangle_t  *tri;
	int             count, match;
	int             i;

	count = 0;
	match = -1;

	for(i = 0, tri = triangles; i < numTriangles; i++, tri++)
	{
		if((tri->indexes[0] == start && tri->indexes[1] == end) ||
		   (tri->indexes[1] == start && tri->indexes[2] == end) || (tri->indexes[2] == start && tri->indexes[0] == end))
		{
			if(i != ignore)
			{
				match = i;
			}

			count++;
		}
		else if((tri->indexes[1] == start && tri->indexes[0] == end) ||
				(tri->indexes[2] == start && tri->indexes[1] == end) || (tri->indexes[0] == start && tri->indexes[2] == end))
		{
			count++;
		}
	}

	// detect edges shared by three triangles and make them seams
	if(count > 2)
	{
		match = -1;
	}

	return match;
}

/*
=================
R_CalcSurfaceTriangleNeighbors
Tr3B - recoded from Q2E
=================
*/
void R_CalcSurfaceTriangleNeighbors(int numTriangles, srfTriangle_t * triangles)
{
	int             i;
	srfTriangle_t  *tri;

	for(i = 0, tri = triangles; i < numTriangles; i++, tri++)
	{
		tri->neighbors[0] = R_FindSurfaceTriangleWithEdge(numTriangles, triangles, tri->indexes[1], tri->indexes[0], i);
		tri->neighbors[1] = R_FindSurfaceTriangleWithEdge(numTriangles, triangles, tri->indexes[2], tri->indexes[1], i);
		tri->neighbors[2] = R_FindSurfaceTriangleWithEdge(numTriangles, triangles, tri->indexes[0], tri->indexes[2], i);
	}
}

/*
=================
R_CalcSurfaceTrianglePlanes
=================
*/
void R_CalcSurfaceTrianglePlanes(int numTriangles, srfTriangle_t * triangles, srfVert_t * verts)
{
	int             i;
	srfTriangle_t  *tri;

	for(i = 0, tri = triangles; i < numTriangles; i++, tri++)
	{
		float          *v1, *v2, *v3;
		vec3_t          d1, d2;

		v1 = verts[tri->indexes[0]].xyz;
		v2 = verts[tri->indexes[1]].xyz;
		v3 = verts[tri->indexes[2]].xyz;

		VectorSubtract(v2, v1, d1);
		VectorSubtract(v3, v1, d2);

		CrossProduct(d2, d1, tri->plane);
		tri->plane[3] = DotProduct(tri->plane, v1);
	}
}

/*
Tr3B: this function breaks the VC9 compiler for some unknown reason ...

float R_CalcFov(float fovX, float width, float height)
{
	static float	x;
	static float	fovY;

	x = width / tan(fovX / 360.0f * M_PI);
	fovY = atan2(height, x);
	fovY = fovY * 360.0f / M_PI;

	return fovY;
}
*/

/*
=================
R_CullLocalBox

Returns CULL_IN, CULL_CLIP, or CULL_OUT
=================
*/
int R_CullLocalBox(vec3_t localBounds[2])
{
#if 0
	int             i, j;
	vec3_t          transformed[8];
	float           dists[8];
	vec3_t          v;
	cplane_t       *frust;
	int             anyBack;
	int             front, back;

	if(r_nocull->integer)
	{
		return CULL_CLIP;
	}

	// transform into world space
	for(i = 0; i < 8; i++)
	{
		v[0] = localBounds[i & 1][0];
		v[1] = localBounds[(i >> 1) & 1][1];
		v[2] = localBounds[(i >> 2) & 1][2];

		R_LocalPointToWorld(v, transformed[i]);
	}

	// check against frustum planes
	anyBack = 0;
	for(i = 0; i < FRUSTUM_PLANES; i++)
	{
		frust = &tr.viewParms.frustum[i];

		front = back = 0;
		for(j = 0; j < 8; j++)
		{
			dists[j] = DotProduct(transformed[j], frust->normal);
			if(dists[j] > frust->dist)
			{
				front = 1;
				if(back)
				{
					break;		// a point is in front
				}
			}
			else
			{
				back = 1;
			}
		}
		if(!front)
		{
			// all points were behind one of the planes
			return CULL_OUT;
		}
		anyBack |= back;
	}

	if(!anyBack)
	{
		return CULL_IN;			// completely inside frustum
	}

	return CULL_CLIP;			// partially clipped
#else
	int             i, j;
	vec3_t          transformed;
	vec3_t          v;
	cplane_t       *frust;
	qboolean        anyClip;
	int             r;
	vec3_t          worldBounds[2];

	if(r_nocull->integer)
	{
		return CULL_CLIP;
	}

	// transform into world space
	ClearBounds(worldBounds[0], worldBounds[1]);

	for(j = 0; j < 8; j++)
	{
		v[0] = localBounds[j & 1][0];
		v[1] = localBounds[(j >> 1) & 1][1];
		v[2] = localBounds[(j >> 2) & 1][2];

		R_LocalPointToWorld(v, transformed);

		AddPointToBounds(transformed, worldBounds[0], worldBounds[1]);
	}

	// check against frustum planes
	anyClip = qfalse;
	for(i = 0; i < FRUSTUM_PLANES; i++)
	{
		frust = &tr.viewParms.frustum[i];

		r = BoxOnPlaneSide(worldBounds[0], worldBounds[1], frust);

		if(r == 2)
		{
			// completely outside frustum
			return CULL_OUT;
		}
		if(r == 3)
		{
			anyClip = qtrue;
		}
	}

	if(!anyClip)
	{
		// completely inside frustum
		return CULL_IN;
	}

	// partially clipped
	return CULL_CLIP;
#endif
}


/*
=================
R_CullLocalPointAndRadius
=================
*/
int R_CullLocalPointAndRadius(vec3_t pt, float radius)
{
	vec3_t          transformed;

	R_LocalPointToWorld(pt, transformed);

	return R_CullPointAndRadius(transformed, radius);
}


/*
=================
R_CullPointAndRadius
=================
*/
int R_CullPointAndRadius(vec3_t pt, float radius)
{
	int             i;
	float           dist;
	cplane_t       *frust;
	qboolean        mightBeClipped = qfalse;

	if(r_nocull->integer)
	{
		return CULL_CLIP;
	}

	// check against frustum planes
	for(i = 0; i < FRUSTUM_PLANES; i++)
	{
		frust = &tr.viewParms.frustum[i];

		dist = DotProduct(pt, frust->normal) - frust->dist;
		if(dist < -radius)
		{
			return CULL_OUT;
		}
		else if(dist <= radius)
		{
			mightBeClipped = qtrue;
		}
	}

	if(mightBeClipped)
	{
		return CULL_CLIP;
	}

	return CULL_IN;				// completely inside frustum
}


/*
=================
R_LocalNormalToWorld
=================
*/
void R_LocalNormalToWorld(const vec3_t local, vec3_t world)
{
	MatrixTransformNormal(tr.orientation.transformMatrix, local, world);
}

/*
=================
R_LocalPointToWorld
=================
*/
void R_LocalPointToWorld(const vec3_t local, vec3_t world)
{
	MatrixTransformPoint(tr.orientation.transformMatrix, local, world);
}


/*
==========================
R_TransformWorldToClip
==========================
*/
void R_TransformWorldToClip(const vec3_t src, const float *cameraViewMatrix, const float *projectionMatrix, vec4_t eye,
							vec4_t dst)
{
	vec4_t          src2;

	VectorCopy(src, src2);
	src2[3] = 1;

	MatrixTransform4(cameraViewMatrix, src2, eye);
	MatrixTransform4(projectionMatrix, eye, dst);
}

/*
==========================
R_TransformModelToClip
==========================
*/
void R_TransformModelToClip(const vec3_t src, const float *modelViewMatrix, const float *projectionMatrix, vec4_t eye, vec4_t dst)
{
	vec4_t          src2;

	VectorCopy(src, src2);
	src2[3] = 1;

	MatrixTransform4(modelViewMatrix, src2, eye);
	MatrixTransform4(projectionMatrix, eye, dst);
}

/*
==========================
R_TransformClipToWindow
==========================
*/
void R_TransformClipToWindow(const vec4_t clip, const viewParms_t * view, vec4_t normalized, vec4_t window)
{
	normalized[0] = clip[0] / clip[3];
	normalized[1] = clip[1] / clip[3];
	normalized[2] = (clip[2] + clip[3]) / (2 * clip[3]);

	window[0] = view->viewportX + (0.5f * (1.0f + normalized[0]) * view->viewportWidth);
	window[1] = view->viewportY + (0.5f * (1.0f + normalized[1]) * view->viewportHeight);
	window[2] = normalized[2];

	window[0] = (int)(window[0] + 0.5);
	window[1] = (int)(window[1] + 0.5);
}

/*
================
R_ProjectRadius
================
*/
float R_ProjectRadius(float r, vec3_t location)
{
	float           pr;
	float           dist;
	float           c;
	vec3_t          p;
	float           projected[4];

	c = DotProduct(tr.viewParms.orientation.axis[0], tr.viewParms.orientation.origin);
	dist = DotProduct(tr.viewParms.orientation.axis[0], location) - c;

	if(dist <= 0)
		return 0;

	p[0] = 0;
	p[1] = fabs(r);
	p[2] = -dist;

	projected[0] = p[0] * tr.viewParms.projectionMatrix[0] +
		p[1] * tr.viewParms.projectionMatrix[4] + p[2] * tr.viewParms.projectionMatrix[8] + tr.viewParms.projectionMatrix[12];

	projected[1] = p[0] * tr.viewParms.projectionMatrix[1] +
		p[1] * tr.viewParms.projectionMatrix[5] + p[2] * tr.viewParms.projectionMatrix[9] + tr.viewParms.projectionMatrix[13];

	projected[2] = p[0] * tr.viewParms.projectionMatrix[2] +
		p[1] * tr.viewParms.projectionMatrix[6] + p[2] * tr.viewParms.projectionMatrix[10] + tr.viewParms.projectionMatrix[14];

	projected[3] = p[0] * tr.viewParms.projectionMatrix[3] +
		p[1] * tr.viewParms.projectionMatrix[7] + p[2] * tr.viewParms.projectionMatrix[11] + tr.viewParms.projectionMatrix[15];


	pr = projected[1] / projected[3];

	if(pr > 1.0f)
		pr = 1.0f;

	return pr;
}

/*
=================
R_SetupEntityWorldBounds
Tr3B - needs R_RotateEntityForViewParms
=================
*/
void R_SetupEntityWorldBounds(trRefEntity_t * ent)
{
	int             j;
	vec3_t          v;

	ClearBounds(ent->worldBounds[0], ent->worldBounds[1]);

	for(j = 0; j < 8; j++)
	{
		v[0] = ent->localBounds[j & 1][0];
		v[1] = ent->localBounds[(j >> 1) & 1][1];
		v[2] = ent->localBounds[(j >> 2) & 1][2];

		// transform local bounds vertices into world space
		R_LocalPointToWorld(v, ent->worldCorners[j]);

		AddPointToBounds(ent->worldCorners[j], ent->worldBounds[0], ent->worldBounds[1]);
	}
}


/*
=================
R_RotateEntityForViewParms

Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateEntityForViewParms(const trRefEntity_t * ent, const viewParms_t * viewParms, orientationr_t * or)
{
	vec3_t          delta;
	float           axisLength;

	if(ent->e.reType != RT_MODEL)
	{
		*or = viewParms->world;
		return;
	}

	VectorCopy(ent->e.origin, or->origin);

	VectorCopy(ent->e.axis[0], or->axis[0]);
	VectorCopy(ent->e.axis[1], or->axis[1]);
	VectorCopy(ent->e.axis[2], or->axis[2]);

	MatrixSetupTransform(or->transformMatrix, or->axis[0], or->axis[1], or->axis[2], or->origin);
	MatrixAffineInverse(or->transformMatrix, or->viewMatrix);
	MatrixMultiply(viewParms->world.viewMatrix, or->transformMatrix, or->modelViewMatrix);

	// calculate the viewer origin in the model's space
	// needed for fog, specular, and environment mapping
	VectorSubtract(viewParms->orientation.origin, or->origin, delta);

	// compensate for scale in the axes if necessary
	if(ent->e.nonNormalizedAxes)
	{
		axisLength = VectorLength(ent->e.axis[0]);
		if(!axisLength)
		{
			axisLength = 0;
		}
		else
		{
			axisLength = 1.0f / axisLength;
		}
	}
	else
	{
		axisLength = 1.0f;
	}

	or->viewOrigin[0] = DotProduct(delta, or->axis[0]) * axisLength;
	or->viewOrigin[1] = DotProduct(delta, or->axis[1]) * axisLength;
	or->viewOrigin[2] = DotProduct(delta, or->axis[2]) * axisLength;
}


/*
=================
R_RotateEntityForLight

Generates an orientation for an entity and light
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateEntityForLight(const trRefEntity_t * ent, const trRefLight_t * light, orientationr_t * or)
{
	vec3_t          delta;
	float           axisLength;

	if(ent->e.reType != RT_MODEL)
	{
		Com_Memset(or, 0, sizeof(*or));

		or->axis[0][0] = 1;
		or->axis[1][1] = 1;
		or->axis[2][2] = 1;

		VectorCopy(light->l.origin, or->viewOrigin);

		MatrixIdentity(or->transformMatrix);
		//MatrixAffineInverse(or->transformMatrix, or->viewMatrix);
		MatrixMultiply(light->viewMatrix, or->transformMatrix, or->viewMatrix);
		MatrixCopy(or->viewMatrix, or->modelViewMatrix);
		return;
	}

	VectorCopy(ent->e.origin, or->origin);

	VectorCopy(ent->e.axis[0], or->axis[0]);
	VectorCopy(ent->e.axis[1], or->axis[1]);
	VectorCopy(ent->e.axis[2], or->axis[2]);

	MatrixSetupTransform(or->transformMatrix, or->axis[0], or->axis[1], or->axis[2], or->origin);
	MatrixAffineInverse(or->transformMatrix, or->viewMatrix);

	MatrixMultiply(light->viewMatrix, or->transformMatrix, or->modelViewMatrix);

	// calculate the viewer origin in the model's space
	// needed for fog, specular, and environment mapping
	VectorSubtract(light->l.origin, or->origin, delta);

	// compensate for scale in the axes if necessary
	if(ent->e.nonNormalizedAxes)
	{
		axisLength = VectorLength(ent->e.axis[0]);
		if(!axisLength)
		{
			axisLength = 0;
		}
		else
		{
			axisLength = 1.0f / axisLength;
		}
	}
	else
	{
		axisLength = 1.0f;
	}

	or->viewOrigin[0] = DotProduct(delta, or->axis[0]) * axisLength;
	or->viewOrigin[1] = DotProduct(delta, or->axis[1]) * axisLength;
	or->viewOrigin[2] = DotProduct(delta, or->axis[2]) * axisLength;
}

/*
=================
R_RotateLightForViewParms
=================
*/
void R_RotateLightForViewParms(const trRefLight_t * light, const viewParms_t * viewParms, orientationr_t * or)
{
	vec3_t          delta;

	VectorCopy(light->l.origin, or->origin);

	QuatToAxis(light->l.rotation, or->axis);

	MatrixSetupTransform(or->transformMatrix, or->axis[0], or->axis[1], or->axis[2], or->origin);
	MatrixAffineInverse(or->transformMatrix, or->viewMatrix);
	MatrixMultiply(viewParms->world.viewMatrix, or->transformMatrix, or->modelViewMatrix);

	// calculate the viewer origin in the light's space
	// needed for fog, specular, and environment mapping
	VectorSubtract(viewParms->orientation.origin, or->origin, delta);

	or->viewOrigin[0] = DotProduct(delta, or->axis[0]);
	or->viewOrigin[1] = DotProduct(delta, or->axis[1]);
	or->viewOrigin[2] = DotProduct(delta, or->axis[2]);
}


/*
=================
R_RotateForViewer

Sets up the modelview matrix for a given viewParm
=================
*/
void R_RotateForViewer(void)
{
	matrix_t        transformMatrix;

//  matrix_t        viewMatrix;

	Com_Memset(&tr.orientation, 0, sizeof(tr.orientation));
	tr.orientation.axis[0][0] = 1;
	tr.orientation.axis[1][1] = 1;
	tr.orientation.axis[2][2] = 1;
	VectorCopy(tr.viewParms.orientation.origin, tr.orientation.viewOrigin);

	// transform by the camera placement
	MatrixSetupTransform(transformMatrix,
						 tr.viewParms.orientation.axis[0], tr.viewParms.orientation.axis[1], tr.viewParms.orientation.axis[2], tr.viewParms.orientation.origin);

	MatrixAffineInverse(transformMatrix, tr.orientation.viewMatrix2);
//  MatrixAffineInverse(transformMatrix, tr.orientation.viewMatrix);

	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	MatrixIdentity(tr.orientation.transformMatrix);
	MatrixMultiply(quakeToOpenGLMatrix, tr.orientation.viewMatrix2, tr.orientation.viewMatrix);
	MatrixCopy(tr.orientation.viewMatrix, tr.orientation.modelViewMatrix);

	tr.viewParms.world = tr.orientation;
}

/*
===============
R_SetupProjection
===============
*/
// *INDENT-OFF*
void R_SetupProjection(void)
{
	float           xMin, xMax, yMin, yMax;
	float           width, height, depth;
	float           zNear, zFar;

//	matrix_t        proj;
	float          *proj = tr.viewParms.projectionMatrix;

	// set up projection matrix
	zNear = r_znear->value;
	zFar = r_zfar->value;

//	if(r_shadows->integer == 3)
//		zFar = 0;

	yMax = zNear * tan(tr.refdef.fov_y * M_PI / 360.0f);
	yMin = -yMax;

	xMax = zNear * tan(tr.refdef.fov_x * M_PI / 360.0f);
	xMin = -xMax;

	width = xMax - xMin;
	height = yMax - yMin;
	depth = zFar - zNear;

	if(zFar <= 0)
	{
		// Tr3B - far plane at infinity, see RobustShadowVolumes.pdf by Nvidia
		proj[0] = 2 * zNear / width;	proj[4] = 0;					proj[8] = (xMax + xMin) / width;	proj[12] = 0;
		proj[1] = 0;					proj[5] = 2 * zNear / height;	proj[9] = (yMax + yMin) / height;	proj[13] = 0;
		proj[2] = 0;					proj[6] = 0;					proj[10] = -1;						proj[14] = -2 * zNear;
		proj[3] = 0;					proj[7] = 0;					proj[11] = -1;						proj[15] = 0;
	}
	else
	{
		// standard OpenGL projection matrix
		proj[0] = 2 * zNear / width;	proj[4] = 0;					proj[8] = (xMax + xMin) / width;	proj[12] = 0;
		proj[1] = 0;					proj[5] = 2 * zNear / height;	proj[9] = (yMax + yMin) / height;	proj[13] = 0;
		proj[2] = 0;					proj[6] = 0;					proj[10] = -(zFar + zNear) / depth;	proj[14] = -2 * zFar * zNear / depth;
		proj[3] = 0;					proj[7] = 0;					proj[11] = -1;						proj[15] = 0;
	}

	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
//	MatrixMultiply(proj, quakeToOpenGLMatrix, tr.viewParms.projectionMatrix);
}
// *INDENT-ON*

/*
=================
R_SetupUnprojection
create a matrix with similar functionality like gluUnproject, project from window space to world space
=================
*/
void R_SetupUnprojection(void)
{
	float          *unprojectMatrix = tr.viewParms.unprojectionMatrix;

	MatrixCopy(tr.viewParms.projectionMatrix, unprojectMatrix);
	MatrixMultiply2(unprojectMatrix, quakeToOpenGLMatrix);
	MatrixMultiply2(unprojectMatrix, tr.viewParms.world.viewMatrix2);
	MatrixInverse(unprojectMatrix);
	MatrixMultiplyTranslation(unprojectMatrix, -1.0, -1.0, -1.0);
	MatrixMultiplyScale(unprojectMatrix, 2.0 * Q_recip((float)glConfig.vidWidth), 2.0 * Q_recip((float)glConfig.vidHeight), 2.0);
}

/*
=================
R_SetupFrustum

Setup that culling frustum planes for the current view
=================
*/
// *INDENT-OFF*
void R_SetupFrustum(frustum_t frustum, const float *modelViewMatrix, const float *projectionMatrix)
{
	// http://www2.ravensoft.com/users/ggribb/plane%20extraction.pdf
	int				i;
	matrix_t        m;

	MatrixMultiply(projectionMatrix, modelViewMatrix, m);

	// left
	frustum[FRUSTUM_LEFT].normal[0]		=  m[ 3] + m[ 0];
	frustum[FRUSTUM_LEFT].normal[1]		=  m[ 7] + m[ 4];
	frustum[FRUSTUM_LEFT].normal[2]		=  m[11] + m[ 8];
	frustum[FRUSTUM_LEFT].dist			=-(m[15] + m[12]);

	// right
	frustum[FRUSTUM_RIGHT].normal[0]	=  m[ 3] - m[ 0];
	frustum[FRUSTUM_RIGHT].normal[1]	=  m[ 7] - m[ 4];
	frustum[FRUSTUM_RIGHT].normal[2]	=  m[11] - m[ 8];
	frustum[FRUSTUM_RIGHT].dist			=-(m[15] - m[12]);

	// bottom
	frustum[FRUSTUM_BOTTOM].normal[0]	=  m[ 3] + m[ 1];
	frustum[FRUSTUM_BOTTOM].normal[1]	=  m[ 7] + m[ 5];
	frustum[FRUSTUM_BOTTOM].normal[2]	=  m[11] + m[ 9];
	frustum[FRUSTUM_BOTTOM].dist		=-(m[15] + m[13]);

	// top
	frustum[FRUSTUM_TOP].normal[0]		=  m[ 3] - m[ 1];
	frustum[FRUSTUM_TOP].normal[1]		=  m[ 7] - m[ 5];
	frustum[FRUSTUM_TOP].normal[2]		=  m[11] - m[ 9];
	frustum[FRUSTUM_TOP].dist			=-(m[15] - m[13]);

	// near
	frustum[FRUSTUM_NEAR].normal[0]		=  m[ 3] + m[ 2];
	frustum[FRUSTUM_NEAR].normal[1]		=  m[ 7] + m[ 6];
	frustum[FRUSTUM_NEAR].normal[2]		=  m[11] + m[10];
	frustum[FRUSTUM_NEAR].dist			=-(m[15] + m[14]);

	// far
	frustum[FRUSTUM_FAR].normal[0]		=  m[ 3] - m[ 2];
	frustum[FRUSTUM_FAR].normal[1]		=  m[ 7] - m[ 6];
	frustum[FRUSTUM_FAR].normal[2]		=  m[11] - m[10];
	frustum[FRUSTUM_FAR].dist			=-(m[15] - m[14]);

	for(i = 0; i < 6; i++)
	{
		vec_t           length, ilength;

		frustum[i].type = PLANE_NON_AXIAL;

		// normalize
		length = VectorLength(frustum[i].normal);
		if(length)
		{
			ilength = 1.0 / length;
			frustum[i].normal[0] *= ilength;
			frustum[i].normal[1] *= ilength;
			frustum[i].normal[2] *= ilength;
			frustum[i].dist *= ilength;
		}

		SetPlaneSignbits(&frustum[i]);
	}
}
// *INDENT-ON*


/*
=================
R_MirrorPoint
=================
*/
void R_MirrorPoint(vec3_t in, orientation_t * surface, orientation_t * camera, vec3_t out)
{
	int             i;
	vec3_t          local;
	vec3_t          transformed;
	float           d;

	VectorSubtract(in, surface->origin, local);

	VectorClear(transformed);
	for(i = 0; i < 3; i++)
	{
		d = DotProduct(local, surface->axis[i]);
		VectorMA(transformed, d, camera->axis[i], transformed);
	}

	VectorAdd(transformed, camera->origin, out);
}

void R_MirrorVector(vec3_t in, orientation_t * surface, orientation_t * camera, vec3_t out)
{
	int             i;
	float           d;

	VectorClear(out);
	for(i = 0; i < 3; i++)
	{
		d = DotProduct(in, surface->axis[i]);
		VectorMA(out, d, camera->axis[i], out);
	}
}



/*
=============
R_PlaneForSurface
=============
*/
void R_PlaneForSurface(surfaceType_t * surfType, cplane_t * plane)
{
	srfTriangles_t *tri;
	srfPoly_t      *poly;
	srfVert_t      *v1, *v2, *v3;
	vec4_t          plane4;

	if(!surfType)
	{
		Com_Memset(plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
	switch (*surfType)
	{
		case SF_FACE:
			*plane = ((srfSurfaceFace_t *) surfType)->plane;
			return;
		case SF_TRIANGLES:
			tri = (srfTriangles_t *) surfType;
			v1 = tri->verts + tri->triangles[0].indexes[0];
			v2 = tri->verts + tri->triangles[0].indexes[1];
			v3 = tri->verts + tri->triangles[0].indexes[2];
			PlaneFromPoints(plane4, v1->xyz, v2->xyz, v3->xyz, qtrue);
			VectorCopy(plane4, plane->normal);
			plane->dist = plane4[3];
			return;
		case SF_POLY:
			poly = (srfPoly_t *) surfType;
			PlaneFromPoints(plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz, qtrue);
			VectorCopy(plane4, plane->normal);
			plane->dist = plane4[3];
			return;
		default:
			Com_Memset(plane, 0, sizeof(*plane));
			plane->normal[0] = 1;
			return;
	}
}


/*
=================
R_GetPortalOrientation

entityNum is the entity that the portal surface is a part of, which may
be moving and rotating.

Returns qtrue if it should be mirrored
=================
*/
static qboolean R_GetPortalOrientations(drawSurf_t * drawSurf, orientation_t * surface, orientation_t * camera, vec3_t pvsOrigin,
										qboolean * mirror)
{
	int             i;
	cplane_t        originalPlane, plane;
	trRefEntity_t  *e;
	float           d;
	vec3_t          transformed;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface(drawSurf->surface, &originalPlane);

	// rotate the plane if necessary
	if(drawSurf->entity != &tr.worldEntity)
	{
		tr.currentEntity = drawSurf->entity;

		// get the orientation of the entity
		R_RotateEntityForViewParms(tr.currentEntity, &tr.viewParms, &tr.orientation);

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld(originalPlane.normal, plane.normal);
		plane.dist = originalPlane.dist + DotProduct(plane.normal, tr.orientation.origin);

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct(originalPlane.normal, tr.orientation.origin);
	}
	else
	{
		plane = originalPlane;
	}

	VectorCopy(plane.normal, surface->axis[0]);
	PerpendicularVector(surface->axis[1], surface->axis[0]);
	CrossProduct(surface->axis[0], surface->axis[1], surface->axis[2]);

#if 0
	// Doom3 style mirror support
	shader = tr.sortedShaders[drawSurf->shaderNum];
	if(shader->isMirror)
	{
		//ri.Printf(PRINT_ALL, "Portal surface with a mirror\n");

		VectorScale(plane.normal, plane.dist, surface->origin);
		VectorCopy(surface->origin, camera->origin);
		VectorSubtract(vec3_origin, surface->axis[0], camera->axis[0]);
		VectorCopy(surface->axis[1], camera->axis[1]);
		VectorCopy(surface->axis[2], camera->axis[2]);

		*mirror = qtrue;
		return qtrue;
	}
#endif

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for(i = 0; i < tr.refdef.numEntities; i++)
	{
		e = &tr.refdef.entities[i];
		if(e->e.reType != RT_PORTALSURFACE)
		{
			continue;
		}

		d = DotProduct(e->e.origin, originalPlane.normal) - originalPlane.dist;
		if(d > 64 || d < -64)
		{
			continue;
		}

		// get the pvsOrigin from the entity
		VectorCopy(e->e.oldorigin, pvsOrigin);

		// if the entity is just a mirror, don't use as a camera point
		if(e->e.oldorigin[0] == e->e.origin[0] && e->e.oldorigin[1] == e->e.origin[1] && e->e.oldorigin[2] == e->e.origin[2])
		{
			//ri.Printf(PRINT_DEVELOPER, "Portal surface with a mirror entity\n");

			VectorScale(plane.normal, plane.dist, surface->origin);
			VectorCopy(surface->origin, camera->origin);
			VectorSubtract(vec3_origin, surface->axis[0], camera->axis[0]);
			VectorCopy(surface->axis[1], camera->axis[1]);
			VectorCopy(surface->axis[2], camera->axis[2]);

			*mirror = qtrue;
			return qtrue;
		}

		// project the origin onto the surface plane to get
		// an origin point we can rotate around
		d = DotProduct(e->e.origin, plane.normal) - plane.dist;
		VectorMA(e->e.origin, -d, surface->axis[0], surface->origin);

		// now get the camera origin and orientation
		VectorCopy(e->e.oldorigin, camera->origin);
		AxisCopy(e->e.axis, camera->axis);
		VectorSubtract(vec3_origin, camera->axis[0], camera->axis[0]);
		VectorSubtract(vec3_origin, camera->axis[1], camera->axis[1]);

		// optionally rotate
		if(e->e.oldframe)
		{
			// if a speed is specified
			if(e->e.frame)
			{
				// continuous rotate
				d = (tr.refdef.time / 1000.0f) * e->e.frame;
				VectorCopy(camera->axis[1], transformed);
				RotatePointAroundVector(camera->axis[1], camera->axis[0], transformed, d);
				CrossProduct(camera->axis[0], camera->axis[1], camera->axis[2]);
			}
			else
			{
				// bobbing rotate, with skinNum being the rotation offset
				d = sin(tr.refdef.time * 0.003f);
				d = e->e.skinNum + d * 4;
				VectorCopy(camera->axis[1], transformed);
				RotatePointAroundVector(camera->axis[1], camera->axis[0], transformed, d);
				CrossProduct(camera->axis[0], camera->axis[1], camera->axis[2]);
			}
		}
		else if(e->e.skinNum)
		{
			d = e->e.skinNum;
			VectorCopy(camera->axis[1], transformed);
			RotatePointAroundVector(camera->axis[1], camera->axis[0], transformed, d);
			CrossProduct(camera->axis[0], camera->axis[1], camera->axis[2]);
		}
		*mirror = qfalse;
		return qtrue;
	}

	// if we didn't locate a portal entity, don't render anything.
	// We don't want to just treat it as a mirror, because without a
	// portal entity the server won't have communicated a proper entity set
	// in the snapshot

	// unfortunately, with local movement prediction it is easily possible
	// to see a surface before the server has communicated the matching
	// portal surface entity, so we don't want to print anything here...

	//ri.Printf( PRINT_ALL, "Portal surface without a portal entity\n" );

	return qfalse;
}

static qboolean IsMirror(const drawSurf_t * drawSurf)
{
	int             i;
	cplane_t        originalPlane, plane;
	trRefEntity_t  *e;
	float           d;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface(drawSurf->surface, &originalPlane);

	// rotate the plane if necessary
	if(tr.currentEntity != &tr.worldEntity)
	{
		// get the orientation of the entity
		R_RotateEntityForViewParms(tr.currentEntity, &tr.viewParms, &tr.orientation);

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld(originalPlane.normal, plane.normal);
		plane.dist = originalPlane.dist + DotProduct(plane.normal, tr.orientation.origin);

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct(originalPlane.normal, tr.orientation.origin);
	}
	else
	{
		plane = originalPlane;
	}

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for(i = 0; i < tr.refdef.numEntities; i++)
	{
		e = &tr.refdef.entities[i];
		if(e->e.reType != RT_PORTALSURFACE)
		{
			continue;
		}

		d = DotProduct(e->e.origin, originalPlane.normal) - originalPlane.dist;
		if(d > 64 || d < -64)
		{
			continue;
		}

		// if the entity is just a mirror, don't use as a camera point
		if(e->e.oldorigin[0] == e->e.origin[0] && e->e.oldorigin[1] == e->e.origin[1] && e->e.oldorigin[2] == e->e.origin[2])
		{
			return qtrue;
		}

		return qfalse;
	}
	return qfalse;
}

/*
** SurfIsOffscreen
**
** Determines if a surface is completely offscreen.
*/
static qboolean SurfIsOffscreen(const drawSurf_t * drawSurf, vec4_t clipDest[128])
{
	float           shortest = 100000000;
	shader_t       *shader;
	int             numTriangles;
	vec4_t          clip, eye;
	int             i;
	unsigned int    pointOr = 0;
	unsigned int    pointAnd = (unsigned int)~0;

	if(glConfig.smpActive)
	{
		// FIXME!  we can't do Tess_Begin/Tess_End stuff with smp!
		return qfalse;
	}

	tr.currentEntity = drawSurf->entity;
	shader = tr.sortedShaders[drawSurf->shaderNum];

	// rotate if necessary
	if(tr.currentEntity != &tr.worldEntity)
	{
		//ri.Printf(PRINT_ALL, "entity Portal surface\n");
		R_RotateEntityForViewParms(tr.currentEntity, &tr.viewParms, &tr.orientation);
	}
	else
	{
		tr.orientation = tr.viewParms.world;
	}

	Tess_Begin(Tess_StageIteratorGeneric, shader, NULL, qfalse, qfalse, -1);
	rb_surfaceTable[*drawSurf->surface] (drawSurf->surface);

	// Tr3B: former assertion
	if(tess.numVertexes >= 128)
	{
		return qfalse;
	}

	for(i = 0; i < tess.numVertexes; i++)
	{
		int             j;
		unsigned int    pointFlags = 0;

		R_TransformModelToClip(tess.xyz[i], tr.orientation.modelViewMatrix, tr.viewParms.projectionMatrix, eye, clip);

		for(j = 0; j < 3; j++)
		{
			if(clip[j] >= clip[3])
			{
				pointFlags |= (1 << (j * 2));
			}
			else if(clip[j] <= -clip[3])
			{
				pointFlags |= (1 << (j * 2 + 1));
			}
		}
		pointAnd &= pointFlags;
		pointOr |= pointFlags;
	}

	// trivially reject
	if(pointAnd)
	{
		return qtrue;
	}

	// determine if this surface is backfaced and also determine the distance
	// to the nearest vertex so we can cull based on portal range.  Culling
	// based on vertex distance isn't 100% correct (we should be checking for
	// range to the surface), but it's good enough for the types of portals
	// we have in the game right now.
	numTriangles = tess.numIndexes / 3;

	for(i = 0; i < tess.numIndexes; i += 3)
	{
		vec3_t          normal;
		float           dot;
		float           len;

		VectorSubtract(tess.xyz[tess.indexes[i]], tr.orientation.viewOrigin, normal);

		len = VectorLengthSquared(normal);	// lose the sqrt
		if(len < shortest)
		{
			shortest = len;
		}

		if((dot = DotProduct(normal, tess.normals[tess.indexes[i]])) >= 0)
		{
			numTriangles--;
		}
	}
	if(!numTriangles)
	{
		//ri.Printf(PRINT_ALL, "entity Portal surface triangles culled\n");
		return qtrue;
	}

	// mirrors can early out at this point, since we don't do a fade over distance
	// with them (although we could)
	if(IsMirror(drawSurf))
	{
		return qfalse;
	}

	if(shortest > (tess.surfaceShader->portalRange * tess.surfaceShader->portalRange))
	{
		return qtrue;
	}

	return qfalse;
}


/*
========================
R_MirrorViewBySurface

Returns qtrue if another view has been rendered
========================
*/
static qboolean R_MirrorViewBySurface(drawSurf_t * drawSurf)
{
	vec4_t          clipDest[128];
	viewParms_t     newParms;
	viewParms_t     oldParms;
	orientation_t   surface, camera;

	// don't recursively mirror
	if(tr.viewParms.isPortal)
	{
		ri.Printf(PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n");
		return qfalse;
	}

	if(r_noportals->integer)
	{
		return qfalse;
	}

	// trivially reject portal/mirror
	if(SurfIsOffscreen(drawSurf, clipDest))
	{
		return qfalse;
	}

	// save old viewParms so we can return to it after the mirror view
	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.isPortal = qtrue;
	if(!R_GetPortalOrientations(drawSurf, &surface, &camera, newParms.pvsOrigin, &newParms.isMirror))
	{
		return qfalse;			// bad portal, no portalentity
	}

	R_MirrorPoint(oldParms.orientation.origin, &surface, &camera, newParms.orientation.origin);

	VectorSubtract(vec3_origin, camera.axis[0], newParms.portalPlane.normal);
	newParms.portalPlane.dist = DotProduct(camera.origin, newParms.portalPlane.normal);

	R_MirrorVector(oldParms.orientation.axis[0], &surface, &camera, newParms.orientation.axis[0]);
	R_MirrorVector(oldParms.orientation.axis[1], &surface, &camera, newParms.orientation.axis[1]);
	R_MirrorVector(oldParms.orientation.axis[2], &surface, &camera, newParms.orientation.axis[2]);

	// OPTIMIZE: restrict the viewport on the mirrored view

	// render the mirror view
	R_RenderView(&newParms);

	tr.viewParms = oldParms;

	return qtrue;
}


/*
=================
R_AddDrawSurf
=================
*/
void R_AddDrawSurf(surfaceType_t * surface, shader_t * shader, int lightmapNum)
{
	int             index;
	drawSurf_t     *drawSurf;

	// instead of checking for overflow, we just mask the index
	// so it wraps around
	index = tr.refdef.numDrawSurfs & DRAWSURF_MASK;

	drawSurf = &tr.refdef.drawSurfs[index];

	drawSurf->entity = tr.currentEntity;
	drawSurf->surface = surface;
	drawSurf->shaderNum = shader->sortedIndex;
	drawSurf->lightmapNum = lightmapNum;

	tr.refdef.numDrawSurfs++;
}

/*
=================
DrawSurfCompare
compare function for qsort()
=================
*/
static int DrawSurfCompare(const void *a, const void *b)
{
#if 1
	// by shader
	if(((drawSurf_t *) a)->shaderNum < ((drawSurf_t *) b)->shaderNum)
		return -1;

	else if(((drawSurf_t *) a)->shaderNum > ((drawSurf_t *) b)->shaderNum)
		return 1;
#endif

#if 1
	// by lightmap
	if(((drawSurf_t *) a)->lightmapNum < ((drawSurf_t *) b)->lightmapNum)
		return -1;

	else if(((drawSurf_t *) a)->lightmapNum > ((drawSurf_t *) b)->lightmapNum)
		return 1;
#endif

#if 1
	// by entity
	if(((drawSurf_t *) a)->entity == &tr.worldEntity && ((drawSurf_t *) b)->entity != &tr.worldEntity)
		return -1;

	else if(((drawSurf_t *) a)->entity != &tr.worldEntity && ((drawSurf_t *) b)->entity == &tr.worldEntity)
		return 1;

	else if(((drawSurf_t *) a)->entity < ((drawSurf_t *) b)->entity)
		return -1;

	else if(((drawSurf_t *) a)->entity > ((drawSurf_t *) b)->entity)
		return 1;
#endif

	return 0;
}


/*
=================
R_SortDrawSurfs
=================
*/
static void R_SortDrawSurfs()
{
	drawSurf_t     *drawSurf;
	shader_t       *shader;
	int             i;

	// it is possible for some views to not have any surfaces
	if(tr.viewParms.numDrawSurfs < 1)
	{
		// we still need to add it for hyperspace cases
		R_AddDrawViewCmd();
		return;
	}

	// if we overflowed MAX_DRAWSURFS, the drawsurfs
	// wrapped around in the buffer and we will be missing
	// the first surfaces, not the last ones
	if(tr.viewParms.numDrawSurfs > MAX_DRAWSURFS)
	{
		tr.viewParms.numDrawSurfs = MAX_DRAWSURFS;
	}

	// if we overflowed MAX_INTERACTIONS, the interactions
	// wrapped around in the buffer and we will be missing
	// the first interactions, not the last ones
	if(tr.viewParms.numInteractions > MAX_INTERACTIONS)
	{
		interaction_t  *ia;

		tr.viewParms.numInteractions = MAX_INTERACTIONS;

		// reset last interaction's next pointer
		ia = &tr.viewParms.interactions[tr.viewParms.numInteractions - 1];
		ia->next = NULL;
	}

	// sort the drawsurfs by sort type, then orientation, then shader
//  qsortFast(drawSurfs, numDrawSurfs, sizeof(drawSurf_t));
	qsort(tr.viewParms.drawSurfs, tr.viewParms.numDrawSurfs, sizeof(drawSurf_t), DrawSurfCompare);

	// check for any pass through drawing, which
	// may cause another view to be rendered first
	for(i = 0, drawSurf = tr.viewParms.drawSurfs; i < tr.viewParms.numDrawSurfs; i++, drawSurf++)
	{
		shader = tr.sortedShaders[drawSurf->shaderNum];

		/*
		   if(shader->sort > SS_PORTAL && !shader->isMirror)
		   {
		   break;
		   }
		 */

		if(!shader->isPortal)
		{
			continue;
		}

		//ri.Printf(PRINT_ALL, "portal or mirror surface\n");

		// no shader should ever have this sort type
		if(shader->sort == SS_BAD)
		{
			ri.Error(ERR_DROP, "Shader '%s'with sort == SS_BAD", shader->name);
		}

		// if the mirror was completely clipped away, we may need to check another surface
		if(R_MirrorViewBySurface(drawSurf))
		{
			// this is a debug option to see exactly what is being mirrored
			if(r_portalOnly->integer)
			{
				return;
			}
			break;				// only one mirror view at a time
		}
	}

	// tell renderer backend to render this view
	R_AddDrawViewCmd();
}


/*
=============
R_AddEntitySurfaces
=============
*/
void R_AddEntitySurfaces(void)
{
	int             i;
	trRefEntity_t  *ent;
	shader_t       *shader;

	if(!r_drawentities->integer)
	{
		return;
	}

	for(i = 0; i < tr.refdef.numEntities; i++)
	{
		ent = tr.currentEntity = &tr.refdef.entities[i];

		//
		// the weapon model must be handled special --
		// we don't want the hacked weapon position showing in
		// mirrors, because the true body position will already be drawn
		//
		if((ent->e.renderfx & RF_FIRST_PERSON) && (tr.viewParms.isPortal || tr.viewParms.isMirror))
		{
			continue;
		}

		// determine if we need zfail algorithm instead of zpass
#if 0
		if(ent->e.renderfx & RF_THIRD_PERSON)
		{
			if(r_shadows->integer == 3 && tr.viewParms.isPortal)
			{
				ent->needZFail = qtrue;
			}
		}
		else
		{
			ent->needZFail = qtrue;
		}
#else
		ent->needZFail = qtrue;
#endif

		// simple generated models, like sprites and beams, are not culled
		switch (ent->e.reType)
		{
			case RT_PORTALSURFACE:
				break;			// don't draw anything
			case RT_SPRITE:
			case RT_BEAM:
			case RT_LIGHTNING:
			case RT_RAIL_CORE:
			case RT_RAIL_RINGS:
				// self blood sprites, talk balloons, etc should not be drawn in the primary
				// view.  We can't just do this check for all entities, because md3
				// entities may still want to cast shadows from them
				if((ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal)
				{
					continue;
				}
				shader = R_GetShaderByHandle(ent->e.customShader);
				R_AddDrawSurf(&entitySurface, shader, -1);
				break;

			case RT_MODEL:
				// we must set up parts of tr.or for model culling
				R_RotateEntityForViewParms(ent, &tr.viewParms, &tr.orientation);

				tr.currentModel = R_GetModelByHandle(ent->e.hModel);
				if(!tr.currentModel)
				{
					R_AddDrawSurf(&entitySurface, tr.defaultShader, -1);
				}
				else
				{
					switch (tr.currentModel->type)
					{
						case MOD_MDX:
							R_AddMDXSurfaces(ent);
							break;

						case MOD_MD5:
							R_AddMD5Surfaces(ent);
							break;

						case MOD_BSP:
							R_AddBSPModelSurfaces(ent);
							break;

						case MOD_BAD:	// null model axis
							if((ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal)
							{
								break;
							}
							VectorClear(ent->localBounds[0]);
							VectorClear(ent->localBounds[1]);
							VectorClear(ent->worldBounds[0]);
							VectorClear(ent->worldBounds[1]);
							shader = R_GetShaderByHandle(ent->e.customShader);
							R_AddDrawSurf(&entitySurface, tr.defaultShader, -1);
							break;

						default:
							ri.Error(ERR_DROP, "R_AddEntitySurfaces: Bad modeltype");
							break;
					}
				}
				break;

			default:
				ri.Error(ERR_DROP, "R_AddEntitySurfaces: Bad reType");
		}
	}
}


/*
=============
R_AddEntityInteractions
=============
*/
void R_AddEntityInteractions(trRefLight_t * light)
{
	int             i;
	trRefEntity_t  *ent;

	if(!r_drawentities->integer)
	{
		return;
	}

	for(i = 0; i < tr.refdef.numEntities; i++)
	{
		ent = tr.currentEntity = &tr.refdef.entities[i];

		//
		// the weapon model must be handled special --
		// we don't want the hacked weapon position showing in
		// mirrors, because the true body position will already be drawn
		//
		if((ent->e.renderfx & RF_FIRST_PERSON) && (tr.viewParms.isPortal || tr.viewParms.isMirror))
		{
			continue;
		}

		// simple generated models, like sprites and beams, are not culled
		switch (ent->e.reType)
		{
			case RT_PORTALSURFACE:
				break;			// don't draw anything
			case RT_SPRITE:
			case RT_BEAM:
			case RT_LIGHTNING:
			case RT_RAIL_CORE:
			case RT_RAIL_RINGS:
				break;

			case RT_MODEL:
				tr.currentModel = R_GetModelByHandle(ent->e.hModel);
				if(!tr.currentModel)
				{
					//R_AddDrawSurf(&entitySurface, tr.defaultShader, 0);
				}
				else
				{
					switch (tr.currentModel->type)
					{
						case MOD_MDX:
							R_AddMDXInteractions(ent, light);
							break;

						case MOD_MD5:
							R_AddMD5Interactions(ent, light);
							break;

						case MOD_BSP:
							R_AddBrushModelInteractions(ent, light);
							break;

						case MOD_BAD:	// null model axis
							break;

						default:
							ri.Error(ERR_DROP, "R_AddEntityInteractions: Bad modeltype");
							break;
					}
				}
				break;

			default:
				ri.Error(ERR_DROP, "R_AddEntityInteractions: Bad reType");
		}
	}

}

/*
=====================
R_AddPolygonInteractions
=====================
*/
void R_AddPolygonInteractions(trRefLight_t * light)
{
	int             i, j;
	shader_t       *shader;
	srfPoly_t      *poly;
	vec3_t          worldBounds[2];

	if(light->l.inverseShadows)
		return;

	tr.currentEntity = &tr.worldEntity;

	for(i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys; i++, poly++)
	{
		shader = R_GetShaderByHandle(poly->hShader);

		if(!shader->interactLight)
			continue;

		// calc AABB of the triangle
		ClearBounds(worldBounds[0], worldBounds[1]);
		for(j = 0; j < poly->numVerts; j++)
		{
			AddPointToBounds(poly->verts[j].xyz, worldBounds[0], worldBounds[1]);
		}

		// do a quick AABB cull
		if(!BoundsIntersect(light->worldBounds[0], light->worldBounds[1], worldBounds[0], worldBounds[1]))
			continue;

		R_AddLightInteraction(light, (void *)poly, shader, CUBESIDE_CLIPALL, IA_LIGHTONLY);
	}
}

/*
=============
R_AddLightInteractions
=============
*/
void R_AddLightInteractions()
{
	int             i, j;
	trRefLight_t   *light;
	bspNode_t     **leafs;
	bspNode_t      *leaf;

	for(i = 0; i < tr.refdef.numLights; i++)
	{
		light = tr.currentLight = &tr.refdef.lights[i];

		if(light->isStatic)
		{
			if(r_noStaticLighting->integer ||
			   ((r_precomputedLighting->integer || r_vertexLighting->integer) && !light->noRadiosity))
			{
				light->cull = CULL_OUT;
				continue;
			}
		}
		else
		{
			if(r_noDynamicLighting->integer)
			{
				light->cull = CULL_OUT;
				continue;
			}
		}

		// we must set up parts of tr.or for light culling
		R_RotateLightForViewParms(light, &tr.viewParms, &tr.orientation);

		// calc local bounds for culling
		if(light->isStatic)
		{
#if 1
			// ignore if not in PVS
			if(!r_noLightVisCull->integer)
			{
				for(j = 0, leafs = light->leafs; j < light->numLeafs; j++, leafs++)
				{
					leaf = *leafs;

					if(leaf->visCounts[tr.visIndex] == tr.visCounts[tr.visIndex])
					{
						light->visCounts[tr.visIndex] = tr.visCounts[tr.visIndex];
					}
				}

				if(light->visCounts[tr.visIndex] != tr.visCounts[tr.visIndex])
				{
					tr.pc.c_pvs_cull_light_out++;
					light->cull = CULL_OUT;
					continue;
				}
			}
#endif
		}
		else
		{
			R_SetupLightLocalBounds(light);
		}

		// look if we have to draw the light including its interactions
		switch (R_CullLocalBox(light->localBounds))
		{
			case CULL_IN:
			default:
				tr.pc.c_box_cull_light_in++;
				light->cull = CULL_IN;
				break;

			case CULL_CLIP:
				tr.pc.c_box_cull_light_clip++;
				light->cull = CULL_CLIP;
				break;

			case CULL_OUT:
				// light is not visible so skip other light setup stuff to save speed
				tr.pc.c_box_cull_light_out++;
				light->cull = CULL_OUT;
				continue;
		}

		if(!light->isStatic)
		{
			// set up light transform matrix
			MatrixSetupTransformFromQuat(light->transformMatrix, light->l.rotation, light->l.origin);

			// set up light origin for lighting and shadowing
			R_SetupLightOrigin(light);

			// set up model to light view matrix
			R_SetupLightView(light);

			// set up projection
			R_SetupLightProjection(light);

			// setup world bounds for intersection tests
			R_SetupLightWorldBounds(light);

			// setup frustum planes for intersection tests
			R_SetupLightFrustum(light);

			// ignore if not in visible bounds
			if(!BoundsIntersect
			   (light->worldBounds[0], light->worldBounds[1], tr.viewParms.visBounds[0], tr.viewParms.visBounds[1]))
			{
				light->cull = CULL_OUT;
				continue;
			}
		}

		// set up view dependent light scissor
		R_SetupLightScissor(light);

		// set up view dependent light depth bounds
		R_SetupLightDepthBounds(light);

		// set up view dependent light Level of Detail
		R_SetupLightLOD(light);

		// look for proper attenuation shader
		R_SetupLightShader(light);

		// setup interactions
		light->firstInteraction = NULL;
		light->lastInteraction = NULL;

		light->numInteractions = 0;
		light->numShadowOnlyInteractions = 0;
		light->numLightOnlyInteractions = 0;
		light->noSort = qfalse;

		if(r_deferredShading->integer && r_shadows->integer <= 3)
		{
			// add one fake interaction for this light
			// because the renderer backend only loops through interactions
			R_AddLightInteraction(light, NULL, NULL, CUBESIDE_CLIPALL, IA_DEFAULT);
		}
		else
		{
			if(light->isStatic)
			{
				R_AddPrecachedWorldInteractions(light);
			}
			else
			{
				R_AddWorldInteractions(light);
			}

			R_AddEntityInteractions(light);

			// Tr3B: fun but slow
			//R_AddPolygonInteractions(light);
		}

		if(light->numInteractions && light->numInteractions != light->numShadowOnlyInteractions)
		{
			R_SortInteractions(light);

			if(light->isStatic)
			{
				tr.pc.c_slights++;
			}
			else
			{
				tr.pc.c_dlights++;
			}
		}
		else
		{
			// skip all interactions of this light because it caused only shadow volumes
			// but no lighting
			tr.refdef.numInteractions -= light->numInteractions;
			light->cull = CULL_OUT;
		}
	}
}


void R_DebugAxis(const vec3_t origin, const matrix_t transformMatrix)
{
#if 0
	vec3_t          forward, left, up;

	MatrixToVectorsFLU(transformMatrix, forward, left, up);
	VectorMA(origin, 16, forward, forward);
	VectorMA(origin, 16, left, left);
	VectorMA(origin, 16, up, up);

	// draw axis
	qglLineWidth(3);
	qglBegin(GL_LINES);

	qglColor3f(1, 0, 0);
	qglVertex3fv(origin);
	qglVertex3fv(forward);

	qglColor3f(0, 1, 0);
	qglVertex3fv(origin);
	qglVertex3fv(left);

	qglColor3f(0, 0, 1);
	qglVertex3fv(origin);
	qglVertex3fv(up);

	qglEnd();
	qglLineWidth(1);
#endif
}

// Tr3B - from botlib
void R_DebugBoundingBox(const vec3_t origin, const vec3_t mins, const vec3_t maxs, vec4_t color)
{
#if 0
	vec3_t          corners[8];
	int             i;

	// upper corners
	corners[0][0] = origin[0] + maxs[0];
	corners[0][1] = origin[1] + maxs[1];
	corners[0][2] = origin[2] + maxs[2];

	corners[1][0] = origin[0] + mins[0];
	corners[1][1] = origin[1] + maxs[1];
	corners[1][2] = origin[2] + maxs[2];

	corners[2][0] = origin[0] + mins[0];
	corners[2][1] = origin[1] + mins[1];
	corners[2][2] = origin[2] + maxs[2];

	corners[3][0] = origin[0] + maxs[0];
	corners[3][1] = origin[1] + mins[1];
	corners[3][2] = origin[2] + maxs[2];

	// lower corners
	Com_Memcpy(corners[4], corners[0], sizeof(vec3_t) * 4);
	for(i = 0; i < 4; i++)
		corners[4 + i][2] = origin[2] + mins[2];

	// draw bounding box
	qglBegin(GL_LINES);
	qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, color);
	for(i = 0; i < 4; i++)
	{
		// top plane
		qglVertex3fv(corners[i]);
		qglVertex3fv(corners[(i + 1) & 3]);

		// bottom plane
		qglVertex3fv(corners[4 + i]);
		qglVertex3fv(corners[4 + ((i + 1) & 3)]);

		// vertical lines
		qglVertex3fv(corners[i]);
		qglVertex3fv(corners[4 + i]);
	}
	qglEnd();
#endif
}

/*
================
R_DebugPolygon
================
*/
void R_DebugPolygon(int color, int numPoints, float *points)
{
#if 0
	int             i;

	GL_BindProgram(0);
	GL_VertexAttribsState(ATTR_DEFAULT);
	GL_State(GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);

	// draw solid shade
	qglColor3f(color & 1, (color >> 1) & 1, (color >> 2) & 1);
	qglBegin(GL_POLYGON);
	for(i = 0; i < numPoints; i++)
	{
		qglVertex3fv(points + i * 3);
	}
	qglEnd();

	// draw wireframe outline
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
	qglDepthRange(0, 0);
	qglColor3f(1, 1, 1);
	qglBegin(GL_POLYGON);
	for(i = 0; i < numPoints; i++)
	{
		qglVertex3fv(points + i * 3);
	}
	qglEnd();
	qglDepthRange(0, 1);
#endif
}

/*
====================
R_DebugGraphics

Visualization aid for movement clipping debugging
====================
*/
static void R_DebugGraphics(void)
{
	if(r_debugSurface->integer)
	{
		// the render thread can't make callbacks to the main thread
		R_SyncRenderThread();

		GL_BindProgram(0);
		GL_SelectTexture(0);
		GL_Bind(tr.whiteImage);
		GL_Cull(CT_FRONT_SIDED);
		ri.CM_DrawDebugSurface(R_DebugPolygon);
	}
}



/*
================
R_RenderView

A view may be either the actual camera view,
or a mirror / remote location
================
*/
void R_RenderView(viewParms_t * parms)
{
	int             firstDrawSurf;
	int             firstInteraction;

	if(parms->viewportWidth <= 0 || parms->viewportHeight <= 0)
	{
		return;
	}

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;
	firstInteraction = tr.refdef.numInteractions;

	tr.viewCount++;

	// set viewParms.world
	R_RotateForViewer();

	// set the projection matrix now that we have the world bounded
	// this needs to be done before entities are
	// added, because they use the projection
	// matrix for lod calculation
	R_SetupProjection();

	R_SetupUnprojection();

	// set camera frustum planes in world space
	R_SetupFrustum(tr.viewParms.frustum, tr.orientation.modelViewMatrix, tr.viewParms.projectionMatrix);

	R_AddWorldSurfaces();

	R_AddPolygonSurfaces();

	R_AddEntitySurfaces();

	R_AddLightInteractions();

	tr.viewParms.drawSurfs = tr.refdef.drawSurfs + firstDrawSurf;
	tr.viewParms.numDrawSurfs = tr.refdef.numDrawSurfs - firstDrawSurf;

	tr.viewParms.interactions = tr.refdef.interactions + firstInteraction;
	tr.viewParms.numInteractions = tr.refdef.numInteractions - firstInteraction;

	R_SortDrawSurfs(tr.viewParms.drawSurfs, tr.viewParms.numDrawSurfs, tr.viewParms.interactions, tr.viewParms.numInteractions);

	// draw main system development information (surface outlines, etc)
	R_DebugGraphics();
}
