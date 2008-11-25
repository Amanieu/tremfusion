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
// tr_shade.c
#include "tr_local.h"

/*
=================================================================================
THIS ENTIRE FILE IS BACK END!

This file deals with applying shaders to surface data in the tess struct.
=================================================================================
*/

enum
{
	ATTR_INDEX_TEXCOORD0	= 8,
	ATTR_INDEX_TEXCOORD1	= 9,
	ATTR_INDEX_TEXCOORD2	= 10,
	ATTR_INDEX_TEXCOORD3	= 11,
	ATTR_INDEX_TANGENT		= 12,
	ATTR_INDEX_BINORMAL		= 13,
//  ATTR_INDEX_NORMAL       = 2,
//  ATTR_INDEX_COLOR        = 3
};


static char    *RB_PrintInfoLog(GLhandleARB object)
{
	static char     msg[4096];
	int             maxLength = 0;

	qglGetObjectParameterivARB(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &maxLength);

	if(maxLength >= (int)sizeof(msg))
	{
		ri.Error(ERR_DROP, "RB_PrintInfoLog: max length >= sizeof(msg)");
		return NULL;
	}

	qglGetInfoLogARB(object, maxLength, &maxLength, msg);

	return msg;
}

static void RB_LoadGPUShader(GLhandleARB program, const char *name, GLenum shaderType)
{

	char            filename[MAX_QPATH];
	GLcharARB      *buffer = NULL;
	int             size;
	GLint           compiled;
	GLhandleARB     shader;

	if(shaderType == GL_VERTEX_SHADER_ARB)
	{
		Com_sprintf(filename, sizeof(filename), "glsl/%s_vp.glsl", name);
	}
	else
	{
		Com_sprintf(filename, sizeof(filename), "glsl/%s_fp.glsl", name);
	}

	ri.Printf(PRINT_DEVELOPER, "...loading '%s'\n", filename);
	size = ri.FS_ReadFile(filename, (void **)&buffer);
	if(!buffer)
	{
		ri.Error(ERR_DROP, "Couldn't load %s (tr_shade.c)", filename);
	}
	
	shader = qglCreateShaderObjectARB(shaderType);
	
	// HACK: add ATI's GLSL quirks
	if(glConfig.hardwareType == GLHW_ATI && shaderType == GL_VERTEX_SHADER_ARB)
	{
		GLcharARB      *bufferATI = "#define ATI\n";
		int             sizeATI;
	
		GLcharARB      *bufferFinal = NULL;
		int             sizeFinal;
		
		sizeATI = strlen(bufferATI);
		sizeFinal = sizeATI + size;
		
		bufferFinal = ri.Hunk_AllocateTempMemory(size + sizeATI);
		
		strcpy(bufferFinal, bufferATI);
		Q_strcat(bufferFinal, sizeFinal, buffer);
		
		qglShaderSourceARB(shader, 1, (const GLcharARB **)&bufferFinal, &sizeFinal);
		
		ri.Hunk_FreeTempMemory(bufferFinal);
	}
	else
	{
		qglShaderSourceARB(shader, 1, (const GLcharARB **)&buffer, &size);
	}

	// compile shader
	qglCompileShaderARB(shader);

	// check if shader compiled
	qglGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
	if(!compiled)
	{
		ri.Error(ERR_DROP, "Couldn't compile %s", RB_PrintInfoLog(shader));
		ri.FS_FreeFile(buffer);
		return;
	}
	ri.Printf(PRINT_DEVELOPER, "info log: %s\n", RB_PrintInfoLog(shader));

	// attach shader to program
	qglAttachObjectARB(program, shader);

	// delete shader, no longer needed
	qglDeleteObjectARB(shader);

	ri.FS_FreeFile(buffer);
}

static void RB_LinkProgram(GLhandleARB program)
{
	GLint           linked;

	qglLinkProgramARB(program);

	qglGetObjectParameterivARB(program, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(!linked)
	{
		ri.Error(ERR_DROP, "%s\nshaders failed to link", RB_PrintInfoLog(program));
	}
}

static void RB_ValidateProgram(GLhandleARB program)
{

	GLint           validated;

	qglValidateProgramARB(program);

	qglGetObjectParameterivARB(program, GL_OBJECT_VALIDATE_STATUS_ARB, &validated);
	if(!validated)
	{
		ri.Error(ERR_DROP, "%s\nshaders failed to validate", RB_PrintInfoLog(program));
	}
}

void RB_ShowProgramUniforms(GLhandleARB program)
{
	int				i, count, size, type;
	char			uniformName[1000];
	
	// install the executables in the program object as part of current state.
	qglUseProgramObjectARB(program);

	// check for GL Errors
	GL_CheckErrors();

	// query the number of active uniforms
	qglGetObjectParameterivARB(program, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &count);

	// Loop over each of the active uniforms, and set their value
	for(i = 0; i < count; i++)
	{
		qglGetActiveUniformARB(program, i, sizeof(uniformName), NULL, &size, &type, uniformName);
		
		ri.Printf(PRINT_DEVELOPER, "active uniform: '%s'\n", uniformName);
	}
	
	qglUseProgramObjectARB(0);
}

static void RB_InitGPUShader(shaderProgram_t * program, const char *name, int attribs, qboolean fragmentShader)
{
	ri.Printf(PRINT_DEVELOPER, "------- GPU shader -------\n");

	program->program = qglCreateProgramObjectARB();
	program->attribs = attribs;

	RB_LoadGPUShader(program->program, name, GL_VERTEX_SHADER_ARB);
	
	if(fragmentShader)
		RB_LoadGPUShader(program->program, name, GL_FRAGMENT_SHADER_ARB);

//  if( attribs & ATTRVERTEX )
//      qglBindAttribLocationARB( program->program, ATTR_INDEX_VERTEX, "attr_Vertex");

	if(attribs & GLCS_TEXCOORD0)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD0, "attr_TexCoord0");

	if(attribs & GLCS_TEXCOORD1)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD1, "attr_TexCoord1");
	
	if(attribs & GLCS_TEXCOORD2)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD2, "attr_TexCoord2");
	
	if(attribs & GLCS_TEXCOORD3)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD3, "attr_TexCoord3");

	if(attribs & GLCS_TANGENT)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TANGENT, "attr_Tangent");

	if(attribs & GLCS_BINORMAL)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_BINORMAL, "attr_Binormal");

//  if( attribs & GLCS_NORMAL )
//      qglBindAttribLocationARB( program->program, ATTR_INDEX_NORMAL, "attr_Normal");

//  if( attribs & GLCS_COLOR )
//      qglBindAttribLocationARB( program->program, ATTR_INDEX_COLOR, "attr_Color");

	RB_LinkProgram(program->program);
	RB_ValidateProgram(program->program);
	RB_ShowProgramUniforms(program->program);
}

void RB_InitGPUShaders(void)
{
	ri.Printf(PRINT_ALL, "------- RB_InitGPUShaders -------\n");

	if(!glConfig2.shadingLanguage100Available)
		return;
	
	//
	// single texture rendering
	//
	RB_InitGPUShader(&tr.genericShader_single,
					  "generic_single",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_COLOR, qtrue);

	tr.genericShader_single.u_ColorMap =
			qglGetUniformLocationARB(tr.genericShader_single.program, "u_ColorMap");

	qglUseProgramObjectARB(tr.genericShader_single.program);
	qglUniform1iARB(tr.genericShader_single.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// black depth fill rendering with textures
	//
	RB_InitGPUShader(&tr.depthFillShader,
					  "depthFill",
					  GLCS_VERTEX | GLCS_TEXCOORD0, qtrue);

	tr.depthFillShader.u_ColorMap =
			qglGetUniformLocationARB(tr.depthFillShader.program, "u_ColorMap");

	qglUseProgramObjectARB(tr.depthFillShader.program);
	qglUniform1iARB(tr.depthFillShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	//
	// directional lighting ( Q3A style )
	//
	RB_InitGPUShader(&tr.lightShader_D_direct,
					 "lighting_D_direct",
					 GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_NORMAL, qtrue);

	tr.lightShader_D_direct.u_DiffuseMap =
		qglGetUniformLocationARB(tr.lightShader_D_direct.program, "u_DiffuseMap");
	tr.lightShader_D_direct.u_AmbientColor =
		qglGetUniformLocationARB(tr.lightShader_D_direct.program, "u_AmbientColor");
	tr.lightShader_D_direct.u_LightDir =
		qglGetUniformLocationARB(tr.lightShader_D_direct.program, "u_LightDir");
	tr.lightShader_D_direct.u_LightColor =
		qglGetUniformLocationARB(tr.lightShader_D_direct.program, "u_LightColor");

	qglUseProgramObjectARB(tr.lightShader_D_direct.program);
	qglUniform1iARB(tr.lightShader_D_direct.u_DiffuseMap, 0);
	qglUseProgramObjectARB(0);

	//
	// directional bump mapping
	//
	RB_InitGPUShader(&tr.lightShader_DB_direct,
					 "lighting_DB_direct",
					 GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD1 | GLCS_TANGENT | GLCS_BINORMAL | GLCS_NORMAL, qtrue);

	tr.lightShader_DB_direct.u_DiffuseMap =
		qglGetUniformLocationARB(tr.lightShader_DB_direct.program, "u_DiffuseMap");
	tr.lightShader_DB_direct.u_NormalMap =
		qglGetUniformLocationARB(tr.lightShader_DB_direct.program, "u_NormalMap");
	tr.lightShader_DB_direct.u_AmbientColor =
		qglGetUniformLocationARB(tr.lightShader_DB_direct.program, "u_AmbientColor");
	tr.lightShader_DB_direct.u_LightDir =
		qglGetUniformLocationARB(tr.lightShader_DB_direct.program, "u_LightDir");
	tr.lightShader_DB_direct.u_LightColor =
		qglGetUniformLocationARB(tr.lightShader_DB_direct.program, "u_LightColor");
//	tr.lightShader_DB_direct.u_BumpScale =
//		qglGetUniformLocationARB(tr.lightShader_DB_direct.program, "u_BumpScale");

	qglUseProgramObjectARB(tr.lightShader_DB_direct.program);
	qglUniform1iARB(tr.lightShader_DB_direct.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_DB_direct.u_NormalMap, 1);
	qglUseProgramObjectARB(0);
	
	//
	// directional specular bump mapping
	//
	RB_InitGPUShader(&tr.lightShader_DBS_direct,
					 "lighting_DBS_direct",
					 GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD1 | GLCS_TEXCOORD2 | GLCS_TANGENT | GLCS_BINORMAL | GLCS_NORMAL,
					 qtrue);

	tr.lightShader_DBS_direct.u_DiffuseMap =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_DiffuseMap");
	tr.lightShader_DBS_direct.u_NormalMap =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_NormalMap");
	tr.lightShader_DBS_direct.u_SpecularMap =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_SpecularMap");
	tr.lightShader_DBS_direct.u_ViewOrigin =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_ViewOrigin");
	tr.lightShader_DBS_direct.u_AmbientColor =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_AmbientColor");
	tr.lightShader_DBS_direct.u_LightDir =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_LightDir");
	tr.lightShader_DBS_direct.u_LightColor =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_LightColor");
	tr.lightShader_DBS_direct.u_SpecularExponent =
		qglGetUniformLocationARB(tr.lightShader_DBS_direct.program, "u_SpecularExponent");

	qglUseProgramObjectARB(tr.lightShader_DBS_direct.program);
	qglUniform1iARB(tr.lightShader_DBS_direct.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_DBS_direct.u_NormalMap, 1);
	qglUniform1iARB(tr.lightShader_DBS_direct.u_SpecularMap, 2);
	qglUseProgramObjectARB(0);
	
	//
	// omni-directional lighting ( Doom3 style )
	//
	RB_InitGPUShader(&tr.lightShader_D_omni,
					  "lighting_D_omni",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_NORMAL, qtrue);

	tr.lightShader_D_omni.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_D_omni.program, "u_DiffuseMap");
	tr.lightShader_D_omni.u_AttenuationMapXY =
			qglGetUniformLocationARB(tr.lightShader_D_omni.program, "u_AttenuationMapXY");
	tr.lightShader_D_omni.u_AttenuationMapZ =
			qglGetUniformLocationARB(tr.lightShader_D_omni.program, "u_AttenuationMapZ");
	tr.lightShader_D_omni.u_LightOrigin =
			qglGetUniformLocationARB(tr.lightShader_D_omni.program, "u_LightOrigin");
	tr.lightShader_D_omni.u_LightColor =
			qglGetUniformLocationARB(tr.lightShader_D_omni.program, "u_LightColor");
	tr.lightShader_D_omni.u_LightScale =
			qglGetUniformLocationARB(tr.lightShader_D_omni.program, "u_LightScale");

	qglUseProgramObjectARB(tr.lightShader_D_omni.program);
	qglUniform1iARB(tr.lightShader_D_omni.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_D_omni.u_AttenuationMapXY, 1);
	qglUniform1iARB(tr.lightShader_D_omni.u_AttenuationMapZ, 2);
	qglUseProgramObjectARB(0);
	
	//
	// omni-directional bump mapping ( Doom3 style )
	//
	RB_InitGPUShader(&tr.lightShader_DB_omni,
					  "lighting_DB_omni",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD1 | GLCS_TANGENT | GLCS_BINORMAL | GLCS_NORMAL, qtrue);

	tr.lightShader_DB_omni.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_DiffuseMap");
	tr.lightShader_DB_omni.u_NormalMap =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_NormalMap");
	tr.lightShader_DB_omni.u_AttenuationMapXY =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_AttenuationMapXY");
	tr.lightShader_DB_omni.u_AttenuationMapZ =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_AttenuationMapZ");
	tr.lightShader_DB_omni.u_LightOrigin =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_LightOrigin");
	tr.lightShader_DB_omni.u_LightColor =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_LightColor");
	tr.lightShader_DB_omni.u_LightScale =
			qglGetUniformLocationARB(tr.lightShader_DB_omni.program, "u_LightScale");

	qglUseProgramObjectARB(tr.lightShader_DB_omni.program);
	qglUniform1iARB(tr.lightShader_DB_omni.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_DB_omni.u_NormalMap, 1);
	qglUniform1iARB(tr.lightShader_DB_omni.u_AttenuationMapXY, 2);
	qglUniform1iARB(tr.lightShader_DB_omni.u_AttenuationMapZ, 3);
	qglUseProgramObjectARB(0);
	
	//
	// omni-directional specular bump mapping ( Doom3 style )
	//
	RB_InitGPUShader(&tr.lightShader_DBS_omni,
					  "lighting_DBS_omni",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD1 | GLCS_TEXCOORD2 | GLCS_TANGENT | GLCS_BINORMAL | GLCS_NORMAL, qtrue);

	tr.lightShader_DBS_omni.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_DiffuseMap");
	tr.lightShader_DBS_omni.u_NormalMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_NormalMap");
	tr.lightShader_DBS_omni.u_SpecularMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_SpecularMap");
	tr.lightShader_DBS_omni.u_AttenuationMapXY =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_AttenuationMapXY");
	tr.lightShader_DBS_omni.u_AttenuationMapZ =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_AttenuationMapZ");
	tr.lightShader_DBS_omni.u_ViewOrigin =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_ViewOrigin");
	tr.lightShader_DBS_omni.u_LightOrigin =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_LightOrigin");
	tr.lightShader_DBS_omni.u_LightColor =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_LightColor");
	tr.lightShader_DBS_omni.u_LightScale =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_LightScale");
	tr.lightShader_DBS_omni.u_SpecularExponent =
			qglGetUniformLocationARB(tr.lightShader_DBS_omni.program, "u_SpecularExponent");

	qglUseProgramObjectARB(tr.lightShader_DBS_omni.program);
	qglUniform1iARB(tr.lightShader_DBS_omni.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_DBS_omni.u_NormalMap, 1);
	qglUniform1iARB(tr.lightShader_DBS_omni.u_SpecularMap, 2);
	qglUniform1iARB(tr.lightShader_DBS_omni.u_AttenuationMapXY, 3);
	qglUniform1iARB(tr.lightShader_DBS_omni.u_AttenuationMapZ, 4);
	qglUseProgramObjectARB(0);
	
	//
	// projective lighting ( Doom3 style )
	//
	RB_InitGPUShader(&tr.lightShader_D_proj,
					  "lighting_D_proj",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_NORMAL, qtrue);

	tr.lightShader_D_proj.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_D_proj.program, "u_DiffuseMap");
	tr.lightShader_D_proj.u_AttenuationMapXY =
			qglGetUniformLocationARB(tr.lightShader_D_proj.program, "u_AttenuationMapXY");
	tr.lightShader_D_proj.u_AttenuationMapZ =
			qglGetUniformLocationARB(tr.lightShader_D_proj.program, "u_AttenuationMapZ");
	tr.lightShader_D_proj.u_LightOrigin =
			qglGetUniformLocationARB(tr.lightShader_D_proj.program, "u_LightOrigin");
	tr.lightShader_D_proj.u_LightColor =
			qglGetUniformLocationARB(tr.lightShader_D_proj.program, "u_LightColor");
	tr.lightShader_D_proj.u_LightScale =
			qglGetUniformLocationARB(tr.lightShader_D_proj.program, "u_LightScale");

	qglUseProgramObjectARB(tr.lightShader_D_proj.program);
	qglUniform1iARB(tr.lightShader_D_proj.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_D_proj.u_AttenuationMapXY, 1);
	qglUniform1iARB(tr.lightShader_D_proj.u_AttenuationMapZ, 2);
	qglUseProgramObjectARB(0);
	
	//
	// radiosity lighting ( Q3A style )
	//
	RB_InitGPUShader(&tr.lightShader_D_radiosity,
					  "lighting_D_radiosity",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD3 | GLCS_NORMAL, qtrue);

	tr.lightShader_D_radiosity.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_D_radiosity.program, "u_DiffuseMap");
	tr.lightShader_D_radiosity.u_LightMap =
			qglGetUniformLocationARB(tr.lightShader_D_radiosity.program, "u_LightMap");

	qglUseProgramObjectARB(tr.lightShader_D_radiosity.program);
	qglUniform1iARB(tr.lightShader_D_radiosity.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_D_radiosity.u_LightMap, 1);
	qglUseProgramObjectARB(0);
	
	//
	// radiosity bump mapping
	//
	RB_InitGPUShader(&tr.lightShader_DB_radiosity,
					  "lighting_DB_radiosity",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD1 | GLCS_TEXCOORD3 | GLCS_TANGENT | GLCS_BINORMAL | GLCS_NORMAL, qtrue);

	tr.lightShader_DB_radiosity.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_DB_radiosity.program, "u_DiffuseMap");
	tr.lightShader_DB_radiosity.u_NormalMap =
			qglGetUniformLocationARB(tr.lightShader_DB_radiosity.program, "u_NormalMap");
	tr.lightShader_DB_radiosity.u_LightMap =
			qglGetUniformLocationARB(tr.lightShader_DB_radiosity.program, "u_LightMap");
	tr.lightShader_DB_radiosity.u_DeluxeMap =
			qglGetUniformLocationARB(tr.lightShader_DB_radiosity.program, "u_DeluxeMap");

	qglUseProgramObjectARB(tr.lightShader_DB_radiosity.program);
	qglUniform1iARB(tr.lightShader_DB_radiosity.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_DB_radiosity.u_NormalMap, 1);
	qglUniform1iARB(tr.lightShader_DB_radiosity.u_LightMap, 2);
	qglUniform1iARB(tr.lightShader_DB_radiosity.u_DeluxeMap, 3);
	qglUseProgramObjectARB(0);
	
	//
	// radiosity specular bump mapping
	//
	RB_InitGPUShader(&tr.lightShader_DBS_radiosity,
					  "lighting_DBS_radiosity",
					  GLCS_VERTEX | GLCS_TEXCOORD0 | GLCS_TEXCOORD1 | GLCS_TEXCOORD2 | GLCS_TEXCOORD3 | GLCS_TANGENT | GLCS_BINORMAL | GLCS_NORMAL, qtrue);

	tr.lightShader_DBS_radiosity.u_DiffuseMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_DiffuseMap");
	tr.lightShader_DBS_radiosity.u_NormalMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_NormalMap");
	tr.lightShader_DBS_radiosity.u_SpecularMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_SpecularMap");
	tr.lightShader_DBS_radiosity.u_LightMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_LightMap");
	tr.lightShader_DBS_radiosity.u_DeluxeMap =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_DeluxeMap");
	tr.lightShader_DBS_radiosity.u_ViewOrigin =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_ViewOrigin");
	tr.lightShader_DBS_radiosity.u_SpecularExponent =
			qglGetUniformLocationARB(tr.lightShader_DBS_radiosity.program, "u_SpecularExponent");

	qglUseProgramObjectARB(tr.lightShader_DBS_radiosity.program);
	qglUniform1iARB(tr.lightShader_DBS_radiosity.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightShader_DBS_radiosity.u_NormalMap, 1);
	qglUniform1iARB(tr.lightShader_DBS_radiosity.u_SpecularMap, 2);
	qglUniform1iARB(tr.lightShader_DBS_radiosity.u_LightMap, 3);
	qglUniform1iARB(tr.lightShader_DBS_radiosity.u_DeluxeMap, 4);
	qglUseProgramObjectARB(0);
	
	//
	// shadow volume extrusion
	//
	RB_InitGPUShader(&tr.shadowShader,
					  "shadow",
					  GLCS_VERTEX, qtrue);

	tr.shadowShader.u_LightOrigin =
			qglGetUniformLocationARB(tr.shadowShader.program, "u_LightOrigin");
	
	//
	// cubemap reflection for abitrary polygons
	//
	RB_InitGPUShader(&tr.reflectionShader_C,
					  "reflection_C",
					  GLCS_VERTEX | GLCS_NORMAL, qtrue);

	tr.reflectionShader_C.u_ColorMap =
			qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_ColorMap");
	tr.reflectionShader_C.u_ViewOrigin =
			qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_ViewOrigin");

	qglUseProgramObjectARB(tr.reflectionShader_C.program);
	qglUniform1iARB(tr.reflectionShader_C.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// cubemap refraction for abitrary polygons
	//
	RB_InitGPUShader(&tr.refractionShader_C,
					  "refraction_C",
					  GLCS_VERTEX | GLCS_NORMAL, qtrue);

	tr.reflectionShader_C.u_ColorMap =
			qglGetUniformLocationARB(tr.refractionShader_C.program, "u_ColorMap");
	tr.refractionShader_C.u_ViewOrigin =
			qglGetUniformLocationARB(tr.refractionShader_C.program, "u_ViewOrigin");
	tr.refractionShader_C.u_RefractionIndex =
			qglGetUniformLocationARB(tr.refractionShader_C.program, "u_RefractionIndex");
	tr.refractionShader_C.u_FresnelPower =
			qglGetUniformLocationARB(tr.refractionShader_C.program, "u_FresnelPower");
	tr.refractionShader_C.u_FresnelScale =
			qglGetUniformLocationARB(tr.refractionShader_C.program, "u_FresnelScale");
	tr.refractionShader_C.u_FresnelBias =
			qglGetUniformLocationARB(tr.refractionShader_C.program, "u_FresnelBias");

	qglUseProgramObjectARB(tr.refractionShader_C.program);
	qglUniform1iARB(tr.refractionShader_C.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// cubemap dispersion for abitrary polygons
	//
	RB_InitGPUShader(&tr.dispersionShader_C,
					  "dispersion_C",
					  GLCS_VERTEX | GLCS_NORMAL, qtrue);

	tr.reflectionShader_C.u_ColorMap =
			qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_ColorMap");
	tr.dispersionShader_C.u_ViewOrigin =
			qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_ViewOrigin");
	tr.dispersionShader_C.u_EtaRatio =
			qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_EtaRatio");
	tr.dispersionShader_C.u_FresnelPower =
			qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_FresnelPower");
	tr.dispersionShader_C.u_FresnelScale =
			qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_FresnelScale");
	tr.dispersionShader_C.u_FresnelBias =
			qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_FresnelBias");

	qglUseProgramObjectARB(tr.dispersionShader_C.program);
	qglUniform1iARB(tr.dispersionShader_C.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// skybox drawing for abitrary polygons
	//
	RB_InitGPUShader(&tr.skyBoxShader,
					  "skybox",
					  GLCS_VERTEX | GLCS_NORMAL, qtrue);

	tr.skyBoxShader.u_ColorMap =
			qglGetUniformLocationARB(tr.skyBoxShader.program, "u_ColorMap");
	tr.skyBoxShader.u_ViewOrigin =
			qglGetUniformLocationARB(tr.skyBoxShader.program, "u_ViewOrigin");

	qglUseProgramObjectARB(tr.skyBoxShader.program);
	qglUniform1iARB(tr.skyBoxShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// heatHaze post process effect
	//
	RB_InitGPUShader(&tr.heatHazeShader,
					  "heatHaze",
					  GLCS_VERTEX | GLCS_TEXCOORD0, qtrue);

	if(glConfig.hardwareType == GLHW_ATI)
	{
		tr.heatHazeShader.u_ProjectionMatrixTranspose =
				qglGetUniformLocationARB(tr.heatHazeShader.program, "u_ProjectionMatrixTranspose");
	}
	tr.heatHazeShader.u_DeformMagnitude =
			qglGetUniformLocationARB(tr.heatHazeShader.program, "u_DeformMagnitude");
	tr.heatHazeShader.u_ColorMap =
			qglGetUniformLocationARB(tr.heatHazeShader.program, "u_ColorMap");
	tr.heatHazeShader.u_NormalMap =
			qglGetUniformLocationARB(tr.heatHazeShader.program, "u_NormalMap");
	tr.heatHazeShader.u_FBufScale =
			qglGetUniformLocationARB(tr.heatHazeShader.program, "u_FBufScale");
	tr.heatHazeShader.u_NPotScale =
			qglGetUniformLocationARB(tr.heatHazeShader.program, "u_NPotScale");

	qglUseProgramObjectARB(tr.heatHazeShader.program);
	qglUniform1iARB(tr.heatHazeShader.u_ColorMap, 0);
	qglUniform1iARB(tr.heatHazeShader.u_NormalMap, 1);
	qglUseProgramObjectARB(0);
	
	//
	// glow post process effect
	//
	RB_InitGPUShader(&tr.glowShader,
					  "glow",
					  GLCS_VERTEX | GLCS_TEXCOORD0, qtrue);

	tr.glowShader.u_ColorMap =
			qglGetUniformLocationARB(tr.glowShader.program, "u_ColorMap");
	tr.glowShader.u_FBufScale =
			qglGetUniformLocationARB(tr.glowShader.program, "u_FBufScale");
	tr.glowShader.u_NPotScale =
			qglGetUniformLocationARB(tr.glowShader.program, "u_NPotScale");
	tr.glowShader.u_BlurMagnitude =
			qglGetUniformLocationARB(tr.glowShader.program, "u_BlurMagnitude");

	qglUseProgramObjectARB(tr.glowShader.program);
	qglUniform1iARB(tr.glowShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// bloom post process effect
	//
	RB_InitGPUShader(&tr.bloomShader,
					  "bloom",
					  GLCS_VERTEX, qtrue);

	tr.bloomShader.u_ColorMap =
			qglGetUniformLocationARB(tr.bloomShader.program, "u_ColorMap");
	tr.bloomShader.u_ContrastMap =
			qglGetUniformLocationARB(tr.bloomShader.program, "u_ContrastMap");
	tr.bloomShader.u_FBufScale =
			qglGetUniformLocationARB(tr.bloomShader.program, "u_FBufScale");
	tr.bloomShader.u_NPotScale =
			qglGetUniformLocationARB(tr.bloomShader.program, "u_NPotScale");
	tr.bloomShader.u_BlurMagnitude =
			qglGetUniformLocationARB(tr.bloomShader.program, "u_BlurMagnitude");

	qglUseProgramObjectARB(tr.bloomShader.program);
	qglUniform1iARB(tr.bloomShader.u_ColorMap, 0);
	qglUniform1iARB(tr.bloomShader.u_ContrastMap, 1);
	qglUseProgramObjectARB(0);
	
	//
	// contrast post process effect
	//
	RB_InitGPUShader(&tr.contrastShader,
					  "contrast",
					  GLCS_VERTEX, qtrue);

	tr.contrastShader.u_ColorMap =
			qglGetUniformLocationARB(tr.contrastShader.program, "u_ColorMap");
	tr.contrastShader.u_FBufScale =
			qglGetUniformLocationARB(tr.contrastShader.program, "u_FBufScale");
	tr.contrastShader.u_NPotScale =
			qglGetUniformLocationARB(tr.contrastShader.program, "u_NPotScale");

	qglUseProgramObjectARB(tr.contrastShader.program);
	qglUniform1iARB(tr.contrastShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// blurX post process effect
	//
	RB_InitGPUShader(&tr.blurXShader,
					  "blurX",
					  GLCS_VERTEX, qtrue);

	tr.blurXShader.u_ColorMap =
			qglGetUniformLocationARB(tr.blurXShader.program, "u_ColorMap");
	tr.blurXShader.u_FBufScale =
			qglGetUniformLocationARB(tr.blurXShader.program, "u_FBufScale");
	tr.blurXShader.u_NPotScale =
			qglGetUniformLocationARB(tr.blurXShader.program, "u_NPotScale");

	qglUseProgramObjectARB(tr.blurXShader.program);
	qglUniform1iARB(tr.blurXShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
	
	//
	// blurY post process effect
	//
	RB_InitGPUShader(&tr.blurYShader,
					  "blurY",
					  GLCS_VERTEX, qtrue);

	tr.blurYShader.u_ColorMap =
			qglGetUniformLocationARB(tr.blurYShader.program, "u_ColorMap");
	tr.blurYShader.u_FBufScale =
			qglGetUniformLocationARB(tr.blurYShader.program, "u_FBufScale");
	tr.blurYShader.u_NPotScale =
			qglGetUniformLocationARB(tr.blurYShader.program, "u_NPotScale");

	qglUseProgramObjectARB(tr.blurYShader.program);
	qglUniform1iARB(tr.blurYShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);
}

void RB_ShutdownGPUShaders(void)
{
	ri.Printf(PRINT_ALL, "------- RB_ShutdownGPUShaders -------\n");

	if(!glConfig2.shadingLanguage100Available)
		return;
	
	GL_Program(0);
	
	if(tr.genericShader_single.program)
	{
		qglDeleteObjectARB(tr.genericShader_single.program);
		tr.genericShader_single.program = 0;
	}
	
	if(tr.depthFillShader.program)
	{
		qglDeleteObjectARB(tr.depthFillShader.program);
		tr.depthFillShader.program = 0;
	}

	if(tr.lightShader_D_direct.program)
	{
		qglDeleteObjectARB(tr.lightShader_D_direct.program);
		tr.lightShader_D_direct.program = 0;
	}

	if(tr.lightShader_DB_direct.program)
	{
		qglDeleteObjectARB(tr.lightShader_DB_direct.program);
		tr.lightShader_DB_direct.program = 0;
	}
	
	if(tr.lightShader_DBS_direct.program)
	{
		qglDeleteObjectARB(tr.lightShader_DBS_direct.program);
		tr.lightShader_DBS_direct.program = 0;
	}
	
	if(tr.lightShader_D_omni.program)
	{
		qglDeleteObjectARB(tr.lightShader_D_omni.program);
		tr.lightShader_D_omni.program = 0;
	}
	
	if(tr.lightShader_DB_omni.program)
	{
		qglDeleteObjectARB(tr.lightShader_DB_omni.program);
		tr.lightShader_DB_omni.program = 0;
	}
	
	if(tr.lightShader_DBS_omni.program)
	{
		qglDeleteObjectARB(tr.lightShader_DBS_omni.program);
		tr.lightShader_DBS_omni.program = 0;
	}
	
	if(tr.lightShader_D_proj.program)
	{
		qglDeleteObjectARB(tr.lightShader_D_proj.program);
		tr.lightShader_D_proj.program = 0;
	}
	
	if(tr.lightShader_D_radiosity.program)
	{
		qglDeleteObjectARB(tr.lightShader_D_radiosity.program);
		tr.lightShader_D_radiosity.program = 0;
	}
	
	if(tr.lightShader_DB_radiosity.program)
	{
		qglDeleteObjectARB(tr.lightShader_DB_radiosity.program);
		tr.lightShader_DB_radiosity.program = 0;
	}
	
	if(tr.lightShader_DBS_radiosity.program)
	{
		qglDeleteObjectARB(tr.lightShader_DBS_radiosity.program);
		tr.lightShader_DBS_radiosity.program = 0;
	}
	
	if(tr.shadowShader.program)
	{
		qglDeleteObjectARB(tr.shadowShader.program);
		tr.shadowShader.program = 0;
	}
	
	if(tr.reflectionShader_C.program)
	{
		qglDeleteObjectARB(tr.reflectionShader_C.program);
		tr.reflectionShader_C.program = 0;
	}
	
	if(tr.refractionShader_C.program)
	{
		qglDeleteObjectARB(tr.refractionShader_C.program);
		tr.refractionShader_C.program = 0;
	}
	
	if(tr.dispersionShader_C.program)
	{
		qglDeleteObjectARB(tr.dispersionShader_C.program);
		tr.dispersionShader_C.program = 0;
	}
	
	if(tr.skyBoxShader.program)
	{
		qglDeleteObjectARB(tr.skyBoxShader.program);
		tr.skyBoxShader.program = 0;
	}
	
	if(tr.heatHazeShader.program)
	{
		qglDeleteObjectARB(tr.heatHazeShader.program);
		tr.heatHazeShader.program = 0;
	}
	
	if(tr.glowShader.program)
	{
		qglDeleteObjectARB(tr.glowShader.program);
		tr.glowShader.program = 0;
	}
	
	if(tr.bloomShader.program)
	{
		qglDeleteObjectARB(tr.bloomShader.program);
		tr.bloomShader.program = 0;
	}
	
	if(tr.contrastShader.program)
	{
		qglDeleteObjectARB(tr.contrastShader.program);
		tr.contrastShader.program = 0;
	}
	
	if(tr.blurXShader.program)
	{
		qglDeleteObjectARB(tr.blurXShader.program);
		tr.blurXShader.program = 0;
	}
	
	if(tr.blurYShader.program)
	{
		qglDeleteObjectARB(tr.blurYShader.program);
		tr.blurYShader.program = 0;
	}
}

void GL_ClientState(unsigned long stateBits)
{
	unsigned long   diff = stateBits ^ glState.glClientStateBits;

	if(!diff)
	{
		return;
	}
	
	/*
	if(diff & GLCS_VERTEX)
	{
		if(stateBits & GLCS_VERTEX)
		{
			qglEnableClientState(GL_VERTEX_ARRAY);
		}
		else
		{
			qglDisableClientState(GL_VERTEX_ARRAY);
		}
	}
	*/
	
	if(diff & GLCS_TEXCOORD0)
	{
		if(stateBits & GLCS_TEXCOORD0)
		{
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD0);
		}
		else
		{
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD0);
		}
	}
	
	if(diff & GLCS_TEXCOORD1)
	{
		if(stateBits & GLCS_TEXCOORD1)
		{
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD1);
		}
		else
		{
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD1);
		}
	}
	
	if(diff & GLCS_TEXCOORD2)
	{
		if(stateBits & GLCS_TEXCOORD2)
		{
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD2);
		}
		else
		{
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD2);
		}
	}
	
	if(diff & GLCS_TEXCOORD3)
	{
		if(stateBits & GLCS_TEXCOORD3)
		{
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD3);
		}
		else
		{
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TEXCOORD3);
		}
	}

	if(diff & GLCS_TANGENT)
	{
		if(stateBits & GLCS_TANGENT)
		{
			qglEnableVertexAttribArrayARB(ATTR_INDEX_TANGENT);
		}
		else
		{
			qglDisableVertexAttribArrayARB(ATTR_INDEX_TANGENT);
		}
	}

	if(diff & GLCS_BINORMAL)
	{
		if(stateBits & GLCS_BINORMAL)
		{
			qglEnableVertexAttribArrayARB(ATTR_INDEX_BINORMAL);
		}
		else
		{
			qglDisableVertexAttribArrayARB(ATTR_INDEX_BINORMAL);
		}
	}
	
	if(diff & GLCS_NORMAL)
	{
		if(stateBits & GLCS_NORMAL)
		{
			qglEnableClientState(GL_NORMAL_ARRAY);
		}
		else
		{
			qglDisableClientState(GL_NORMAL_ARRAY);
		}
	}
	
	if(diff & GLCS_COLOR)
	{
		if(stateBits & GLCS_COLOR)
		{
			qglEnableClientState(GL_COLOR_ARRAY);
		}
		else
		{
			qglDisableClientState(GL_COLOR_ARRAY);
		}
	}
	
	glState.glClientStateBits = stateBits;
}

void GL_SetVertexAttribs()
{
	//static GLuint   oldVertexesVBO = 0;
	
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		/*
		if(oldVertexesVBO == tess.vertexesVBO)
		{
			// no update needed so avoid expensive glVertexPointer call
			return;
		}
		
		oldVertexesVBO = tess.vertexesVBO;
		*/
		
		//if(glState.glClientStateBits & GLCS_VERTEX)
		//	qglVertexPointer(3, GL_FLOAT, 16, BUFFER_OFFSET(tess.ofsXYZ));
	
		if(glState.glClientStateBits & GLCS_TEXCOORD0)
			qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(tess.ofsTexCoords));
	
		if(glState.glClientStateBits & GLCS_TEXCOORD1)
			qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(tess.ofsTexCoords));
		
		if(glState.glClientStateBits & GLCS_TEXCOORD2)
			qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(tess.ofsTexCoords));
		
		if(glState.glClientStateBits & GLCS_TEXCOORD3)
			qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD3, 4, GL_FLOAT, 0, 0, BUFFER_OFFSET(tess.ofsTexCoords2));
	
		if(glState.glClientStateBits & GLCS_TANGENT)
			qglVertexAttribPointerARB(ATTR_INDEX_TANGENT, 3, GL_FLOAT, 0, 16, BUFFER_OFFSET(tess.ofsTangents));
	
		if(glState.glClientStateBits & GLCS_BINORMAL)
			qglVertexAttribPointerARB(ATTR_INDEX_BINORMAL, 3, GL_FLOAT, 0, 16, BUFFER_OFFSET(tess.ofsBinormals));
	
		if(glState.glClientStateBits & GLCS_NORMAL)
			qglNormalPointer(GL_FLOAT, 16, BUFFER_OFFSET(tess.ofsNormals));
	
		if(glState.glClientStateBits & GLCS_COLOR)
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, BUFFER_OFFSET(tess.ofsColors));
	}
	else
	{
		//if(glState.glClientStateBits & GLCS_VERTEX)
		//	qglVertexPointer(4, GL_FLOAT, 0, tess.xyz);
	
		if(glState.glClientStateBits & GLCS_TEXCOORD0)
			qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD0, 2, GL_FLOAT, 0, 0, tess.svars.texCoords[TB_COLORMAP]);
	
		if(glState.glClientStateBits & GLCS_TEXCOORD1)
		{
			if(tess.svars.skipCoords[TB_NORMALMAP])
				qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD1, 2, GL_FLOAT, 0, 0, tess.svars.texCoords[TB_DIFFUSEMAP]);
			else
				qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD1, 2, GL_FLOAT, 0, 0, tess.svars.texCoords[TB_NORMALMAP]);
		}
		
		if(glState.glClientStateBits & GLCS_TEXCOORD2)
		{
			if(tess.svars.skipCoords[TB_SPECULARMAP])
				qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD2, 2, GL_FLOAT, 0, 0, tess.svars.texCoords[TB_DIFFUSEMAP]);
			else
				qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD2, 2, GL_FLOAT, 0, 0, tess.svars.texCoords[TB_SPECULARMAP]);
		}
		
		if(glState.glClientStateBits & GLCS_TEXCOORD3)
			qglVertexAttribPointerARB(ATTR_INDEX_TEXCOORD3, 2, GL_FLOAT, 0, 0, tess.svars.texCoords[TB_LIGHTMAP]);
	
		if(glState.glClientStateBits & GLCS_TANGENT)
			qglVertexAttribPointerARB(ATTR_INDEX_TANGENT, 3, GL_FLOAT, 0, 16, tess.tangents);
	
		if(glState.glClientStateBits & GLCS_BINORMAL)
			qglVertexAttribPointerARB(ATTR_INDEX_BINORMAL, 3, GL_FLOAT, 0, 16, tess.binormals);
	
		if(glState.glClientStateBits & GLCS_NORMAL)
			qglNormalPointer(GL_FLOAT, 16, tess.normals);
	
		if(glState.glClientStateBits & GLCS_COLOR)
			qglColorPointer(4, GL_UNSIGNED_BYTE, 0, tess.svars.colors);
	}
}


/*
==================
R_DrawElements
==================
*/
static void R_DrawElements()
{
	// move tess data through the GPU, finally
	if(glConfig2.vertexBufferObjectAvailable && tess.indexesVBO)
	{
		qglDrawElements(GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, 0);
		backEnd.pc.c_indexes += tess.numIndexes;
	}
	else
	{
		qglDrawElements(GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, tess.indexes);
		backEnd.pc.c_indexes += tess.numIndexes;
	}
	
	// update performance counters
	backEnd.pc.c_drawElements++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	
	if(tess.indexesVBO)
	{
		backEnd.pc.c_vboIndexes += tess.numIndexes;
	}
		
	if(tess.vertexesVBO)
	{
		backEnd.pc.c_vboVertexes += tess.numVertexes;
	}
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t tess;


/*
=================
R_BindAnimatedImage
=================
*/
static void R_BindAnimatedImage(textureBundle_t * bundle)
{
	int             index;

	if(bundle->isVideoMap)
	{
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if(bundle->numImageAnimations <= 1)
	{
		GL_Bind(bundle->image[0]);
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = myftol(tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE);
	index >>= FUNCTABLE_SIZE2;

	if(index < 0)
	{
		index = 0;				// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	GL_Bind(bundle->image[index]);
}

/*
=================
R_BindLightMap
=================
*/
static void R_BindLightMap()
{
	image_t        *lightmap;
	
	if(tess.lightmapNum >= 0 && tess.lightmapNum < tr.numLightmaps)
	{
		lightmap = tr.lightmaps[tess.lightmapNum];
	}
	else
	{
		lightmap = NULL;	
	}
	
	if(!tr.numLightmaps || !lightmap)
	{
		GL_Bind(tr.whiteImage);
		return;
	}

	GL_Bind(lightmap);
}

/*
=================
R_BindDeluxeMap
=================
*/
static void R_BindDeluxeMap()
{
	image_t        *deluxemap;
	
	if(tess.lightmapNum >= 0 && tess.lightmapNum < tr.numLightmaps)
	{
		deluxemap = tr.lightmaps[tess.lightmapNum + 1];
	}
	else
	{
		deluxemap = NULL;
	}
	
	if(!tr.numLightmaps || !deluxemap)
	{
		GL_Bind(tr.flatImage);
		return;
	}

	GL_Bind(deluxemap);
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris()
{
	switch (tess.currentStageIteratorType)
	{
		case SIT_LIGHTING:
		case SIT_LIGHTING_STENCIL:
			qglColor3f(1, 0, 0);
			break;
		
		default:
		case SIT_DEFAULT:
			if(tess.indexesVBO && tess.vertexesVBO)
			{
				qglColor3f(1, 0, 0);
			}
			else if(tess.indexesVBO)
			{
				qglColor3f(0, 1, 0);
			}
			else if(tess.vertexesVBO)
			{
				qglColor3f(0, 0, 1);
			}
			else
			{
				qglColor3f(1, 1, 1);
			}
			break;
	}
	
	GL_Program(0);
	GL_SelectTexture(0);
	GL_Bind(tr.whiteImage);
	
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);
	GL_ClientState(GLCS_VERTEX);
	qglDepthRange(0, 0);

	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		qglVertexPointer(3, GL_FLOAT, 16, BUFFER_OFFSET(tess.ofsXYZ));
	}
	else
	{
		qglVertexPointer(4, GL_FLOAT, 0, tess.xyz);
	}
	
	if(qglLockArraysEXT)
	{
		qglLockArraysEXT(0, tess.numVertexes);
		GLimp_LogComment("glLockArraysEXT\n");
	}

	R_DrawElements();

	if(qglUnlockArraysEXT)
	{
		qglUnlockArraysEXT();
		GLimp_LogComment("glUnlockArraysEXT\n");
	}
	qglDepthRange(0, 1);
}


/*
================
DrawTangentSpaces

Draws vertex tangent spaces for debugging
================
*/
static void DrawTangentSpaces()
{
	int             i;
	vec3_t          temp;

	GL_Program(0);
	GL_SelectTexture(0);
	GL_Bind(tr.whiteImage);
	qglDepthRange(0, 0);		// never occluded
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);

	qglBegin(GL_LINES);
	for(i = 0; i < tess.numVertexes; i++)
	{
		qglColor3f(1, 0, 0);
		qglVertex3fv(tess.xyz[i]);
		VectorMA(tess.xyz[i], 2, tess.tangents[i], temp);
		qglVertex3fv(temp);

		qglColor3f(0, 1, 0);
		qglVertex3fv(tess.xyz[i]);
		VectorMA(tess.xyz[i], 2, tess.binormals[i], temp);
		qglVertex3fv(temp);

		qglColor3f(0, 0, 1);
		qglVertex3fv(tess.xyz[i]);
		VectorMA(tess.xyz[i], 2, tess.normals[i], temp);
		qglVertex3fv(temp);
	}
	qglEnd();

	qglDepthRange(0, 1);
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals()
{
	int             i;
	vec3_t          temp;

	GL_Program(0);
	GL_SelectTexture(0);
	GL_Bind(tr.whiteImage);
	qglColor3f(1, 1, 1);
	qglDepthRange(0, 0);		// never occluded
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);

	qglBegin(GL_LINES);
	for(i = 0; i < tess.numVertexes; i++)
	{
		qglVertex3fv(tess.xyz[i]);
		VectorMA(tess.xyz[i], 2, tess.normals[i], temp);
		qglVertex3fv(temp);
	}
	qglEnd();

	qglDepthRange(0, 1);
}

/*
==============
Tess_Begin

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void Tess_Begin(shader_t * surfaceShader, shader_t * lightShader,
					 int lightmapNum,
					 int fogNum,
					 qboolean skipTangentSpaces,
					 qboolean shadowVolume)
{
	shader_t       *state = (surfaceShader->remappedShader) ? surfaceShader->remappedShader : surfaceShader;
	
	tess.indexesVBO = 0;
	tess.vertexesVBO = 0;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.surfaceShader = state;
	
	tess.surfaceStages = state->stages;
	tess.numSurfaceStages = state->numStages;
	
	tess.lightShader = lightShader;
	
	switch (tess.currentStageIteratorType)
	{
		case SIT_LIGHTING:
		case SIT_LIGHTING_STENCIL:
			tess.currentStageIteratorFunc = Tess_StageIteratorLighting;
			break;
		
		default:
		case SIT_DEFAULT:
			tess.currentStageIteratorFunc = state->isSky ? Tess_StageIteratorSky : Tess_StageIteratorGeneric;
			break;
	}

	tess.shaderTime = backEnd.refdef.floatTime - tess.surfaceShader->timeOffset;
	if(tess.surfaceShader->clampTime && tess.shaderTime >= tess.surfaceShader->clampTime)
	{
		tess.shaderTime = tess.surfaceShader->clampTime;
	}
	
	tess.lightmapNum = lightmapNum;
	
	tess.fogNum = fogNum;
	
	tess.skipTangentSpaces = skipTangentSpaces;
	tess.shadowVolume = shadowVolume;
}

static void ComputeColors(shaderStage_t * pStage);
static void ComputeColor(shaderStage_t * pStage);

static void Render_generic_single_FFP(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_generic_single_FFP ---\n");

	pStage = tess.surfaceStages[stage];
	
	GL_Program(0);
	GL_State(pStage->stateBits);
	
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		ComputeColor(pStage);
		qglColor4fv(tess.svars.color);
		GL_ClientState(GLCS_VERTEX);
	}
	else
	{
		ComputeColors(pStage);
		GL_ClientState(GLCS_VERTEX | GLCS_COLOR);
	}
	
	GL_SetVertexAttribs();
	
	GL_SelectTexture(0);
//	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_COLORMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		qglTexCoordPointer(4, GL_FLOAT, 0, BUFFER_OFFSET(tess.ofsTexCoords));
	}
	else
	{
		qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[TB_COLORMAP]);
	}

	R_BindAnimatedImage(&pStage->bundle[0]);

	R_DrawElements();
	
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
//	qglDisable(GL_TEXTURE_2D);
}

#if 1
#define Render_generic_single Render_generic_single_FFP
#else
static void Render_generic_single(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_generic_single ---\n");

	pStage = tess.surfaceStages[stage];
	
	ComputeColors(pStage);
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	
	// FIXME: VBO surfaces have only tess.svars.color
	GL_Program(tr.genericShader_single.program);
	GL_ClientState(tr.genericShader_single.attribs);
	GL_SetVertexAttribs();

	// bind colorMap
	GL_SelectTexture(0);
	R_BindAnimatedImage(&pStage->bundle[TB_COLORMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_COLORMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
}
#endif

static void Render_depthFill_FFP(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_depthFill_FFP ---\n");

	pStage = tess.surfaceStages[stage];
	
#if 1
	qglColor3f(0, 0, 0);
#else
	qglColor3f(1, 1, 1);
#endif
	
	GL_Program(0);
	GL_State(pStage->stateBits);
	GL_ClientState(GLCS_VERTEX);
	GL_SetVertexAttribs();
	
	GL_SelectTexture(0);
	
	if(pStage->stateBits & GLS_ATEST_BITS)
	{
		//qglEnable(GL_TEXTURE_2D);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		qglMatrixMode(GL_TEXTURE);
		qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
		qglMatrixMode(GL_MODELVIEW);

		if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
		{
			qglTexCoordPointer(4, GL_FLOAT, 0, BUFFER_OFFSET(tess.ofsTexCoords));
		}
		else
		{
			qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[TB_DIFFUSEMAP]);
		}
		
		GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	}
	else
	{
		GL_Bind(tr.whiteImage);
	}

	R_DrawElements();
	
	if(pStage->stateBits & GLS_ATEST_BITS)
	{
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
		//qglDisable(GL_TEXTURE_2D);
	}
}

#if 1
#define Render_depthFill Render_depthFill_FFP
#else
static void Render_depthFill(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_depthFill ---\n");

	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.depthFillShader.program);
	GL_ClientState(tr.depthFillShader.attribs);
	GL_SetVertexAttribs();

	// bind colorMap
	GL_SelectTexture(0);
	
	if(pStage->stateBits & GLS_ATEST_BITS)
	{
		qglMatrixMode(GL_TEXTURE);
		qglLoadMatrixf(tess.svars.texMatrices[TB_COLORMAP]);
		qglMatrixMode(GL_MODELVIEW);
		
		GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	}
	else
	{
		GL_Bind(tr.whiteImage);
	}

	R_DrawElements();
}
#endif

/*
===================
DrawMultiTextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void Render_generic_multi_FFP(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_generic_multi_FFP ---\n");

	pStage = tess.surfaceStages[stage];
	
	ComputeColors(pStage);
	
	GL_Program(0);
	GL_State(pStage->stateBits);
	GL_ClientState(GLCS_VERTEX | GLCS_COLOR);
	GL_SetVertexAttribs();

	// base
	GL_SelectTexture(0);
//	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[TB_DIFFUSEMAP]);
	R_BindAnimatedImage(&pStage->bundle[0]);

	// lightmap/secondary pass
	GL_SelectTexture(1);
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[1]);

	if(r_showLightMaps->integer)
	{
		GL_TexEnv(GL_REPLACE);
	}
	else
	{
		GL_TexEnv(tess.surfaceShader->collapseTextureEnv);
	}

	R_BindAnimatedImage(&pStage->bundle[1]);

	R_DrawElements();

	// clean up
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);
	
	GL_SelectTexture(0);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
//	qglDisable(GL_TEXTURE_2D);

	GL_CheckErrors();
}

static void Render_lighting_D_direct(int stage)
{
	shaderStage_t  *pStage;

	vec4_t          ambientLight;
	vec3_t          lightDir;
	vec4_t          directedLight;
	trRefEntity_t  *ent = backEnd.currentEntity;
	
	GLimp_LogComment("--- Render_lighting_D_direct ---\n");
	
	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_D_direct.program);
	GL_ClientState(tr.lightShader_D_direct.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorScale(ent->ambientLight, (1.0 / 255.0) * 0.3, ambientLight);
	ClampColor(ambientLight);
	VectorScale(ent->directedLight, 1.0 / 255.0, directedLight);
	ClampColor(directedLight);
	VectorCopy(ent->lightDir, lightDir);
	
	qglUniform3fARB(tr.lightShader_D_direct.u_AmbientColor, ambientLight[0], ambientLight[1], ambientLight[2]);
	qglUniform3fARB(tr.lightShader_D_direct.u_LightDir, lightDir[0], lightDir[1], lightDir[2]);
	qglUniform3fARB(tr.lightShader_D_direct.u_LightColor, directedLight[0], directedLight[1], directedLight[2]);

	// bind diffusemap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}


static void Render_lighting_DB_direct(int stage)
{
	shaderStage_t  *pStage;

	vec4_t          ambientLight;
	vec3_t          lightDir;
	vec4_t          directedLight;
	trRefEntity_t  *ent = backEnd.currentEntity;
	
	GLimp_LogComment("--- Render_lighting_DB_direct ---\n");
	
	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_DB_direct.program);
	GL_ClientState(tr.lightShader_DB_direct.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorScale(ent->ambientLight, (1.0 / 255.0) * 0.3, ambientLight);
	ClampColor(ambientLight);
	VectorScale(ent->directedLight, 1.0 / 255.0, directedLight);
	ClampColor(directedLight);
	VectorCopy(ent->lightDir, lightDir);
	
	qglUniform3fARB(tr.lightShader_DB_direct.u_AmbientColor, ambientLight[0], ambientLight[1], ambientLight[2]);
	qglUniform3fARB(tr.lightShader_DB_direct.u_LightDir, lightDir[0], lightDir[1], lightDir[2]);
	qglUniform3fARB(tr.lightShader_DB_direct.u_LightColor, directedLight[0], directedLight[1], directedLight[2]);


	// bind diffusemap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind normalmap
	GL_SelectTexture(1);
	GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_NORMALMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_lighting_DBS_direct(int stage)
{
	shaderStage_t  *pStage;

	vec3_t			viewOrigin;
	vec4_t          ambientLight;
	vec3_t          lightDir;
	vec4_t          directedLight;
	float           specularExponent;
	trRefEntity_t  *ent = backEnd.currentEntity;
	
	GLimp_LogComment("--- Render_lighting_DBS_direct ---\n");
	
	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_Program(tr.lightShader_DBS_direct.program);
	GL_ClientState(tr.lightShader_DBS_direct.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorCopy(backEnd.or.viewOrigin, viewOrigin);
	VectorScale(ent->ambientLight, (1.0 / 255.0) * 0.3, ambientLight);
	ClampColor(ambientLight);
	VectorScale(ent->directedLight, 1.0 / 255.0, directedLight);
	ClampColor(directedLight);
	VectorCopy(ent->lightDir, lightDir);
	specularExponent = RB_EvalExpression(&pStage->specularExponentExp, r_specularExponent->value);
	
	qglUniform3fARB(tr.lightShader_DBS_direct.u_AmbientColor, ambientLight[0], ambientLight[1], ambientLight[2]);
	qglUniform3fARB(tr.lightShader_DBS_direct.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);
	qglUniform3fARB(tr.lightShader_DBS_direct.u_LightDir, lightDir[0], lightDir[1], lightDir[2]);
	qglUniform3fARB(tr.lightShader_DBS_direct.u_LightColor, directedLight[0], directedLight[1], directedLight[2]);
	qglUniform1fARB(tr.lightShader_DBS_direct.u_SpecularExponent, specularExponent);


	// bind diffusemap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind normalmap
	GL_SelectTexture(1);
	GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_NORMALMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind specularmap
	GL_SelectTexture(2);
	GL_Bind(pStage->bundle[TB_SPECULARMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_SPECULARMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_lighting_D_omni(	shaderStage_t * diffuseStage,
									shaderStage_t * attenuationXYStage,
									shaderStage_t * attenuationZStage,
									trRefDlight_t * dlight)
{
	vec3_t          lightOrigin;
	vec4_t          lightColor;	
	
	GLimp_LogComment("--- Render_lighting_D_omni ---\n");
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_D_omni.program);
	GL_ClientState(tr.lightShader_D_omni.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorCopy(dlight->transformed, lightOrigin);
	VectorCopy(tess.svars.color, lightColor);
	
	qglUniform3fARB(tr.lightShader_D_omni.u_LightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2]);
	qglUniform3fARB(tr.lightShader_D_omni.u_LightColor, lightColor[0], lightColor[1], lightColor[2]);
	qglUniform1fARB(tr.lightShader_D_omni.u_LightScale, r_lightScale->value);

	GL_SelectTexture(0);
	GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(1);
	R_BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(dlight->attenuationMatrix2);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(2);
	R_BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	// update performance counters
	backEnd.pc.c_dlightVertexes += tess.numVertexes;
	backEnd.pc.c_dlightIndexes += tess.numIndexes;
	
	GL_CheckErrors();
}

static void Render_lighting_DB_omni(	shaderStage_t * diffuseStage,
										shaderStage_t * attenuationXYStage,
										shaderStage_t * attenuationZStage,
										trRefDlight_t * dlight)
{
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	
	GLimp_LogComment("--- Render_lighting_DB_omni ---\n");
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_DB_omni.program);
	GL_ClientState(tr.lightShader_DB_omni.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorCopy(dlight->transformed, lightOrigin);
	VectorCopy(tess.svars.color, lightColor);
	
	qglUniform3fARB(tr.lightShader_DB_omni.u_LightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2]);
	qglUniform3fARB(tr.lightShader_DB_omni.u_LightColor, lightColor[0], lightColor[1], lightColor[2]);
	qglUniform1fARB(tr.lightShader_DB_omni.u_LightScale, r_lightScale->value);

	GL_SelectTexture(0);
	GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(1);
	GL_Bind(diffuseStage->bundle[TB_NORMALMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_NORMALMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(2);
	R_BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(dlight->attenuationMatrix2);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(3);
	R_BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);
	
	R_DrawElements();
	
	// update performance counters
	backEnd.pc.c_dlightVertexes += tess.numVertexes;
	backEnd.pc.c_dlightIndexes += tess.numIndexes;
	
	GL_CheckErrors();
}

static void Render_lighting_DBS_omni(	shaderStage_t * diffuseStage,
										shaderStage_t * attenuationXYStage,
										shaderStage_t * attenuationZStage,
										trRefDlight_t * dlight)
{
	vec3_t			viewOrigin;
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	float           specularExponent;
	
	GLimp_LogComment("--- Render_lighting_DBS_omni ---\n");
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_DBS_omni.program);
	GL_ClientState(tr.lightShader_DBS_omni.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorCopy(backEnd.or.viewOrigin, viewOrigin);
	VectorCopy(dlight->transformed, lightOrigin);
	VectorCopy(tess.svars.color, lightColor);
	specularExponent = RB_EvalExpression(&diffuseStage->specularExponentExp, r_specularExponent->value);
	
	qglUniform3fARB(tr.lightShader_DBS_omni.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);
	qglUniform3fARB(tr.lightShader_DBS_omni.u_LightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2]);
	qglUniform3fARB(tr.lightShader_DBS_omni.u_LightColor, lightColor[0], lightColor[1], lightColor[2]);
	qglUniform1fARB(tr.lightShader_DBS_omni.u_LightScale, r_lightScale->value);
	qglUniform1fARB(tr.lightShader_DBS_omni.u_SpecularExponent, specularExponent);

	GL_SelectTexture(0);
	GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(1);
	GL_Bind(diffuseStage->bundle[TB_NORMALMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_NORMALMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(2);
	GL_Bind(diffuseStage->bundle[TB_SPECULARMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_SPECULARMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(3);
	R_BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(dlight->attenuationMatrix2);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(4);
	R_BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);
	
	R_DrawElements();
	
	// update performance counters
	backEnd.pc.c_dlightVertexes += tess.numVertexes;
	backEnd.pc.c_dlightIndexes += tess.numIndexes;
	
	GL_CheckErrors();
}

static void Render_lighting_D_proj(	shaderStage_t * diffuseStage,
									shaderStage_t * attenuationXYStage,
									shaderStage_t * attenuationZStage,
									trRefDlight_t * dlight)
{
	vec3_t          lightOrigin;
	vec4_t          lightColor;	
	
	GLimp_LogComment("--- Render_lighting_D_proj ---\n");
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_D_proj.program);
	GL_ClientState(tr.lightShader_D_proj.attribs);
	GL_SetVertexAttribs();

	// set uniforms
	VectorCopy(dlight->transformed, lightOrigin);
	VectorCopy(tess.svars.color, lightColor);
	
	qglUniform3fARB(tr.lightShader_D_proj.u_LightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2]);
	qglUniform3fARB(tr.lightShader_D_proj.u_LightColor, lightColor[0], lightColor[1], lightColor[2]);
	qglUniform1fARB(tr.lightShader_D_proj.u_LightScale, r_lightScale->value);

	GL_SelectTexture(0);
	GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(1);
	R_BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(dlight->attenuationMatrix2);
	qglMatrixMode(GL_MODELVIEW);
	
	GL_SelectTexture(2);
	R_BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);
	
	R_DrawElements();
	
	// update performance counters
	backEnd.pc.c_dlightVertexes += tess.numVertexes;
	backEnd.pc.c_dlightIndexes += tess.numIndexes;
	
	GL_CheckErrors();
}

static void Render_lightmap_FFP(int stage, int texCoordsIndex)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_lightmap_FFP ---\n");

	pStage = tess.surfaceStages[stage];
	
	GL_Program(0);
	
	qglColor4fv(colorWhite);
	
	if(r_showLightMaps->integer)
	{
		GL_State(pStage->stateBits & ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS));
	}
	else
	{
		GL_State(pStage->stateBits);
	}
	
	GL_ClientState(GLCS_VERTEX);
	GL_SetVertexAttribs();

	GL_SelectTexture(0);
//	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[texCoordsIndex]);
	qglMatrixMode(GL_MODELVIEW);
	
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		if(texCoordsIndex == TB_LIGHTMAP)
			qglTexCoordPointer(4, GL_FLOAT, 0, BUFFER_OFFSET(tess.ofsTexCoords2));
		else
			qglTexCoordPointer(4, GL_FLOAT, 0, BUFFER_OFFSET(tess.ofsTexCoords));
	}
	else
	{
		qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[texCoordsIndex]);
	}
	
	R_BindLightMap();

	R_DrawElements();
	
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
//	qglDisable(GL_TEXTURE_2D);

	GL_CheckErrors();
}

static void Render_deluxemap_FFP(int stage, int texCoordsIndex)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_deluxemap_FFP ---\n");

	pStage = tess.surfaceStages[stage];
	
	if(!pStage->bundle[TB_LIGHTMAP].image[1])
		return;
	
	GL_Program(0);
	
	qglColor4fv(colorWhite);
	
	if(r_showLightMaps->integer)
	{
		GL_State(pStage->stateBits & ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS));
	}
	else
	{
		GL_State(pStage->stateBits);
	}
	
	GL_ClientState(GLCS_VERTEX);
	GL_SetVertexAttribs();

	GL_SelectTexture(0);
//	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[texCoordsIndex]);
	qglMatrixMode(GL_MODELVIEW);
	
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		if(texCoordsIndex == TB_LIGHTMAP)
			qglTexCoordPointer(4, GL_FLOAT, 0, BUFFER_OFFSET(tess.ofsTexCoords2));
		else
			qglTexCoordPointer(4, GL_FLOAT, 0, BUFFER_OFFSET(tess.ofsTexCoords));
	}
	else
	{
		qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[texCoordsIndex]);
	}
	
	R_BindDeluxeMap();

	R_DrawElements();
	
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
//	qglDisable(GL_TEXTURE_2D);

	GL_CheckErrors();
}

static void Render_lighting_D_radiosity_FFP(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_lighting_D_radiosity_FFP ---\n");

	pStage = tess.surfaceStages[stage];
	
	ComputeColors(pStage);
	
	GL_Program(0);
	GL_SelectTexture(0);
	GL_State(pStage->stateBits);
	GL_ClientState(GLCS_VERTEX | GLCS_COLOR);
	GL_SetVertexAttribs();

	// base
	GL_SelectTexture(0);
//	qglEnable(GL_TEXTURE_2D);
	R_BindAnimatedImage(&pStage->bundle[TB_DIFFUSEMAP]);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);

	// lightmap/secondary pass
	GL_SelectTexture(1);
	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[TB_LIGHTMAP]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_LIGHTMAP]);
	qglMatrixMode(GL_MODELVIEW);
	R_BindLightMap();

	R_DrawElements();

	// clean up
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	qglDisable(GL_TEXTURE_2D);
	
	GL_SelectTexture(0);
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
//	qglDisable(GL_TEXTURE_2D);

	GL_CheckErrors();
}

static void Render_lighting_D_radiosity(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_lighting_D_radiosity ---\n");
	
	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_D_radiosity.program);
	GL_ClientState(tr.lightShader_D_radiosity.attribs);
	GL_SetVertexAttribs();

	// bind diffusemap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind lightmap
	GL_SelectTexture(1);
	R_BindLightMap();
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_LIGHTMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_lighting_DB_radiosity(int stage)
{
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_lighting_DB_radiosity ---\n");
	
	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_DB_radiosity.program);
	GL_ClientState(tr.lightShader_DB_radiosity.attribs);
	GL_SetVertexAttribs();

	// bind diffusemap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind normalmap
	GL_SelectTexture(1);
	GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_NORMALMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind lightmap
	GL_SelectTexture(2);
	R_BindLightMap();
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_LIGHTMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind deluxemap
	GL_SelectTexture(3);
	R_BindDeluxeMap();
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_lighting_DBS_radiosity(int stage)
{
	vec3_t			viewOrigin;
	float           specularExponent;
	shaderStage_t  *pStage;
	
	GLimp_LogComment("--- Render_lighting_DBS_radiosity ---\n");
	
	pStage = tess.surfaceStages[stage];
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.lightShader_DBS_radiosity.program);
	GL_ClientState(tr.lightShader_DBS_radiosity.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	VectorCopy(backEnd.or.viewOrigin, viewOrigin);
	specularExponent = RB_EvalExpression(&pStage->specularExponentExp, r_specularExponent->value);
	
	qglUniform3fARB(tr.lightShader_DBS_radiosity.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);
	qglUniform1fARB(tr.lightShader_DBS_radiosity.u_SpecularExponent, specularExponent);

	// bind diffusemap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_DIFFUSEMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind normalmap
	GL_SelectTexture(1);
	GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_NORMALMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind specular
	GL_SelectTexture(2);
	GL_Bind(pStage->bundle[TB_SPECULARMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_SPECULARMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind lightmap
	GL_SelectTexture(3);
	R_BindLightMap();
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_LIGHTMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	// bind deluxemap
	GL_SelectTexture(4);
	R_BindDeluxeMap();
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_reflection_C(int stage)
{
	vec3_t			viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	
	GLimp_LogComment("--- Render_reflection_C ---\n");
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.reflectionShader_C.program);
	GL_ClientState(tr.reflectionShader_C.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	VectorCopy(backEnd.viewParms.or.origin, viewOrigin);	// in world space
	qglUniform3fARB(tr.reflectionShader_C.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);

	// bind colormap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(backEnd.or.transformMatrix);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_refraction_C(int stage)
{
	vec3_t			viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	
	GLimp_LogComment("--- Render_refraction_C ---\n");
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.refractionShader_C.program);
	GL_ClientState(tr.refractionShader_C.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	VectorCopy(backEnd.viewParms.or.origin, viewOrigin);	// in world space
	qglUniform3fARB(tr.refractionShader_C.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);
	qglUniform1fARB(tr.refractionShader_C.u_RefractionIndex, RB_EvalExpression(&pStage->refractionIndexExp, 1.0));
	qglUniform1fARB(tr.refractionShader_C.u_FresnelPower, RB_EvalExpression(&pStage->fresnelPowerExp, 2.0));
	qglUniform1fARB(tr.refractionShader_C.u_FresnelScale, RB_EvalExpression(&pStage->fresnelScaleExp, 2.0));
	qglUniform1fARB(tr.refractionShader_C.u_FresnelBias, RB_EvalExpression(&pStage->fresnelBiasExp, 1.0));

	// bind colormap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(backEnd.or.transformMatrix);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_dispersion_C(int stage)
{
	vec3_t			viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	float			eta;
	float			etaDelta;
	
	GLimp_LogComment("--- Render_dispersion_C ---\n");
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.dispersionShader_C.program);
	GL_ClientState(tr.dispersionShader_C.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	VectorCopy(backEnd.viewParms.or.origin, viewOrigin);	// in world space
	eta	= RB_EvalExpression(&pStage->etaExp, (float)1.1);
	etaDelta = RB_EvalExpression(&pStage->etaDeltaExp, (float)-0.02);
	
	qglUniform3fARB(tr.dispersionShader_C.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);
	qglUniform3fARB(tr.dispersionShader_C.u_EtaRatio, eta, eta + etaDelta, eta + (etaDelta * 2));
	qglUniform1fARB(tr.dispersionShader_C.u_FresnelPower, RB_EvalExpression(&pStage->fresnelPowerExp, 2.0f));
	qglUniform1fARB(tr.dispersionShader_C.u_FresnelScale, RB_EvalExpression(&pStage->fresnelScaleExp, 2.0f));
	qglUniform1fARB(tr.dispersionShader_C.u_FresnelBias, RB_EvalExpression(&pStage->fresnelBiasExp, 1.0f));

	// bind colormap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(backEnd.or.transformMatrix);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_skybox(int stage)
{
	vec3_t			viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	
	GLimp_LogComment("--- Render_skybox ---\n");
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.skyBoxShader.program);
	GL_ClientState(tr.skyBoxShader.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	VectorCopy(backEnd.viewParms.or.origin, viewOrigin);	// in world space
	qglUniform3fARB(tr.skyBoxShader.u_ViewOrigin, viewOrigin[0], viewOrigin[1], viewOrigin[2]);

	// bind colormap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(backEnd.or.transformMatrix);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_heatHaze(int stage)
{
	float           deformMagnitude;
	float			fbufWidthScale, fbufHeightScale;
	float			npotWidthScale, npotHeightScale;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	
	GLimp_LogComment("--- Render_heatHaze ---\n");
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.heatHazeShader.program);
	GL_ClientState(tr.heatHazeShader.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	deformMagnitude = RB_EvalExpression(&pStage->deformMagnitudeExp, 1.0);
	fbufWidthScale = Q_recip((float)glConfig.vidWidth);
	fbufHeightScale = Q_recip((float)glConfig.vidHeight);
	npotWidthScale = (float)glConfig.vidWidth / (float)tr.currentRenderLinearImage->uploadWidth;
	npotHeightScale = (float)glConfig.vidHeight / (float)tr.currentRenderLinearImage->uploadHeight;
	
	qglUniform1fARB(tr.heatHazeShader.u_DeformMagnitude, deformMagnitude);
	qglUniform2fARB(tr.heatHazeShader.u_FBufScale, fbufWidthScale, fbufHeightScale);
	qglUniform2fARB(tr.heatHazeShader.u_NPotScale, npotWidthScale, npotHeightScale);
	
	if(glConfig.hardwareType == GLHW_ATI)
	{
		matrix_t        projectionMatrixTranspose;
		
		MatrixTranspose(backEnd.viewParms.projectionMatrix, projectionMatrixTranspose);
		qglUniformMatrix4fvARB(tr.heatHazeShader.u_ProjectionMatrixTranspose, 1, GL_FALSE, projectionMatrixTranspose);
	}

	// bind colormap
	GL_SelectTexture(0);
	GL_Bind(tr.currentRenderLinearImage);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderLinearImage->uploadWidth, tr.currentRenderLinearImage->uploadHeight);
	
	// bind normalmap
	GL_SelectTexture(1);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_COLORMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_glow(int stage)
{
	float           blurMagnitude;
	float			fbufWidthScale, fbufHeightScale;
	float			npotWidthScale, npotHeightScale;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	
	GLimp_LogComment("--- Render_glow ---\n");
	
	GL_State(pStage->stateBits);
	
	// enable shader, set arrays
	GL_Program(tr.glowShader.program);
	GL_ClientState(tr.glowShader.attribs);
	GL_SetVertexAttribs();
	
	// set uniforms
	blurMagnitude = RB_EvalExpression(&pStage->blurMagnitudeExp, 3.0);
	fbufWidthScale = Q_recip((float)glConfig.vidWidth);
	fbufHeightScale = Q_recip((float)glConfig.vidHeight);
	npotWidthScale = (float)glConfig.vidWidth / (float)tr.currentRenderImage->uploadWidth;
	npotHeightScale = (float)glConfig.vidHeight / (float)tr.currentRenderImage->uploadHeight;
	
	qglUniform1fARB(tr.glowShader.u_BlurMagnitude, blurMagnitude);
	qglUniform2fARB(tr.glowShader.u_FBufScale, fbufWidthScale, fbufHeightScale);
	qglUniform2fARB(tr.glowShader.u_NPotScale, npotWidthScale, npotHeightScale);

	// bind colormap
	GL_SelectTexture(0);
//	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	GL_Bind(tr.currentRenderImage);
	qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);
	qglMatrixMode(GL_TEXTURE);
	qglLoadMatrixf(tess.svars.texMatrices[TB_COLORMAP]);
	qglMatrixMode(GL_MODELVIEW);
	
	R_DrawElements();
	
	GL_CheckErrors();
}

static void Render_fog()
{
	fog_t          *fog;
	int             i;
	
	GLimp_LogComment("--- Render_fog ---\n");

	fog = tr.world->fogs + tess.fogNum;

	for(i = 0; i < tess.numVertexes; i++)
	{
		*(int *)&tess.svars.colors[i] = fog->colorInt;
	}

	RB_CalcFogTexCoords((float *)tess.svars.texCoords[0]);

	GL_Program(0);
	GL_ClientState(GLCS_VERTEX | GLCS_COLOR);
	GL_SetVertexAttribs();

	GL_SelectTexture(0);
//	qglEnable(GL_TEXTURE_2D);
	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer(2, GL_FLOAT, 0, tess.svars.texCoords[TB_COLORMAP]);
	GL_Bind(tr.fogImage);

	if(tess.surfaceShader->fogPass == FP_EQUAL)
	{
		GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL);
	}
	else
	{
		GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
	}

	R_DrawElements();

	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
//	qglDisable(GL_TEXTURE_2D);

	GL_CheckErrors();
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors(shaderStage_t * pStage)
{
	int             i;
	
	GLimp_LogComment("--- ComputeColors ---\n");

	// rgbGen
	switch (pStage->rgbGen)
	{
		case CGEN_IDENTITY:
			Com_Memset(tess.svars.colors, 0xff, tess.numVertexes * 4);
			break;
			
		default:
		case CGEN_IDENTITY_LIGHTING:
			Com_Memset(tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4);
			break;
			
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor((unsigned char *)tess.svars.colors);
			break;
			
		case CGEN_EXACT_VERTEX:
			Com_Memcpy(tess.svars.colors, tess.colors, tess.numVertexes * sizeof(tess.colors[0]));
			break;
			
		case CGEN_CONST:
			for(i = 0; i < tess.numVertexes; i++)
			{
				*(int *)tess.svars.colors[i] = *(int *)pStage->constantColor;
			}
			break;
			
		case CGEN_VERTEX:
			if(tr.identityLight == 1)
			{
				Com_Memcpy(tess.svars.colors, tess.colors, tess.numVertexes * sizeof(tess.colors[0]));
			}
			else
			{
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][0] = tess.colors[i][0] * tr.identityLight;
					tess.svars.colors[i][1] = tess.colors[i][1] * tr.identityLight;
					tess.svars.colors[i][2] = tess.colors[i][2] * tr.identityLight;
					tess.svars.colors[i][3] = tess.colors[i][3];
				}
			}
			break;
			
		case CGEN_ONE_MINUS_VERTEX:
			if(tr.identityLight == 1)
			{
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][0] = 255 - tess.colors[i][0];
					tess.svars.colors[i][1] = 255 - tess.colors[i][1];
					tess.svars.colors[i][2] = 255 - tess.colors[i][2];
				}
			}
			else
			{
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][0] = (255 - tess.colors[i][0]) * tr.identityLight;
					tess.svars.colors[i][1] = (255 - tess.colors[i][1]) * tr.identityLight;
					tess.svars.colors[i][2] = (255 - tess.colors[i][2]) * tr.identityLight;
				}
			}
			break;
			
		case CGEN_FOG:
		{
			fog_t          *fog;

			fog = tr.world->fogs + tess.fogNum;

			for(i = 0; i < tess.numVertexes; i++)
			{
				*(int *)&tess.svars.colors[i] = fog->colorInt;
			}
			break;
		}
			
		case CGEN_WAVEFORM:
			RB_CalcWaveColor(&pStage->rgbWave, (unsigned char *)tess.svars.colors);
			break;
			
		case CGEN_ENTITY:
			RB_CalcColorFromEntity((unsigned char *)tess.svars.colors);
			break;
			
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity((unsigned char *)tess.svars.colors);
			break;
			
		case CGEN_CUSTOM_RGB:
			RB_CalcCustomColor(&pStage->rgbExp, (unsigned char *)tess.svars.colors);
			break;
			
		case CGEN_CUSTOM_RGBs:
			RB_CalcCustomColors(&pStage->redExp, &pStage->greenExp, &pStage->blueExp, (unsigned char *)tess.svars.colors);
			break;
	}

	// alphaGen
	switch (pStage->alphaGen)
	{
		case AGEN_SKIP:
			break;
			
		case AGEN_IDENTITY:
			if(pStage->rgbGen != CGEN_IDENTITY)
			{
				if((pStage->rgbGen == CGEN_VERTEX && tr.identityLight != 1) || pStage->rgbGen != CGEN_VERTEX)
				{
					for(i = 0; i < tess.numVertexes; i++)
					{
						tess.svars.colors[i][3] = 0xff;
					}
				}
			}
			break;
			
		case AGEN_CONST:
			if(pStage->rgbGen != CGEN_CONST)
			{
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][3] = pStage->constantColor[3];
				}
			}
			break;
			
		case AGEN_WAVEFORM:
			RB_CalcWaveAlpha(&pStage->alphaWave, (unsigned char *)tess.svars.colors);
			break;
			
		case AGEN_LIGHTING_SPECULAR:
			RB_CalcSpecularAlpha((unsigned char *)tess.svars.colors);
			break;
			
		case AGEN_ENTITY:
			RB_CalcAlphaFromEntity((unsigned char *)tess.svars.colors);
			break;
			
		case AGEN_ONE_MINUS_ENTITY:
			RB_CalcAlphaFromOneMinusEntity((unsigned char *)tess.svars.colors);
			break;
			
		case AGEN_VERTEX:
			if(pStage->rgbGen != CGEN_VERTEX)
			{
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][3] = tess.colors[i][3];
				}
			}
			break;
			
		case AGEN_ONE_MINUS_VERTEX:
			for(i = 0; i < tess.numVertexes; i++)
			{
				tess.svars.colors[i][3] = 255 - tess.colors[i][3];
			}
			break;
			
		case AGEN_PORTAL:
		{
			unsigned char   alpha;

			for(i = 0; i < tess.numVertexes; i++)
			{
				float           len;
				vec3_t          v;

				VectorSubtract(tess.xyz[i], backEnd.viewParms.or.origin, v);
				len = VectorLength(v);

				len /= tess.surfaceShader->portalRange;

				if(len < 0)
				{
					alpha = 0;
				}
				else if(len > 1)
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
			break;
		}
		
		case AGEN_CUSTOM:
			RB_CalcCustomAlpha(&pStage->alphaExp, (unsigned char *)tess.svars.colors);
			break;
	}

	// fog adjustment for colors to fade out as fog increases
	if(tess.fogNum)
	{
		switch (pStage->adjustColorsForFog)
		{
			case ACFF_MODULATE_RGB:
				RB_CalcModulateColorsByFog((unsigned char *)tess.svars.colors);
				break;
				
			case ACFF_MODULATE_ALPHA:
				RB_CalcModulateAlphasByFog((unsigned char *)tess.svars.colors);
				break;
				
			case ACFF_MODULATE_RGBA:
				RB_CalcModulateRGBAsByFog((unsigned char *)tess.svars.colors);
				break;
				
			case ACFF_NONE:
				break;
		}
	}
}


/*
===============
ComputeColor
===============
*/
static void ComputeColor(shaderStage_t * pStage)
{
	float           rgb;
	float           red;
	float			green;
	float			blue;
	float           alpha;
	
	GLimp_LogComment("--- ComputeColor ---\n");

	// rgbGen
	switch (pStage->rgbGen)
	{
		case CGEN_IDENTITY:
		{
			tess.svars.color[0] = 1.0;
			tess.svars.color[1] = 1.0;
			tess.svars.color[2] = 1.0;
			tess.svars.color[3] = 1.0;
			break;
		}
			
		default:
		case CGEN_IDENTITY_LIGHTING:
		{
			tess.svars.color[0] = tr.identityLightByte * (1.0 / 255.0);
			tess.svars.color[1] = tr.identityLightByte * (1.0 / 255.0);
			tess.svars.color[2] = tr.identityLightByte * (1.0 / 255.0);
			tess.svars.color[3] = tr.identityLightByte * (1.0 / 255.0);
			break;
		}
			
		case CGEN_CONST:
		{
			tess.svars.color[0] = pStage->constantColor[0] * (1.0 / 255.0);
			tess.svars.color[1] = pStage->constantColor[1] * (1.0 / 255.0);
			tess.svars.color[2] = pStage->constantColor[2] * (1.0 / 255.0);
			tess.svars.color[3] = pStage->constantColor[3] * (1.0 / 255.0);
			break;
		}
			
		case CGEN_ENTITY:
		{
			if(backEnd.currentLight)
			{
				tess.svars.color[0] = Q_bound(0.0, backEnd.currentLight->l.color[0], 1.0);
				tess.svars.color[1] = Q_bound(0.0, backEnd.currentLight->l.color[1], 1.0);
				tess.svars.color[2] = Q_bound(0.0, backEnd.currentLight->l.color[2], 1.0);
				tess.svars.color[3] = 1.0;
			}
			else if(backEnd.currentEntity)
			{
				tess.svars.color[0] = Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[0] * (1.0 / 255.0), 1.0);
				tess.svars.color[1] = Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[1] * (1.0 / 255.0), 1.0);
				tess.svars.color[2] = Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[2] * (1.0 / 255.0), 1.0);
				tess.svars.color[3] = Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[3] * (1.0 / 255.0), 1.0);
			}
			else
			{
				tess.svars.color[0] = 1.0;
				tess.svars.color[1] = 1.0;
				tess.svars.color[2] = 1.0;
				tess.svars.color[3] = 1.0;
			}
			break;
		}
			
		case CGEN_ONE_MINUS_ENTITY:
		{
			if(backEnd.currentLight)
			{
				tess.svars.color[0] = 1.0 - Q_bound(0.0, backEnd.currentLight->l.color[0], 1.0);
				tess.svars.color[1] = 1.0 - Q_bound(0.0, backEnd.currentLight->l.color[1], 1.0);
				tess.svars.color[2] = 1.0 - Q_bound(0.0, backEnd.currentLight->l.color[2], 1.0);
				tess.svars.color[3] = 0.0;	// FIXME
			}
			else if(backEnd.currentEntity)
			{
				tess.svars.color[0] = 1.0 - Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[0] * (1.0 / 255.0), 1.0);
				tess.svars.color[1] = 1.0 - Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[1] * (1.0 / 255.0), 1.0);
				tess.svars.color[2] = 1.0 - Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[2] * (1.0 / 255.0), 1.0);
				tess.svars.color[3] = 1.0 - Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[3] * (1.0 / 255.0), 1.0);
			}
			else
			{
				tess.svars.color[0] = 0.0;
				tess.svars.color[1] = 0.0;
				tess.svars.color[2] = 0.0;
				tess.svars.color[3] = 0.0;
			}
			break;
		}
			
		case CGEN_CUSTOM_RGB:
		{
			rgb = Q_bound(0.0, RB_EvalExpression(&pStage->rgbExp, 1.0), 1.0);
			
			tess.svars.color[0] = rgb;
			tess.svars.color[1] = rgb;
			tess.svars.color[2] = rgb;
			break;
		}
			
		case CGEN_CUSTOM_RGBs:
		{
			if(backEnd.currentLight)
			{
				red = Q_bound(0.0, RB_EvalExpression(&pStage->redExp, backEnd.currentLight->l.color[0]), 1.0);
				green = Q_bound(0.0, RB_EvalExpression(&pStage->greenExp, backEnd.currentLight->l.color[1]), 1.0);
				blue = Q_bound(0.0, RB_EvalExpression(&pStage->blueExp, backEnd.currentLight->l.color[2]), 1.0);
			}
			else if(backEnd.currentEntity)
			{
				red = Q_bound(0.0, RB_EvalExpression(&pStage->redExp, backEnd.currentEntity->e.shaderRGBA[0] * (1.0 / 255.0)), 1.0);
				green = Q_bound(0.0, RB_EvalExpression(&pStage->greenExp, backEnd.currentEntity->e.shaderRGBA[1] * (1.0 / 255.0)), 1.0);
				blue = Q_bound(0.0, RB_EvalExpression(&pStage->blueExp, backEnd.currentEntity->e.shaderRGBA[2] * (1.0 / 255.0)), 1.0);
			}
			else
			{
				red = Q_bound(0.0, RB_EvalExpression(&pStage->redExp, 1.0), 1.0);
				green = Q_bound(0.0, RB_EvalExpression(&pStage->greenExp, 1.0), 1.0);
				blue = Q_bound(0.0, RB_EvalExpression(&pStage->blueExp, 1.0), 1.0);
			}
			
			tess.svars.color[0] = red;
			tess.svars.color[1] = green;
			tess.svars.color[2] = blue;
			break;
		}
	}

	// alphaGen
	switch (pStage->alphaGen)
	{
		case AGEN_SKIP:
			break;
			
		default:
		case AGEN_IDENTITY:
		{
			if(pStage->rgbGen != CGEN_IDENTITY)
			{
				if((pStage->rgbGen == CGEN_VERTEX && tr.identityLight != 1) || pStage->rgbGen != CGEN_VERTEX)
				{
					tess.svars.color[3] = 1.0;
				}
			}
			break;
		}
			
		case AGEN_CONST:
		{
			if(pStage->rgbGen != CGEN_CONST)
			{
				tess.svars.color[3] = pStage->constantColor[3] * (1.0 / 255.0);
			}
			break;
		}
			
		case AGEN_ENTITY:
		{
			if(backEnd.currentLight)
			{
				tess.svars.color[3] = 1.0;	// FIXME
			}
			else if(backEnd.currentEntity)
			{
				tess.svars.color[3] = Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[3] * (1.0 / 255.0), 1.0);
			}
			else
			{
				tess.svars.color[3] = 1.0;
			}
			break;
		}
			
		case AGEN_ONE_MINUS_ENTITY:
		{
			if(backEnd.currentLight)
			{
				tess.svars.color[3] = 0.0;	// FIXME
			}
			else if(backEnd.currentEntity)
			{
				tess.svars.color[3] = 1.0 - Q_bound(0.0, backEnd.currentEntity->e.shaderRGBA[3] * (1.0 / 255.0), 1.0);
			}
			else
			{
				tess.svars.color[3] = 0.0;
			}
			break;
		}
		
		case AGEN_CUSTOM:
		{
			alpha = Q_bound(0.0, RB_EvalExpression(&pStage->alphaExp, 1.0), 1.0);
			
			tess.svars.color[3] = alpha;
			break;
		}
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords(shaderStage_t * pStage)
{
	int             i;
	int             b;
	
	GLimp_LogComment("--- ComputeTexCoords ---\n");

	for(b = 0; b < MAX_TEXTURE_BUNDLES; b++)
	{
		int             tm;
		
		// reset texture matrix
		MatrixIdentity(tess.svars.texMatrices[b]);
		
		// generate the texture coordinates by default
		tess.svars.skipCoords[b] = qfalse;

		// generate the texture coordinates
		switch(pStage->bundle[b].tcGen)
		{
			case TCGEN_BAD:
			case TCGEN_SKIP:
				tess.svars.skipCoords[b] = qtrue;
				continue;
			
			case TCGEN_IDENTITY:
				Com_Memset(tess.svars.texCoords[b], 0, sizeof(float) * 2 * tess.numVertexes);
				break;
				
			case TCGEN_TEXTURE:
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.texCoords[b][i][0] = tess.texCoords[i][0][0];
					tess.svars.texCoords[b][i][1] = tess.texCoords[i][0][1];
				}
				break;
				
			case TCGEN_LIGHTMAP:
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.texCoords[b][i][0] = tess.texCoords[i][1][0];
					tess.svars.texCoords[b][i][1] = tess.texCoords[i][1][1];
				}
				break;
				
			case TCGEN_VECTOR:
				for(i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.texCoords[b][i][0] = DotProduct(tess.xyz[i], pStage->bundle[b].tcGenVectors[0]);
					tess.svars.texCoords[b][i][1] = DotProduct(tess.xyz[i], pStage->bundle[b].tcGenVectors[1]);
				}
				break;
				
			case TCGEN_FOG:
				RB_CalcFogTexCoords((float *)tess.svars.texCoords[b]);
				break;
				
			case TCGEN_ENVIRONMENT_MAPPED:
				RB_CalcEnvironmentTexCoords((float *)tess.svars.texCoords[b]);
				break;
		}

		// alter texture coordinates
		for(tm = 0; tm < pStage->bundle[b].numTexMods; tm++)
		{
			if(pStage->bundle[b].tcGen == TCGEN_SKIP)
				break;
			
			switch(pStage->bundle[b].texMods[tm].type)
			{
				case TMOD_NONE:
					tm = TR_MAX_TEXMODS;	// break out of for loop
					break;

				case TMOD_TURBULENT:
					RB_CalcTurbulentTexCoords(&pStage->bundle[b].texMods[tm].wave, (float *)tess.svars.texCoords[b]);
					break;

				case TMOD_ENTITY_TRANSLATE:
					RB_CalcScrollTexCoords(backEnd.currentEntity->e.shaderTexCoord, (float *)tess.svars.texCoords[b]);
					break;

				case TMOD_SCROLL:
					RB_CalcScrollTexCoords(pStage->bundle[b].texMods[tm].scroll, (float *)tess.svars.texCoords[b]);
					break;
					
				case TMOD_SCALE:
					RB_CalcScaleTexCoords(pStage->bundle[b].texMods[tm].scale, (float *)tess.svars.texCoords[b]);
					break;

				case TMOD_STRETCH:
					RB_CalcStretchTexCoords(&pStage->bundle[b].texMods[tm].wave, (float *)tess.svars.texCoords[b]);
					break;

				case TMOD_TRANSFORM:
					RB_CalcTransformTexCoords(&pStage->bundle[b].texMods[tm], (float *)tess.svars.texCoords[b]);
					break;

				case TMOD_ROTATE:
					RB_CalcRotateTexCoords(pStage->bundle[b].texMods[tm].rotateSpeed, (float *)tess.svars.texCoords[b]);
					break;
				
				case TMOD_SCROLL2:
					RB_CalcScrollTexCoords2( &pStage->bundle[b].texMods[tm].sExp,
											 &pStage->bundle[b].texMods[tm].tExp, (float *)tess.svars.texCoords[b]);
					break;
					
				case TMOD_SCALE2:
					RB_CalcScaleTexCoords2( &pStage->bundle[b].texMods[tm].sExp,
											&pStage->bundle[b].texMods[tm].tExp, (float *)tess.svars.texCoords[b]);
					break;
					
				case TMOD_CENTERSCALE:
					RB_CalcCenterScaleTexCoords(	&pStage->bundle[b].texMods[tm].sExp,
													&pStage->bundle[b].texMods[tm].tExp, (float *)tess.svars.texCoords[b]);
					break;
					
				case TMOD_SHEAR:
					RB_CalcShearTexCoords(	&pStage->bundle[b].texMods[tm].sExp,
											&pStage->bundle[b].texMods[tm].tExp, (float *)tess.svars.texCoords[b]);
					break;
					
				case TMOD_ROTATE2:
					RB_CalcRotateTexCoords2(&pStage->bundle[b].texMods[tm].rExp, (float *)tess.svars.texCoords[b]);
					break;
				
				default:
					ri.Error(ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n",
							 pStage->bundle[b].texMods[tm].type, tess.surfaceShader->name);
					break;
			}
		}
	}
}


/*
===============
ComputeTexMatrices
===============
*/
static void ComputeTexMatrices(shaderStage_t * pStage)
{
	int             i, j;
	vec_t          *matrix;
	float           x, y;
	
	GLimp_LogComment("--- ComputeTexMatrices ---\n");

	for(i = 0; i < MAX_TEXTURE_BUNDLES; i++)
	{
		matrix = tess.svars.texMatrices[i];
		
		MatrixIdentity(matrix);

		for(j = 0; j < pStage->bundle[i].numTexMods; j++)
		{
			switch(pStage->bundle[i].texMods[j].type)
			{
				case TMOD_NONE:
					j = TR_MAX_TEXMODS;	// break out of for loop
					break;
					
				case TMOD_SCROLL2:
				{
					x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
					y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);

					// clamp so coordinates don't continuously get larger, causing problems
					// with hardware limits
					x = x - floor(x);
					y = y - floor(y);
				
					MatrixMultiplyTranslation(matrix, x, y, 0.0);
					break;
				}
			
				case TMOD_SCALE2:
				{
					x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
					y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);
				
					MatrixMultiplyScale(matrix, x, y, 0.0);
					break;
				}
				
				case TMOD_CENTERSCALE:
				{
					x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
					y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);
				
					MatrixMultiplyTranslation(matrix, 0.5, 0.5, 0.0);
					MatrixMultiplyScale(matrix, x, y, 0.0);
					MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);
					break;
				}
				
				case TMOD_SHEAR:
				{
					x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
					y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);
				
					MatrixMultiplyTranslation(matrix, 0.5, 0.5, 0.0);
					MatrixMultiplyShear(matrix, x, y);
					MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);
					break;
				}
				
				case TMOD_ROTATE2:
				{
					x = RAD2DEG(RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].rExp, 0)) * 5.0;
	
					MatrixMultiplyTranslation(matrix, 0.5, 0.5, 0.0);
					MatrixMultiplyZRotation(matrix, x);
					MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);
					break;
				}
				
				default:
					break;
			}
		}
	}
}

/*
===============
ComputeFinalAttenuation
===============
*/
static void ComputeFinalAttenuation(shaderStage_t * pStage, trRefDlight_t * dlight)
{
	int             i;
	matrix_t		matrix;
	float           x, y;
	
	GLimp_LogComment("--- ComputeFinalAttenuation ---\n");
	
	MatrixIdentity(matrix);

	for(i = 0; i < pStage->bundle[TB_COLORMAP].numTexMods; i++)
	{
		switch(pStage->bundle[TB_COLORMAP].texMods[i].type)
		{
			case TMOD_NONE:
				i = TR_MAX_TEXMODS;	// break out of for loop
				break;

			
			case TMOD_SCROLL2:
			{
				x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
				y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);

				// clamp so coordinates don't continuously get larger, causing problems
				// with hardware limits
				x = x - floor(x);
				y = y - floor(y);
				
				MatrixMultiplyTranslation(matrix, x, y, 0.0);
				break;
			}
			
			case TMOD_SCALE2:
			{
				x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
				y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);
				
				MatrixMultiplyScale(matrix, x, y, 0.0);
				break;
			}
				
			case TMOD_CENTERSCALE:
			{
				x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
				y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);
				
				MatrixMultiplyTranslation(matrix, 0.5, 0.5, 0.0);
				MatrixMultiplyScale(matrix, x, y, 0.0);
				MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);
				break;
			}
				
			case TMOD_SHEAR:
			{
				x = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].sExp, 0);
				y = RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].tExp, 0);
				
				MatrixMultiplyTranslation(matrix, 0.5, 0.5, 0.0);
				MatrixMultiplyShear(matrix, x, y);
				MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);
				break;
			}
				
			case TMOD_ROTATE2:
			{
				x = RAD2DEG(RB_EvalExpression(&pStage->bundle[TB_COLORMAP].texMods[i].rExp, 0)) * 5.0;
	
				MatrixMultiplyTranslation(matrix, 0.5, 0.5, 0.0);
				MatrixMultiplyZRotation(matrix, x);
				MatrixMultiplyTranslation(matrix, -0.5, -0.5, 0.0);
				break;
			}
			
			default:
				break;
		}
	}
	
	MatrixMultiply(matrix, dlight->attenuationMatrix, dlight->attenuationMatrix2);
}


void Tess_StageIteratorLighting()
{
	int				i, j;
	trRefDlight_t  *dl;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;
	
	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va("--- Tess_StageIteratorLighting( %s surface, %s light, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name, tess.lightShader->name, tess.numVertexes, tess.numIndexes / 3));
	}
	
	if(!tess.surfaceShader->interactLight)
	{
		// Tr3B: abort if surface shader has no ability to interact with light
		// this will save texcoords and matrix calculations
		return;	
	}
	
	dl = backEnd.currentLight;
	
	RB_DeformTessGeometry();

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
	}

	// lock XYZ
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		qglVertexPointer(3, GL_FLOAT, 16, BUFFER_OFFSET(tess.ofsXYZ));
	}
	else
	{
		qglVertexPointer(4, GL_FLOAT, 0, tess.xyz);
	}
	
	if(qglLockArraysEXT)
	{
		qglLockArraysEXT(0, tess.numVertexes);
		GLimp_LogComment("glLockArraysEXT\n");
	}

	// call shader function
	attenuationZStage = tess.lightShader->stages[0];
		
	for(i = 0; i < MAX_SHADER_STAGES; i++)
	{
		shaderStage_t  *diffuseStage = tess.surfaceStages[i];

		if(!diffuseStage)
		{
			break;
		}
		
		if(!RB_EvalExpression(&diffuseStage->ifExp, 1.0))
		{
			continue;
		}
			
		if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
		{
			ComputeTexMatrices(diffuseStage);
		}
		else
		{
			ComputeTexCoords(diffuseStage);
		}
		
		for(j = 1; j < MAX_SHADER_STAGES; j++)
		{
			attenuationXYStage = tess.lightShader->stages[j];
						
			if(!attenuationXYStage)
			{
				break;
			}
			
			if(attenuationXYStage->type != ST_ATTENUATIONMAP_XY)
			{
				continue;
			}
			
			if(!RB_EvalExpression(&attenuationXYStage->ifExp, 1.0))
			{
				continue;
			}
		
			ComputeColor(attenuationXYStage);
			ComputeFinalAttenuation(attenuationXYStage, dl);
				
			switch(diffuseStage->type)
			{
				case ST_DIFFUSEMAP:
				case ST_COLLAPSE_lighting_D_radiosity:
					if(glConfig2.shadingLanguage100Available)
					{
						if(dl->l.rlType == RL_OMNI)
						{
							Render_lighting_D_omni(diffuseStage, attenuationXYStage, attenuationZStage, dl);
						}
						else if(dl->l.rlType == RL_PROJ)
						{
							Render_lighting_D_proj(diffuseStage, attenuationXYStage, attenuationZStage, dl);
						}
						else
						{
							// TODO
						}
					}
					else
					{
							// TODO
					}
					break;
						
				case ST_COLLAPSE_lighting_DB_radiosity:
				case ST_COLLAPSE_lighting_DB_direct:
				case ST_COLLAPSE_lighting_DB_generic:
					if(glConfig2.shadingLanguage100Available)
					{
						if(r_lighting->integer == 1)
						{
							if(dl->l.rlType == RL_OMNI)
							{
								Render_lighting_DB_omni(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else if(dl->l.rlType == RL_PROJ)
							{
								Render_lighting_D_proj(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else
							{
								// TODO
							}
						}
						else
						{
							if(dl->l.rlType == RL_OMNI)
							{
								Render_lighting_D_omni(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else if(dl->l.rlType == RL_PROJ)
							{
								Render_lighting_D_proj(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else
							{
								// TODO
							}
						}
					}
					else
					{
							// TODO
					}
					break;
						
				case ST_COLLAPSE_lighting_DBS_radiosity:
				case ST_COLLAPSE_lighting_DBS_direct:
				case ST_COLLAPSE_lighting_DBS_generic:
					if(glConfig2.shadingLanguage100Available)
					{
						if(r_lighting->integer == 2)
						{
							if(dl->l.rlType == RL_OMNI)
							{
								Render_lighting_DBS_omni(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else if(dl->l.rlType == RL_PROJ)
							{
								Render_lighting_D_proj(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else
							{
								// TODO
							}
						}
						else if(r_lighting->integer == 1)
						{
							if(dl->l.rlType == RL_OMNI)
							{
								Render_lighting_DB_omni(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else if(dl->l.rlType == RL_PROJ)
							{
								Render_lighting_D_proj(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else
							{
								// TODO
							}
						}
						else
						{
							if(dl->l.rlType == RL_OMNI)
							{
								Render_lighting_D_omni(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else if(dl->l.rlType == RL_PROJ)
							{
								Render_lighting_D_proj(diffuseStage, attenuationXYStage, attenuationZStage, dl);
							}
							else
							{
								// TODO
							}
						}
					}
					else
					{
							// TODO
					}
					break;
						
				default:
					break;
			}
		}
	}

	// unlock arrays
	if(qglUnlockArraysEXT)
	{
		qglUnlockArraysEXT();
		GLimp_LogComment("glUnlockArraysEXT\n");
	}

	// reset polygon offset
	if(tess.surfaceShader->polygonOffset)
	{
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}
}

void Tess_StageIteratorGeneric()
{
	int             stage;
	
	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va("--- Tess_StageIteratorGeneric( %s surface, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name, tess.numVertexes, tess.numIndexes / 3));
	}
	
	RB_DeformTessGeometry();

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
	}

	// lock XYZ
	if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
	{
		qglVertexPointer(3, GL_FLOAT, 16, BUFFER_OFFSET(tess.ofsXYZ));
	}
	else
	{
		qglVertexPointer(4, GL_FLOAT, 0, tess.xyz);
	}
	
	if(qglLockArraysEXT)
	{
		qglLockArraysEXT(0, tess.numVertexes);
		GLimp_LogComment("glLockArraysEXT\n");
	}

	// call shader function
	for(stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t  *pStage = tess.surfaceStages[stage];

		if(!pStage)
		{
			break;
		}
		
		if(!RB_EvalExpression(&pStage->ifExp, 1.0))
		{
			continue;
		}

		if(glConfig2.vertexBufferObjectAvailable && tess.vertexesVBO)
		{
			ComputeColor(pStage);
			ComputeTexMatrices(pStage);
		}
		else
		{
			ComputeTexCoords(pStage);
		}

		switch(pStage->type)
		{
			case ST_DIFFUSEMAP:
			case ST_COLLAPSE_lighting_DB_generic:
			case ST_COLLAPSE_lighting_DBS_generic:
			{
				if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_generic_single(stage);
					}
					else
					{
						Render_generic_single_FFP(stage);
					}
				}
				break;
			}
			
			case ST_LIGHTMAP:
			{
				if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					Render_lightmap_FFP(stage, TB_COLORMAP);
				}
				break;
			}
			
			// skip these if not merge into diffuse stage
			case ST_NORMALMAP:
			case ST_SPECULARMAP:
				break;
			
			case ST_COLLAPSE_Generic_multi:
			{
				Render_generic_multi_FFP(stage);
				break;
			}
			
			case ST_COLLAPSE_lighting_D_radiosity:
			{
				if(r_showLightMaps->integer)
				{
					Render_lightmap_FFP(stage, TB_LIGHTMAP);
				}
				else if(tr.worldDeluxeMapping && r_showDeluxeMaps->integer)
				{
					Render_deluxemap_FFP(stage, TB_LIGHTMAP);
				}
				else if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_lighting_D_radiosity(stage);
					}
					else
					{
						Render_lighting_D_radiosity_FFP(stage);
					}
				}
				break;
			}
			
			case ST_COLLAPSE_lighting_DB_radiosity:
			{
				if(r_showLightMaps->integer)
				{
					Render_lightmap_FFP(stage, TB_LIGHTMAP);
				}
				else if(tr.worldDeluxeMapping && r_showDeluxeMaps->integer)
				{
					Render_deluxemap_FFP(stage, TB_LIGHTMAP);
				}
				else if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					if(glConfig2.shadingLanguage100Available)
					{
						if(tr.worldDeluxeMapping && r_lighting->integer == 1)
						{
							Render_lighting_DB_radiosity(stage);
						}
						else
						{
							Render_lighting_D_radiosity(stage);
						}
					}
					else
					{
						Render_lighting_D_radiosity_FFP(stage);
					}
				}
				break;
			}
			
			case ST_COLLAPSE_lighting_DBS_radiosity:
			{
				if(r_showLightMaps->integer)
				{
					Render_lightmap_FFP(stage, TB_LIGHTMAP);
				}
				else if(tr.worldDeluxeMapping && r_showDeluxeMaps->integer)
				{
					Render_deluxemap_FFP(stage, TB_LIGHTMAP);
				}
				else if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					if(glConfig2.shadingLanguage100Available)
					{
						if(tr.worldDeluxeMapping && r_lighting->integer == 2)
						{
							Render_lighting_DBS_radiosity(stage);
						}
						else if(tr.worldDeluxeMapping && r_lighting->integer == 1)
						{
							Render_lighting_DB_radiosity(stage);
						}
						else
						{
							Render_lighting_D_radiosity(stage);
						}
					}
					else
					{
						Render_lighting_D_radiosity_FFP(stage);
					}
				}
				break;
			}
			
			case ST_COLLAPSE_lighting_DB_direct:
			{
				if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					if(glConfig2.shadingLanguage100Available)
					{
						if(r_lighting->integer == 1)
						{
							Render_lighting_DB_direct(stage);
						}
						else
						{
							Render_lighting_D_direct(stage);
						}
					}
					else
					{
						Render_generic_single_FFP(stage);
					}
				}
				break;
			}
			
			case ST_COLLAPSE_lighting_DBS_direct:
			{
				if(r_dynamicLighting->integer == 2)
				{
					if(glConfig2.shadingLanguage100Available)
					{
						Render_depthFill(stage);
					}
					else
					{
						Render_depthFill_FFP(stage);
					}
				}
				else
				{
					if(glConfig2.shadingLanguage100Available)
					{
						if(r_lighting->integer == 2)
						{
							Render_lighting_DBS_direct(stage);
						}
						else if(r_lighting->integer == 1)
						{
							Render_lighting_DB_direct(stage);
						}
						else
						{
							Render_lighting_D_direct(stage);
						}
					}
					else
					{
						Render_generic_single_FFP(stage);
					}
				}
				break;
			}
			
			case ST_REFLECTIONMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_reflection_C(stage);
				}
				else
				{
					// TODO
				}
				break;
			}
			
			case ST_REFRACTIONMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_refraction_C(stage);
				}
				else
				{
					// TODO
				}
				break;
			}
			
			case ST_DISPERSIONMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_dispersion_C(stage);
				}
				else
				{
					// TODO
				}
				break;
			}
			
			case ST_SKYBOXMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_skybox(stage);
				}
				else
				{
					// TODO
				}
				break;
			}
			
			case ST_HEATHAZEMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_heatHaze(stage);
				}
				else
				{
					// TODO
				}
				break;
			}
			
			case ST_GLOWMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_glow(stage);
				}
				else
				{
					// TODO
				}
				break;
			}
	
			default:
			case ST_COLORMAP:
			{
				if(glConfig2.shadingLanguage100Available)
				{
					Render_generic_single(stage);
				}
				else
				{
					Render_generic_single_FFP(stage);
				}
				break;
			}
		}
		
		// allow skipping out to show just lightmaps during development
		if(r_showLightMaps->integer && (pStage->type == ST_LIGHTMAP || pStage->bundle[TB_LIGHTMAP].isLightMap))
		{
			break;
		}
	}
	
	if(tess.fogNum >= 1 && tess.surfaceShader->fogPass)
	{
		Render_fog();
	}

	// unlock arrays
	if(qglUnlockArraysEXT)
	{
		qglUnlockArraysEXT();
		GLimp_LogComment("glUnlockArraysEXT\n");
	}

	// reset polygon offset
	if(tess.surfaceShader->polygonOffset)
	{
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}
}

/*
=================
Render_shadowVolume
=================
*/
static void Render_shadowVolume()
{
	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va("--- Render_shadowVolume( %i vertices, %i triangles ) ---\n", tess.numVertexes, tess.numIndexes / 3));
	}
	
	if(r_showShadowVolumes->integer)
	{
		//GL_State(GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
		GL_State(GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
		//GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
		//GL_State(GLS_DEPTHMASK_TRUE);
#if 1
		GL_Cull(CT_FRONT_SIDED);
		//qglColor4f(1.0f, 1.0f, 0.7f, 0.05f);
		qglColor4f(1.0f, 0.0f, 0.0f, 0.05f);
		R_DrawElements();
#endif

#if 1
		GL_Cull(CT_BACK_SIDED);
		qglColor4f(0.0f, 1.0f, 0.0f, 0.05f);
		R_DrawElements();
#endif
	
#if 1
		GL_State(GLS_DEPTHFUNC_LESS | GLS_POLYMODE_LINE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
		GL_Cull(CT_TWO_SIDED);
		qglColor4f(0.0f, 0.0f, 1.0f, 0.05f);
		R_DrawElements();
#endif
	}
	else
	{
		if(backEnd.currentEntity->needZFail)
		{
			// mirrors have the culling order reversed
			//if(backEnd.viewParms.isMirror)
			//	qglFrontFace(GL_CW);
			
			if(qglActiveStencilFaceEXT && glConfig2.stencilWrapAvailable)
			{
				// render both sides at once
				GL_Cull(CT_TWO_SIDED);
				
				qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
				
				qglActiveStencilFaceEXT(GL_BACK);
				qglStencilOp(GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

				qglActiveStencilFaceEXT(GL_FRONT);
				qglStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
				
				R_DrawElements();
				
				qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			}
			else
			{
				// draw only the front faces of the shadow volume
				GL_Cull(CT_FRONT_SIDED);
		
				// increment the stencil value on zfail
				if(glConfig2.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
				}
		
				R_DrawElements();
		
				// draw only the back faces of the shadow volume
				GL_Cull(CT_BACK_SIDED);
		
				// decrement the stencil value on zfail
				if(glConfig2.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
				}
		
				R_DrawElements();
			}
			
			//if(backEnd.viewParms.isMirror)
			//	qglFrontFace(GL_CCW);
		}
		else
		{
			// Tr3B - zpass rendering is cheaper because we can skip the lightcap and darkcap
			// see GPU Gems1 9.5.4
			
			// mirrors have the culling order reversed
			//if(backEnd.viewParms.isMirror)
			//	qglFrontFace(GL_CW);
			
			if(qglActiveStencilFaceEXT && glConfig2.stencilWrapAvailable)
			{
				// render both sides at once
				GL_Cull(CT_TWO_SIDED);
				
				qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

				qglActiveStencilFaceEXT(GL_BACK);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
				
				qglActiveStencilFaceEXT(GL_FRONT);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
				
				R_DrawElements();
				
				qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
				
				qglEnable(GL_CULL_FACE);
			}
			else
			{
				// draw only the back faces of the shadow volume
				GL_Cull(CT_BACK_SIDED);
					
				// increment the stencil value on zpass
				if(glConfig2.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				}
					
				R_DrawElements();
		
				// draw only the front faces of the shadow volume
				GL_Cull(CT_FRONT_SIDED);
		
				// decrement the stencil value on zpass
				if(glConfig2.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
				}
		
				R_DrawElements();
			}
			
			//if(backEnd.viewParms.isMirror)
			//	qglFrontFace(GL_CCW);
		}
	}
	
	backEnd.pc.c_shadowBatches++;
}

void Tess_End()
{
	if(tess.numIndexes == 0)
	{
		return;
	}

	if(tess.indexes[SHADER_MAX_INDEXES - 1] != 0)
	{
		ri.Error(ERR_DROP, "Tess_End() - SHADER_MAX_INDEXES hit");
	}
	if(tess.xyz[SHADER_MAX_VERTEXES - 1][0] != 0)
	{
		ri.Error(ERR_DROP, "Tess_End() - SHADER_MAX_VERTEXES hit");
	}
	
	// for debugging of sort order issues, stop rendering after a given sort value
	if(r_debugSort->integer && r_debugSort->integer < tess.surfaceShader->sort)
	{
		return;
	}

	// update performance counter
	backEnd.pc.c_batches++;

	// draw content	
	if(tess.shadowVolume)
	{
		Render_shadowVolume();
	}
	else
	{
		// call off to shader specific tess end function
		tess.currentStageIteratorFunc();
	
		// draw debugging stuff
		if(r_showtris->integer)
		{
			DrawTris(&tess);
		}
		if(r_shownormals->integer)
		{
			DrawNormals(&tess);
		}
		if(r_showTangentSpaces->integer)
		{
			DrawTangentSpaces(&tess);
		}
	}
	
	// unbind VBO
	if(glConfig2.vertexBufferObjectAvailable)
	{
		if(tess.indexesVBO)
		{
			qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			tess.indexesVBO = 0;
			
			backEnd.pc.c_vboIndexBuffers++;
		}
		
		if(tess.vertexesVBO)
		{
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			tess.vertexesVBO = 0;
			
			backEnd.pc.c_vboVertexBuffers++;
		}
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

	GLimp_LogComment("----------\n");
}
