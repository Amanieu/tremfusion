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
// tr_models.c -- model loading and caching

#include "tr_local.h"

#define	LL(x) x=LittleLong(x)
#define	LF(x) x=LittleFloat(x)

static qboolean R_LoadMD3(model_t * mod, int lod, void *buffer, const char *name);
static qboolean R_LoadMD5(model_t * mod, void *buffer, const char *name);

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
qhandle_t RE_RegisterModel(const char *name)
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
		
		if(!Q_stricmpn((const char*)buffer, "MD5Version", 10))
		{
			loaded = R_LoadMD5(mod, buffer, name);
		}
		else
		{
			if(ident != MD3_IDENT)
			{
				ri.Printf(PRINT_WARNING, "RE_RegisterModel: unknown fileid for %s\n", name);
				goto fail;
			}

			loaded = R_LoadMD3(mod, lod, buffer, name);
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
R_LoadMD3
=================
*/
static qboolean R_LoadMD3(model_t * mod, int lod, void *buffer, const char *modName)
{
	int             i, j;
	
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
	mdxSurface_t   *surf;
	mdxShader_t    *shader;
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

//	Com_Memcpy(mod->md3[lod], buffer, LittleLong(md3Model->ofsEnd));

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

	return qtrue;
}

/*
=================
R_LoadMD5
=================
*/
static qboolean R_LoadMD5(model_t * mod, void *buffer, const char *modName)
{
	int             i, j, k;
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

	buf_p = (char *) buffer;
	
	// skip MD5Version indent string
	COM_ParseExt(&buf_p, qfalse);
	
	// check version
	token = COM_ParseExt(&buf_p, qfalse);
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
	token = COM_ParseExt(&buf_p, qtrue);
	token = COM_ParseExt(&buf_p, qtrue);
//	ri.Printf(PRINT_ALL, "%s\n", token);
	
	// parse numJoints <number>
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numJoints"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numJoints' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	md5->numBones = atoi(token);
	
	// parse numMeshes <number>
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "numMeshes"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numMeshes' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	md5->numSurfaces = atoi(token);
	//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has %i surfaces\n", modName, md5->numSurfaces);

	
	if(md5->numBones < 1)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: '%s' has no bones\n", modName);
		return qfalse;
	}
	if(md5->numBones > MD5_MAX_BONES)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: '%s' has more than %i bones (%i)\n", modName, MD5_MAX_BONES, md5->numBones);
		return qfalse;
	}
	//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has %i bones\n", modName, md5->numBones);
	
	// parse all the bones
	md5->bones = ri.Hunk_Alloc(sizeof(*bone) * md5->numBones, h_low);
	
	// parse joints {
	token = COM_ParseExt(&buf_p, qtrue);
	if(Q_stricmp(token, "joints"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'joints' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	token = COM_ParseExt(&buf_p, qfalse);
	if(Q_stricmp(token, "{"))
	{
		ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '{' found '%s' in model '%s'\n", token, modName);
		return qfalse;
	}
	
	for(i = 0, bone = md5->bones; i < md5->numBones; i++, bone++)
	{
		token = COM_ParseExt(&buf_p, qtrue);
		Q_strncpyz(bone->name, token, sizeof(bone->name));
		
		//ri.Printf(PRINT_ALL, "R_LoadMD5: '%s' has bone '%s'\n", modName, bone->name);
		
		token = COM_ParseExt(&buf_p, qfalse);
		bone->parentIndex = atoi(token);
		
		if(bone->parentIndex >= md5->numBones)
		{
			ri.Error(ERR_DROP, "R_LoadMD5: '%s' has bone '%s' with bad parent index %i while numBones is %i\n", modName,
					 bone->name, bone->parentIndex, md5->numBones);
		}
		
		// skip (
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		
		for(j = 0; j < 3; j++)
		{
			token = COM_ParseExt(&buf_p, qfalse);
			boneOrigin[j] = atof(token);
		}
		
		// skip )
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected ')' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		
		// skip (
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, "("))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		
		for(j = 0; j < 3; j++)
		{
			token = COM_ParseExt(&buf_p, qfalse);
			boneQuat[j] = atof(token);
		}
		QuatCalcW(boneQuat);
		MatrixFromQuat(boneMat, boneQuat);

		VectorCopy(boneOrigin, bone->origin);
		QuatCopy(boneQuat, bone->rotation);
		
		// skip )
		token = COM_ParseExt(&buf_p, qfalse);
		if(Q_stricmp(token, ")"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
	}
	
	// parse }
	token = COM_ParseExt(&buf_p, qtrue);
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
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "mesh"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'mesh' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = COM_ParseExt(&buf_p, qfalse);
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
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "shader"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'shader' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = COM_ParseExt(&buf_p, qfalse);
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
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "numVerts"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numVerts' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = COM_ParseExt(&buf_p, qfalse);
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
			token = COM_ParseExt(&buf_p, qtrue);
			if(Q_stricmp(token, "vert"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'vert' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			COM_ParseExt(&buf_p, qfalse);
			
			// skip (
			token = COM_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, "("))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
		
			for(k = 0; k < 2; k++)
			{
				token = COM_ParseExt(&buf_p, qfalse);
				v->texCoords[k] = atof(token);
			}
		
			// skip )
			token = COM_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, ")"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected ')' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			
			token = COM_ParseExt(&buf_p, qfalse);
			v->firstWeight = atoi(token);
			
			token = COM_ParseExt(&buf_p, qfalse);
			v->numWeights = atoi(token);
		}
		
		// parse numTris <number>
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "numTris"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numTris' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = COM_ParseExt(&buf_p, qfalse);
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
			token = COM_ParseExt(&buf_p, qtrue);
			if(Q_stricmp(token, "tri"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'tri' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			COM_ParseExt(&buf_p, qfalse);
		
			for(k = 0; k < 3; k++)
			{
				token = COM_ParseExt(&buf_p, qfalse);
				tri->indexes[k] = atoi(token);
			}
		}
		
		R_CalcSurfaceTriangleNeighbors(surf->numTriangles, surf->triangles);
		
		// parse numWeights <number>
		token = COM_ParseExt(&buf_p, qtrue);
		if(Q_stricmp(token, "numWeights"))
		{
			ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'numWeights' found '%s' in model '%s'\n", token, modName);
			return qfalse;
		}
		token = COM_ParseExt(&buf_p, qfalse);
		surf->numWeights = atoi(token);
		
		surf->weights = ri.Hunk_Alloc(sizeof(*weight) * surf->numWeights, h_low);
		for(j = 0, weight = surf->weights; j < surf->numWeights; j++, weight++)
		{
			// skip weight <number>
			token = COM_ParseExt(&buf_p, qtrue);
			if(Q_stricmp(token, "weight"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected 'weight' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
			COM_ParseExt(&buf_p, qfalse);
			
			token = COM_ParseExt(&buf_p, qfalse);
			weight->boneIndex = atoi(token);
			
			token = COM_ParseExt(&buf_p, qfalse);
			weight->boneWeight = atof(token);
			
			// skip (
			token = COM_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, "("))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected '(' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
		
			for(k = 0; k < 3; k++)
			{
				token = COM_ParseExt(&buf_p, qfalse);
				weight->offset[k] = atof(token);
			}
		
			// skip )
			token = COM_ParseExt(&buf_p, qfalse);
			if(Q_stricmp(token, ")"))
			{
				ri.Printf(PRINT_WARNING, "R_LoadMD5: expected ')' found '%s' in model '%s'\n", token, modName);
				return qfalse;
			}
		}
		
		// parse }
		token = COM_ParseExt(&buf_p, qtrue);
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

	return qtrue;
}

//=============================================================================

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration(glConfig_t * glconfigOut, glConfig2_t * glconfig2Out)
{
	R_Init();

	*glconfigOut = glConfig;
	*glconfig2Out = glConfig2;

	R_SyncRenderThread();

	tr.viewCluster = -1;		// force markleafs to regenerate
	R_ClearFlares();
	RE_ClearScene();
	
	// HACK: give world entity white color for "colored" shader keyword
	tr.worldEntity.e.shaderRGBA[0] = 255;
	tr.worldEntity.e.shaderRGBA[1] = 255;
	tr.worldEntity.e.shaderRGBA[2] = 255;
	tr.worldEntity.e.shaderRGBA[3] = 255;
	
	// FIXME: world entity shadows always use zfail algorithm which is slower than zpass
	tr.worldEntity.needZFail = qtrue;

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
		return 0;
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

	return 0;
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

	if(model->bmodel)
	{
		VectorCopy(model->bmodel->bounds[0], mins);
		VectorCopy(model->bmodel->bounds[1], maxs);
		return;
	}

	if(!model->mdx[0])
	{
		VectorClear(mins);
		VectorClear(maxs);
		return;
	}

	header = model->mdx[0];

	frame = header->frames;

	VectorCopy(frame->bounds[0], mins);
	VectorCopy(frame->bounds[1], maxs);
}
