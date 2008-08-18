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
// tr_surface.c
#include "tr_local.h"

/*
==============================================================================
THIS ENTIRE FILE IS BACK END!

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
==============================================================================
*/

/*
==============
Tess_EndBegin
==============
*/
void Tess_EndBegin()
{
	Tess_End();
	Tess_Begin(tess.surfaceShader, tess.lightShader, tess.lightmapNum, tess.fogNum, tess.skipTangentSpaces,
					tess.shadowVolume);
}

/*
==============
Tess_CheckOverflow
==============
*/
void Tess_CheckOverflow(int verts, int indexes)
{
	if(glConfig2.vertexBufferObjectAvailable && (tess.indexesVBO || tess.vertexesVBO))
	{
		Tess_EndBegin();
		return;
	}

	if(tess.numVertexes + verts < SHADER_MAX_VERTEXES && tess.numIndexes + indexes < SHADER_MAX_INDEXES)
	{
		return;
	}

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_CheckOverflow(%i + %i vertices, %i + %i triangles ) ---\n", tess.numVertexes, verts,
						  (tess.numIndexes / 3), indexes));
	}

	Tess_End();

	if(verts >= SHADER_MAX_VERTEXES)
	{
		ri.Error(ERR_DROP, "Tess_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES);
	}
	if(indexes >= SHADER_MAX_INDEXES)
	{
		ri.Error(ERR_DROP, "Tess_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES);
	}

	Tess_Begin(tess.surfaceShader, tess.lightShader, tess.lightmapNum, tess.fogNum, tess.skipTangentSpaces,
					tess.shadowVolume);
}


/*
==============
Tess_AddQuadStampExt
==============
*/
void Tess_AddQuadStampExt(vec3_t origin, vec3_t left, vec3_t up, byte * color, float s1, float t1, float s2, float t2)
{
	vec3_t          normal;
	int             ndx;

	GLimp_LogComment("--- Tess_AddQuadStampExt ---\n");

	Tess_CheckOverflow(4, 6);

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[tess.numIndexes] = ndx;
	tess.indexes[tess.numIndexes + 1] = ndx + 1;
	tess.indexes[tess.numIndexes + 2] = ndx + 3;

	tess.indexes[tess.numIndexes + 3] = ndx + 3;
	tess.indexes[tess.numIndexes + 4] = ndx + 1;
	tess.indexes[tess.numIndexes + 5] = ndx + 2;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];
	tess.xyz[ndx][3] = 1;

	tess.xyz[ndx + 1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx + 1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx + 1][2] = origin[2] - left[2] + up[2];
	tess.xyz[ndx + 1][3] = 1;

	tess.xyz[ndx + 2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx + 2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx + 2][2] = origin[2] - left[2] - up[2];
	tess.xyz[ndx + 2][3] = 1;

	tess.xyz[ndx + 3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx + 3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx + 3][2] = origin[2] + left[2] - up[2];
	tess.xyz[ndx + 3][3] = 1;


	// constant normal all the way around
	VectorSubtract(vec3_origin, backEnd.viewParms.or.axis[0], normal);

	tess.normals[ndx][0] = tess.normals[ndx + 1][0] = tess.normals[ndx + 2][0] = tess.normals[ndx + 3][0] = normal[0];
	tess.normals[ndx][1] = tess.normals[ndx + 1][1] = tess.normals[ndx + 2][1] = tess.normals[ndx + 3][1] = normal[1];
	tess.normals[ndx][2] = tess.normals[ndx + 1][2] = tess.normals[ndx + 2][2] = tess.normals[ndx + 3][2] = normal[2];

	// standard square texture coordinates
	tess.texCoords[ndx][0][0] = tess.texCoords[ndx][1][0] = s1;
	tess.texCoords[ndx][0][1] = tess.texCoords[ndx][1][1] = t1;

	tess.texCoords[ndx + 1][0][0] = tess.texCoords[ndx + 1][1][0] = s2;
	tess.texCoords[ndx + 1][0][1] = tess.texCoords[ndx + 1][1][1] = t1;

	tess.texCoords[ndx + 2][0][0] = tess.texCoords[ndx + 2][1][0] = s2;
	tess.texCoords[ndx + 2][0][1] = tess.texCoords[ndx + 2][1][1] = t2;

	tess.texCoords[ndx + 3][0][0] = tess.texCoords[ndx + 3][1][0] = s1;
	tess.texCoords[ndx + 3][0][1] = tess.texCoords[ndx + 3][1][1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	*(unsigned int *)&tess.colors[ndx] =
		*(unsigned int *)&tess.colors[ndx + 1] =
		*(unsigned int *)&tess.colors[ndx + 2] = *(unsigned int *)&tess.colors[ndx + 3] = *(unsigned int *)color;


	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

/*
==============
Tess_AddQuadStamp
==============
*/
void Tess_AddQuadStamp(vec3_t origin, vec3_t left, vec3_t up, byte * color)
{
	Tess_AddQuadStampExt(origin, left, up, color, 0, 0, 1, 1);
}

/*
==============
Tess_SurfaceSprite
==============
*/
static void Tess_SurfaceSprite(void)
{
	vec3_t          left, up;
	float           radius;

	GLimp_LogComment("--- Tess_SurfaceSprite ---\n");

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if(backEnd.currentEntity->e.rotation == 0)
	{
		VectorScale(backEnd.viewParms.or.axis[1], radius, left);
		VectorScale(backEnd.viewParms.or.axis[2], radius, up);
	}
	else
	{
		float           s, c;
		float           ang;

		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin(ang);
		c = cos(ang);

		VectorScale(backEnd.viewParms.or.axis[1], c * radius, left);
		VectorMA(left, -s * radius, backEnd.viewParms.or.axis[2], left);

		VectorScale(backEnd.viewParms.or.axis[2], c * radius, up);
		VectorMA(up, s * radius, backEnd.viewParms.or.axis[1], up);
	}
	if(backEnd.viewParms.isMirror)
	{
		VectorSubtract(vec3_origin, left, left);
	}

	Tess_AddQuadStamp(backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA);
}


/*
=============
Tess_SurfacePolychain
=============
*/
static void Tess_SurfacePolychain(srfPoly_t * p, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	int             i;
	int             numv;

	GLimp_LogComment("--- Tess_SurfacePolychain ---\n");

	if(tess.shadowVolume)
	{
		return;
	}

	Tess_CheckOverflow(p->numVerts, 3 * (p->numVerts - 2));

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for(i = 0; i < p->numVerts; i++)
	{
		VectorCopy(p->verts[i].xyz, tess.xyz[numv]);
		tess.xyz[numv][3] = 1;
		tess.texCoords[numv][0][0] = p->verts[i].st[0];
		tess.texCoords[numv][0][1] = p->verts[i].st[1];
		*(int *)&tess.colors[numv] = *(int *)p->verts[i].modulate;

		numv++;
	}

	// generate fan indexes into the tess array
	for(i = 0; i < p->numVerts - 2; i++)
	{
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	}

	tess.numVertexes = numv;
}

/*
==============
Tess_SurfaceFace
==============
*/
static void Tess_SurfaceFace(srfSurfaceFace_t * cv, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	int             i;
	srfTriangle_t  *tri;
	srfVert_t      *dv;
	float          *xyz, *tangent, *binormal, *normal, *texCoords;
	byte           *color;
	vec3_t          lightOrigin;
	float           d;

	GLimp_LogComment("--- Tess_SurfaceFace ---\n");

	if(tess.shadowVolume)
	{
		VectorCopy(backEnd.currentLight->transformed, lightOrigin);
		
		if(numShadowIndexes)
		{
			// this case is always zfail with light and dark caps
			Tess_CheckOverflow(cv->numVerts, numShadowIndexes);

			for(i = 0; i < numShadowIndexes; i++)
			{
				tess.indexes[tess.numIndexes + i] = tess.numVertexes + shadowIndexes[i];
			}

			tess.numIndexes += numShadowIndexes;
			backEnd.pc.c_shadowIndexes += numShadowIndexes;
		}
		else
		{
			// decide which triangles face the light
			sh.numFacing = 0;
			for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
			{
				d = DotProduct(tri->plane, lightOrigin) - tri->plane[3];
				if(d > 0 && !tess.surfaceShader->cullType == CT_BACK_SIDED)
				{
					sh.facing[i] = qtrue;
					sh.numFacing++;
				}
				else
				{
					sh.facing[i] = qfalse;
				}
			}

			if(backEnd.currentEntity->needZFail)
			{
				Tess_CheckOverflow(cv->numVerts * 2, sh.numFacing * (6 + 2) * 3);
			}
			else
			{
				Tess_CheckOverflow(cv->numVerts * 2, sh.numFacing * 6 * 3);
			}

			// set up indices for silhouette edges
			for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
			{
				if(!sh.facing[i])
				{
					continue;
				}

				if((tri->neighbors[0] < 0) || (tri->neighbors[0] >= 0 && !sh.facing[tri->neighbors[0]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[0] + cv->numVerts;
	
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[0] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[1] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}

				if((tri->neighbors[1] < 0) || (tri->neighbors[1] >= 0 && !sh.facing[tri->neighbors[1]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[1] + cv->numVerts;

					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
	
					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}

				if((tri->neighbors[2] < 0) || (tri->neighbors[2] >= 0 && !sh.facing[tri->neighbors[2]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
	
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}
			}

			// set up indices for light and dark caps
			if(backEnd.currentEntity->needZFail)
			{
				for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
				{
					if(!sh.facing[i])
					{
						continue;
					}

					// light cap
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2];
	
					// dark cap
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}
			}
		}

		// copy vertexes and extrude to infinity
		for(i = 0, dv = cv->verts, xyz = tess.xyz[tess.numVertexes]; i < cv->numVerts; i++, dv++, xyz += 4)
		{
			xyz[0] = dv->xyz[0];
			xyz[1] = dv->xyz[1];
			xyz[2] = dv->xyz[2];
			xyz[3] = 1.0;
		}

		for(i = 0, dv = cv->verts, xyz = tess.xyz[tess.numVertexes + cv->numVerts]; i < cv->numVerts; i++, dv++, xyz += 4)
		{
			#if 1
			xyz[0] = dv->xyz[0];
			xyz[1] = dv->xyz[1];
			xyz[2] = dv->xyz[2];
			#else
			xyz[0] = dv->xyz[0] - lightOrigin[0];
			xyz[1] = dv->xyz[1] - lightOrigin[1];
			xyz[2] = dv->xyz[2] - lightOrigin[2];
			#endif
			xyz[3] = 0.0;
		}

		tess.numVertexes += cv->numVerts * 2;
		backEnd.pc.c_shadowVertexes += cv->numVerts * 2;

		backEnd.pc.c_shadowSurfaces++;
	}
	else
	{
		if(glConfig2.vertexBufferObjectAvailable && (cv->indexesVBO || cv->vertsVBO) && r_vboFaces->integer)
		{
			Tess_EndBegin();
		}

		if(glConfig2.vertexBufferObjectAvailable && cv->indexesVBO && r_vboFaces->integer)
		{
			Tess_CheckOverflow(cv->numVerts, cv->numTriangles * 3);

			tess.indexesVBO = cv->indexesVBO;
			qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, tess.indexesVBO);

			tess.numIndexes += cv->numTriangles * 3;
		}
		else
		{
			if(numLightIndexes)
			{
				Tess_CheckOverflow(cv->numVerts, numLightIndexes);

				for(i = 0; i < numLightIndexes; i++)
				{
					tess.indexes[tess.numIndexes + i] = tess.numVertexes + lightIndexes[i];
				}

				tess.numIndexes += numLightIndexes;
			}
			else
			{
				Tess_CheckOverflow(cv->numVerts, cv->numTriangles * 3);

				for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
				{
					tess.indexes[tess.numIndexes + i * 3 + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + i * 3 + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + i * 3 + 2] = tess.numVertexes + tri->indexes[2];
				}

				tess.numIndexes += cv->numTriangles * 3;
			}
		}

		if(glConfig2.vertexBufferObjectAvailable && cv->vertsVBO && r_vboFaces->integer)
		{
			tess.vertexesVBO = cv->vertsVBO;
			tess.ofsXYZ = cv->ofsXYZ;
			tess.ofsTexCoords = cv->ofsTexCoords;
			tess.ofsTexCoords2 = cv->ofsTexCoords2;
			tess.ofsTangents = cv->ofsTangents;
			tess.ofsBinormals = cv->ofsBinormals;
			tess.ofsNormals = cv->ofsNormals;
			tess.ofsColors = cv->ofsColors;

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, tess.vertexesVBO);
		}
		else
		{
			dv = cv->verts;
			xyz = tess.xyz[tess.numVertexes];
			tangent = tess.tangents[tess.numVertexes];
			binormal = tess.binormals[tess.numVertexes];
			normal = tess.normals[tess.numVertexes];
			texCoords = tess.texCoords[tess.numVertexes][0];
			color = tess.colors[tess.numVertexes];

			for(i = 0; i < cv->numVerts;
				i++, dv++, xyz += 4, tangent += 4, binormal += 4, normal += 4, texCoords += 4, color += 4)
			{
				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				xyz[3] = 1;

				if(!tess.skipTangentSpaces)
				{
					tangent[0] = dv->tangent[0];
					tangent[1] = dv->tangent[1];
					tangent[2] = dv->tangent[2];

					binormal[0] = dv->binormal[0];
					binormal[1] = dv->binormal[1];
					binormal[2] = dv->binormal[2];

					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
				}

				texCoords[0] = dv->st[0];
				texCoords[1] = dv->st[1];

				texCoords[2] = dv->lightmap[0];
				texCoords[3] = dv->lightmap[1];

				*(int *)color = *(int *)dv->color;
			}
		}

		tess.numVertexes += cv->numVerts;
	}
}

/*
=============
Tess_SurfaceGrid
=============
*/
static void Tess_SurfaceGrid(srfGridMesh_t * cv, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	int             i;
	srfTriangle_t  *tri;
	srfVert_t      *dv;
	float          *xyz, *tangent, *binormal, *normal, *texCoords;
	byte           *color;
	vec3_t          lightOrigin;
	float           d;

	GLimp_LogComment("--- Tess_SurfaceGrid ---\n");

	if(tess.shadowVolume)
	{
		VectorCopy(backEnd.currentLight->transformed, lightOrigin);
		
		if(numShadowIndexes)
		{
			// this case is always zfail with light and dark caps
			Tess_CheckOverflow(cv->numVerts, numShadowIndexes);

			for(i = 0; i < numShadowIndexes; i++)
			{
				tess.indexes[tess.numIndexes + i] = tess.numVertexes + shadowIndexes[i];
			}

			tess.numIndexes += numShadowIndexes;
			backEnd.pc.c_shadowIndexes += numShadowIndexes;
		}
		else
		{
			// decide which triangles face the light
			sh.numFacing = 0;
			for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
			{
				d = DotProduct(tri->plane, lightOrigin) - tri->plane[3];
				if(d > 0 && !tess.surfaceShader->cullType == CT_BACK_SIDED)
				{
					sh.facing[i] = qtrue;
					sh.numFacing++;
				}
				else
				{
					sh.facing[i] = qfalse;
				}
			}

			if(backEnd.currentEntity->needZFail)
			{
				Tess_CheckOverflow(cv->numVerts * 2, sh.numFacing * (6 + 2) * 3);
			}
			else
			{
				Tess_CheckOverflow(cv->numVerts * 2, sh.numFacing * 6 * 3);
			}

			// set up indices for silhouette edges
			for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
			{
				if(!sh.facing[i])
				{
					continue;
				}

				if((tri->neighbors[0] < 0) || (tri->neighbors[0] >= 0 && !sh.facing[tri->neighbors[0]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[0] + cv->numVerts;
	
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[0] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[1] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}

				if((tri->neighbors[1] < 0) || (tri->neighbors[1] >= 0 && !sh.facing[tri->neighbors[1]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[1] + cv->numVerts;

					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
	
					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}

				if((tri->neighbors[2] < 0) || (tri->neighbors[2] >= 0 && !sh.facing[tri->neighbors[2]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
	
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}
			}

			// set up indices for light and dark caps
			if(backEnd.currentEntity->needZFail)
			{
				for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
				{
					if(!sh.facing[i])
					{
						continue;
					}

					// light cap
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2];
	
					// dark cap
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}
			}
		}

		// copy vertexes and extrude to infinity
		for(i = 0, dv = cv->verts, xyz = tess.xyz[tess.numVertexes]; i < cv->numVerts; i++, dv++, xyz += 4)
		{
			xyz[0] = dv->xyz[0];
			xyz[1] = dv->xyz[1];
			xyz[2] = dv->xyz[2];
			xyz[3] = 1.0;
		}

		for(i = 0, dv = cv->verts, xyz = tess.xyz[tess.numVertexes + cv->numVerts]; i < cv->numVerts; i++, dv++, xyz += 4)
		{
			#if 1
			xyz[0] = dv->xyz[0];
			xyz[1] = dv->xyz[1];
			xyz[2] = dv->xyz[2];
			#else
			xyz[0] = dv->xyz[0] - lightOrigin[0];
			xyz[1] = dv->xyz[1] - lightOrigin[1];
			xyz[2] = dv->xyz[2] - lightOrigin[2];
			#endif
			xyz[3] = 0.0;
		}

		tess.numVertexes += cv->numVerts * 2;
		backEnd.pc.c_shadowVertexes += cv->numVerts * 2;

		backEnd.pc.c_shadowSurfaces++;
	}
	else
	{
		if(glConfig2.vertexBufferObjectAvailable && (cv->indexesVBO || cv->vertsVBO) && r_vboCurves->integer)
		{
			Tess_EndBegin();
		}

		if(glConfig2.vertexBufferObjectAvailable && cv->indexesVBO && r_vboCurves->integer)
		{
			Tess_CheckOverflow(cv->numVerts, cv->numTriangles * 3);

			tess.indexesVBO = cv->indexesVBO;
			qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, tess.indexesVBO);

			tess.numIndexes += cv->numTriangles * 3;
		}
		else
		{
			if(numLightIndexes)
			{
				Tess_CheckOverflow(cv->numVerts, numLightIndexes);

				for(i = 0; i < numLightIndexes; i++)
				{
					tess.indexes[tess.numIndexes + i] = tess.numVertexes + lightIndexes[i];
				}

				tess.numIndexes += numLightIndexes;
			}
			else
			{
				Tess_CheckOverflow(cv->numVerts, cv->numTriangles * 3);

				for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
				{
					tess.indexes[tess.numIndexes + i * 3 + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + i * 3 + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + i * 3 + 2] = tess.numVertexes + tri->indexes[2];
				}

				tess.numIndexes += cv->numTriangles * 3;
			}
		}

		if(glConfig2.vertexBufferObjectAvailable && cv->vertsVBO && r_vboCurves->integer)
		{
			tess.vertexesVBO = cv->vertsVBO;
			tess.ofsXYZ = cv->ofsXYZ;
			tess.ofsTexCoords = cv->ofsTexCoords;
			tess.ofsTexCoords2 = cv->ofsTexCoords2;
			tess.ofsTangents = cv->ofsTangents;
			tess.ofsBinormals = cv->ofsBinormals;
			tess.ofsNormals = cv->ofsNormals;
			tess.ofsColors = cv->ofsColors;

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, tess.vertexesVBO);
		}
		else
		{
			dv = cv->verts;
			xyz = tess.xyz[tess.numVertexes];
			tangent = tess.tangents[tess.numVertexes];
			binormal = tess.binormals[tess.numVertexes];
			normal = tess.normals[tess.numVertexes];
			texCoords = tess.texCoords[tess.numVertexes][0];
			color = tess.colors[tess.numVertexes];

			for(i = 0; i < cv->numVerts;
				i++, dv++, xyz += 4, tangent += 4, binormal += 4, normal += 4, texCoords += 4, color += 4)
			{
				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				xyz[3] = 1;

				if(!tess.skipTangentSpaces)
				{
					tangent[0] = dv->tangent[0];
					tangent[1] = dv->tangent[1];
					tangent[2] = dv->tangent[2];

					binormal[0] = dv->binormal[0];
					binormal[1] = dv->binormal[1];
					binormal[2] = dv->binormal[2];

					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
				}

				texCoords[0] = dv->st[0];
				texCoords[1] = dv->st[1];

				texCoords[2] = dv->lightmap[0];
				texCoords[3] = dv->lightmap[1];

				*(int *)color = *(int *)dv->color;
			}
		}

		tess.numVertexes += cv->numVerts;
	}
}

/*
=============
Tess_SurfaceTriangles
=============
*/
static void Tess_SurfaceTriangles(srfTriangles_t * cv, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	int             i;
	srfTriangle_t  *tri;
	srfVert_t      *dv;
	float          *xyz, *tangent, *binormal, *normal, *texCoords;
	byte           *color;
	vec3_t          lightOrigin;
	float           d;

	GLimp_LogComment("--- Tess_SurfaceTriangles ---\n");

	if(tess.shadowVolume)
	{
		VectorCopy(backEnd.currentLight->transformed, lightOrigin);
		
		if(numShadowIndexes)
		{
			// this case is always zfail with light and dark caps
			Tess_CheckOverflow(cv->numVerts, numShadowIndexes);

			for(i = 0; i < numShadowIndexes; i++)
			{
				tess.indexes[tess.numIndexes + i] = tess.numVertexes + shadowIndexes[i];
			}

			tess.numIndexes += numShadowIndexes;
			backEnd.pc.c_shadowIndexes += numShadowIndexes;
		}
		else
		{
			// decide which triangles face the light
			sh.numFacing = 0;
			for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
			{
				d = DotProduct(tri->plane, lightOrigin) - tri->plane[3];
				if(d > 0 && !tess.surfaceShader->cullType == CT_BACK_SIDED)
				{
					sh.facing[i] = qtrue;
					sh.numFacing++;
				}
				else
				{
					sh.facing[i] = qfalse;
				}
			}

			if(backEnd.currentEntity->needZFail)
			{
				Tess_CheckOverflow(cv->numVerts * 2, sh.numFacing * (6 + 2) * 3);
			}
			else
			{
				Tess_CheckOverflow(cv->numVerts * 2, sh.numFacing * 6 * 3);
			}

			// set up indices for silhouette edges
			for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
			{
				if(!sh.facing[i])
				{
					continue;
				}

				if((tri->neighbors[0] < 0) || (tri->neighbors[0] >= 0 && !sh.facing[tri->neighbors[0]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[0] + cv->numVerts;
	
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[0] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[1] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}

				if((tri->neighbors[1] < 0) || (tri->neighbors[1] >= 0 && !sh.facing[tri->neighbors[1]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[1] + cv->numVerts;

					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
	
					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}

				if((tri->neighbors[2] < 0) || (tri->neighbors[2] >= 0 && !sh.facing[tri->neighbors[2]]))
				{
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[2];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
	
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}
			}

			// set up indices for light and dark caps
			if(backEnd.currentEntity->needZFail)
			{
				for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
				{
					if(!sh.facing[i])
					{
						continue;
					}

					// light cap
					tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2];
	
					// dark cap
					tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2] + cv->numVerts;
					tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + cv->numVerts;
					tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + cv->numVerts;

					tess.numIndexes += 6;
					backEnd.pc.c_shadowIndexes += 6;
				}
			}
		}

		// copy vertexes and extrude to infinity
		for(i = 0, dv = cv->verts, xyz = tess.xyz[tess.numVertexes]; i < cv->numVerts; i++, dv++, xyz += 4)
		{
			xyz[0] = dv->xyz[0];
			xyz[1] = dv->xyz[1];
			xyz[2] = dv->xyz[2];
			xyz[3] = 1.0;
		}

		for(i = 0, dv = cv->verts, xyz = tess.xyz[tess.numVertexes + cv->numVerts]; i < cv->numVerts; i++, dv++, xyz += 4)
		{
			#if 1
			xyz[0] = dv->xyz[0];
			xyz[1] = dv->xyz[1];
			xyz[2] = dv->xyz[2];
			#else
			xyz[0] = dv->xyz[0] - lightOrigin[0];
			xyz[1] = dv->xyz[1] - lightOrigin[1];
			xyz[2] = dv->xyz[2] - lightOrigin[2];
			#endif
			xyz[3] = 0.0;
		}

		tess.numVertexes += cv->numVerts * 2;
		backEnd.pc.c_shadowVertexes += cv->numVerts * 2;

		backEnd.pc.c_shadowSurfaces++;
	}
	else
	{
		if(glConfig2.vertexBufferObjectAvailable && (cv->indexesVBO || cv->vertsVBO) && r_vboTriangles->integer)
		{
			Tess_EndBegin();
		}

		if(glConfig2.vertexBufferObjectAvailable && cv->indexesVBO && r_vboTriangles->integer)
		{
			Tess_CheckOverflow(cv->numVerts, cv->numTriangles * 3);

			tess.indexesVBO = cv->indexesVBO;
			qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, tess.indexesVBO);

			tess.numIndexes += cv->numTriangles * 3;
		}
		else
		{
			if(numLightIndexes)
			{
				Tess_CheckOverflow(cv->numVerts, numLightIndexes);

				for(i = 0; i < numLightIndexes; i++)
				{
					tess.indexes[tess.numIndexes + i] = tess.numVertexes + lightIndexes[i];
				}

				tess.numIndexes += numLightIndexes;
			}
			else
			{
				Tess_CheckOverflow(cv->numVerts, cv->numTriangles * 3);

				for(i = 0, tri = cv->triangles; i < cv->numTriangles; i++, tri++)
				{
					tess.indexes[tess.numIndexes + i * 3 + 0] = tess.numVertexes + tri->indexes[0];
					tess.indexes[tess.numIndexes + i * 3 + 1] = tess.numVertexes + tri->indexes[1];
					tess.indexes[tess.numIndexes + i * 3 + 2] = tess.numVertexes + tri->indexes[2];
				}

				tess.numIndexes += cv->numTriangles * 3;
			}
		}

		if(glConfig2.vertexBufferObjectAvailable && cv->vertsVBO && r_vboTriangles->integer)
		{
			tess.vertexesVBO = cv->vertsVBO;
			tess.ofsXYZ = cv->ofsXYZ;
			tess.ofsTexCoords = cv->ofsTexCoords;
			tess.ofsTexCoords2 = cv->ofsTexCoords;
			tess.ofsTangents = cv->ofsTangents;
			tess.ofsBinormals = cv->ofsBinormals;
			tess.ofsNormals = cv->ofsNormals;
			tess.ofsColors = cv->ofsColors;

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, tess.vertexesVBO);
		}
		else
		{
			dv = cv->verts;
			xyz = tess.xyz[tess.numVertexes];
			tangent = tess.tangents[tess.numVertexes];
			binormal = tess.binormals[tess.numVertexes];
			normal = tess.normals[tess.numVertexes];
			texCoords = tess.texCoords[tess.numVertexes][0];
			color = tess.colors[tess.numVertexes];

			for(i = 0; i < cv->numVerts;
				i++, dv++, xyz += 4, tangent += 4, binormal += 4, normal += 4, texCoords += 4, color += 4)
			{
				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				xyz[3] = 1;

				if(!tess.skipTangentSpaces)
				{
					tangent[0] = dv->tangent[0];
					tangent[1] = dv->tangent[1];
					tangent[2] = dv->tangent[2];

					binormal[0] = dv->binormal[0];
					binormal[1] = dv->binormal[1];
					binormal[2] = dv->binormal[2];

					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
				}

				texCoords[0] = dv->st[0];
				texCoords[1] = dv->st[1];

				texCoords[2] = dv->st[0];
				texCoords[3] = dv->st[1];

				*(int *)color = *(int *)dv->color;
			}
		}

		tess.numVertexes += cv->numVerts;
	}
}



/*
==============
Tess_SurfaceBeam
==============
*/
static void Tess_SurfaceBeam(void)
{
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int             i;
	vec3_t          perpvec;
	vec3_t          direction, normalized_direction;
	vec3_t          start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t          oldorigin, origin;

	GLimp_LogComment("--- Tess_SurfaceBeam ---\n");

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

	if(VectorNormalize(normalized_direction) == 0)
		return;

	PerpendicularVector(perpvec, normalized_direction);

	VectorScale(perpvec, 4, perpvec);

	for(i = 0; i < NUM_BEAM_SEGS; i++)
	{
		RotatePointAroundVector(start_points[i], normalized_direction, perpvec, (360.0 / NUM_BEAM_SEGS) * i);
//      VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd(start_points[i], direction, end_points[i]);
	}

	GL_Program(0);
	GL_SelectTexture(0);
	GL_Bind(tr.whiteImage);

	GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);

	qglColor3f(1, 0, 0);

	qglBegin(GL_TRIANGLE_STRIP);
	for(i = 0; i <= NUM_BEAM_SEGS; i++)
	{
		qglVertex3fv(start_points[i % NUM_BEAM_SEGS]);
		qglVertex3fv(end_points[i % NUM_BEAM_SEGS]);
	}
	qglEnd();
}

//================================================================================

static void Tess_DoRailCore(const vec3_t start, const vec3_t end, const vec3_t up, float len, float spanWidth)
{
	float           spanWidth2;
	int             vbase;
	float           t = len / 256.0f;

	vbase = tess.numVertexes;

	spanWidth2 = -spanWidth;

	// FIXME: use quad stamp?
	VectorMA(start, spanWidth, up, tess.xyz[tess.numVertexes]);
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = 0;
	tess.colors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0] * 0.25;
	tess.colors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1] * 0.25;
	tess.colors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2] * 0.25;
	tess.numVertexes++;

	VectorMA(start, spanWidth2, up, tess.xyz[tess.numVertexes]);
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = 1;
	tess.colors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.colors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.colors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	VectorMA(end, spanWidth, up, tess.xyz[tess.numVertexes]);
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0][0] = t;
	tess.texCoords[tess.numVertexes][0][1] = 0;
	tess.colors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.colors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.colors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	VectorMA(end, spanWidth2, up, tess.xyz[tess.numVertexes]);
	tess.xyz[tess.numVertexes][3] = 1;
	tess.texCoords[tess.numVertexes][0][0] = t;
	tess.texCoords[tess.numVertexes][0][1] = 1;
	tess.colors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.colors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.colors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

static void Tess_DoRailDiscs(int numSegs, const vec3_t start, const vec3_t dir, const vec3_t right, const vec3_t up)
{
	int             i;
	vec3_t          pos[4];
	vec3_t          v;
	int             spanWidth = r_railWidth->integer;
	float           c, s;
	float           scale;

	if(numSegs > 1)
		numSegs--;
	if(!numSegs)
		return;

	scale = 0.25;

	for(i = 0; i < 4; i++)
	{
		c = cos(DEG2RAD(45 + i * 90));
		s = sin(DEG2RAD(45 + i * 90));
		v[0] = (right[0] * c + up[0] * s) * scale * spanWidth;
		v[1] = (right[1] * c + up[1] * s) * scale * spanWidth;
		v[2] = (right[2] * c + up[2] * s) * scale * spanWidth;
		VectorAdd(start, v, pos[i]);

		if(numSegs > 1)
		{
			// offset by 1 segment if we're doing a long distance shot
			VectorAdd(pos[i], dir, pos[i]);
		}
	}

	for(i = 0; i < numSegs; i++)
	{
		int             j;

		Tess_CheckOverflow(4, 6);

		for(j = 0; j < 4; j++)
		{
			VectorCopy(pos[j], tess.xyz[tess.numVertexes]);
			tess.xyz[tess.numVertexes][3] = 1;
			tess.texCoords[tess.numVertexes][0][0] = (j < 2);
			tess.texCoords[tess.numVertexes][0][1] = (j && j != 3);
			tess.colors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
			tess.colors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
			tess.colors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
			tess.numVertexes++;

			VectorAdd(pos[j], dir, pos[j]);
		}

		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 0;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 3;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 1;
		tess.indexes[tess.numIndexes++] = tess.numVertexes - 4 + 2;
	}
}

/*
==============
Tess_SurfaceRailRings
==============
*/
static void Tess_SurfaceRailRings(void)
{
	refEntity_t *e;
	int             numSegs;
	int             len;
	vec3_t          vec;
	vec3_t          right, up;
	vec3_t          start, end;

	GLimp_LogComment("--- Tess_SurfaceRailRings ---\n");

	e = &backEnd.currentEntity->e;

	VectorCopy(e->oldorigin, start);
	VectorCopy(e->origin, end);

	// compute variables
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	MakeNormalVectors(vec, right, up);
	numSegs = (len) / r_railSegmentLength->value;
	if(numSegs <= 0)
	{
		numSegs = 1;
	}

	VectorScale(vec, r_railSegmentLength->value, vec);

	Tess_DoRailDiscs(numSegs, start, vec, right, up);
}

/*
==============
Tess_SurfaceRailCore
==============
*/
static void Tess_SurfaceRailCore(void)
{
	refEntity_t *e;
	int             len;
	vec3_t          right;
	vec3_t          vec;
	vec3_t          start, end;
	vec3_t          v1, v2;

	GLimp_LogComment("--- Tess_SurfaceRailCore ---\n");

	e = &backEnd.currentEntity->e;

	VectorCopy(e->oldorigin, start);
	VectorCopy(e->origin, end);

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	// compute side vector
	VectorSubtract(start, backEnd.viewParms.or.origin, v1);
	VectorNormalize(v1);
	VectorSubtract(end, backEnd.viewParms.or.origin, v2);
	VectorNormalize(v2);
	CrossProduct(v1, v2, right);
	VectorNormalize(right);

	Tess_DoRailCore(start, end, right, len, r_railCoreWidth->integer);
}

/*
==============
Tess_SurfaceLightningBolt
==============
*/
static void Tess_SurfaceLightningBolt(void)
{
	refEntity_t *e;
	int             len;
	vec3_t          right;
	vec3_t          vec;
	vec3_t          start, end;
	vec3_t          v1, v2;
	int             i;

	GLimp_LogComment("--- Tess_SurfaceLightningBolt ---\n");

	e = &backEnd.currentEntity->e;

	VectorCopy(e->oldorigin, end);
	VectorCopy(e->origin, start);

	// compute variables
	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	// compute side vector
	VectorSubtract(start, backEnd.viewParms.or.origin, v1);
	VectorNormalize(v1);
	VectorSubtract(end, backEnd.viewParms.or.origin, v2);
	VectorNormalize(v2);
	CrossProduct(v1, v2, right);
	VectorNormalize(right);

	for(i = 0; i < 4; i++)
	{
		vec3_t          temp;

		Tess_DoRailCore(start, end, right, len, 8);
		RotatePointAroundVector(temp, vec, right, 45);
		VectorCopy(temp, right);
	}
}

/*
==============
VectorArrayNormalize

The inputs to this routing seem to always be close to length = 1.0 (about 0.6 to 2.0)
This means that we don't have to worry about zero length or enormously long vectors.
==============
*/
static void VectorArrayNormalize(vec4_t * normals, unsigned int count)
{
//    assert(count);

#if idppc
	{
		register float  half = 0.5;
		register float  one = 1.0;
		float          *components = (float *)normals;

		// Vanilla PPC code, but since PPC has a reciprocal square root estimate instruction,
		// runs *much* faster than calling sqrt().  We'll use a single Newton-Raphson
		// refinement step to get a little more precision.  This seems to yeild results
		// that are correct to 3 decimal places and usually correct to at least 4 (sometimes 5).
		// (That is, for the given input range of about 0.6 to 2.0).
		do
		{
			float           x, y, z;
			float           B, y0, y1;

			x = components[0];
			y = components[1];
			z = components[2];
			components += 4;
			B = x * x + y * y + z * z;

#ifdef __GNUC__
		  asm("frsqrte %0,%1": "=f"(y0):"f"(B));
#else
			y0 = __frsqrte(B);
#endif
			y1 = y0 + half * y0 * (one - B * y0 * y0);

			x = x * y1;
			y = y * y1;
			components[-4] = x;
			z = z * y1;
			components[-3] = y;
			components[-2] = z;
		} while(count--);
	}
#else							// No assembly version for this architecture, or C_ONLY defined
	// given the input, it's safe to call VectorNormalizeFast
	while(count--)
	{
		VectorNormalizeFast(normals[0]);
		normals++;
	}
#endif

}

/*
=============
Tess_SurfaceMDX
=============
*/
void Tess_SurfaceMDX(mdxSurface_t * srf, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	int             i, j;
	int             numIndexes = 0;
	int             numVertexes;
	mdxModel_t     *model;
	mdxVertex_t    *oldVert, *newVert;
	mdxSt_t        *st;
	srfTriangle_t  *tri;
	vec3_t          lightOrigin;
	float           backlerp;
	float           oldXyzScale, newXyzScale;
	
	GLimp_LogComment("--- Tess_SurfaceMDX ---\n");

	if(backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame)
	{
		backlerp = 0;
	}
	else
	{
		backlerp = backEnd.currentEntity->e.backlerp;
	}
	
	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	oldXyzScale = MD3_XYZ_SCALE * backlerp;

	if(tess.shadowVolume)
	{
		if(backEnd.currentEntity->needZFail)
		{
			Tess_CheckOverflow(srf->numVerts * 2, srf->numTriangles * (6 + 2) * 3);
		}
		else
		{
			Tess_CheckOverflow(srf->numVerts * 2, srf->numTriangles * 6 * 3);
		}

		model = srf->model;

		VectorCopy(backEnd.currentLight->transformed, lightOrigin);

		// lerp vertices and extrude to infinity
		newVert = srf->verts + (backEnd.currentEntity->e.frame * srf->numVerts);
		oldVert = srf->verts + (backEnd.currentEntity->e.oldframe * srf->numVerts);
		
		numVertexes = srf->numVerts;
		for(j = 0; j < numVertexes; j++, newVert++, oldVert++)
		{
			vec3_t          tmpVert;
			
			if(backlerp == 0)
			{
				// just copy
				tmpVert[0] = newVert->xyz[0] * newXyzScale;
				tmpVert[1] = newVert->xyz[1] * newXyzScale;
				tmpVert[2] = newVert->xyz[2] * newXyzScale;
			}
			else
			{
				// interpolate the xyz
				tmpVert[0] = oldVert->xyz[0] * oldXyzScale + newVert->xyz[0] * newXyzScale;
				tmpVert[1] = oldVert->xyz[1] * oldXyzScale + newVert->xyz[1] * newXyzScale;
				tmpVert[2] = oldVert->xyz[2] * oldXyzScale + newVert->xyz[2] * newXyzScale;
			}

			tess.xyz[tess.numVertexes + j][0] = tmpVert[0];
			tess.xyz[tess.numVertexes + j][1] = tmpVert[1];
			tess.xyz[tess.numVertexes + j][2] = tmpVert[2];
			tess.xyz[tess.numVertexes + j][3] = 1;
			
			#if 1
			tess.xyz[tess.numVertexes + numVertexes + j][0] = tmpVert[0];
			tess.xyz[tess.numVertexes + numVertexes + j][1] = tmpVert[1];
			tess.xyz[tess.numVertexes + numVertexes + j][2] = tmpVert[2];
			tess.xyz[tess.numVertexes + numVertexes + j][3] = 0;
			#else
			tess.xyz[tess.numVertexes + numVertexes + j][0] = tmpVert[0] - lightOrigin[0];
			tess.xyz[tess.numVertexes + numVertexes + j][1] = tmpVert[1] - lightOrigin[1];
			tess.xyz[tess.numVertexes + numVertexes + j][2] = tmpVert[2] - lightOrigin[2];
			tess.xyz[tess.numVertexes + numVertexes + j][3] = 0;
			#endif
		}

		// decide which triangles face the light
		for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
		{
			float          *v1, *v2, *v3;
			vec3_t          d1, d2;
			vec4_t          plane;
			float           d;

			v1 = tess.xyz[tess.numVertexes + tri->indexes[0]];
			v2 = tess.xyz[tess.numVertexes + tri->indexes[1]];
			v3 = tess.xyz[tess.numVertexes + tri->indexes[2]];

			VectorSubtract(v2, v1, d1);
			VectorSubtract(v3, v1, d2);

			CrossProduct(d2, d1, plane);
			plane[3] = DotProduct(plane, v1);

			d = DotProduct(plane, lightOrigin) - plane[3];
			if(d > 0 && !tess.surfaceShader->cullType == CT_BACK_SIDED)
			{
				sh.facing[i] = qtrue;
			}
			else
			{
				sh.facing[i] = qfalse;
			}
		}

		// set up indices for silhouette edges
		for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
		{
			if(!sh.facing[i])
			{
				continue;
			}

			if((tri->neighbors[0] < 0) || (tri->neighbors[0] >= 0 && !sh.facing[tri->neighbors[0]]))
			{
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[0] + srf->numVerts;

				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[0] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[1] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}

			if((tri->neighbors[1] < 0) || (tri->neighbors[1] >= 0 && !sh.facing[tri->neighbors[1]]))
			{
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[2];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[1] + srf->numVerts;

				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2];
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[2] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}

			if((tri->neighbors[2] < 0) || (tri->neighbors[2] >= 0 && !sh.facing[tri->neighbors[2]]))
			{
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[2];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2] + srf->numVerts;

				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[2] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}
		}

		// set up indices for light and dark caps
		if(backEnd.currentEntity->needZFail)
		{
			for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
			{
				if(!sh.facing[i])
				{
					continue;
				}

				// light cap
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2];

				// dark cap
				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2] + srf->numVerts;
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}
		}

		tess.numVertexes += srf->numVerts * 2;
		backEnd.pc.c_shadowVertexes += srf->numVerts * 2;

		backEnd.pc.c_shadowSurfaces++;
	}
	else
	{
		Tess_CheckOverflow(srf->numVerts, srf->numTriangles * 3);

		model = srf->model;

		numIndexes = srf->numTriangles * 3;
		for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
		{
			tess.indexes[tess.numIndexes + i * 3 + 0] = tess.numVertexes + tri->indexes[0];
			tess.indexes[tess.numIndexes + i * 3 + 1] = tess.numVertexes + tri->indexes[1];
			tess.indexes[tess.numIndexes + i * 3 + 2] = tess.numVertexes + tri->indexes[2];
		}

		newVert = srf->verts + (backEnd.currentEntity->e.frame * srf->numVerts);
		oldVert = srf->verts + (backEnd.currentEntity->e.oldframe * srf->numVerts);
		st = srf->st;
		
		numVertexes = srf->numVerts;
		for(j = 0; j < numVertexes; j++, newVert++, oldVert++, st++)
		{
			vec3_t          tmpVert;
			
			if(backlerp == 0)
			{
				// just copy
				tmpVert[0] = newVert->xyz[0] * newXyzScale;
				tmpVert[1] = newVert->xyz[1] * newXyzScale;
				tmpVert[2] = newVert->xyz[2] * newXyzScale;
			}
			else
			{
				// interpolate the xyz
				tmpVert[0] = oldVert->xyz[0] * oldXyzScale + newVert->xyz[0] * newXyzScale;
				tmpVert[1] = oldVert->xyz[1] * oldXyzScale + newVert->xyz[1] * newXyzScale;
				tmpVert[2] = oldVert->xyz[2] * oldXyzScale + newVert->xyz[2] * newXyzScale;
			}

			tess.xyz[tess.numVertexes + j][0] = tmpVert[0];
			tess.xyz[tess.numVertexes + j][1] = tmpVert[1];
			tess.xyz[tess.numVertexes + j][2] = tmpVert[2];
			tess.xyz[tess.numVertexes + j][3] = 1;

			tess.texCoords[tess.numVertexes + j][0][0] = st->st[0];
			tess.texCoords[tess.numVertexes + j][0][1] = st->st[1];
		}

		// calc tangent spaces
		if(!tess.skipTangentSpaces)
		{
			int             i;
			vec3_t          faceNormal;
			float          *v;
			const float    *v0, *v1, *v2;
			const float    *t0, *t1, *t2;
			vec3_t          tangent;
			vec3_t          binormal;
			vec3_t          normal;
			int            *indices;

			for(i = 0; i < numVertexes; i++)
			{
				VectorClear(tess.tangents[tess.numVertexes + i]);
				VectorClear(tess.binormals[tess.numVertexes + i]);
				VectorClear(tess.normals[tess.numVertexes + i]);
			}

			for(i = 0, indices = tess.indexes + tess.numIndexes; i < numIndexes; i += 3, indices += 3)
			{
				v0 = tess.xyz[indices[0]];
				v1 = tess.xyz[indices[1]];
				v2 = tess.xyz[indices[2]];

				t0 = tess.texCoords[indices[0]][0];
				t1 = tess.texCoords[indices[1]][0];
				t2 = tess.texCoords[indices[2]][0];
#if 1
				R_CalcNormalForTriangle(faceNormal, v0, v1, v2);
				R_CalcTangentSpace(tangent, binormal, normal, v0, v1, v2, t0, t1, t2, faceNormal);
#else
				R_CalcNormalForTriangle(normal, v0, v1, v2);
				R_CalcTangentsForTriangle(tangent, binormal, v0, v1, v2, t0, t1, t2);
#endif
				for(j = 0; j < 3; j++)
				{
					v = tess.tangents[indices[j]];
					VectorAdd(v, tangent, v);
					v = tess.binormals[indices[j]];
					VectorAdd(v, binormal, v);
					v = tess.normals[indices[j]];
					VectorAdd(v, normal, v);
				}
			}

			VectorArrayNormalize((vec4_t *) tess.tangents[tess.numVertexes], numVertexes);
			VectorArrayNormalize((vec4_t *) tess.binormals[tess.numVertexes], numVertexes);
			VectorArrayNormalize((vec4_t *) tess.normals[tess.numVertexes], numVertexes);
		}

		tess.numIndexes += numIndexes;
		tess.numVertexes += numVertexes;
	}
}

/*
==============
Tess_SurfaceMD5
==============
*/
void Tess_SurfaceMD5(md5Surface_t * srf, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	int             i, j, k;
	int             numIndexes = 0;
	int             numVertexes;
	md5Model_t     *model;
	md5Vertex_t    *v;
	md5Bone_t      *bone;
	srfTriangle_t  *tri;
	vec3_t          offsetVec;
	vec3_t          lightOrigin;
	float          *xyzw, *xyzw2;

	GLimp_LogComment("--- Tess_SurfaceMD5 ---\n");

	if(tess.shadowVolume)
	{
		if(backEnd.currentEntity->needZFail)
		{
			Tess_CheckOverflow(srf->numVerts * 2, srf->numTriangles * (6 + 2) * 3);
		}
		else
		{
			Tess_CheckOverflow(srf->numVerts * 2, srf->numTriangles * 6 * 3);
		}

		model = srf->model;

		VectorCopy(backEnd.currentLight->transformed, lightOrigin);

		// deform the vertexes by the lerped bones and extrude to infinity
		numVertexes = srf->numVerts;
		xyzw = tess.xyz[tess.numVertexes];
		xyzw2 = tess.xyz[tess.numVertexes + srf->numVerts];
		for(j = 0, v = srf->verts; j < numVertexes; j++, v++, xyzw += 4, xyzw2 += 4)
		{
			vec3_t          tmpVert;
			md5Weight_t    *w;

			VectorClear(tmpVert);

			for(k = 0, w = v->weights[0]; k < v->numWeights; k++, w++)
			{
				bone = &model->bones[w->boneIndex];

				if(backEnd.currentEntity->e.skeleton.type == SK_ABSOLUTE)
				{
					QuatTransformVector(backEnd.currentEntity->e.skeleton.bones[w->boneIndex].rotation, w->offset, offsetVec);

					offsetVec[0] *= backEnd.currentEntity->e.skeleton.scale[0];
					offsetVec[1] *= backEnd.currentEntity->e.skeleton.scale[1];
					offsetVec[2] *= backEnd.currentEntity->e.skeleton.scale[2];

					VectorAdd(backEnd.currentEntity->e.skeleton.bones[w->boneIndex].origin, offsetVec, offsetVec);
				}
				else
				{
					QuatTransformVector(bone->rotation, w->offset, offsetVec);
					VectorAdd(bone->origin, offsetVec, offsetVec);
				}

				VectorMA(tmpVert, w->boneWeight, offsetVec, tmpVert);
			}

			xyzw[0] = tmpVert[0];
			xyzw[1] = tmpVert[1];
			xyzw[2] = tmpVert[2];
			xyzw[3] = 1;

			#if 1
			xyzw2[0] = tmpVert[0];
			xyzw2[1] = tmpVert[1];
			xyzw2[2] = tmpVert[2];
			#else
			xyzw2[0] = tmpVert[0] - lightOrigin[0];
			xyzw2[1] = tmpVert[1] - lightOrigin[1];
			xyzw2[2] = tmpVert[2] - lightOrigin[2];
			#endif
			xyzw2[3] = 0;
		}

		// decide which triangles face the light
		for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
		{
			float          *v1, *v2, *v3;
			vec3_t          d1, d2;
			vec4_t          plane;
			float           d;

			v1 = tess.xyz[tess.numVertexes + tri->indexes[0]];
			v2 = tess.xyz[tess.numVertexes + tri->indexes[1]];
			v3 = tess.xyz[tess.numVertexes + tri->indexes[2]];

			VectorSubtract(v2, v1, d1);
			VectorSubtract(v3, v1, d2);

			CrossProduct(d2, d1, plane);
			plane[3] = DotProduct(plane, v1);

			d = DotProduct(plane, lightOrigin) - plane[3];
			if(d > 0 && !tess.surfaceShader->cullType == CT_BACK_SIDED)
			{
				sh.facing[i] = qtrue;
			}
			else
			{
				sh.facing[i] = qfalse;
			}
		}

		// set up indices for silhouette edges
		for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
		{
			if(!sh.facing[i])
			{
				continue;
			}

			if((tri->neighbors[0] < 0) || (tri->neighbors[0] >= 0 && !sh.facing[tri->neighbors[0]]))
			{
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[0] + srf->numVerts;

				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[0] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[1] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}

			if((tri->neighbors[1] < 0) || (tri->neighbors[1] >= 0 && !sh.facing[tri->neighbors[1]]))
			{
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[2];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[1] + srf->numVerts;

				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2];
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[2] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}

			if((tri->neighbors[2] < 0) || (tri->neighbors[2] >= 0 && !sh.facing[tri->neighbors[2]]))
			{
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[2];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2] + srf->numVerts;

				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[2] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}
		}

		// set up indices for light and dark caps
		if(backEnd.currentEntity->needZFail)
		{
			for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
			{
				if(!sh.facing[i])
				{
					continue;
				}

				// light cap
				tess.indexes[tess.numIndexes + 0] = tess.numVertexes + tri->indexes[0];
				tess.indexes[tess.numIndexes + 1] = tess.numVertexes + tri->indexes[1];
				tess.indexes[tess.numIndexes + 2] = tess.numVertexes + tri->indexes[2];

				// dark cap
				tess.indexes[tess.numIndexes + 3] = tess.numVertexes + tri->indexes[2] + srf->numVerts;
				tess.indexes[tess.numIndexes + 4] = tess.numVertexes + tri->indexes[1] + srf->numVerts;
				tess.indexes[tess.numIndexes + 5] = tess.numVertexes + tri->indexes[0] + srf->numVerts;

				tess.numIndexes += 6;
				backEnd.pc.c_shadowIndexes += 6;
			}
		}

		tess.numVertexes += srf->numVerts * 2;
		backEnd.pc.c_shadowVertexes += srf->numVerts * 2;

		backEnd.pc.c_shadowSurfaces++;
	}
	else
	{
		Tess_CheckOverflow(srf->numVerts, srf->numTriangles * 3);

		model = srf->model;

		numIndexes = srf->numTriangles * 3;
		for(i = 0, tri = srf->triangles; i < srf->numTriangles; i++, tri++)
		{
			tess.indexes[tess.numIndexes + i * 3 + 0] = tess.numVertexes + tri->indexes[0];
			tess.indexes[tess.numIndexes + i * 3 + 1] = tess.numVertexes + tri->indexes[1];
			tess.indexes[tess.numIndexes + i * 3 + 2] = tess.numVertexes + tri->indexes[2];
		}

		// deform the vertexes by the lerped bones
		numVertexes = srf->numVerts;
		for(j = 0, v = srf->verts; j < numVertexes; j++, v++)
		{
			vec3_t          tmpVert;
			md5Weight_t    *w;

			VectorClear(tmpVert);

			for(k = 0, w = v->weights[0]; k < v->numWeights; k++, w++)
			{
				bone = &model->bones[w->boneIndex];

				if(backEnd.currentEntity->e.skeleton.type == SK_ABSOLUTE)
				{
					QuatTransformVector(backEnd.currentEntity->e.skeleton.bones[w->boneIndex].rotation, w->offset, offsetVec);

					offsetVec[0] *= backEnd.currentEntity->e.skeleton.scale[0];
					offsetVec[1] *= backEnd.currentEntity->e.skeleton.scale[1];
					offsetVec[2] *= backEnd.currentEntity->e.skeleton.scale[2];

					VectorAdd(backEnd.currentEntity->e.skeleton.bones[w->boneIndex].origin, offsetVec, offsetVec);
				}
				else
				{
					QuatTransformVector(bone->rotation, w->offset, offsetVec);
					VectorAdd(bone->origin, offsetVec, offsetVec);
				}

				VectorMA(tmpVert, w->boneWeight, offsetVec, tmpVert);
			}

			tess.xyz[tess.numVertexes + j][0] = tmpVert[0];
			tess.xyz[tess.numVertexes + j][1] = tmpVert[1];
			tess.xyz[tess.numVertexes + j][2] = tmpVert[2];
			tess.xyz[tess.numVertexes + j][3] = 1;

			tess.texCoords[tess.numVertexes + j][0][0] = v->texCoords[0];
			tess.texCoords[tess.numVertexes + j][0][1] = v->texCoords[1];
		}

		// calc tangent spaces
		if(!tess.skipTangentSpaces)
		{
			int             i;
			vec3_t          faceNormal;
			float          *v;
			const float    *v0, *v1, *v2;
			const float    *t0, *t1, *t2;
			vec3_t          tangent;
			vec3_t          binormal;
			vec3_t          normal;
			int            *indices;

			for(i = 0; i < numVertexes; i++)
			{
				VectorClear(tess.tangents[tess.numVertexes + i]);
				VectorClear(tess.binormals[tess.numVertexes + i]);
				VectorClear(tess.normals[tess.numVertexes + i]);
			}

			for(i = 0, indices = tess.indexes + tess.numIndexes; i < numIndexes; i += 3, indices += 3)
			{
				v0 = tess.xyz[indices[0]];
				v1 = tess.xyz[indices[1]];
				v2 = tess.xyz[indices[2]];

				t0 = tess.texCoords[indices[0]][0];
				t1 = tess.texCoords[indices[1]][0];
				t2 = tess.texCoords[indices[2]][0];
#if 1
				R_CalcNormalForTriangle(faceNormal, v0, v1, v2);
				R_CalcTangentSpace(tangent, binormal, normal, v0, v1, v2, t0, t1, t2, faceNormal);
#else
				R_CalcNormalForTriangle(normal, v0, v1, v2);
				R_CalcTangentsForTriangle(tangent, binormal, v0, v1, v2, t0, t1, t2);
#endif
				for(j = 0; j < 3; j++)
				{
					v = tess.tangents[indices[j]];
					VectorAdd(v, tangent, v);
					v = tess.binormals[indices[j]];
					VectorAdd(v, binormal, v);
					v = tess.normals[indices[j]];
					VectorAdd(v, normal, v);
				}
			}

			VectorArrayNormalize((vec4_t *) tess.tangents[tess.numVertexes], numVertexes);
			VectorArrayNormalize((vec4_t *) tess.binormals[tess.numVertexes], numVertexes);
			VectorArrayNormalize((vec4_t *) tess.normals[tess.numVertexes], numVertexes);
		}

		tess.numIndexes += numIndexes;
		tess.numVertexes += numVertexes;
	}
}

/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
Tess_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void Tess_SurfaceAxis(void)
{
	GLimp_LogComment("--- Tess_SurfaceAxis ---\n");

	GL_Program(0);
	GL_SelectTexture(0);
	GL_Bind(tr.whiteImage);

	qglLineWidth(3);
	qglBegin(GL_LINES);
	qglColor3f(1, 0, 0);
	qglVertex3f(0, 0, 0);
	qglVertex3f(16, 0, 0);
	qglColor3f(0, 1, 0);
	qglVertex3f(0, 0, 0);
	qglVertex3f(0, 16, 0);
	qglColor3f(0, 0, 1);
	qglVertex3f(0, 0, 0);
	qglVertex3f(0, 0, 16);
	qglEnd();
	qglLineWidth(1);
}

//===========================================================================

/*
====================
Tess_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
static void Tess_SurfaceEntity(surfaceType_t * surfType, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	GLimp_LogComment("--- Tess_SurfaceEntity ---\n");

	if(tess.shadowVolume)
	{
		return;
	}

	switch (backEnd.currentEntity->e.reType)
	{
		case RT_SPRITE:
			Tess_SurfaceSprite();
			break;
		case RT_BEAM:
			Tess_SurfaceBeam();
			break;
		case RT_RAIL_CORE:
			Tess_SurfaceRailCore();
			break;
		case RT_RAIL_RINGS:
			Tess_SurfaceRailRings();
			break;
		case RT_LIGHTNING:
			Tess_SurfaceLightningBolt();
			break;
		default:
			Tess_SurfaceAxis();
			break;
	}
	return;
}

static void Tess_SurfaceBad(surfaceType_t * surfType, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	GLimp_LogComment("--- Tess_SurfaceBad ---\n");

	ri.Printf(PRINT_ALL, "Bad surface tesselated.\n");
}

static void Tess_SurfaceFlare(srfFlare_t * surf, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
	if(!r_drawFlares->integer)
		return;
		
	GLimp_LogComment("--- Tess_SurfaceFlare ---\n");
	

	if(tess.shadowVolume)
		return;

	RB_AddFlare((void *)surf, tess.fogNum, surf->origin, surf->color, surf->normal);
}

static void Tess_SurfaceSkip(void *surf, int numLightIndexes, int *lightIndexes, int numShadowIndexes, int *shadowIndexes)
{
}


void            (*tess_surfaceTable[SF_NUM_SURFACE_TYPES]) (void *, int numLightIndexes, int *lightIndexes, int numShadowIndexes,
														  int *shadowIndexes) =
{
	(void (*)(void *, int, int *, int, int *))Tess_SurfaceBad,	// SF_BAD, 
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceSkip,	// SF_SKIP, 
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceFace,	// SF_FACE,
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceGrid,	// SF_GRID,
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceTriangles,	// SF_TRIANGLES,
		(void (*)(void *, int, int *, int, int *))Tess_SurfacePolychain,	// SF_POLY,
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceMDX,	// SF_MDX,
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceMD5,	// SF_MD5,
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceFlare,	// SF_FLARE,
		(void (*)(void *, int, int *, int, int *))Tess_SurfaceEntity	// SF_ENTITY
};
