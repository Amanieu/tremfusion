/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2009 Robert Beckebans <trebor_7@users.sourceforge.net>

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
// tr_models.c -- model loading and caching
#include "tr_local.h"

#define	LL(x) x=LittleLong(x)
#define	LF(x) x=LittleFloat(x)

static qboolean R_LoadMD3(model_t * mod, int lod, void *buffer, int bufferSize, const char *name, qboolean forceStatic);
static qboolean R_LoadMD5(model_t * mod, void *buffer, int bufferSize, const char *name);
static qboolean R_LoadPSK(model_t * mod, void *buffer, int bufferSize, const char *name);

model_t        *loadmodel;

/*
** R_GetModelByHandle
*/
model_t        *R_GetModelByHandle(qhandle_t index)
{
	model_t        *mod;

	// out of range gets the defualt model
	if(index < 1 || index >= tr.numModels)
	{
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t        *R_AllocModel(void)
{
	model_t        *mod;

	if(tr.numModels == MAX_MOD_KNOWN)
	{
		return NULL;
	}

	mod = ri.Hunk_Alloc(sizeof(*tr.models[tr.numModels]), h_low);
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel(const char *name, qboolean forceStatic)
{
	model_t        *mod;
	unsigned       *buffer;
	int             bufferLen;
	int             lod;
	int             ident;
	qboolean        loaded;
	qhandle_t       hModel;
	int             numLoaded;

	if(!name || !name[0])
	{
		ri.Printf(PRINT_ALL, "RE_RegisterModel: NULL name\n");
		return 0;
	}

	if(strlen(name) >= MAX_QPATH)
	{
		Com_Printf("Model name exceeds MAX_QPATH\n");
		return 0;
	}

	// search the currently loaded models
	for(hModel = 1; hModel < tr.numModels; hModel++)
	{
		mod = tr.models[hModel];
		if(!strcmp(mod->name, name))
		{
			if(mod->type == MOD_BAD)
			{
				return 0;
			}
			return hModel;
		}
	}

	// allocate a new model_t
	if((mod = R_AllocModel()) == NULL)
	{
		ri.Printf(PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz(mod->name, name, sizeof(mod->name));

	// make sure the render thread is stopped
	R_SyncRenderThread();

	mod->numLods = 0;

	// load the files
	numLoaded = 0;

	for(lod = MD3_MAX_LODS - 1; lod >= 0; lod--)
	{
		char            filename[1024];
		int             len;

		strcpy(filename, name);

		if(lod != 0)
		{
			char            namebuf[80];

			if(strrchr(filename, '.'))
			{
				*strrchr(filename, '.') = 0;
			}
			sprintf(namebuf, "_%d.md3", lod);
			strcat(filename, namebuf);
		}

		bufferLen = ri.FS_ReadFile(filename, (void **)&buffer);
		if(!buffer)
		{
			continue;
		}

		loadmodel = mod;

		ident = LittleLong(*(unsigned *)buffer);

		len = strlen(filename);

		if(!Q_stricmpn((const char *)buffer, "MD5Version", 10))
		{
			loaded = R_LoadMD5(mod, buffer, bufferLen, name);
		}
		else if(!Q_stricmpn((const char *)buffer, PSK_IDENTSTRING, PSK_IDENTLEN))
		{
			loaded = R_LoadPSK(mod, buffer, bufferLen, name);
		}
		else
		{
			if(ident != MD3_IDENT)
			{
				ri.Printf(PRINT_WARNING, "RE_RegisterModel: unknown fileid for %s\n", name);
				goto fail;
			}

			loaded = R_LoadMD3(mod, lod, buffer, bufferLen, name, forceStatic);
		}

		ri.FS_FreeFile(buffer);

		if(!loaded)
		{
			if(lod == 0)
			{
				goto fail;
			}
			else
			{
				break;
			}
		}
		else
		{
			// make sure the VBO glState entries are save
			R_BindNullVBO();
			R_BindNullIBO();

			mod->numLods++;
			numLoaded++;
			// if we have a valid model and are biased
			// so that we won't see any higher detail ones,
			// stop loading them
//          if ( lod <= r_lodbias->integer ) {
//              break;
//          }
		}
	}

	if(numLoaded)
	{
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for(lod--; lod >= 0; lod--)
		{
			mod->numLods++;
			mod->mdx[lod] = mod->mdx[lod + 1];
		}

		return mod->index;
	}
#ifdef _DEBUG
	else
	{
		ri.Printf(PRINT_WARNING, "couldn't load '%s'\n", name);
	}
#endif

  fail:
	// we still keep the model_t around, so if the model name is asked for
	// again, we won't bother scanning the filesystem
	mod->type = MOD_BAD;
	return 0;
}


/*
=================
MDXSurfaceCompare
compare function for qsort()
=================
*/
static int MDXSurfaceCompare(const void *a, const void *b)
{
	mdxSurface_t   *aa, *bb;

	aa = *(mdxSurface_t **) a;
	bb = *(mdxSurface_t **) b;

	// shader first
	if(&aa->shader < &bb->shader)
		return -1;

	else if(&aa->shader > &bb->shader)
		return 1;

	return 0;
}

/*
=================
R_LoadMD3
=================
*/
static qboolean R_LoadMD3(model_t * mod, int lod, void *buffer, int bufferSize, const char *modName, qboolean forceStatic)
{
	int             i, j, k, l;

	md3Header_t    *md3Model;
	md3Frame_t     *md3Frame;
	md3Surface_t   *md3Surf;
	md3Shader_t    *md3Shader;
	md3Triangle_t  *md3Tri;
	md3St_t        *md3st;
	md3XyzNormal_t *md3xyz;
	md3Tag_t       *md3Tag;

	mdxModel_t     *mdxModel;
	mdxFrame_t     *frame;
	mdxSurface_t   *surf, *surface;
	srfTriangle_t  *tri;
	mdxVertex_t    *v;
	mdxSt_t        *st;
	mdxTag_t       *tag;

	int             version;
	int             size;

	md3Model = (md3Header_t *) buffer;

	version = LittleLong(md3Model->version);
	if(version != MD3_VERSION)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has wrong version (%i should be %i)\n", modName, version, MD3_VERSION);
		return qfalse;
	}

	mod->type = MOD_MDX;
	size = LittleLong(md3Model->ofsEnd);
	mod->dataSize += size;
	mdxModel = mod->mdx[lod] = ri.Hunk_Alloc(sizeof(mdxModel_t), h_low);

//  Com_Memcpy(mod->md3[lod], buffer, LittleLong(md3Model->ofsEnd));

	LL(md3Model->ident);
	LL(md3Model->version);
	LL(md3Model->numFrames);
	LL(md3Model->numTags);
	LL(md3Model->numSurfaces);
	LL(md3Model->ofsFrames);
	LL(md3Model->ofsTags);
	LL(md3Model->ofsSurfaces);
	LL(md3Model->ofsEnd);

	if(md3Model->numFrames < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD3: %s has no frames\n", modName);
		return qfalse;
	}

	// swap all the frames
	mdxModel->numFrames = md3Model->numFrames;
	mdxModel->frames = frame = ri.Hunk_Alloc(sizeof(*frame) * md3Model->numFrames, h_low);

	md3Frame = (md3Frame_t *) ((byte *) md3Model + md3Model->ofsFrames);
	for(i = 0; i < md3Model->numFrames; i++, frame++, md3Frame++)
	{
		frame->radius = LittleFloat(md3Frame->radius);
		for(j = 0; j < 3; j++)
		{
			frame->bounds[0][j] = LittleFloat(md3Frame->bounds[0][j]);
			frame->bounds[1][j] = LittleFloat(md3Frame->bounds[1][j]);
			frame->localOrigin[j] = LittleFloat(md3Frame->localOrigin[j]);
		}
	}

	// swap all the tags
	mdxModel->numTags = md3Model->numTags;
	mdxModel->tags = tag = ri.Hunk_Alloc(sizeof(*tag) * (md3Model->numTags * md3Model->numFrames), h_low);

	md3Tag = (md3Tag_t *) ((byte *) md3Model + md3Model->ofsTags);
	for(i = 0; i < md3Model->numTags * md3Model->numFrames; i++, tag++, md3Tag++)
	{
		for(j = 0; j < 3; j++)
		{
			tag->origin[j] = LittleFloat(md3Tag->origin[j]);
			tag->axis[0][j] = LittleFloat(md3Tag->axis[0][j]);
			tag->axis[1][j] = LittleFloat(md3Tag->axis[1][j]);
			tag->axis[2][j] = LittleFloat(md3Tag->axis[2][j]);
		}

		Q_strncpyz(tag->name, md3Tag->name, sizeof(tag->name));
	}

	// swap all the surfaces
	mdxModel->numSurfaces = md3Model->numSurfaces;
	mdxModel->surfaces = surf = ri.Hunk_Alloc(sizeof(*surf) * md3Model->numSurfaces, h_low);

	md3Surf = (md3Surface_t *) ((byte *) md3Model + md3Model->ofsSurfaces);
	for(i = 0; i < md3Model->numSurfaces; i++)
	{
		LL(md3Surf->ident);
		LL(md3Surf->flags);
		LL(md3Surf->numFrames);
		LL(md3Surf->numShaders);
		LL(md3Surf->numTriangles);
		LL(md3Surf->ofsTriangles);
		LL(md3Surf->numVerts);
		LL(md3Surf->ofsShaders);
		LL(md3Surf->ofsSt);
		LL(md3Surf->ofsXyzNormals);
		LL(md3Surf->ofsEnd);

		if(md3Surf->numVerts > SHADER_MAX_VERTEXES)
		{
			ri.Error(ERR_DROP, "R_LoadMD3: %s has more than %i verts on a surface (%i)",
					 modName, SHADER_MAX_VERTEXES, md3Surf->numVerts);
		}
		if(md3Surf->numTriangles * 3 > SHADER_MAX_INDEXES)
		{
			ri.Error(ERR_DROP, "R_LoadMD3: %s has more than %i triangles on a surface (%i)",
					 modName, SHADER_MAX_INDEXES / 3, md3Surf->numTriangles);
		}

		// change to surface identifier
		surf->surfaceType = SF_MDX;

		// give pointer to model for Tess_SurfaceMDX
		surf->model = mdxModel;

		// copy surface name
		Q_strncpyz(surf->name, md3Surf->name, sizeof(surf->name));

		// lowercase the surface name so skin compares are faster
		Q_strlwr(surf->name);

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen(surf->name);
		if(j > 2 && surf->name[j - 2] == '_')
		{
			surf->name[j - 2] = 0;
		}

		// register the shaders
		/*
		   surf->numShaders = md3Surf->numShaders;
		   surf->shaders = shader = ri.Hunk_Alloc(sizeof(*shader) * md3Surf->numShaders, h_low);

		   md3Shader = (md3Shader_t *) ((byte *) md3Surf + md3Surf->ofsShaders);
		   for(j = 0; j < md3Surf->numShaders; j++, shader++, md3Shader++)
		   {
		   shader_t       *sh;

		   sh = R_FindShader(md3Shader->name, SHADER_3D_DYNAMIC, qtrue);
		   if(sh->defaultShader)
		   {
		   shader->shaderIndex = 0;
		   }
		   else
		   {
		   shader->shaderIndex = sh->index;
		   }
		   }
		 */

		// only consider the first shader
		md3Shader = (md3Shader_t *) ((byte *) md3Surf + md3Surf->ofsShaders);
		surf->shader = R_FindShader(md3Shader->name, SHADER_3D_DYNAMIC, qtrue);

		// swap all the triangles
		surf->numTriangles = md3Surf->numTriangles;
		surf->triangles = tri = ri.Hunk_Alloc(sizeof(*tri) * md3Surf->numTriangles, h_low);

		md3Tri = (md3Triangle_t *) ((byte *) md3Surf + md3Surf->ofsTriangles);
		for(j = 0; j < md3Surf->numTriangles; j++, tri++, md3Tri++)
		{
			tri->indexes[0] = LittleLong(md3Tri->indexes[0]);
			tri->indexes[1] = LittleLong(md3Tri->indexes[1]);
			tri->indexes[2] = LittleLong(md3Tri->indexes[2]);
		}

		R_CalcSurfaceTriangleNeighbors(surf->numTriangles, surf->triangles);

		// swap all the XyzNormals
		surf->numVerts = md3Surf->numVerts;
		surf->verts = v = ri.Hunk_Alloc(sizeof(*v) * (md3Surf->numVerts * md3Surf->numFrames), h_low);

		md3xyz = (md3XyzNormal_t *) ((byte *) md3Surf + md3Surf->ofsXyzNormals);
		for(j = 0; j < md3Surf->numVerts * md3Surf->numFrames; j++, md3xyz++, v++)
		{
			v->xyz[0] = LittleShort(md3xyz->xyz[0]);
			v->xyz[1] = LittleShort(md3xyz->xyz[1]);
			v->xyz[2] = LittleShort(md3xyz->xyz[2]);
		}

		// swap all the ST
		surf->st = st = ri.Hunk_Alloc(sizeof(*st) * md3Surf->numVerts, h_low);

		md3st = (md3St_t *) ((byte *) md3Surf + md3Surf->ofsSt);
		for(j = 0; j < md3Surf->numVerts; j++, md3st++, st++)
		{
			st->st[0] = LittleFloat(md3st->st[0]);
			st->st[1] = LittleFloat(md3st->st[1]);
		}

		// find the next surface
		md3Surf = (md3Surface_t *) ((byte *) md3Surf + md3Surf->ofsEnd);
		surf++;
	}

	// build static VBO surfaces
	if(r_vboModels->integer && forceStatic)
	{
		int             vertexesNum;
		byte           *data;
		int             dataSize;
		int             dataOfs;

		GLuint          ofsTexCoords;
		GLuint          ofsTangents;
		GLuint          ofsBinormals;
		GLuint          ofsNormals;
		GLuint          ofsColors;

		int             indexesNum;
		byte           *indexes;
		int             indexesSize;
		int             indexesOfs;

		shader_t       *shader, *oldShader;

		int             numSurfaces;
		mdxSurface_t  **surfacesSorted;

		vec4_t          tmp;
		int             index;

		static vec3_t   xyzs[SHADER_MAX_VERTEXES];
		static vec2_t   texcoords[SHADER_MAX_VERTEXES];
		static vec3_t   tangents[SHADER_MAX_VERTEXES];
		static vec3_t   binormals[SHADER_MAX_VERTEXES];
		static vec3_t   normals[SHADER_MAX_VERTEXES];
		static int      indexes2[SHADER_MAX_INDEXES];

		growList_t      vboSurfaces;
		srfVBOMesh_t   *vboSurf;

		vec4_t          tmpColor = { 1, 1, 1, 1 };


		//ri.Printf(PRINT_ALL, "...trying to calculate VBOs for model '%s'\n", modName);

		// count number of surfaces that we want to merge
		numSurfaces = 0;
		for(i = 0, surf = mdxModel->surfaces; i < mdxModel->numSurfaces; i++, surf++)
		{
			// remove all deformVertexes surfaces
			shader = surf->shader;
			if(shader->numDeforms)
				continue;

			numSurfaces++;
		}

		// build surfaces list
		surfacesSorted = ri.Hunk_AllocateTempMemory(numSurfaces * sizeof(surfacesSorted[0]));

		for(i = 0, surf = mdxModel->surfaces; i < numSurfaces; i++, surf++)
		{
			surfacesSorted[i] = surf;
		}

		// sort interaction caches by shader
		qsort(surfacesSorted, numSurfaces, sizeof(surfacesSorted), MDXSurfaceCompare);

		// create a VBO for each shader
		shader = oldShader = NULL;

		Com_InitGrowList(&vboSurfaces, 10);

		for(k = 0; k < numSurfaces; k++)
		{
			surf = surfacesSorted[k];
			shader = surf->shader;

			if(shader != oldShader)
			{
				oldShader = shader;

				// count vertices and indices
				vertexesNum = 0;
				indexesNum = 0;

				for(l = k; l < numSurfaces; l++)
				{
					surface = surfacesSorted[l];

					if(surface->shader != shader)
						continue;

					indexesNum += surface->numTriangles * 3;
					vertexesNum += surface->numVerts;
				}

				if(!vertexesNum || !indexesNum)
					continue;

				//ri.Printf(PRINT_ALL, "...calculating MD3 mesh VBOs ( %s, %i verts %i tris )\n", shader->name, vertexesNum, indexesNum / 3);

				// create surface
				vboSurf = ri.Hunk_Alloc(sizeof(*vboSurf), h_low);
				Com_AddToGrowList(&vboSurfaces, vboSurf);

				vboSurf->surfaceType = SF_VBO_MESH;
				vboSurf->shader = shader;
				vboSurf->lightmapNum = -1;
				vboSurf->numIndexes = indexesNum;
				vboSurf->numVerts = vertexesNum;

				dataSize = vertexesNum * (sizeof(vec4_t) * 7);
				data = ri.Hunk_AllocateTempMemory(dataSize);
				dataOfs = 0;
				vertexesNum = 0;

				indexesSize = indexesNum * sizeof(int);
				indexes = ri.Hunk_AllocateTempMemory(indexesSize);
				indexesOfs = 0;
				indexesNum = 0;

				// build triangle indices
				for(l = k; l < numSurfaces; l++)
				{
					surface = surfacesSorted[l];

					if(surface->shader != shader)
						continue;

					// set up triangle indices
					if(surface->numTriangles)
					{
						srfTriangle_t  *tri;

						for(i = 0, tri = surface->triangles; i < surface->numTriangles; i++, tri++)
						{
							for(j = 0; j < 3; j++)
							{
								index = vertexesNum + tri->indexes[j];

								Com_Memcpy(indexes + indexesOfs, &index, sizeof(int));
								indexesOfs += sizeof(int);
							}

							for(j = 0; j < 3; j++)
							{
								indexes2[indexesNum + i * 3 + j] = vertexesNum + tri->indexes[j];
							}
						}

						indexesNum += surface->numTriangles * 3;
					}

					if(surface->numVerts)
						vertexesNum += surface->numVerts;
				}

				// don't forget to recount vertexesNum
				vertexesNum = 0;

				// feed vertex XYZ
				for(l = k; l < numSurfaces; l++)
				{
					surface = surfacesSorted[l];

					if(surface->shader != shader)
						continue;

					if(surface->numVerts)
					{
						// set up xyz array
						for(i = 0; i < surface->numVerts; i++)
						{
							for(j = 0; j < 3; j++)
							{
								tmp[j] = surface->verts[i].xyz[j] * MD3_XYZ_SCALE;
							}
							tmp[3] = 1;
							Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
							dataOfs += sizeof(vec4_t);
						}
					}
				}

				// feed vertex texcoords
				ofsTexCoords = dataOfs;
				for(l = k; l < numSurfaces; l++)
				{
					surface = surfacesSorted[l];

					if(surface->shader != shader)
						continue;

					if(surface->numVerts)
					{
						// set up xyz array
						for(i = 0; i < surface->numVerts; i++)
						{
							for(j = 0; j < 2; j++)
							{
								tmp[j] = surface->st[i].st[j];
							}
							tmp[2] = 0;
							tmp[3] = 1;
							Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
							dataOfs += sizeof(vec4_t);

						}
					}
				}

				// prepare positions and texcoords for tangent space calculations
				for(l = k; l < numSurfaces; l++)
				{
					surface = surfacesSorted[l];

					if(surface->shader != shader)
						continue;

					if(surface->numVerts)
					{
						// set up xyz array
						for(i = 0; i < surface->numVerts; i++)
						{
							for(j = 0; j < 3; j++)
							{
								xyzs[vertexesNum + i][j] = surface->verts[i].xyz[j] * MD3_XYZ_SCALE;
							}
							for(j = 0; j < 2; j++)
							{
								texcoords[vertexesNum + i][j] = surface->st[i].st[j];
							}
						}

						vertexesNum += surface->numVerts;
					}
				}

				// calc tangent spaces
				{
					float          *v;
					const float    *v0, *v1, *v2;
					const float    *t0, *t1, *t2;
					vec3_t          tangent;
					vec3_t          binormal;
					vec3_t          normal;

					for(i = 0; i < vertexesNum; i++)
					{
						VectorClear(tangents[i]);
						VectorClear(binormals[i]);
						VectorClear(normals[i]);
					}

					for(i = 0; i < indexesNum; i += 3)
					{
						v0 = xyzs[indexes2[i + 0]];
						v1 = xyzs[indexes2[i + 1]];
						v2 = xyzs[indexes2[i + 2]];

						t0 = texcoords[indexes2[i + 0]];
						t1 = texcoords[indexes2[i + 1]];
						t2 = texcoords[indexes2[i + 2]];

						R_CalcTangentSpace(tangent, binormal, normal, v0, v1, v2, t0, t1, t2);

						for(j = 0; j < 3; j++)
						{
							v = tangents[indexes2[i + j]];
							VectorAdd(v, tangent, v);

							v = binormals[indexes2[i + j]];
							VectorAdd(v, binormal, v);

							v = normals[indexes2[i + j]];
							VectorAdd(v, normal, v);
						}
					}

					for(i = 0; i < vertexesNum; i++)
					{
						VectorNormalize(tangents[i]);
						VectorNormalize(binormals[i]);
						VectorNormalize(normals[i]);
					}

					// do another extra smoothing for normals to avoid flat shading
					for(i = 0; i < vertexesNum; i++)
					{
						for(j = 0; j < vertexesNum; j++)
						{
							if(i == j)
								continue;

							if(VectorCompare(xyzs[i], xyzs[j]))
							{
								VectorAdd(normals[i], normals[j], normals[i]);
							}
						}

						VectorNormalize(normals[i]);
					}
				}

				// feed vertex tangents
				ofsTangents = dataOfs;
				for(i = 0; i < vertexesNum; i++)
				{
					for(j = 0; j < 3; j++)
					{
						tmp[j] = tangents[i][j];
					}
					tmp[3] = 1;
					Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
					dataOfs += sizeof(vec4_t);
				}

				// feed vertex binormals
				ofsBinormals = dataOfs;
				for(i = 0; i < vertexesNum; i++)
				{
					for(j = 0; j < 3; j++)
					{
						tmp[j] = binormals[i][j];
					}
					tmp[3] = 1;
					Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
					dataOfs += sizeof(vec4_t);
				}

				// feed vertex normals
				ofsNormals = dataOfs;
				for(i = 0; i < vertexesNum; i++)
				{
					for(j = 0; j < 3; j++)
					{
						tmp[j] = normals[i][j];
					}
					tmp[3] = 1;
					Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
					dataOfs += sizeof(vec4_t);
				}

				// feed vertex colors
				ofsColors = dataOfs;
				for(i = 0; i < vertexesNum; i++)
				{
					Com_Memcpy(data + dataOfs, tmpColor, sizeof(vec4_t));
					dataOfs += sizeof(vec4_t);
				}

				vboSurf->vbo =
					R_CreateVBO(va("staticMD3Mesh_VBO %i", vboSurfaces.currentElements), data, dataSize, GL_STATIC_DRAW_ARB);
				vboSurf->vbo->ofsXYZ = 0;
				vboSurf->vbo->ofsTexCoords = ofsTexCoords;
				vboSurf->vbo->ofsLightCoords = ofsTexCoords;
				vboSurf->vbo->ofsTangents = ofsTangents;
				vboSurf->vbo->ofsBinormals = ofsBinormals;
				vboSurf->vbo->ofsNormals = ofsNormals;
				vboSurf->vbo->ofsColors = ofsColors;
				vboSurf->vbo->ofsLightCoords = ofsColors;		// not required anyway
				vboSurf->vbo->ofsLightDirections = ofsColors;	// not required anyway

				vboSurf->ibo =
					R_CreateIBO(va("staticMD3Mesh_IBO %i", vboSurfaces.currentElements), indexes, indexesSize,
								GL_STATIC_DRAW_ARB);

				ri.Hunk_FreeTempMemory(indexes);
				ri.Hunk_FreeTempMemory(data);

				// megs
				/*
				   ri.Printf(PRINT_ALL, "md3 mesh data VBO size: %d.%02d MB\n", dataSize / (1024 * 1024),
				   (dataSize % (1024 * 1024)) * 100 / (1024 * 1024));
				   ri.Printf(PRINT_ALL, "md3 mesh tris VBO size: %d.%02d MB\n", indexesSize / (1024 * 1024),
				   (indexesSize % (1024 * 1024)) * 100 / (1024 * 1024));
				 */

			}
		}

		ri.Hunk_FreeTempMemory(surfacesSorted);

		// move VBO surfaces list to hunk
		mdxModel->numVBOSurfaces = vboSurfaces.currentElements;
		mdxModel->vboSurfaces = ri.Hunk_Alloc(mdxModel->numVBOSurfaces * sizeof(*mdxModel->vboSurfaces), h_low);

		for(i = 0; i < mdxModel->numVBOSurfaces; i++)
		{
			mdxModel->vboSurfaces[i] = (srfVBOMesh_t *) Com_GrowListElement(&vboSurfaces, i);
		}

		Com_DestroyGrowList(&vboSurfaces);

		//ri.Printf(PRINT_ALL, "%i MD3 VBO surfaces created\n", mdxModel->numVBOSurfaces);
	}

	return qtrue;
}

//=============================================================================


typedef struct
{
	int             indexes[3];
	md5Vertex_t    *vertexes[3];
	qboolean		referenced;
} skelTriangle_t;

static int CompareBoneIndices(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static int CompareTrianglesByBoneReferences(const void *a, const void *b)
{
	int             i, j, k;

	skelTriangle_t *t1, *t2;
	md5Vertex_t    *v1, *v2;
	int				b1[MAX_BONES], b2[MAX_BONES];
	int				s1, s2;

	t1 = (skelTriangle_t *) *(void **)a;
	t2 = (skelTriangle_t *) *(void **)b;

#if 1
	for(i = 0; i < MAX_BONES; i++)
	{
		b1[i] = b2[i] = 0;
	}

	for(i = 0; i < 3; i++)
	{
		v1 = t1->vertexes[i];
		v2 = t1->vertexes[i];

		for(j = 0; j < MAX_WEIGHTS; j++)
		{
			if(j < v1->numWeights)
			{
				b1[v1->weights[j]->boneIndex]++;
			}

			if(j < v2->numWeights)
			{
				b1[v2->weights[j]->boneIndex]++;
			}
		}
	}

	qsort(b1, MAX_WEIGHTS * 3, sizeof(int), CompareBoneIndices);
	qsort(b2, MAX_WEIGHTS * 3, sizeof(int), CompareBoneIndices);

	for(j = 0; j < MAX_BONES; j++)
	{
		if(b1[j] < b2[j])
			return -1;

		if(b1[j] > b2[j])
			return 1;
	}
#else

	// calculate the bone sums
	s1 = s2 = 0;
	for(i = 0; i < 3; i++)
	{
		v1 = t1->vertexes[i];
		v2 = t1->vertexes[i];

		for(j = 0; j < MAX_WEIGHTS; j++)
		{
			s1 = (j < v1->numWeights) ? (v1->weights[j]->boneIndex * v1->weights[j]->boneWeight) : 0;
			s2 = (j < v2->numWeights) ? (v2->weights[j]->boneIndex * v2->weights[j]->boneWeight) : 0;
		}
	}

	if(s1 < s2)
		return -1;

	if(s1 > s2)
		return 1;

#endif

	return 0;
}




static qboolean AddTriangleToVBOTriangleList(growList_t * vboTriangles, skelTriangle_t * tri, int * numBoneReferences, int boneReferences[MAX_BONES])
{
	int				i, j, k;
	md5Vertex_t    *v;
	int				boneIndex;
	int				numNewReferences;
	int				newReferences[MAX_WEIGHTS * 3];	// a single triangle can have up to 12 new bone references !
	qboolean		hasWeights;

	hasWeights = qfalse;

	numNewReferences = 0;
	Com_Memset(newReferences, -1, sizeof(newReferences));

	for(i = 0; i < 3; i++)
	{
		v = tri->vertexes[i];

		// can the bones be referenced?
		for(j = 0; j < MAX_WEIGHTS; j++)
		{
			if(j < v->numWeights)
			{
				boneIndex = v->weights[j]->boneIndex;
				hasWeights = qtrue;

				// is the bone already referenced?
				if(!boneReferences[boneIndex])
				{
					// the bone isn't yet and we have to test if we can give the mesh this bone at all
					if((*numBoneReferences + numNewReferences) >= glConfig.maxVertexSkinningBones)
					{
						return qfalse;
					}
					else
					{
						for(k = 0; k < (MAX_WEIGHTS * 3); k++)
						{
							if(newReferences[k] == boneIndex)
								break;
						}

						if(k == (MAX_WEIGHTS * 3))
						{
							newReferences[numNewReferences] = boneIndex;
							numNewReferences++;
						}
					}
				}
			}
		}
	}

	// reference them!
	for(j = 0; j < numNewReferences; j++)
	{
		boneIndex = newReferences[j];

		boneReferences[boneIndex]++;

		*numBoneReferences = *numBoneReferences + 1;
	}

#if 0
	if(numNewReferences)
	{
		ri.Printf(PRINT_ALL, "bone indices: %i %i %i %i %i %i %i %i %i %i %i %i\n",
				newReferences[0],
				newReferences[1],
				newReferences[2],
				newReferences[3],
				newReferences[4],
				newReferences[5],
				newReferences[6],
				newReferences[7],
				newReferences[8],
				newReferences[9],
				newReferences[10],
				newReferences[11]);
	}
#endif

	if(hasWeights)
	{
		Com_AddToGrowList(vboTriangles, tri);
		return qtrue;
	}

	return qfalse;
}

static void AddSurfaceToVBOSurfacesList(growList_t * vboSurfaces, growList_t * vboTriangles, md5Model_t * md5, md5Surface_t * surf, int skinIndex, int numBoneReferences, int boneReferences[MAX_BONES])
{
	int				i, j, k;

	int             vertexesNum;
	byte           *data;
	int             dataSize;
	int             dataOfs;

	GLuint          ofsTexCoords;
	GLuint          ofsTangents;
	GLuint          ofsBinormals;
	GLuint          ofsNormals;
	GLuint          ofsColors;
	GLuint          ofsBoneIndexes;
	GLuint          ofsBoneWeights;

	int             indexesNum;
	byte           *indexes;
	int             indexesSize;
	int             indexesOfs;

	skelTriangle_t  *tri;

	vec4_t          tmp;
	int             index;

	srfVBOMD5Mesh_t *vboSurf;
	md5Vertex_t     *v;

	vec4_t          tmpColor = { 1, 1, 1, 1 };

	vertexesNum = surf->numVerts;
	indexesNum = vboTriangles->currentElements * 3;

	// create surface
	vboSurf = ri.Hunk_Alloc(sizeof(*vboSurf), h_low);
	Com_AddToGrowList(vboSurfaces, vboSurf);

	vboSurf->surfaceType = SF_VBO_MD5MESH;
	vboSurf->md5Model = md5;
	vboSurf->shader = R_GetShaderByHandle(surf->shaderIndex);
	vboSurf->skinIndex = skinIndex;
	vboSurf->numIndexes = indexesNum;
	vboSurf->numVerts = vertexesNum;

	dataSize = vertexesNum * (sizeof(vec4_t) * 8);
	data = ri.Hunk_AllocateTempMemory(dataSize);
	dataOfs = 0;

	indexesSize = indexesNum * sizeof(int);
	indexes = ri.Hunk_AllocateTempMemory(indexesSize);
	indexesOfs = 0;

	//ri.Printf(PRINT_ALL, "AddSurfaceToVBOSurfacesList( %i verts, %i tris )\n", surf->numVerts, vboTriangles->currentElements);

	vboSurf->numBoneRemap = 0;
	Com_Memset(vboSurf->boneRemap, 0, sizeof(vboSurf->boneRemap));
	Com_Memset(vboSurf->boneRemapInverse, 0, sizeof(vboSurf->boneRemapInverse));

	//ri.Printf(PRINT_ALL, "referenced bones: ");
	for(j = 0; j < MAX_BONES; j++)
	{
		if(boneReferences[j] > 0)
		{
			vboSurf->boneRemap[j] = vboSurf->numBoneRemap;
			vboSurf->boneRemapInverse[vboSurf->numBoneRemap] = j;

			vboSurf->numBoneRemap++;

			//ri.Printf(PRINT_ALL, "(%i -> %i) ", j, vboSurf->boneRemap[j]);
		}
	}
	//ri.Printf(PRINT_ALL, "\n");

	//for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
	for(j = 0; j < vboTriangles->currentElements; j++)
	{
		tri = Com_GrowListElement(vboTriangles, j);

		for(k = 0; k < 3; k++)
		{
			index = tri->indexes[k];

			Com_Memcpy(indexes + indexesOfs, &index, sizeof(int));
			indexesOfs += sizeof(int);
		}
	}

	// feed vertex XYZ
	for(j = 0; j < vertexesNum; j++)
	{
		for(k = 0; k < 3; k++)
		{
			tmp[k] = surf->verts[j].position[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex texcoords
	ofsTexCoords = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		for(k = 0; k < 2; k++)
		{
			tmp[k] = surf->verts[j].texCoords[k];
		}
		tmp[2] = 0;
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex tangents
	ofsTangents = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		for(k = 0; k < 3; k++)
		{
			tmp[k] = surf->verts[j].tangent[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex binormals
	ofsBinormals = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		for(k = 0; k < 3; k++)
		{
			tmp[k] = surf->verts[j].binormal[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex normals
	ofsNormals = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		for(k = 0; k < 3; k++)
		{
			tmp[k] = surf->verts[j].normal[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex colors
	ofsColors = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		Com_Memcpy(data + dataOfs, tmpColor, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed bone indices
	ofsBoneIndexes = dataOfs;
	for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
	{
		for(k = 0; k < MAX_WEIGHTS; k++)
		{
			if(k < v->numWeights)
				index = vboSurf->boneRemap[v->weights[k]->boneIndex];
			else
				index = 0;

			Com_Memcpy(data + dataOfs, &index, sizeof(int));
			dataOfs += sizeof(int);
		}
	}

	// feed bone weights
	ofsBoneWeights = dataOfs;
	for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
	{
		for(k = 0; k < MAX_WEIGHTS; k++)
		{
			if(k < v->numWeights)
				tmp[k] = v->weights[k]->boneWeight;
			else
				tmp[k] = 0;
		}
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	vboSurf->vbo = R_CreateVBO(va("staticMD5Mesh_VBO %i", vboSurfaces->currentElements), data, dataSize, GL_STATIC_DRAW_ARB);
	vboSurf->vbo->ofsXYZ = 0;
	vboSurf->vbo->ofsTexCoords = ofsTexCoords;
	vboSurf->vbo->ofsLightCoords = ofsTexCoords;
	vboSurf->vbo->ofsTangents = ofsTangents;
	vboSurf->vbo->ofsBinormals = ofsBinormals;
	vboSurf->vbo->ofsNormals = ofsNormals;
	vboSurf->vbo->ofsColors = ofsColors;
	vboSurf->vbo->ofsLightCoords = ofsColors;		// not required anyway
	vboSurf->vbo->ofsLightDirections = ofsColors;	// not required anyway
	vboSurf->vbo->ofsBoneIndexes = ofsBoneIndexes;
	vboSurf->vbo->ofsBoneWeights = ofsBoneWeights;

	vboSurf->ibo = R_CreateIBO(va("staticMD5Mesh_IBO %i", vboSurfaces->currentElements), indexes, indexesSize, GL_STATIC_DRAW_ARB);

	ri.Hunk_FreeTempMemory(indexes);
	ri.Hunk_FreeTempMemory(data);

	// megs
	/*
	   ri.Printf(PRINT_ALL, "md5 mesh data VBO size: %d.%02d MB\n", dataSize / (1024 * 1024),
	   (dataSize % (1024 * 1024)) * 100 / (1024 * 1024));
	   ri.Printf(PRINT_ALL, "md5 mesh tris VBO size: %d.%02d MB\n", indexesSize / (1024 * 1024),
	   (indexesSize % (1024 * 1024)) * 100 / (1024 * 1024));
	 */
}

static void AddSurfaceToVBOSurfacesList2(growList_t * vboSurfaces, growList_t * vboTriangles, growList_t * vboVertexes, md5Model_t * md5, int skinIndex, const char *materialName, int numBoneReferences, int boneReferences[MAX_BONES])
{
	int				i, j, k;

	int             vertexesNum;
	byte           *data;
	int             dataSize;
	int             dataOfs;

	GLuint          ofsTexCoords;
	GLuint          ofsTangents;
	GLuint          ofsBinormals;
	GLuint          ofsNormals;
	GLuint          ofsColors;
	GLuint          ofsBoneIndexes;
	GLuint          ofsBoneWeights;

	int             indexesNum;
	byte           *indexes;
	int             indexesSize;
	int             indexesOfs;

	skelTriangle_t  *tri;

	vec4_t          tmp;
	int             index;

	srfVBOMD5Mesh_t *vboSurf;
	md5Vertex_t     *v;

	vec4_t          tmpColor = { 1, 1, 1, 1 };

	shader_t       *shader;
	int				shaderIndex;

	vertexesNum = vboVertexes->currentElements;
	indexesNum = vboTriangles->currentElements * 3;

	// create surface
	vboSurf = ri.Hunk_Alloc(sizeof(*vboSurf), h_low);
	Com_AddToGrowList(vboSurfaces, vboSurf);

	vboSurf->surfaceType = SF_VBO_MD5MESH;
	vboSurf->md5Model = md5;

	ri.Printf(PRINT_ALL, "AddSurfaceToVBOSurfacesList2: loading shader '%s'", materialName);
	shader = R_FindShader(materialName, SHADER_3D_DYNAMIC, qtrue);
	if(shader->defaultShader)
	{
		shaderIndex = 0;
	}
	else
	{
		shaderIndex = shader->index;
	}
	vboSurf->shader = R_GetShaderByHandle(shaderIndex);

	vboSurf->skinIndex = skinIndex;
	vboSurf->numIndexes = indexesNum;
	vboSurf->numVerts = vertexesNum;

	dataSize = vertexesNum * (sizeof(vec4_t) * 8);
	data = ri.Hunk_AllocateTempMemory(dataSize);
	dataOfs = 0;

	indexesSize = indexesNum * sizeof(int);
	indexes = ri.Hunk_AllocateTempMemory(indexesSize);
	indexesOfs = 0;

	//ri.Printf(PRINT_ALL, "AddSurfaceToVBOSurfacesList( %i verts, %i tris )\n", surf->numVerts, vboTriangles->currentElements);

	vboSurf->numBoneRemap = 0;
	Com_Memset(vboSurf->boneRemap, 0, sizeof(vboSurf->boneRemap));
	Com_Memset(vboSurf->boneRemapInverse, 0, sizeof(vboSurf->boneRemapInverse));

	//ri.Printf(PRINT_ALL, "referenced bones: ");
	for(j = 0; j < MAX_BONES; j++)
	{
		if(boneReferences[j] > 0)
		{
			vboSurf->boneRemap[j] = vboSurf->numBoneRemap;
			vboSurf->boneRemapInverse[vboSurf->numBoneRemap] = j;

			vboSurf->numBoneRemap++;

			//ri.Printf(PRINT_ALL, "(%i -> %i) ", j, vboSurf->boneRemap[j]);
		}
	}
	//ri.Printf(PRINT_ALL, "\n");

	//for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
	for(j = 0; j < vboTriangles->currentElements; j++)
	{
		tri = Com_GrowListElement(vboTriangles, j);

		for(k = 0; k < 3; k++)
		{
			index = tri->indexes[k];

			Com_Memcpy(indexes + indexesOfs, &index, sizeof(int));
			indexesOfs += sizeof(int);
		}
	}

	// feed vertex XYZ
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < 3; k++)
		{
			tmp[k] = v->position[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex texcoords
	ofsTexCoords = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < 2; k++)
		{
			tmp[k] = v->texCoords[k];
		}
		tmp[2] = 0;
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex tangents
	ofsTangents = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < 3; k++)
		{
			tmp[k] = v->tangent[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex binormals
	ofsBinormals = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < 3; k++)
		{
			tmp[k] = v->binormal[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex normals
	ofsNormals = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < 3; k++)
		{
			tmp[k] = v->normal[k];
		}
		tmp[3] = 1;
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed vertex colors
	ofsColors = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		Com_Memcpy(data + dataOfs, tmpColor, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	// feed bone indices
	ofsBoneIndexes = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < MAX_WEIGHTS; k++)
		{
			if(k < v->numWeights)
				index = vboSurf->boneRemap[v->weights[k]->boneIndex];
			else
				index = 0;

			Com_Memcpy(data + dataOfs, &index, sizeof(int));
			dataOfs += sizeof(int);
		}
	}

	// feed bone weights
	ofsBoneWeights = dataOfs;
	for(j = 0; j < vertexesNum; j++)
	{
		v = Com_GrowListElement(vboVertexes, j);

		for(k = 0; k < MAX_WEIGHTS; k++)
		{
			if(k < v->numWeights)
				tmp[k] = v->weights[k]->boneWeight;
			else
				tmp[k] = 0;
		}
		Com_Memcpy(data + dataOfs, (vec_t *) tmp, sizeof(vec4_t));
		dataOfs += sizeof(vec4_t);
	}

	vboSurf->vbo = R_CreateVBO(va("staticMD5Mesh_VBO %i", vboSurfaces->currentElements), data, dataSize, GL_STATIC_DRAW_ARB);
	vboSurf->vbo->ofsXYZ = 0;
	vboSurf->vbo->ofsTexCoords = ofsTexCoords;
	vboSurf->vbo->ofsLightCoords = ofsTexCoords;
	vboSurf->vbo->ofsTangents = ofsTangents;
	vboSurf->vbo->ofsBinormals = ofsBinormals;
	vboSurf->vbo->ofsNormals = ofsNormals;
	vboSurf->vbo->ofsColors = ofsColors;
	vboSurf->vbo->ofsLightCoords = ofsColors;		// not required anyway
	vboSurf->vbo->ofsLightDirections = ofsColors;	// not required anyway
	vboSurf->vbo->ofsBoneIndexes = ofsBoneIndexes;
	vboSurf->vbo->ofsBoneWeights = ofsBoneWeights;

	vboSurf->ibo = R_CreateIBO(va("staticMD5Mesh_IBO %i", vboSurfaces->currentElements), indexes, indexesSize, GL_STATIC_DRAW_ARB);

	ri.Hunk_FreeTempMemory(indexes);
	ri.Hunk_FreeTempMemory(data);

	// megs
	/*
	   ri.Printf(PRINT_ALL, "md5 mesh data VBO size: %d.%02d MB\n", dataSize / (1024 * 1024),
	   (dataSize % (1024 * 1024)) * 100 / (1024 * 1024));
	   ri.Printf(PRINT_ALL, "md5 mesh tris VBO size: %d.%02d MB\n", indexesSize / (1024 * 1024),
	   (indexesSize % (1024 * 1024)) * 100 / (1024 * 1024));
	 */

	ri.Printf(PRINT_ALL, "created VBO surface %i with %i vertices and %i triangles\n", vboSurfaces->currentElements, vboSurf->numVerts, vboSurf->numIndexes / 3);
}

/*
=================
R_LoadMD5
=================
*/
static qboolean R_LoadMD5(model_t * mod, void *buffer, int bufferSize, const char *modName)
{
	int             i, j, k, l;
	md5Model_t     *md5;
	md5Bone_t      *bone;
	md5Surface_t   *surf;
	srfTriangle_t  *tri;
	md5Vertex_t    *v;
	md5Weight_t    *weight;
	int             version;
	shader_t       *sh;
	char           *buf_p;
	char           *token;
	vec3_t          boneOrigin;
	quat_t          boneQuat;
	matrix_t        boneMat;

	int				numRemaining;
	growList_t		sortedTriangles;
	growList_t      vboTriangles;
	growList_t      vboSurfaces;

	int				numBoneReferences;
	int				boneReferences[MAX_BONES];

	buf_p = (char *)buffer;

	// skip MD5Version indent string
	Com_ParseExt(&buf_p, qfalse);

	// check version
	token = Com_ParseExt(&buf_p, qfalse);
	version = atoi(token);
	if(version != MD5_VERSION)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: %s has wrong version (%i should be %i)\n", modName, version, MD5_VERSION);
		return qfalse;
	}

	mod->type = MOD_MD5;
	mod->dataSize += sizeof(md5Model_t);
	md5 = mod->md5 = ri.Hunk_Alloc(sizeof(md5Model_t), h_low);

	// skip commandline <arguments string>
	token = Com_ParseExt(&buf_p, qtrue);
	token = Com_ParseExt(&buf_p, qtrue);
//  ri.Printf(PRINT_ALL, "%s\n", token);

	// parse numJoints <number>
	token = Com_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numJoints"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numJoints' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	token = Com_ParseExt(&buf_p, qfalse);
	md5->numBones = atoi(token);

	// parse numMeshes <number>
	token = Com_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numMeshes"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numMeshes' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	token = Com_ParseExt(&buf_p, qfalse);
	md5->numSurfaces = atoi(token);
	//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has %i surfaces\n", modName, md5->numSurfaces);


	if(md5->numBones < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: '%s' has no bones\n", modName);
		return qfalse;
	}
	if(md5->numBones > MAX_BONES)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: '%s' has more than %i bones (%i)\n", modName, MAX_BONES, md5->numBones);
		return qfalse;
	}
	//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has %i bones\n", modName, md5->numBones);

	// parse all the bones
	md5->bones = ri.Hunk_Alloc(sizeof(*bone) * md5->numBones, h_low);

	// parse joints {
	token = Com_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "joints"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'joints' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	token = Com_ParseExt(&buf_p, qfalse);
	if(Q_stricmp(token, "{"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '{' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}

	for(i = 0, bone = md5->bones; i < md5->numBones; i++, bone++)
	{
		token = Com_ParseExt(&buf_p, qtrue);
		Q_strncpyz(bone->name, token, sizeof(bone->name));

		//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has bone '%s'\n", modName, bone->name);

		token = Com_ParseExt(&buf_p, qfalse);
		bone->parentIndex = atoi(token);

		//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has bone '%s' with parent index %i\n", modName, bone->name, bone->parentIndex);

		if(bone->parentIndex >= md5->numBones)
		{
			ri.Error(ERR_DROP, "R_LoadMD5: '%s' has bone '%s' with bad parent index %i while numBones is %i\n", modName,
					 bone->name, bone->parentIndex, md5->numBones);
		}

		// skip (
		token = Com_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}

		for(j = 0; j < 3; j++)
		{
			token = Com_ParseExt(&buf_p, qfalse);
			boneOrigin[j] = atof(token);
		}

		// skip )
		token = Com_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected ')' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}

		// skip (
		token = Com_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}

		for(j = 0; j < 3; j++)
		{
			token = Com_ParseExt(&buf_p, qfalse);
			boneQuat[j] = atof(token);
		}
		QuatCalcW(boneQuat);
		MatrixFromQuat(boneMat, boneQuat);

		VectorCopy(boneOrigin, bone->origin);
		QuatCopy(boneQuat, bone->rotation);

		MatrixSetupTransformFromQuat(bone->inverseTransform, boneQuat, boneOrigin);
		MatrixInverse(bone->inverseTransform);

		// skip )
		token = Com_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
	}

	// parse }
	token = Com_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "}"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '}' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}

	// parse all the surfaces
	if(md5->numSurfaces < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: '%s' has no surfaces\n", modName);
		return qfalse;
	}
	//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has %i surfaces\n", modName, md5->numSurfaces);

	md5->surfaces = ri.Hunk_Alloc(sizeof(*surf) * md5->numSurfaces, h_low);
	for(i = 0, surf = md5->surfaces; i < md5->numSurfaces; i++, surf++)
	{
		// parse mesh {
		token = Com_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "mesh"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'mesh' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = Com_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "{"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '{' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}

		// change to surface identifier
		surf->surfaceType = SF_MD5;

		// give pointer to model for Tess_SurfaceMD5
		surf->model = md5;

		// parse shader <name>
		token = Com_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "shader"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'shader' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = Com_ParseExt(&buf_p, qfalse);
		Q_strncpyz(surf->shader, token, sizeof(surf->shader));

		//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' uses shader '%s'\n", modName, surf->shader);

		// FIXME .md5mesh meshes don't have surface names
		// lowercase the surface name so skin compares are faster
		//Q_strlwr(surf->name);
		//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has surface '%s'\n", modName, surf->name);

		// register the shaders
		sh = R_FindShader(surf->shader, SHADER_3D_DYNAMIC, qtrue);
		if(sh->defaultShader)
		{
			surf->shaderIndex = 0;
		}
		else
		{
			surf->shaderIndex = sh->index;
		}

		// parse numVerts <number>
		token = Com_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "numVerts"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numVerts' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = Com_ParseExt(&buf_p, qfalse);
		surf->numVerts = atoi(token);

		if(surf->numVerts > SHADER_MAX_VERTEXES)
		{
			ri.Error(ERR_DROP, "R_LoadMD5: '%s' has more than %i verts on a surface (%i)",
					 modName, SHADER_MAX_VERTEXES, surf->numVerts);
		}

		surf->verts = ri.Hunk_Alloc(sizeof(*v) * surf->numVerts, h_low);
		for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
		{
			// skip vert <number>
			token = Com_ParseExt(&buf_p, qtrue);
			if(Q_stricmp(token, "vert"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'vert' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			Com_ParseExt(&buf_p, qfalse);

			// skip (
			token = Com_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, "("))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}

			for(k = 0; k < 2; k++)
			{
				token = Com_ParseExt(&buf_p, qfalse);
				v->texCoords[k] = atof(token);
			}

			// skip )
			token = Com_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, ")"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected ')' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}

			token = Com_ParseExt(&buf_p, qfalse);
			v->firstWeight = atoi(token);

			token = Com_ParseExt(&buf_p, qfalse);
			v->numWeights = atoi(token);

			if(v->numWeights > MAX_WEIGHTS)
			{
				ri.Error(ERR_DROP, "R_LoadMD5: vertex %i requires more than %i weights on surface (%i) in model '%s'",
						 j, MAX_WEIGHTS, i, modName);
			}
		}

		// parse numTris <number>
		token = Com_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "numTris"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numTris' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = Com_ParseExt(&buf_p, qfalse);
		surf->numTriangles = atoi(token);

		if(surf->numTriangles > SHADER_MAX_TRIANGLES)
		{
			ri.Error(ERR_DROP, "R_LoadMD5: '%s' has more than %i triangles on a surface (%i)",
					 modName, SHADER_MAX_TRIANGLES, surf->numTriangles);
		}

		surf->triangles = ri.Hunk_Alloc(sizeof(*tri) * surf->numTriangles, h_low);
		for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
		{
			// skip tri <number>
			token = Com_ParseExt(&buf_p, qtrue);
			if(Q_stricmp(token, "tri"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'tri' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			Com_ParseExt(&buf_p, qfalse);

			for(k = 0; k < 3; k++)
			{
				token = Com_ParseExt(&buf_p, qfalse);
				tri->indexes[k] = atoi(token);
			}
		}

		R_CalcSurfaceTriangleNeighbors(surf->numTriangles, surf->triangles);

		// parse numWeights <number>
		token = Com_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "numWeights"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numWeights' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = Com_ParseExt(&buf_p, qfalse);
		surf->numWeights = atoi(token);

		surf->weights = ri.Hunk_Alloc(sizeof(*weight) * surf->numWeights, h_low);
		for(j = 0, weight = surf->weights; j < surf->numWeights; j++, weight++)
		{
			// skip weight <number>
			token = Com_ParseExt(&buf_p, qtrue);
			if(Q_stricmp(token, "weight"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'weight' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			Com_ParseExt(&buf_p, qfalse);

			token = Com_ParseExt(&buf_p, qfalse);
			weight->boneIndex = atoi(token);

			token = Com_ParseExt(&buf_p, qfalse);
			weight->boneWeight = atof(token);

			// skip (
			token = Com_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, "("))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}

			for(k = 0; k < 3; k++)
			{
				token = Com_ParseExt(&buf_p, qfalse);
				weight->offset[k] = atof(token);
			}

			// skip )
			token = Com_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, ")"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected ')' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
		}

		// parse }
		token = Com_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "}"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '}' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}

		// loop trough all vertices and set up the vertex weights
		for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
		{
			v->weights = ri.Hunk_Alloc(sizeof(*v->weights) * v->numWeights, h_low);

			for(k = 0; k < v->numWeights; k++)
			{
				v->weights[k] = surf->weights + (v->firstWeight + k);
			}
		}
	}

	// loading is done now calculate the bounding box and tangent spaces
	ClearBounds(md5->bounds[0], md5->bounds[1]);

	for(i = 0, surf = md5->surfaces; i < md5->numSurfaces; i++, surf++)
	{
		for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
		{
			vec3_t          tmpVert;
			md5Weight_t    *w;

			VectorClear(tmpVert);

			for(k = 0, w = v->weights[0]; k < v->numWeights; k++, w++)
			{
				vec3_t          offsetVec;

				bone = &md5->bones[w->boneIndex];

				QuatTransformVector(bone->rotation, w->offset, offsetVec);
				VectorAdd(bone->origin, offsetVec, offsetVec);

				VectorMA(tmpVert, w->boneWeight, offsetVec, tmpVert);
			}

			VectorCopy(tmpVert, v->position);
			AddPointToBounds(tmpVert, md5->bounds[0], md5->bounds[1]);
		}

		// calc tangent spaces
		{
			const float    *v0, *v1, *v2;
			const float    *t0, *t1, *t2;
			vec3_t          tangent;
			vec3_t          binormal;
			vec3_t          normal;

			for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
			{
				VectorClear(v->tangent);
				VectorClear(v->binormal);
				VectorClear(v->normal);
			}

			for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
			{
				v0 = surf->verts[tri->indexes[0]].position;
				v1 = surf->verts[tri->indexes[1]].position;
				v2 = surf->verts[tri->indexes[2]].position;

				t0 = surf->verts[tri->indexes[0]].texCoords;
				t1 = surf->verts[tri->indexes[1]].texCoords;
				t2 = surf->verts[tri->indexes[2]].texCoords;

				R_CalcTangentSpace(tangent, binormal, normal, v0, v1, v2, t0, t1, t2);

				for(k = 0; k < 3; k++)
				{
					float          *v;

					v = surf->verts[tri->indexes[k]].tangent;
					VectorAdd(v, tangent, v);

					v = surf->verts[tri->indexes[k]].binormal;
					VectorAdd(v, binormal, v);

					v = surf->verts[tri->indexes[k]].normal;
					VectorAdd(v, normal, v);
				}
			}

			for(j = 0, v = surf->verts; j < surf->numVerts; j++, v++)
			{
				VectorNormalize(v->tangent);
				VectorNormalize(v->binormal);
				VectorNormalize(v->normal);
			}

			#if 1
			// do another extra smoothing for normals to avoid flat shading
			for(j = 0; j < surf->numVerts; j++)
			{
				for(k = 0; k < surf->numVerts; k++)
				{
					if(j == k)
						continue;

					if(VectorCompare(surf->verts[j].position, surf->verts[k].position))
					{
						VectorAdd(surf->verts[j].normal, surf->verts[k].normal, surf->verts[j].normal);
					}
				}

				VectorNormalize(surf->verts[j].normal);
			}
			#endif

		}
	}

	// split the surfaces into VBO surfaces by the maximum number of GPU vertex skinning bones
	Com_InitGrowList(&vboSurfaces, 10);

	for(i = 0, surf = md5->surfaces; i < md5->numSurfaces; i++, surf++)
	{
		// sort triangles
		Com_InitGrowList(&sortedTriangles, 1000);

		for(j = 0, tri = surf->triangles; j < surf->numTriangles; j++, tri++)
		{
			skelTriangle_t *sortTri = Com_Allocate(sizeof(*sortTri));

			for(k = 0; k < 3; k++)
			{
				sortTri->indexes[k] = tri->indexes[k];
				sortTri->vertexes[k] = &surf->verts[tri->indexes[k]];
			}
			sortTri->referenced = qfalse;

			Com_AddToGrowList(&sortedTriangles, sortTri);
		}

		//qsort(sortedTriangles.elements, sortedTriangles.currentElements, sizeof(void *), CompareTrianglesByBoneReferences);

#if 0
		for(j = 0; j < sortedTriangles.currentElements; j++)
		{
			int		b[MAX_WEIGHTS * 3];

			skelTriangle_t *sortTri = Com_GrowListElement(&sortedTriangles, j);

			for(k = 0; k < 3; k++)
			{
				v = sortTri->vertexes[k];

				for(l = 0; l < MAX_WEIGHTS; l++)
				{
					b[k * 3 + l] = (l < v->numWeights) ? v->weights[l]->boneIndex : 9999;
				}

				qsort(b, MAX_WEIGHTS * 3, sizeof(int), CompareBoneIndices);
				//ri.Printf(PRINT_ALL, "bone indices: %i %i %i %i\n", b[k * 3 + 0], b[k * 3 + 1], b[k * 3 + 2], b[k * 3 + 3]);
			}
		}
#endif

		numRemaining = sortedTriangles.currentElements;
		while(numRemaining)
		{
			numBoneReferences = 0;
			Com_Memset(boneReferences, 0, sizeof(boneReferences));

			Com_InitGrowList(&vboTriangles, 1000);

			for(j = 0; j < sortedTriangles.currentElements; j++)
			{
				skelTriangle_t *sortTri = Com_GrowListElement(&sortedTriangles, j);

				if(sortTri->referenced)
					continue;

				if(AddTriangleToVBOTriangleList(&vboTriangles, sortTri, &numBoneReferences, boneReferences))
				{
					sortTri->referenced = qtrue;
				}
			}

			if(!vboTriangles.currentElements)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: could not add triangles to a remaining VBO surfaces for model '%s'\n", modName);
				break;
			}

			AddSurfaceToVBOSurfacesList(&vboSurfaces, &vboTriangles, md5, surf, i, numBoneReferences, boneReferences);
			numRemaining -= vboTriangles.currentElements;

			Com_DestroyGrowList(&vboTriangles);
		}

		for(j = 0; j < sortedTriangles.currentElements; j++)
		{
			skelTriangle_t *sortTri = Com_GrowListElement(&sortedTriangles, j);

			Com_Dealloc(sortTri);
		}
		Com_DestroyGrowList(&sortedTriangles);
	}

	// move VBO surfaces list to hunk
	md5->numVBOSurfaces = vboSurfaces.currentElements;
	md5->vboSurfaces = ri.Hunk_Alloc(md5->numVBOSurfaces * sizeof(*md5->vboSurfaces), h_low);

	for(i = 0; i < md5->numVBOSurfaces; i++)
	{
		md5->vboSurfaces[i] = (srfVBOMD5Mesh_t *) Com_GrowListElement(&vboSurfaces, i);
	}

	Com_DestroyGrowList(&vboSurfaces);

	return qtrue;
}






//=============================================================================



static void GetChunkHeader(memStream_t *s, axChunkHeader_t * chunkHeader)
{
	int             i;

	for(i = 0; i < 20; i++)
	{
		chunkHeader->ident[i] = MemStreamGetC(s);
	}

	chunkHeader->flags = MemStreamGetLong(s);
	chunkHeader->dataSize = MemStreamGetLong(s);
	chunkHeader->numData = MemStreamGetLong(s);
}

static void PrintChunkHeader(axChunkHeader_t * chunkHeader)
{
#if 0
	ri.Printf(PRINT_ALL, "----------------------\n");
	ri.Printf(PRINT_ALL, "R_LoadPSK: chunk header ident: '%s'\n", chunkHeader->ident);
	ri.Printf(PRINT_ALL, "R_LoadPSK: chunk header flags: %i\n", chunkHeader->flags);
	ri.Printf(PRINT_ALL, "R_LoadPSK: chunk header data size: %i\n", chunkHeader->dataSize);
	ri.Printf(PRINT_ALL, "R_LoadPSK: chunk header num items: %i\n", chunkHeader->numData);
#endif
}

static void GetBone(memStream_t *s, axBone_t * bone)
{
	int             i;

	for(i = 0; i < 4; i++)
	{
		bone->quat[i] = MemStreamGetFloat(s);
	}

	for(i = 0; i < 3; i++)
	{
		bone->position[i] = MemStreamGetFloat(s);
	}

	bone->length = MemStreamGetFloat(s);

	bone->xSize = MemStreamGetFloat(s);
	bone->ySize = MemStreamGetFloat(s);
	bone->zSize = MemStreamGetFloat(s);
}

static int CompareTrianglesByMaterialIndex(const void *a, const void *b)
{
	axTriangle_t *t1, *t2;

	t1 = (axTriangle_t *)a;
	t2 = (axTriangle_t *)b;

	if(t1->materialIndex < t2->materialIndex)
		return -1;

	if(t1->materialIndex > t2->materialIndex)
		return 1;

	return 0;
}

/*
=================
R_LoadPSK
=================
*/
static qboolean R_LoadPSK(model_t * mod, void *buffer, int bufferSize, const char *modName)
{
	int             i, j, k;
	memStream_t    *stream;

	axChunkHeader_t	chunkHeader;

	int				numPoints;
	axPoint_t      *point;
	axPoint_t      *points;

	int				numVertexes;
	axVertex_t     *vertex;
	axVertex_t     *vertexes;

	int				numSmoothGroups;
	int				numTriangles;
	axTriangle_t   *triangle;
	axTriangle_t   *triangles;

	int				numMaterials;
	axMaterial_t   *material;
	axMaterial_t   *materials;

	int				numReferenceBones;
	axReferenceBone_t *refBone;
	axReferenceBone_t *refBones;

	int				numWeights;
	axBoneWeight_t *axWeight;
	axBoneWeight_t *axWeights;

	md5Model_t     *md5;
	md5Bone_t      *md5Bone;
	md5Weight_t    *weight;

	vec3_t          boneOrigin;
	quat_t          boneQuat;
	matrix_t        boneMat;

	int				materialIndex, oldMaterialIndex;

	int				numRemaining;

	growList_t		sortedTriangles;
	growList_t      vboVertexes;
	growList_t      vboTriangles;
	growList_t      vboSurfaces;

	int				numBoneReferences;
	int				boneReferences[MAX_BONES];

	matrix_t		unrealToQuake;

	//MatrixSetupScale(unrealToQuake, 1, -1, 1);
	MatrixFromAngles(unrealToQuake, 0, 90, 0);

	stream = AllocMemStream(buffer, bufferSize);
	GetChunkHeader(stream, &chunkHeader);

	// check indent again
	if(Q_stricmpn(chunkHeader.ident, "ACTRHEAD", 8))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "ACTRHEAD");
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	mod->type = MOD_MD5;
	mod->dataSize += sizeof(md5Model_t);
	md5 = mod->md5 = ri.Hunk_Alloc(sizeof(md5Model_t), h_low);

	// read points
	GetChunkHeader(stream, &chunkHeader);
	if(Q_stricmpn(chunkHeader.ident, "PNTS0000", 8))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "PNTS0000");
		FreeMemStream(stream);
		return qfalse;
	}

	if(chunkHeader.dataSize != sizeof(axPoint_t))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk dataSize ('%i' should be '%i')\n", modName, chunkHeader.dataSize, sizeof(axPoint_t));
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	numPoints = chunkHeader.numData;
	points = Com_Allocate(numPoints * sizeof(axPoint_t));
	for(i = 0, point = points; i < numPoints; i++, point++)
	{
		point->point[0] = MemStreamGetFloat(stream);
		point->point[1] = MemStreamGetFloat(stream);
		point->point[2] = MemStreamGetFloat(stream);

#if 0
		// Tr3B: HACK convert from Unreal coordinate system to the Quake one
		MatrixTransformPoint2(unrealToQuake, point->point);
#endif
	}

	// read vertices
	GetChunkHeader(stream, &chunkHeader);
	if(Q_stricmpn(chunkHeader.ident, "VTXW0000", 8))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "VTXW0000");
		FreeMemStream(stream);
		return qfalse;
	}

	if(chunkHeader.dataSize != sizeof(axVertex_t))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk dataSize ('%i' should be '%i')\n", modName, chunkHeader.dataSize, sizeof(axVertex_t));
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	numVertexes = chunkHeader.numData;
	vertexes = Com_Allocate(numVertexes * sizeof(axVertex_t));
	for(i = 0, vertex = vertexes; i < numVertexes; i++, vertex++)
	{
		vertex->pointIndex = MemStreamGetShort(stream);
		if(vertex->pointIndex < 0 || vertex->pointIndex >= numPoints)
		{
			ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has vertex with point index out of range (%i while max %i)\n", modName, vertex->pointIndex, numPoints);
			FreeMemStream(stream);
			return qfalse;
		}

		vertex->unknownA = MemStreamGetShort(stream);
		vertex->st[0] = MemStreamGetFloat(stream);
		vertex->st[1] = MemStreamGetFloat(stream);
		vertex->materialIndex = MemStreamGetC(stream);
		vertex->reserved = MemStreamGetC(stream);
		vertex->unknownB = MemStreamGetShort(stream);

#if 0
		ri.Printf(PRINT_ALL, "R_LoadPSK: axVertex_t(%i):\n"
				"axVertex:pointIndex: %i\n"
				"axVertex:unknownA: %i\n"
				"axVertex::st: %f %f\n"
				"axVertex:materialIndex: %i\n"
				"axVertex:reserved: %d\n"
				"axVertex:unknownB: %d\n",
				i,
				vertex->pointIndex,
				vertex->unknownA,
				vertex->st[0], vertex->st[1],
				vertex->materialIndex,
				vertex->reserved,
				vertex->unknownB);
#endif
	}

	// read triangles
	GetChunkHeader(stream, &chunkHeader);
	if(Q_stricmpn(chunkHeader.ident, "FACE0000", 8))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "FACE0000");
		FreeMemStream(stream);
		return qfalse;
	}

	if(chunkHeader.dataSize != sizeof(axTriangle_t))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk dataSize ('%i' should be '%i')\n", modName, chunkHeader.dataSize, sizeof(axTriangle_t));
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	numTriangles = chunkHeader.numData;
	triangles = Com_Allocate(numTriangles * sizeof(axTriangle_t));
	for(i = 0, triangle = triangles; i < numTriangles; i++, triangle++)
	{
		for(j = 0; j < 3; j++)
		//for(j = 2; j >= 0; j--)
		{
			triangle->indexes[j] = MemStreamGetShort(stream);
			if(triangle->indexes[j] < 0 || triangle->indexes[j] >= numVertexes)
			{
				ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has triangle with vertex index out of range (%i while max %i)\n", modName, triangle->indexes[j], numVertexes);
				FreeMemStream(stream);
				return qfalse;
			}
		}

		triangle->materialIndex = MemStreamGetC(stream);
		triangle->materialIndex2 = MemStreamGetC(stream);
		triangle->smoothingGroups = MemStreamGetLong(stream);
	}

	// read materials
	GetChunkHeader(stream, &chunkHeader);
	if(Q_stricmpn(chunkHeader.ident, "MATT0000", 8))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "MATT0000");
		FreeMemStream(stream);
		return qfalse;
	}

	if(chunkHeader.dataSize != sizeof(axMaterial_t))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk dataSize ('%i' should be '%i')\n", modName, chunkHeader.dataSize, sizeof(axMaterial_t));
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	numMaterials = chunkHeader.numData;
	materials = Com_Allocate(numMaterials * sizeof(axMaterial_t));
	for(i = 0, material = materials; i < numMaterials; i++, material++)
	{
		MemStreamRead(stream, material->name, sizeof(material->name));

		ri.Printf(PRINT_ALL, "R_LoadPSK: material name: '%s'\n", material->name);

		material->shaderIndex = MemStreamGetLong(stream);
		material->polyFlags = MemStreamGetLong(stream);
		material->auxMaterial = MemStreamGetLong(stream);
		material->auxFlags = MemStreamGetLong(stream);
		material->lodBias = MemStreamGetLong(stream);
		material->lodStyle = MemStreamGetLong(stream);
	}

	for(i = 0, vertex = vertexes; i < numVertexes; i++, vertex++)
	{
		if(vertex->materialIndex < 0 || vertex->materialIndex >= numMaterials)
		{
			ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has vertex with material index out of range (%i while max %i)\n", modName, vertex->materialIndex, numMaterials);
			FreeMemStream(stream);
			return qfalse;
		}
	}

	for(i = 0, triangle = triangles; i < numTriangles; i++, triangle++)
	{
		if(triangle->materialIndex < 0 || triangle->materialIndex >= numMaterials)
		{
			ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has triangle with material index out of range (%i while max %i)\n", modName, triangle->materialIndex, numMaterials);
			FreeMemStream(stream);
			return qfalse;
		}
	}

	// read reference bones
	GetChunkHeader(stream, &chunkHeader);
	if(Q_stricmpn(chunkHeader.ident, "REFSKELT", 8))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "REFSKELT");
		FreeMemStream(stream);
		return qfalse;
	}

	if(chunkHeader.dataSize != sizeof(axReferenceBone_t))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk dataSize ('%i' should be '%i')\n", modName, chunkHeader.dataSize, sizeof(axReferenceBone_t));
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	numReferenceBones = chunkHeader.numData;
	refBones = Com_Allocate(numReferenceBones * sizeof(axReferenceBone_t));
	for(i = 0, refBone = refBones; i < numReferenceBones; i++, refBone++)
	{
		MemStreamRead(stream, refBone->name, sizeof(refBone->name));

		//ri.Printf(PRINT_ALL, "R_LoadPSK: reference bone name: '%s'\n", refBone->name);

		refBone->flags = MemStreamGetLong(stream);
		refBone->numChildren = MemStreamGetLong(stream);
		refBone->parentIndex = MemStreamGetLong(stream);

		GetBone(stream, &refBone->bone);

#if 0
		ri.Printf(PRINT_ALL, "R_LoadPSK: axReferenceBone_t(%i):\n"
				"axReferenceBone_t::name: '%s'\n"
				"axReferenceBone_t::flags: %i\n"
				"axReferenceBone_t::numChildren %i\n"
				"axReferenceBone_t::parentIndex: %i\n"
				"axReferenceBone_t::quat: %f %f %f %f\n"
				"axReferenceBone_t::position: %f %f %f\n"
				"axReferenceBone_t::length: %f\n"
				"axReferenceBone_t::xSize: %f\n"
				"axReferenceBone_t::ySize: %f\n"
				"axReferenceBone_t::zSize: %f\n",
				i,
				refBone->name,
				refBone->flags,
				refBone->numChildren,
				refBone->parentIndex,
				refBone->bone.quat[0], refBone->bone.quat[1], refBone->bone.quat[2], refBone->bone.quat[3],
				refBone->bone.position[0], refBone->bone.position[1], refBone->bone.position[2],
				refBone->bone.length,
				refBone->bone.xSize,
				refBone->bone.ySize,
				refBone->bone.zSize);
#endif
	}

	// read  bone weights
	GetChunkHeader(stream, &chunkHeader);
	if(Q_stricmpn(chunkHeader.ident, "RAWWEIGHTS", 10))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk indent ('%s' should be '%s')\n", modName, chunkHeader.ident, "RAWWEIGHTS");
		FreeMemStream(stream);
		return qfalse;
	}

	if(chunkHeader.dataSize != sizeof(axBoneWeight_t))
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has wrong chunk dataSize ('%i' should be '%i')\n", modName, chunkHeader.dataSize, sizeof(axBoneWeight_t));
		FreeMemStream(stream);
		return qfalse;
	}

	PrintChunkHeader(&chunkHeader);

	numWeights = chunkHeader.numData;
	axWeights = Com_Allocate(numWeights * sizeof(axBoneWeight_t));
	for(i = 0, axWeight = axWeights; i < numWeights; i++, axWeight++)
	{
		axWeight->weight = MemStreamGetFloat(stream);
		axWeight->pointIndex = MemStreamGetLong(stream);
		axWeight->boneIndex = MemStreamGetLong(stream);

#if 0
		ri.Printf(PRINT_ALL, "R_LoadPSK: axBoneWeight_t(%i):\n"
				"axBoneWeight_t::weight: %f\n"
				"axBoneWeight_t::pointIndex %i\n"
				"axBoneWeight_t::boneIndex: %i\n",
				i,
				axWeight->weight,
				axWeight->pointIndex,
				axWeight->boneIndex);
#endif
	}


	//
	// convert the model to an internal MD5 representation
	//
	md5->numBones = numReferenceBones;

	// calc numMeshes <number>
	/*
	numSmoothGroups = 0;
	for(i = 0, triangle = triangles; i < numTriangles; i++, triangle++)
	{
		if(triangle->smoothingGroups)
		{

		}
	}
	*/

	if(md5->numBones < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has no bones\n", modName);
		return qfalse;
	}
	if(md5->numBones > MAX_BONES)
	{
		ri.Printf(PRINT_WARNING, "R_LoadPSK: '%s' has more than %i bones (%i)\n", modName, MAX_BONES, md5->numBones);
		return qfalse;
	}
	//ri.Printf(PRINT_ALL, "R_LoadPSK: '%s' has %i bones\n", modName, md5->numBones);

	// copy all reference bones
	md5->bones = ri.Hunk_Alloc(sizeof(*md5Bone) * md5->numBones, h_low);
	for(i = 0, md5Bone = md5->bones, refBone = refBones; i < md5->numBones; i++, md5Bone++, refBone++)
	{
		Q_strncpyz(md5Bone->name, refBone->name, sizeof(md5Bone->name));

		if(i == 0)
		{
			md5Bone->parentIndex = refBone->parentIndex -1;
		}
		else
		{
			md5Bone->parentIndex = refBone->parentIndex;
		}

		//ri.Printf(PRINT_ALL, "R_LoadPSK: '%s' has bone '%s' with parent index %i\n", modName, md5Bone->name, md5Bone->parentIndex);

		if(md5Bone->parentIndex >= md5->numBones)
		{
			ri.Error(ERR_DROP, "R_LoadPSK: '%s' has bone '%s' with bad parent index %i while numBones is %i\n", modName,
					 md5Bone->name, md5Bone->parentIndex, md5->numBones);
		}

		for(j = 0; j < 3; j++)
		{
			boneOrigin[j] = refBone->bone.position[j];
		}

		// Tr3B: I have really no idea why the .psk format stores the first quaternion with inverted quats.
		// Furthermore only the X and Z components of the first quat are inverted ?!?!
		if(i == 0)
		{
			boneQuat[0] = refBone->bone.quat[0];
			boneQuat[1] = -refBone->bone.quat[1];
			boneQuat[2] = refBone->bone.quat[2];
			boneQuat[3] = refBone->bone.quat[3];
		}
		else
		{
			boneQuat[0] = -refBone->bone.quat[0];
			boneQuat[1] = -refBone->bone.quat[1];
			boneQuat[2] = -refBone->bone.quat[2];
			boneQuat[3] = refBone->bone.quat[3];
		}

		VectorCopy(boneOrigin, md5Bone->origin);
		//MatrixTransformPoint(unrealToQuake, boneOrigin, md5Bone->origin);

		QuatCopy(boneQuat, md5Bone->rotation);

		//QuatClear(md5Bone->rotation);


#if 0
		ri.Printf(PRINT_ALL, "R_LoadPSK: md5Bone_t(%i):\n"
						"md5Bone_t::name: '%s'\n"
						"md5Bone_t::parentIndex: %i\n"
						"md5Bone_t::quat: %f %f %f %f\n"
						"md5bone_t::position: %f %f %f\n",
						i,
						md5Bone->name,
						md5Bone->parentIndex,
						md5Bone->rotation[0], md5Bone->rotation[1], md5Bone->rotation[2], md5Bone->rotation[3],
						md5Bone->origin[0], md5Bone->origin[1], md5Bone->origin[2]);
#endif

		if(md5Bone->parentIndex >= 0)
		{
			vec3_t          rotated;
			quat_t          quat;

			md5Bone_t      *parent;

			parent = &md5->bones[md5Bone->parentIndex];

			QuatTransformVector(parent->rotation, md5Bone->origin, rotated);
			//QuatTransformVector(md5Bone->rotation, md5Bone->origin, rotated);

			VectorAdd(parent->origin, rotated, md5Bone->origin);

			QuatMultiply1(parent->rotation, md5Bone->rotation, quat);
			QuatCopy(quat, md5Bone->rotation);
		}

		MatrixSetupTransformFromQuat(md5Bone->inverseTransform, md5Bone->rotation, md5Bone->origin);
		MatrixInverse(md5Bone->inverseTransform);

#if 0
		ri.Printf(PRINT_ALL, "R_LoadPSK: md5Bone_t(%i):\n"
						"md5Bone_t::name: '%s'\n"
						"md5Bone_t::parentIndex: %i\n"
						"md5Bone_t::quat: %f %f %f %f\n"
						"md5bone_t::position: %f %f %f\n",
						i,
						md5Bone->name,
						md5Bone->parentIndex,
						md5Bone->rotation[0], md5Bone->rotation[1], md5Bone->rotation[2], md5Bone->rotation[3],
						md5Bone->origin[0], md5Bone->origin[1], md5Bone->origin[2]);
#endif
	}

	Com_InitGrowList(&vboVertexes, 10000);
	for(i = 0, vertex = vertexes; i < numVertexes; i++, vertex++)
	{
		md5Vertex_t *vboVert = Com_Allocate(sizeof(*vboVert));

		for(j = 0; j < 3; j++)
		{
			vboVert->position[j] = points[vertex->pointIndex].point[j];
		}

		vboVert->texCoords[0] = vertex->st[0];
		vboVert->texCoords[1] = vertex->st[1];

		// find number of associated weights
		vboVert->numWeights = 0;
		for(j = 0, axWeight = axWeights; j < numWeights; j++, axWeight++)
		{
			if(axWeight->pointIndex == vertex->pointIndex && axWeight->weight > 0.0f)
			{
				vboVert->numWeights++;
			}
		}

		if(vboVert->numWeights > MAX_WEIGHTS)
		{
			ri.Error(ERR_DROP, "R_LoadPSK: vertex %i requires more weights %i than the maximum of %i in model '%s'", i, vboVert->numWeights, MAX_WEIGHTS, modName);
			//ri.Printf(PRINT_WARNING, "R_LoadPSK: vertex %i requires more weights %i than the maximum of %i in model '%s'\n", i, vboVert->numWeights, MAX_WEIGHTS, modName);
		}

		vboVert->weights = ri.Hunk_Alloc(sizeof(*vboVert->weights) * vboVert->numWeights, h_low);
		for(j = 0, axWeight = axWeights, k = 0; j < numWeights; j++, axWeight++)
		{
			if(axWeight->pointIndex == vertex->pointIndex && axWeight->weight > 0.0f)
			{
				weight = ri.Hunk_Alloc(sizeof(*weight), h_low);

				weight->boneIndex = axWeight->boneIndex;
				weight->boneWeight = axWeight->weight;

				// FIXME?
				weight->offset[0] = refBones[axWeight->boneIndex].bone.xSize;
				weight->offset[1] = refBones[axWeight->boneIndex].bone.ySize;
				weight->offset[2] = refBones[axWeight->boneIndex].bone.zSize;

				vboVert->weights[k++] = weight;
			}
		}

		Com_AddToGrowList(&vboVertexes, vboVert);
	}

	ClearBounds(md5->bounds[0], md5->bounds[1]);
	for(i = 0, vertex = vertexes; i < numVertexes; i++, vertex++)
	{
		AddPointToBounds(points[vertex->pointIndex].point, md5->bounds[0], md5->bounds[1]);
	}

#if 0
	ri.Printf(PRINT_ALL, "R_LoadPSK: AABB (%i %i %i) (%i %i %i)\n",
			(int)md5->bounds[0][0],
			(int)md5->bounds[0][1],
			(int)md5->bounds[0][2],
			(int)md5->bounds[1][0],
			(int)md5->bounds[1][1],
			(int)md5->bounds[1][2]);
#endif

	// sort triangles
	qsort(triangles, numTriangles, sizeof(axTriangle_t), CompareTrianglesByMaterialIndex);

	Com_InitGrowList(&sortedTriangles, 1000);
	for(i = 0, triangle = triangles; i < numTriangles; i++, triangle++)
	{
		skelTriangle_t *sortTri = Com_Allocate(sizeof(*sortTri));

		for(j = 0; j < 3; j++)
		{
			sortTri->indexes[j] = triangle->indexes[j];
			sortTri->vertexes[j] = Com_GrowListElement(&vboVertexes, triangle->indexes[j]);
		}
		sortTri->referenced = qfalse;

		Com_AddToGrowList(&sortedTriangles, sortTri);
	}

	// calc tangent spaces
	{
		md5Vertex_t    *v0, *v1, *v2;
		const float    *p0, *p1, *p2;
		const float    *t0, *t1, *t2;
		vec3_t          tangent;
		vec3_t          binormal;
		vec3_t          normal;

		for(j = 0; j < vboVertexes.currentElements; j++)
		{
			v0 = Com_GrowListElement(&vboVertexes, j);

			VectorClear(v0->tangent);
			VectorClear(v0->binormal);
			VectorClear(v0->normal);
		}

		for(j = 0; j < sortedTriangles.currentElements; j++)
		{
			skelTriangle_t *tri = Com_GrowListElement(&sortedTriangles, j);

			v0 = Com_GrowListElement(&vboVertexes, tri->indexes[0]);
			v1 = Com_GrowListElement(&vboVertexes, tri->indexes[1]);
			v2 = Com_GrowListElement(&vboVertexes, tri->indexes[2]);

			p0 = v0->position;
			p1 = v1->position;
			p2 = v2->position;

			t0 = v0->texCoords;
			t1 = v1->texCoords;
			t2 = v2->texCoords;

			R_CalcTangentSpace(tangent, binormal, normal, p0, p1, p2, t0, t1, t2);

			for(k = 0; k < 3; k++)
			{
				float          *v;

				v0 = Com_GrowListElement(&vboVertexes, tri->indexes[k]);

				v = v0->tangent;
				VectorAdd(v, tangent, v);

				v = v0->binormal;
				VectorAdd(v, binormal, v);

				v = v0->normal;
				VectorAdd(v, normal, v);
			}
		}

		for(j = 0; j < vboVertexes.currentElements; j++)
		{
			v0 = Com_GrowListElement(&vboVertexes, j);

			VectorNormalize(v0->tangent);
			VectorNormalize(v0->binormal);
			VectorNormalize(v0->normal);
		}

		#if 1
		// do another extra smoothing for normals to avoid flat shading
		for(j = 0; j < vboVertexes.currentElements; j++)
		{
			v0 = Com_GrowListElement(&vboVertexes, j);

			for(k = 0; k < vboVertexes.currentElements; k++)
			{
				if(j == k)
					continue;

				v1 = Com_GrowListElement(&vboVertexes, k);

				if(VectorCompare(v0->position, v1->position))
				{
					VectorAdd(v0->position, v1->normal, v0->normal);
				}
			}

			VectorNormalize(v0->normal);
		}
		#endif

	}

	// split the surfaces into VBO surfaces by the maximum number of GPU vertex skinning bones
	Com_InitGrowList(&vboSurfaces, 10);

	materialIndex = oldMaterialIndex = -1;
	for(i = 0; i < numTriangles; i++)
	{
		triangle = &triangles[i];
		materialIndex = triangle->materialIndex;

		if(materialIndex != oldMaterialIndex)
		{
			oldMaterialIndex = materialIndex;

			numRemaining = sortedTriangles.currentElements - i;
			while(numRemaining)
			{
				numBoneReferences = 0;
				Com_Memset(boneReferences, 0, sizeof(boneReferences));

				Com_InitGrowList(&vboTriangles, 1000);

				for(j = i; j < sortedTriangles.currentElements; j++)
				{
					skelTriangle_t *sortTri;

					triangle = &triangles[j];
					materialIndex = triangle->materialIndex;

					if(materialIndex != oldMaterialIndex)
						continue;

					sortTri = Com_GrowListElement(&sortedTriangles, j);

					if(sortTri->referenced)
						continue;

					if(AddTriangleToVBOTriangleList(&vboTriangles, sortTri, &numBoneReferences, boneReferences))
					{
						sortTri->referenced = qtrue;
					}
				}

				for(j = 0; j < MAX_BONES; j++)
				{
					if(boneReferences[j] > 0)
					{
						ri.Printf(PRINT_ALL, "R_LoadPSK: referenced bone: '%s'\n", (j < numReferenceBones) ? refBones[j].name : NULL);
					}
				}

				if(!vboTriangles.currentElements)
				{
					ri.Printf(PRINT_WARNING, "R_LoadPSK: could not add triangles to a remaining VBO surface for model '%s'\n", modName);
					break;
				}

				// FIXME skinIndex
				AddSurfaceToVBOSurfacesList2(&vboSurfaces, &vboTriangles, &vboVertexes, md5, vboSurfaces.currentElements, materials[oldMaterialIndex].name, numBoneReferences, boneReferences);
				numRemaining -= vboTriangles.currentElements;

				Com_DestroyGrowList(&vboTriangles);
			}
		}
	}

	for(j = 0; j < sortedTriangles.currentElements; j++)
	{
		skelTriangle_t *sortTri = Com_GrowListElement(&sortedTriangles, j);
		Com_Dealloc(sortTri);
	}
	Com_DestroyGrowList(&sortedTriangles);

	for(j = 0; j < vboVertexes.currentElements; j++)
	{
		md5Vertex_t *v = Com_GrowListElement(&vboVertexes, j);
		Com_Dealloc(v);
	}
	Com_DestroyGrowList(&vboVertexes);

	// move VBO surfaces list to hunk
	md5->numVBOSurfaces = vboSurfaces.currentElements;
	md5->vboSurfaces = ri.Hunk_Alloc(md5->numVBOSurfaces * sizeof(*md5->vboSurfaces), h_low);

	for(i = 0; i < md5->numVBOSurfaces; i++)
	{
		md5->vboSurfaces[i] = (srfVBOMD5Mesh_t *) Com_GrowListElement(&vboSurfaces, i);
	}

	Com_DestroyGrowList(&vboSurfaces);

	FreeMemStream(stream);
	Com_Dealloc(points);
	Com_Dealloc(vertexes);
	Com_Dealloc(triangles);
	Com_Dealloc(materials);

	ri.Printf(PRINT_ALL, "%i VBO surfaces created for PSK model '%s'\n", md5->numVBOSurfaces, modName);

	return qtrue;
}


//=============================================================================

/*
=================
R_XMLError
=================
*/
void R_XMLError(void *ctx, const char *fmt, ...)
{
	va_list         argptr;
	static char     msg[4096];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	ri.Printf(PRINT_WARNING, "%s", msg);
}

/*
=================
R_LoadDAE
=================
*/
/*
static qboolean R_LoadDAE(model_t * mod, void *buffer, int bufferLen, const char *modName)
{
	xmlDocPtr       doc;
	xmlNodePtr      node;

	// setup error function handler
	xmlInitParser();
	xmlSetGenericErrorFunc(NULL, R_XMLError);

	ri.Printf(PRINT_ALL, "...loading DAE '%s'\n", modName);

	doc = xmlParseMemory(buffer, bufferLen);
	if(doc == NULL)
	{
		ri.Printf(PRINT_WARNING, "R_LoadDAE: '%s' xmlParseMemory returned NULL\n", modName);
		return qfalse;
	}
	node = xmlDocGetRootElement(doc);

	if(node == NULL)
	{
		ri.Printf(PRINT_WARNING, "R_LoadDAE: '%s' empty document\n", modName);
		xmlFreeDoc(doc);
		return qfalse;
	}

	if(xmlStrcmp(node->name, (const xmlChar *) "COLLADA"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadDAE: '%s' document of the wrong type, root node != COLLADA", modName);
		xmlFreeDoc(doc);
		return qfalse;
	}

	//TODO

	xmlFreeDoc(doc);

	ri.Printf(PRINT_ALL, "...finished DAE '%s'\n", modName);

	return qfalse;
}
*/

//=============================================================================

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration(glConfig_t * glconfigOut)
{
	R_Init();

	*glconfigOut = glConfig;

	R_SyncRenderThread();

	tr.visIndex = 0;
	memset(tr.visClusters, -2, sizeof(tr.visClusters));	// force markleafs to regenerate

	R_ClearFlares();
	RE_ClearScene();

	// HACK: give world entity white color for "colored" shader keyword
	tr.worldEntity.e.shaderRGBA[0] = 255;
	tr.worldEntity.e.shaderRGBA[1] = 255;
	tr.worldEntity.e.shaderRGBA[2] = 255;
	tr.worldEntity.e.shaderRGBA[3] = 255;

	// FIXME: world entity shadows always use zfail algorithm which is slower than zpass
	tr.worldEntity.needZFail = qtrue;
	tr.worldEntity.e.nonNormalizedAxes = qfalse;

	tr.registered = qtrue;

	// NOTE: this sucks, for some reason the first stretch pic is never drawn
	// without this we'd see a white flash on a level load because the very
	// first time the level shot would not be drawn
	RE_StretchPic(0, 0, 0, 0, 0, 0, 1, 1, 0);
}

//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit(void)
{
	model_t        *mod;

	// leave a space for NULL model
	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = MOD_BAD;
}


/*
================
R_Modellist_f
================
*/
void R_Modellist_f(void)
{
	int             i, j;
	model_t        *mod;
	int             total;
	int             lods;

	total = 0;
	for(i = 1; i < tr.numModels; i++)
	{
		mod = tr.models[i];
		lods = 1;
		for(j = 1; j < MD3_MAX_LODS; j++)
		{
			if(mod->mdx[j] && mod->mdx[j] != mod->mdx[j - 1])
			{
				lods++;
			}
		}
		ri.Printf(PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, lods, mod->name);
		total += mod->dataSize;
	}
	ri.Printf(PRINT_ALL, "%8i : Total models\n", total);

#if	0							// not working right with new hunk
	if(tr.world)
	{
		ri.Printf(PRINT_ALL, "\n%8i : %s\n", tr.world->dataSize, tr.world->name);
	}
#endif
}


//=============================================================================


/*
================
R_GetTag
================
*/
static mdxTag_t *R_GetTag(mdxModel_t * model, int frame, const char *tagName)
{
	mdxTag_t       *tag;
	int             i;

	if(frame >= model->numFrames)
	{
		// it is possible to have a bad frame while changing models, so don't error
		frame = model->numFrames - 1;
	}

	tag = model->tags + frame * model->numTags;
	for(i = 0; i < model->numTags; i++, tag++)
	{
		if(!strcmp(tag->name, tagName))
		{
			return tag;			// found it
		}
	}

	return NULL;
}

/*
================
RE_LerpTag
================
*/
int RE_LerpTag(orientation_t * tag, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName)
{
	mdxTag_t       *start, *end;
	int             i;
	float           frontLerp, backLerp;
	model_t        *model;

	model = R_GetModelByHandle(handle);
	if(!model->mdx[0])
	{
		AxisClear(tag->axis);
		VectorClear(tag->origin);
		return qfalse;
	}

	start = R_GetTag(model->mdx[0], startFrame, tagName);
	end = R_GetTag(model->mdx[0], endFrame, tagName);
	if(!start || !end)
	{
		AxisClear(tag->axis);
		VectorClear(tag->origin);
		return qfalse;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for(i = 0; i < 3; i++)
	{
		tag->origin[i] = start->origin[i] * backLerp + end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp + end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp + end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp + end->axis[2][i] * frontLerp;
	}
	VectorNormalize(tag->axis[0]);
	VectorNormalize(tag->axis[1]);
	VectorNormalize(tag->axis[2]);
	return qtrue;
}

/*
================
RE_BoneIndex
================
*/
int RE_BoneIndex(qhandle_t hModel, const char *boneName)
{
	int             i;
	md5Bone_t      *bone;
	md5Model_t     *md5;
	model_t        *model;

	model = R_GetModelByHandle(hModel);
	if(!model->md5)
	{
		return -1;
	}
	else
	{
		md5 = model->md5;
	}

	for(i = 0, bone = md5->bones; i < md5->numBones; i++, bone++)
	{
		if(!Q_stricmp(bone->name, boneName))
		{
			return i;
		}
	}

	return -1;
}



/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
	model_t        *model;
	mdxModel_t     *header;
	mdxFrame_t     *frame;

	model = R_GetModelByHandle(handle);

	if(model->bsp)
	{
		VectorCopy(model->bsp->bounds[0], mins);
		VectorCopy(model->bsp->bounds[1], maxs);
	}
	else if(model->mdx[0])
	{
		header = model->mdx[0];

		frame = header->frames;

		VectorCopy(frame->bounds[0], mins);
		VectorCopy(frame->bounds[1], maxs);
	}
	else if(model->md5)
	{
		VectorCopy(model->md5->bounds[0], mins);
		VectorCopy(model->md5->bounds[1], maxs);
	}
	else
	{
		VectorClear(mins);
		VectorClear(maxs);
	}
}
