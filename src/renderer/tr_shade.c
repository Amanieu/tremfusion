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
// tr_shade.c
#include "tr_local.h"

/*
=================================================================================
THIS ENTIRE FILE IS BACK END!

This file deals with applying shaders to surface data in the tess struct.
=================================================================================
*/

static void GLSL_PrintInfoLog(GLhandleARB object, qboolean developerOnly)
{
	char           *msg;
	static char     msgPart[1024];
	int             maxLength = 0;
	int             i;

	qglGetObjectParameterivARB(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &maxLength);

	msg = Com_Allocate(maxLength);

	qglGetInfoLogARB(object, maxLength, &maxLength, msg);

	if(developerOnly)
	{
		ri.Printf(PRINT_DEVELOPER, "compile log:\n");
	}
	else
	{
		ri.Printf(PRINT_ALL, "compile log:\n");
	}

	for(i = 0; i < maxLength; i += 1024)
	{
		Q_strncpyz(msgPart, msg + i, sizeof(msgPart));

		if(developerOnly)
			ri.Printf(PRINT_DEVELOPER, "%s\n", msgPart);
		else
			ri.Printf(PRINT_ALL, "%s\n", msgPart);
	}

	Com_Dealloc(msg);
}

static void GLSL_PrintShaderSource(GLhandleARB object)
{
	char           *msg;
	static char     msgPart[1024];
	int             maxLength = 0;
	int             i;

	qglGetObjectParameterivARB(object, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &maxLength);

	msg = Com_Allocate(maxLength);

	qglGetShaderSourceARB(object, maxLength, &maxLength, msg);

	for(i = 0; i < maxLength; i += 1024)
	{
		Q_strncpyz(msgPart, msg + i, sizeof(msgPart));
		ri.Printf(PRINT_ALL, "%s\n", msgPart);
	}

	Com_Dealloc(msg);
}

static void GLSL_LoadGPUShader(GLhandleARB program, const char *name, GLenum shaderType)
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

	ri.Printf(PRINT_ALL, "...loading '%s'\n", filename);
	size = ri.FS_ReadFile(filename, (void **)&buffer);
	if(!buffer)
	{
		ri.Error(ERR_DROP, "Couldn't load %s", filename);
	}

	shader = qglCreateShaderObjectARB(shaderType);

	{
		static char     bufferExtra[32000];
		int             sizeExtra;

		char           *bufferFinal = NULL;
		int             sizeFinal;

		float           fbufWidthScale, fbufHeightScale;
		float           npotWidthScale, npotHeightScale;

		Com_Memset(bufferExtra, 0, sizeof(bufferExtra));

		// HACK: abuse the GLSL preprocessor to turn GLSL 1.20 shaders into 1.30 ones
		if(glConfig.driverType == GLDRV_OPENGL3)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#version 130\n");

			if(shaderType == GL_VERTEX_SHADER_ARB)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#define attribute in\n");
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#define varying out\n");
			}
			else
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#define varying in\n");

				Q_strcat(bufferExtra, sizeof(bufferExtra), "out vec4 out_Color;\n");
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#define gl_FragColor out_Color\n");
			}
		}
		else
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#version 120\n");
		}

#if defined(COMPAT_Q3A)
		Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef COMPAT_Q3A\n#define COMPAT_Q3A 1\n#endif\n");
#endif

		// HACK: add some macros to avoid extra uniforms and save speed and code maintenance
		Q_strcat(bufferExtra, sizeof(bufferExtra),
				 va("#ifndef r_SpecularExponent\n#define r_SpecularExponent %f\n#endif\n", r_specularExponent->value));
		Q_strcat(bufferExtra, sizeof(bufferExtra),
				 va("#ifndef r_SpecularScale\n#define r_SpecularScale %f\n#endif\n", r_specularScale->value));
		//Q_strcat(bufferExtra, sizeof(bufferExtra),
		//       va("#ifndef r_NormalScale\n#define r_NormalScale %f\n#endif\n", r_normalScale->value));

		Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef colorGen_t\n"
							"#define colorGen_t\n"
							"#define CGEN_VERTEX %i\n"
							"#define CGEN_ONE_MINUS_VERTEX %i\n"
							"#endif\n",
							CGEN_VERTEX,
							CGEN_ONE_MINUS_VERTEX));

		Q_strcat(bufferExtra, sizeof(bufferExtra),
								 va("#ifndef alphaGen_t\n"
									"#define alphaGen_t\n"
									"#define AGEN_VERTEX %i\n"
									"#define AGEN_ONE_MINUS_VERTEX %i\n"
									"#endif\n",
									AGEN_VERTEX,
									AGEN_ONE_MINUS_VERTEX));

		Q_strcat(bufferExtra, sizeof(bufferExtra),
								 va("#ifndef alphaTest_t\n"
									"#define alphaTest_t\n"
									"#define ATEST_GT_0 %i\n"
									"#define ATEST_LT_128 %i\n"
									"#define ATEST_GE_128 %i\n"
									"#endif\n",
									ATEST_GT_0,
									ATEST_LT_128,
									ATEST_GE_128));

		fbufWidthScale = Q_recip((float)glConfig.vidWidth);
		fbufHeightScale = Q_recip((float)glConfig.vidHeight);
		Q_strcat(bufferExtra, sizeof(bufferExtra),
				 va("#ifndef r_FBufScale\n#define r_FBufScale vec2(%f, %f)\n#endif\n", fbufWidthScale, fbufHeightScale));

		if(glConfig.textureNPOTAvailable)
		{
			npotWidthScale = 1;
			npotHeightScale = 1;
		}
		else
		{
			npotWidthScale = (float)glConfig.vidWidth / (float)NearestPowerOfTwo(glConfig.vidWidth);
			npotHeightScale = (float)glConfig.vidHeight / (float)NearestPowerOfTwo(glConfig.vidHeight);
		}
		Q_strcat(bufferExtra, sizeof(bufferExtra),
				 va("#ifndef r_NPOTScale\n#define r_NPOTScale vec2(%f, %f)\n#endif\n", npotWidthScale, npotHeightScale));



		if(glConfig.driverType == GLDRV_MESA)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef MESA\n#define MESA 1\n#endif\n");
		}

		if(glConfig.hardwareType == GLHW_ATI)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef GLHW_ATI\n#define GLHW_ATI 1\n#endif\n");
		}
		else if(glConfig.hardwareType == GLHW_ATI_DX10)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef GLHW_ATI_DX10\n#define GLHW_ATI_DX10 1\n#endif\n");
		}
		else if(glConfig.hardwareType == GLHW_NV_DX10)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef GLHW_NV_DX10\n#define GLHW_NV_DX10 1\n#endif\n");
		}

		if(r_shadows->integer >= 4 && r_shadows->integer <= 5 && glConfig.textureFloatAvailable &&
		   glConfig.framebufferObjectAvailable)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef VSM\n#define VSM 1\n#endif\n");

			if(glConfig.hardwareType == GLHW_ATI)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef VSM_CLAMP\n#define VSM_CLAMP 1\n#endif\n");
			}

			if((glConfig.hardwareType == GLHW_NV_DX10 || glConfig.hardwareType == GLHW_ATI_DX10) && r_shadows->integer == 5)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef VSM_EPSILON\n#define VSM_EPSILON 0.000001\n#endif\n");
			}
			else
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef VSM_EPSILON\n#define VSM_EPSILON 0.0001\n#endif\n");
			}

			if(r_debugShadowMaps->integer)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef DEBUG_VSM\n#define DEBUG_VSM %i\n#endif\n", r_debugShadowMaps->integer));
			}

			if(r_lightBleedReduction->value)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef r_LightBleedReduction\n#define r_LightBleedReduction %f\n#endif\n",
							r_lightBleedReduction->value));
			}

			if(r_softShadows->integer == 1)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_2X2\n#define PCF_2X2 1\n#endif\n");
			}
			else if(r_softShadows->integer == 2)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_3X3\n#define PCF_3X3 1\n#endif\n");
			}
			else if(r_softShadows->integer == 3)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_4X4\n#define PCF_4X4 1\n#endif\n");
			}
			else if(r_softShadows->integer == 4)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_5X5\n#define PCF_5X5 1\n#endif\n");
			}
			else if(r_softShadows->integer == 5)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_6X6\n#define PCF_6X6 1\n#endif\n");
			}
			else if(r_softShadows->integer == 6)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCSS\n#define PCSS 1\n#endif\n");
			}
		}

		if(r_shadows->integer == 6 && glConfig.textureFloatAvailable && glConfig.framebufferObjectAvailable)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef ESM\n#define ESM 1\n#endif\n");

			if(r_debugShadowMaps->integer)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef DEBUG_ESM\n#define DEBUG_ESM %i\n#endif\n", r_debugShadowMaps->integer));
			}

			if(r_lightBleedReduction->value)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef r_LightBleedReduction\n#define r_LightBleedReduction %f\n#endif\n",
							r_lightBleedReduction->value));
			}

			if(r_overDarkeningFactor->value)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef r_OverDarkeningFactor\n#define r_OverDarkeningFactor %f\n#endif\n",
							r_overDarkeningFactor->value));
			}

			if(r_shadowMapDepthScale->value)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 va("#ifndef r_ShadowMapDepthScale\n#define r_ShadowMapDepthScale %f\n#endif\n",
							r_shadowMapDepthScale->value));
			}

			if(r_softShadows->integer == 1)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_2X2\n#define PCF_2X2 1\n#endif\n");
			}
			else if(r_softShadows->integer == 2)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_3X3\n#define PCF_3X3 1\n#endif\n");
			}
			else if(r_softShadows->integer == 3)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_4X4\n#define PCF_4X4 1\n#endif\n");
			}
			else if(r_softShadows->integer == 4)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_5X5\n#define PCF_5X5 1\n#endif\n");
			}
			else if(r_softShadows->integer == 5)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCF_6X6\n#define PCF_6X6 1\n#endif\n");
			}
			else if(r_softShadows->integer == 6)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef PCSS\n#define PCSS 1\n#endif\n");
			}
		}

		if(r_deferredShading->integer && glConfig.maxColorAttachments >= 4 && glConfig.textureFloatAvailable &&
		   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_deferredShading\n#define r_deferredShading 1\n#endif\n");

			if(glConfig.framebufferMixedFormatsAvailable)
			{
				Q_strcat(bufferExtra, sizeof(bufferExtra),
						 "#ifndef GL_EXTX_framebuffer_mixed_formats\n#define GL_EXTX_framebuffer_mixed_formats 1\n#endif\n");
			}
		}

		if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_HDRRendering\n#define r_HDRRendering 1\n#endif\n");

			Q_strcat(bufferExtra, sizeof(bufferExtra),
					 va("#ifndef r_HDRContrastThreshold\n#define r_HDRContrastThreshold %f\n#endif\n",
						r_hdrContrastThreshold->value));

			Q_strcat(bufferExtra, sizeof(bufferExtra),
					 va("#ifndef r_HDRContrastOffset\n#define r_HDRContrastOffset %f\n#endif\n",
						r_hdrContrastOffset->value));

			Q_strcat(bufferExtra, sizeof(bufferExtra),
					 va("#ifndef r_HDRToneMappingOperator\n#define r_HDRToneMappingOperator_%i\n#endif\n",
						r_hdrToneMappingOperator->integer));

			Q_strcat(bufferExtra, sizeof(bufferExtra),
					 va("#ifndef r_HDRGamma\n#define r_HDRGamma %f\n#endif\n",
						r_hdrGamma->value));
		}

		if(r_precomputedLighting->integer)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra),
					 "#ifndef r_precomputedLighting\n#define r_precomputedLighting 1\n#endif\n");
		}

		if(r_heatHazeFix->integer && glConfig.framebufferBlitAvailable)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_heatHazeFix\n#define r_heatHazeFix 1\n#endif\n");
		}

		if(r_showLightMaps->integer)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_showLightMaps\n#define r_showLightMaps 1\n#endif\n");
		}

		if(r_showDeluxeMaps->integer)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_showDeluxeMaps\n#define r_showDeluxeMaps 1\n#endif\n");
		}

		if(r_screenSpaceAmbientOcclusion->integer)
		{
			int             i;
			static vec3_t   jitter[32];
			static qboolean jitterInit = qfalse;

			if(!jitterInit)
			{
				for(i = 0; i < 32; i++)
				{
					float          *jit = &jitter[i][0];

					float           rad = crandom() * 1024.0f;	// FIXME radius;
					float           a = crandom() * M_PI * 2;
					float           b = crandom() * M_PI * 2;

					jit[0] = rad * sin(a) * cos(b);
					jit[1] = rad * sin(a) * sin(b);
					jit[2] = rad * cos(a);
				}

				jitterInit = qtrue;
			}

			// TODO
		}

		if(glConfig.vboVertexSkinningAvailable)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_VertexSkinning\n#define r_VertexSkinning 1\n#endif\n");

			Q_strcat(bufferExtra, sizeof(bufferExtra),
								 va("#ifndef MAX_GLSL_BONES\n#define MAX_GLSL_BONES %i\n#endif\n", glConfig.maxVertexSkinningBones));
		}

		/*
		   if(glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
		   {
		   //Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef GL_ARB_draw_buffers\n#define GL_ARB_draw_buffers 1\n#endif\n");
		   Q_strcat(bufferExtra, sizeof(bufferExtra), "#extension GL_ARB_draw_buffers : enable\n");
		   }
		 */

		if(r_normalMapping->integer)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_NormalMapping\n#define r_NormalMapping 1\n#endif\n");
		}

		if( /* TODO: check for shader model 3 hardware  && */ r_normalMapping->integer && r_parallaxMapping->integer)
		{
			Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef r_ParallaxMapping\n#define r_ParallaxMapping 1\n#endif\n");
		}

		/*
		   if(glConfig.textureFloatAvailable)
		   {
		   Q_strcat(bufferExtra, sizeof(bufferExtra), "#ifndef GL_ARB_texture_float\n#define GL_ARB_texture_float 1\n#endif\n");
		   }
		 */

		sizeExtra = strlen(bufferExtra);
		sizeFinal = sizeExtra + size;

		//ri.Printf(PRINT_ALL, "GLSL extra: %s\n", bufferExtra);

		bufferFinal = ri.Hunk_AllocateTempMemory(size + sizeExtra);

		strcpy(bufferFinal, bufferExtra);
		Q_strcat(bufferFinal, sizeFinal, buffer);

		qglShaderSourceARB(shader, 1, (const GLcharARB **)&bufferFinal, &sizeFinal);

		ri.Hunk_FreeTempMemory(bufferFinal);
	}

	// compile shader
	qglCompileShaderARB(shader);

	// check if shader compiled
	qglGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
	if(!compiled)
	{
		GLSL_PrintShaderSource(shader);
		GLSL_PrintInfoLog(shader, qfalse);
		ri.Error(ERR_DROP, "Couldn't compile %s", filename);
		ri.FS_FreeFile(buffer);
		return;
	}

	GLSL_PrintInfoLog(shader, qtrue);
	//ri.Printf(PRINT_ALL, "%s\n", GLSL_PrintShaderSource(shader));

	// attach shader to program
	qglAttachObjectARB(program, shader);

	// delete shader, no longer needed
	qglDeleteObjectARB(shader);

	ri.FS_FreeFile(buffer);
}

static void GLSL_LinkProgram(GLhandleARB program)
{
	GLint           linked;

	qglLinkProgramARB(program);

	qglGetObjectParameterivARB(program, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(!linked)
	{
		GLSL_PrintInfoLog(program, qfalse);
		ri.Error(ERR_DROP, "%s\nshaders failed to link");
	}
}

static void GLSL_ValidateProgram(GLhandleARB program)
{
	GLint           validated;

	qglValidateProgramARB(program);

	qglGetObjectParameterivARB(program, GL_OBJECT_VALIDATE_STATUS_ARB, &validated);
	if(!validated)
	{
		GLSL_PrintInfoLog(program, qfalse);
		ri.Error(ERR_DROP, "%s\nshaders failed to validate");
	}
}

static void GLSL_ShowProgramUniforms(GLhandleARB program)
{
	int             i, count, size;
	GLenum			type;
	char            uniformName[1000];

	// install the executables in the program object as part of current state.
	qglUseProgramObjectARB(program);

	// check for GL Errors

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

static void GLSL_InitGPUShader(shaderProgram_t * program, const char *name, int attribs, qboolean fragmentShader)
{
	ri.Printf(PRINT_DEVELOPER, "------- GPU shader -------\n");

	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "GLSL_InitGPUShader: \"%s\" is too long\n", name);
	}

	Q_strncpyz(program->name, name, sizeof(program->name));

	program->program = qglCreateProgramObjectARB();
	program->attribs = attribs;

	GLSL_LoadGPUShader(program->program, name, GL_VERTEX_SHADER_ARB);

	if(fragmentShader)
		GLSL_LoadGPUShader(program->program, name, GL_FRAGMENT_SHADER_ARB);

	if(attribs & ATTR_POSITION)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_POSITION, "attr_Position");

	if(attribs & ATTR_TEXCOORD)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD0, "attr_TexCoord0");

	if(attribs & ATTR_LIGHTCOORD)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD1, "attr_TexCoord1");

//  if(attribs & ATTR_TEXCOORD2)
//      qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD2, "attr_TexCoord2");

//  if(attribs & ATTR_TEXCOORD3)
//      qglBindAttribLocationARB(program->program, ATTR_INDEX_TEXCOORD3, "attr_TexCoord3");

	if(attribs & ATTR_TANGENT)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_TANGENT, "attr_Tangent");

	if(attribs & ATTR_BINORMAL)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_BINORMAL, "attr_Binormal");

	if(attribs & ATTR_NORMAL)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_NORMAL, "attr_Normal");

	if(attribs & ATTR_COLOR)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_COLOR, "attr_Color");

	if(attribs & ATTR_PAINTCOLOR)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_PAINTCOLOR, "attr_PaintColor");

	if(attribs & ATTR_LIGHTDIRECTION)
		qglBindAttribLocationARB(program->program, ATTR_INDEX_LIGHTDIRECTION, "attr_LightDirection");

	if(glConfig.vboVertexSkinningAvailable)
	{
		qglBindAttribLocationARB(program->program, ATTR_INDEX_BONE_INDEXES, "attr_BoneIndexes");
		qglBindAttribLocationARB(program->program, ATTR_INDEX_BONE_WEIGHTS, "attr_BoneWeights");
	}

	GLSL_LinkProgram(program->program);
}


void GLSL_InitGPUShaders(void)
{
	int             startTime, endTime;

	ri.Printf(PRINT_ALL, "------- GLSL_InitGPUShaders -------\n");

	// make sure the render thread is stopped
	R_SyncRenderThread();

	startTime = ri.Milliseconds();

	// single texture rendering
	GLSL_InitGPUShader(&tr.genericSingleShader, "genericSingle", ATTR_POSITION | ATTR_TEXCOORD | ATTR_NORMAL | ATTR_COLOR, qtrue);

	tr.genericSingleShader.u_ColorMap = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ColorMap");
	tr.genericSingleShader.u_ColorTextureMatrix =
		qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ColorTextureMatrix");
	tr.genericSingleShader.u_ColorGen = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ColorGen");
	tr.genericSingleShader.u_AlphaGen = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_AlphaGen");
	tr.genericSingleShader.u_Color = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_Color");
	tr.genericSingleShader.u_AlphaTest = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_AlphaTest");
	tr.genericSingleShader.u_ViewOrigin = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ViewOrigin");
	tr.genericSingleShader.u_TCGen_Environment =
			qglGetUniformLocationARB(tr.genericSingleShader.program, "u_TCGen_Environment");
	tr.genericSingleShader.u_PortalClipping = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_PortalClipping");
	tr.genericSingleShader.u_PortalPlane = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_PortalPlane");
	tr.genericSingleShader.u_ModelMatrix = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ModelMatrix");
	/*
	   tr.genericSingleShader.u_ModelViewMatrix =
	   qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ModelViewMatrix");
	   tr.genericSingleShader.u_ProjectionMatrix =
	   qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ProjectionMatrix");
	 */
	tr.genericSingleShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.genericSingleShader.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.genericSingleShader.u_VertexSkinning = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_VertexSkinning");
		tr.genericSingleShader.u_BoneMatrix = qglGetUniformLocationARB(tr.genericSingleShader.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.genericSingleShader.program);
	qglUniform1iARB(tr.genericSingleShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.genericSingleShader.program);
	GLSL_ShowProgramUniforms(tr.genericSingleShader.program);
	GL_CheckErrors();

	// simple vertex color shading for entities
	GLSL_InitGPUShader(&tr.vertexLightingShader_DBS_entity,
					   "vertexLighting_DBS_entity",
					   ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL, qtrue);

	tr.vertexLightingShader_DBS_entity.u_DiffuseMap =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_DiffuseMap");
	tr.vertexLightingShader_DBS_entity.u_NormalMap =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_NormalMap");
	tr.vertexLightingShader_DBS_entity.u_SpecularMap =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_SpecularMap");
	tr.vertexLightingShader_DBS_entity.u_DiffuseTextureMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_DiffuseTextureMatrix");
	tr.vertexLightingShader_DBS_entity.u_NormalTextureMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_NormalTextureMatrix");
	tr.vertexLightingShader_DBS_entity.u_SpecularTextureMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_SpecularTextureMatrix");
	tr.vertexLightingShader_DBS_entity.u_AlphaTest =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_AlphaTest");
	tr.vertexLightingShader_DBS_entity.u_ViewOrigin =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_ViewOrigin");
	tr.vertexLightingShader_DBS_entity.u_AmbientColor =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_AmbientColor");
	tr.vertexLightingShader_DBS_entity.u_LightDir =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_LightDir");
	tr.vertexLightingShader_DBS_entity.u_LightColor =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_LightColor");
	tr.vertexLightingShader_DBS_entity.u_ParallaxMapping =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_ParallaxMapping");
	tr.vertexLightingShader_DBS_entity.u_DepthScale =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_DepthScale");
	tr.vertexLightingShader_DBS_entity.u_PortalClipping =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_PortalClipping");
	tr.vertexLightingShader_DBS_entity.u_PortalPlane =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_PortalPlane");
	tr.vertexLightingShader_DBS_entity.u_ModelMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_ModelMatrix");
	tr.vertexLightingShader_DBS_entity.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.vertexLightingShader_DBS_entity.u_VertexSkinning =
			qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_VertexSkinning");
		tr.vertexLightingShader_DBS_entity.u_BoneMatrix =
			qglGetUniformLocationARB(tr.vertexLightingShader_DBS_entity.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.vertexLightingShader_DBS_entity.program);
	qglUniform1iARB(tr.vertexLightingShader_DBS_entity.u_DiffuseMap, 0);
	qglUniform1iARB(tr.vertexLightingShader_DBS_entity.u_NormalMap, 1);
	qglUniform1iARB(tr.vertexLightingShader_DBS_entity.u_SpecularMap, 2);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.vertexLightingShader_DBS_entity.program);
	GLSL_ShowProgramUniforms(tr.vertexLightingShader_DBS_entity.program);
	GL_CheckErrors();

	// simple vertex color shading for the world
	GLSL_InitGPUShader(&tr.vertexLightingShader_DBS_world,
					   "vertexLighting_DBS_world",
					   ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL | ATTR_COLOR | ATTR_LIGHTDIRECTION, qtrue);

	tr.vertexLightingShader_DBS_world.u_DiffuseMap =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_DiffuseMap");
	tr.vertexLightingShader_DBS_world.u_NormalMap =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_NormalMap");
	tr.vertexLightingShader_DBS_world.u_SpecularMap =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_SpecularMap");
	tr.vertexLightingShader_DBS_world.u_DiffuseTextureMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_DiffuseTextureMatrix");
	tr.vertexLightingShader_DBS_world.u_NormalTextureMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_NormalTextureMatrix");
	tr.vertexLightingShader_DBS_world.u_SpecularTextureMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_SpecularTextureMatrix");
	tr.vertexLightingShader_DBS_world.u_AlphaTest =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_AlphaTest");
	tr.vertexLightingShader_DBS_world.u_ColorGen =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_ColorGen");
	tr.vertexLightingShader_DBS_world.u_AlphaGen =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_AlphaGen");
	tr.vertexLightingShader_DBS_world.u_Color =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_Color");
	tr.vertexLightingShader_DBS_world.u_ViewOrigin =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_ViewOrigin");
	tr.vertexLightingShader_DBS_world.u_ParallaxMapping =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_ParallaxMapping");
	tr.vertexLightingShader_DBS_world.u_DepthScale =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_DepthScale");
	tr.vertexLightingShader_DBS_world.u_PortalClipping =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_PortalClipping");
	tr.vertexLightingShader_DBS_world.u_PortalPlane =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_PortalPlane");
	tr.vertexLightingShader_DBS_world.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.vertexLightingShader_DBS_world.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.vertexLightingShader_DBS_world.program);
	qglUniform1iARB(tr.vertexLightingShader_DBS_world.u_DiffuseMap, 0);
	qglUniform1iARB(tr.vertexLightingShader_DBS_world.u_NormalMap, 1);
	qglUniform1iARB(tr.vertexLightingShader_DBS_world.u_SpecularMap, 2);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.vertexLightingShader_DBS_world.program);
	GLSL_ShowProgramUniforms(tr.vertexLightingShader_DBS_world.program);
	GL_CheckErrors();

	// standard light mapping
	GLSL_InitGPUShader(&tr.lightMappingShader,
					   "lightMapping", ATTR_POSITION | ATTR_TEXCOORD | ATTR_LIGHTCOORD | ATTR_NORMAL, qtrue);

	tr.lightMappingShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.lightMappingShader.program, "u_ModelViewProjectionMatrix");

	tr.lightMappingShader.u_DiffuseMap = qglGetUniformLocationARB(tr.lightMappingShader.program, "u_DiffuseMap");
	tr.lightMappingShader.u_LightMap = qglGetUniformLocationARB(tr.lightMappingShader.program, "u_LightMap");
	tr.lightMappingShader.u_DiffuseTextureMatrix =
		qglGetUniformLocationARB(tr.lightMappingShader.program, "u_DiffuseTextureMatrix");
	tr.lightMappingShader.u_AlphaTest = qglGetUniformLocationARB(tr.lightMappingShader.program, "u_AlphaTest");

	qglUseProgramObjectARB(tr.lightMappingShader.program);
	qglUniform1iARB(tr.lightMappingShader.u_DiffuseMap, 0);
	qglUniform1iARB(tr.lightMappingShader.u_LightMap, 1);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.lightMappingShader.program);
	GLSL_ShowProgramUniforms(tr.lightMappingShader.program);
	GL_CheckErrors();

	// directional light mapping aka deluxe mapping
	GLSL_InitGPUShader(&tr.deluxeMappingShader,
					   "deluxeMapping",
					   ATTR_POSITION | ATTR_TEXCOORD | ATTR_LIGHTCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL, qtrue);

	tr.deluxeMappingShader.u_DiffuseMap = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_DiffuseMap");
	tr.deluxeMappingShader.u_NormalMap = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_NormalMap");
	tr.deluxeMappingShader.u_SpecularMap = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_SpecularMap");
	tr.deluxeMappingShader.u_LightMap = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_LightMap");
	tr.deluxeMappingShader.u_DeluxeMap = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_DeluxeMap");
	tr.deluxeMappingShader.u_DiffuseTextureMatrix =
		qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_DiffuseTextureMatrix");
	tr.deluxeMappingShader.u_NormalTextureMatrix =
		qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_NormalTextureMatrix");
	tr.deluxeMappingShader.u_SpecularTextureMatrix =
		qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_SpecularTextureMatrix");
	tr.deluxeMappingShader.u_AlphaTest = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_AlphaTest");
	tr.deluxeMappingShader.u_ViewOrigin = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_ViewOrigin");
	tr.deluxeMappingShader.u_ParallaxMapping = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_ParallaxMapping");
	tr.deluxeMappingShader.u_DepthScale = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_DepthScale");
	tr.deluxeMappingShader.u_PortalClipping = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_PortalClipping");
	tr.deluxeMappingShader.u_PortalPlane = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_PortalPlane");
	tr.deluxeMappingShader.u_ModelMatrix = qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_ModelMatrix");
	tr.deluxeMappingShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.deluxeMappingShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.deluxeMappingShader.program);
	qglUniform1iARB(tr.deluxeMappingShader.u_DiffuseMap, 0);
	qglUniform1iARB(tr.deluxeMappingShader.u_NormalMap, 1);
	qglUniform1iARB(tr.deluxeMappingShader.u_SpecularMap, 2);
	qglUniform1iARB(tr.deluxeMappingShader.u_LightMap, 3);
	qglUniform1iARB(tr.deluxeMappingShader.u_DeluxeMap, 4);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.deluxeMappingShader.program);
	GLSL_ShowProgramUniforms(tr.deluxeMappingShader.program);
	GL_CheckErrors();

	// geometric-buffer fill rendering with diffuse + bump + specular
	if(r_deferredShading->integer && glConfig.maxColorAttachments >= 4 && glConfig.textureFloatAvailable &&
	   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GLSL_InitGPUShader(&tr.geometricFillShader_DBS, "geometricFill_DBS",
						   ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL, qtrue);

		tr.geometricFillShader_DBS.u_DiffuseMap = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_DiffuseMap");
		tr.geometricFillShader_DBS.u_NormalMap = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_NormalMap");
		tr.geometricFillShader_DBS.u_SpecularMap = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_SpecularMap");
		tr.geometricFillShader_DBS.u_DiffuseTextureMatrix =
			qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_DiffuseTextureMatrix");
		tr.geometricFillShader_DBS.u_NormalTextureMatrix =
			qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_NormalTextureMatrix");
		tr.geometricFillShader_DBS.u_SpecularTextureMatrix =
			qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_SpecularTextureMatrix");
		tr.geometricFillShader_DBS.u_AlphaTest = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_AlphaTest");
		tr.geometricFillShader_DBS.u_ViewOrigin = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_ViewOrigin");
		tr.geometricFillShader_DBS.u_AmbientColor =
			qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_AmbientColor");
		tr.geometricFillShader_DBS.u_ParallaxMapping = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_ParallaxMapping");
		tr.geometricFillShader_DBS.u_DepthScale = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_DepthScale");
		tr.geometricFillShader_DBS.u_ModelMatrix = qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_ModelMatrix");
		tr.geometricFillShader_DBS.u_ModelViewMatrix =
			qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_ModelViewMatrix");
		tr.geometricFillShader_DBS.u_ModelViewProjectionMatrix =
			qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_ModelViewProjectionMatrix");
		if(glConfig.vboVertexSkinningAvailable)
		{
			tr.geometricFillShader_DBS.u_VertexSkinning =
				qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_VertexSkinning");
			tr.geometricFillShader_DBS.u_BoneMatrix =
				qglGetUniformLocationARB(tr.geometricFillShader_DBS.program, "u_BoneMatrix");
		}

		qglUseProgramObjectARB(tr.geometricFillShader_DBS.program);
		qglUniform1iARB(tr.geometricFillShader_DBS.u_DiffuseMap, 0);
		qglUniform1iARB(tr.geometricFillShader_DBS.u_NormalMap, 1);
		qglUniform1iARB(tr.geometricFillShader_DBS.u_SpecularMap, 2);
		qglUseProgramObjectARB(0);

		GLSL_ValidateProgram(tr.geometricFillShader_DBS.program);
		GLSL_ShowProgramUniforms(tr.geometricFillShader_DBS.program);
		GL_CheckErrors();

		// deferred omni-directional lighting post process effect
		GLSL_InitGPUShader(&tr.deferredLightingShader_DBS_omni, "deferredLighting_DBS_omni", ATTR_POSITION, qtrue);

		tr.deferredLightingShader_DBS_omni.u_DiffuseMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_DiffuseMap");
		tr.deferredLightingShader_DBS_omni.u_NormalMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_NormalMap");
		tr.deferredLightingShader_DBS_omni.u_SpecularMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_SpecularMap");
		tr.deferredLightingShader_DBS_omni.u_DepthMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_DepthMap");
		tr.deferredLightingShader_DBS_omni.u_AttenuationMapXY =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_AttenuationMapXY");
		tr.deferredLightingShader_DBS_omni.u_AttenuationMapZ =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_AttenuationMapZ");
		tr.deferredLightingShader_DBS_omni.u_ShadowMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_ShadowMap");
		tr.deferredLightingShader_DBS_omni.u_ViewOrigin =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_ViewOrigin");
		tr.deferredLightingShader_DBS_omni.u_LightOrigin =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_LightOrigin");
		tr.deferredLightingShader_DBS_omni.u_LightColor =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_LightColor");
		tr.deferredLightingShader_DBS_omni.u_LightRadius =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_LightRadius");
		tr.deferredLightingShader_DBS_omni.u_LightScale =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_LightScale");
		tr.deferredLightingShader_DBS_omni.u_LightAttenuationMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_LightAttenuationMatrix");
		tr.deferredLightingShader_DBS_omni.u_LightFrustum =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_LightFrustum");
		tr.deferredLightingShader_DBS_omni.u_ShadowCompare =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_ShadowCompare");
		tr.deferredLightingShader_DBS_omni.u_PortalClipping =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_PortalClipping");
		tr.deferredLightingShader_DBS_omni.u_PortalPlane =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_PortalPlane");
		tr.deferredLightingShader_DBS_omni.u_ModelViewProjectionMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_ModelViewProjectionMatrix");
		tr.deferredLightingShader_DBS_omni.u_UnprojectMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_omni.program, "u_UnprojectMatrix");

		qglUseProgramObjectARB(tr.deferredLightingShader_DBS_omni.program);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_DiffuseMap, 0);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_NormalMap, 1);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_SpecularMap, 2);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_DepthMap, 3);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_AttenuationMapXY, 4);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_AttenuationMapZ, 5);
		qglUniform1iARB(tr.deferredLightingShader_DBS_omni.u_ShadowMap, 6);
		qglUseProgramObjectARB(0);

		GLSL_ValidateProgram(tr.deferredLightingShader_DBS_omni.program);
		GLSL_ShowProgramUniforms(tr.deferredLightingShader_DBS_omni.program);
		GL_CheckErrors();

		// deferred projective lighting post process effect
		GLSL_InitGPUShader(&tr.deferredLightingShader_DBS_proj, "deferredLighting_DBS_proj", ATTR_POSITION, qtrue);

		tr.deferredLightingShader_DBS_proj.u_DiffuseMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_DiffuseMap");
		tr.deferredLightingShader_DBS_proj.u_NormalMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_NormalMap");
		tr.deferredLightingShader_DBS_proj.u_SpecularMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_SpecularMap");
		tr.deferredLightingShader_DBS_proj.u_DepthMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_DepthMap");
		tr.deferredLightingShader_DBS_proj.u_AttenuationMapXY =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_AttenuationMapXY");
		tr.deferredLightingShader_DBS_proj.u_AttenuationMapZ =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_AttenuationMapZ");
		tr.deferredLightingShader_DBS_proj.u_ShadowMap =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_ShadowMap");
		tr.deferredLightingShader_DBS_proj.u_ViewOrigin =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_ViewOrigin");
		tr.deferredLightingShader_DBS_proj.u_LightOrigin =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_LightOrigin");
		tr.deferredLightingShader_DBS_proj.u_LightColor =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_LightColor");
		tr.deferredLightingShader_DBS_proj.u_LightRadius =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_LightRadius");
		tr.deferredLightingShader_DBS_proj.u_LightScale =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_LightScale");
		tr.deferredLightingShader_DBS_proj.u_LightAttenuationMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_LightAttenuationMatrix");
		tr.deferredLightingShader_DBS_proj.u_LightFrustum =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_LightFrustum");
		tr.deferredLightingShader_DBS_proj.u_ShadowMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_ShadowMatrix");
		tr.deferredLightingShader_DBS_proj.u_ShadowCompare =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_ShadowCompare");
		tr.deferredLightingShader_DBS_proj.u_PortalClipping =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_PortalClipping");
		tr.deferredLightingShader_DBS_proj.u_PortalPlane =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_PortalPlane");
		tr.deferredLightingShader_DBS_proj.u_ModelViewProjectionMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_ModelViewProjectionMatrix");
		tr.deferredLightingShader_DBS_proj.u_UnprojectMatrix =
			qglGetUniformLocationARB(tr.deferredLightingShader_DBS_proj.program, "u_UnprojectMatrix");

		qglUseProgramObjectARB(tr.deferredLightingShader_DBS_proj.program);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_DiffuseMap, 0);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_NormalMap, 1);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_SpecularMap, 2);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_DepthMap, 3);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_AttenuationMapXY, 4);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_AttenuationMapZ, 5);
		qglUniform1iARB(tr.deferredLightingShader_DBS_proj.u_ShadowMap, 6);
		qglUseProgramObjectARB(0);

		GLSL_ValidateProgram(tr.deferredLightingShader_DBS_proj.program);
		GLSL_ShowProgramUniforms(tr.deferredLightingShader_DBS_proj.program);
		GL_CheckErrors();
	}

	// black depth fill rendering with textures
	GLSL_InitGPUShader(&tr.depthFillShader, "depthFill", ATTR_POSITION | ATTR_TEXCOORD | ATTR_COLOR, qtrue);

	tr.depthFillShader.u_ColorMap = qglGetUniformLocationARB(tr.depthFillShader.program, "u_ColorMap");
	tr.depthFillShader.u_ColorTextureMatrix = qglGetUniformLocationARB(tr.depthFillShader.program, "u_ColorTextureMatrix");
	tr.depthFillShader.u_AlphaTest = qglGetUniformLocationARB(tr.depthFillShader.program, "u_AlphaTest");
	tr.depthFillShader.u_AmbientColor = qglGetUniformLocationARB(tr.depthFillShader.program, "u_AmbientColor");
	tr.depthFillShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.depthFillShader.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.depthFillShader.u_VertexSkinning = qglGetUniformLocationARB(tr.depthFillShader.program, "u_VertexSkinning");
		tr.depthFillShader.u_BoneMatrix = qglGetUniformLocationARB(tr.depthFillShader.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.depthFillShader.program);
	qglUniform1iARB(tr.depthFillShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.depthFillShader.program);
	GLSL_ShowProgramUniforms(tr.depthFillShader.program);
	GL_CheckErrors();

	// colored depth test rendering for occlusion testing
	GLSL_InitGPUShader(&tr.depthTestShader, "depthTest", ATTR_POSITION | ATTR_TEXCOORD, qtrue);

	tr.depthTestShader.u_ColorMap = qglGetUniformLocationARB(tr.depthTestShader.program, "u_ColorMap");
	tr.depthTestShader.u_CurrentMap = qglGetUniformLocationARB(tr.depthTestShader.program, "u_CurrentMap");
	tr.depthTestShader.u_ColorTextureMatrix = qglGetUniformLocationARB(tr.depthTestShader.program, "u_ColorTextureMatrix");
	tr.depthTestShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.depthTestShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.depthTestShader.program);
	qglUniform1iARB(tr.depthTestShader.u_ColorMap, 0);
	qglUniform1iARB(tr.depthTestShader.u_CurrentMap, 1);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.depthTestShader.program);
	GLSL_ShowProgramUniforms(tr.depthTestShader.program);
	GL_CheckErrors();

	// depth to color encoding
	GLSL_InitGPUShader(&tr.depthToColorShader, "depthToColor", ATTR_POSITION, qtrue);

	tr.depthToColorShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.depthToColorShader.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.depthToColorShader.u_VertexSkinning = qglGetUniformLocationARB(tr.depthToColorShader.program, "u_VertexSkinning");
		tr.depthToColorShader.u_BoneMatrix = qglGetUniformLocationARB(tr.depthToColorShader.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.depthToColorShader.program);
	//qglUniform1iARB(tr.depthToColorShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.depthToColorShader.program);
	GLSL_ShowProgramUniforms(tr.depthToColorShader.program);
	GL_CheckErrors();

	// shadow volume extrusion
	GLSL_InitGPUShader(&tr.shadowExtrudeShader, "shadowExtrude", ATTR_POSITION, qtrue);

	tr.shadowExtrudeShader.u_LightOrigin = qglGetUniformLocationARB(tr.shadowExtrudeShader.program, "u_LightOrigin");
	tr.shadowExtrudeShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.shadowExtrudeShader.program, "u_ModelViewProjectionMatrix");

	GLSL_ValidateProgram(tr.shadowExtrudeShader.program);
	GLSL_ShowProgramUniforms(tr.shadowExtrudeShader.program);
	GL_CheckErrors();

	// shadowmap distance compression
	GLSL_InitGPUShader(&tr.shadowFillShader, "shadowFill", ATTR_POSITION | ATTR_TEXCOORD, qtrue);

	tr.shadowFillShader.u_ColorMap = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_ColorMap");
	tr.shadowFillShader.u_ColorTextureMatrix = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_ColorTextureMatrix");
	tr.shadowFillShader.u_AlphaTest = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_AlphaTest");
	tr.shadowFillShader.u_LightOrigin = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_LightOrigin");
	tr.shadowFillShader.u_LightRadius = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_LightRadius");
	tr.shadowFillShader.u_ModelMatrix = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_ModelMatrix");
	tr.shadowFillShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.shadowFillShader.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.shadowFillShader.u_VertexSkinning = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_VertexSkinning");
		tr.shadowFillShader.u_BoneMatrix = qglGetUniformLocationARB(tr.shadowFillShader.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.shadowFillShader.program);
	qglUniform1iARB(tr.shadowFillShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.shadowFillShader.program);
	GLSL_ShowProgramUniforms(tr.shadowFillShader.program);
	GL_CheckErrors();

	// omni-directional specular bump mapping ( Doom3 style )
	GLSL_InitGPUShader(&tr.forwardLightingShader_DBS_omni,
					   "forwardLighting_DBS_omni",
					   ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL | ATTR_COLOR, qtrue);

	tr.forwardLightingShader_DBS_omni.u_DiffuseMap =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_DiffuseMap");
	tr.forwardLightingShader_DBS_omni.u_NormalMap =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_NormalMap");
	tr.forwardLightingShader_DBS_omni.u_SpecularMap =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_SpecularMap");
	tr.forwardLightingShader_DBS_omni.u_AttenuationMapXY =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_AttenuationMapXY");
	tr.forwardLightingShader_DBS_omni.u_AttenuationMapZ =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_AttenuationMapZ");
	if(r_shadows->integer >= 4)
	{
		tr.forwardLightingShader_DBS_omni.u_ShadowMap =
			qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ShadowMap");
	}
	tr.forwardLightingShader_DBS_omni.u_DiffuseTextureMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_DiffuseTextureMatrix");
	tr.forwardLightingShader_DBS_omni.u_NormalTextureMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_NormalTextureMatrix");
	tr.forwardLightingShader_DBS_omni.u_SpecularTextureMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_SpecularTextureMatrix");
	tr.forwardLightingShader_DBS_omni.u_ViewOrigin =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ViewOrigin");
//	tr.forwardLightingShader_DBS_omni.u_InverseVertexColor =
//		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_InverseVertexColor");
	tr.forwardLightingShader_DBS_omni.u_LightOrigin =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_LightOrigin");
	tr.forwardLightingShader_DBS_omni.u_LightColor =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_LightColor");
	tr.forwardLightingShader_DBS_omni.u_LightRadius =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_LightRadius");
	tr.forwardLightingShader_DBS_omni.u_LightScale =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_LightScale");
	tr.forwardLightingShader_DBS_omni.u_LightAttenuationMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_LightAttenuationMatrix");
	tr.forwardLightingShader_DBS_omni.u_ShadowCompare =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ShadowCompare");
	tr.forwardLightingShader_DBS_omni.u_ShadowTexelSize =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ShadowTexelSize");
	tr.forwardLightingShader_DBS_omni.u_ShadowBlur =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ShadowBlur");
	tr.forwardLightingShader_DBS_omni.u_PortalClipping =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_PortalClipping");
	tr.forwardLightingShader_DBS_omni.u_PortalPlane =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_PortalPlane");
	tr.forwardLightingShader_DBS_omni.u_ModelMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ModelMatrix");
	tr.forwardLightingShader_DBS_omni.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.forwardLightingShader_DBS_omni.u_VertexSkinning =
			qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_VertexSkinning");
		tr.forwardLightingShader_DBS_omni.u_BoneMatrix =
			qglGetUniformLocationARB(tr.forwardLightingShader_DBS_omni.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.forwardLightingShader_DBS_omni.program);
	qglUniform1iARB(tr.forwardLightingShader_DBS_omni.u_DiffuseMap, 0);
	qglUniform1iARB(tr.forwardLightingShader_DBS_omni.u_NormalMap, 1);
	qglUniform1iARB(tr.forwardLightingShader_DBS_omni.u_SpecularMap, 2);
	qglUniform1iARB(tr.forwardLightingShader_DBS_omni.u_AttenuationMapXY, 3);
	qglUniform1iARB(tr.forwardLightingShader_DBS_omni.u_AttenuationMapZ, 4);
	if(r_shadows->integer >= 4)
	{
		qglUniform1iARB(tr.forwardLightingShader_DBS_omni.u_ShadowMap, 5);
	}
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.forwardLightingShader_DBS_omni.program);
	GLSL_ShowProgramUniforms(tr.forwardLightingShader_DBS_omni.program);
	GL_CheckErrors();

	// projective lighting ( Doom3 style )
	GLSL_InitGPUShader(&tr.forwardLightingShader_DBS_proj, "forwardLighting_DBS_proj",
					   ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL | ATTR_COLOR, qtrue);

	tr.forwardLightingShader_DBS_proj.u_DiffuseMap =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_DiffuseMap");
	tr.forwardLightingShader_DBS_proj.u_NormalMap =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_NormalMap");
	tr.forwardLightingShader_DBS_proj.u_SpecularMap =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_SpecularMap");
	tr.forwardLightingShader_DBS_proj.u_AttenuationMapXY =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_AttenuationMapXY");
	tr.forwardLightingShader_DBS_proj.u_AttenuationMapZ =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_AttenuationMapZ");
	if(r_shadows->integer >= 4)
	{
		tr.forwardLightingShader_DBS_proj.u_ShadowMap =
			qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ShadowMap");
	}
	tr.forwardLightingShader_DBS_proj.u_DiffuseTextureMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_DiffuseTextureMatrix");
	tr.forwardLightingShader_DBS_proj.u_NormalTextureMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_NormalTextureMatrix");
	tr.forwardLightingShader_DBS_proj.u_SpecularTextureMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_SpecularTextureMatrix");
	tr.forwardLightingShader_DBS_proj.u_ViewOrigin =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ViewOrigin");
//	tr.forwardLightingShader_DBS_proj.u_InverseVertexColor =
//		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_InverseVertexColor");
	tr.forwardLightingShader_DBS_proj.u_LightOrigin =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_LightOrigin");
	tr.forwardLightingShader_DBS_proj.u_LightColor =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_LightColor");
	tr.forwardLightingShader_DBS_proj.u_LightRadius =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_LightRadius");
	tr.forwardLightingShader_DBS_proj.u_LightScale =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_LightScale");
	tr.forwardLightingShader_DBS_proj.u_LightAttenuationMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_LightAttenuationMatrix");
	tr.forwardLightingShader_DBS_proj.u_ShadowMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ShadowMatrix");
	tr.forwardLightingShader_DBS_proj.u_ShadowCompare =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ShadowCompare");
	tr.forwardLightingShader_DBS_proj.u_ShadowTexelSize =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ShadowTexelSize");
	tr.forwardLightingShader_DBS_proj.u_ShadowBlur =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ShadowBlur");
	tr.forwardLightingShader_DBS_proj.u_PortalClipping =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_PortalClipping");
	tr.forwardLightingShader_DBS_proj.u_PortalPlane =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_PortalPlane");
	tr.forwardLightingShader_DBS_proj.u_ModelMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ModelMatrix");
	tr.forwardLightingShader_DBS_proj.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.forwardLightingShader_DBS_proj.u_VertexSkinning =
			qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_VertexSkinning");
		tr.forwardLightingShader_DBS_proj.u_BoneMatrix =
			qglGetUniformLocationARB(tr.forwardLightingShader_DBS_proj.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.forwardLightingShader_DBS_proj.program);
	qglUniform1iARB(tr.forwardLightingShader_DBS_proj.u_DiffuseMap, 0);
	qglUniform1iARB(tr.forwardLightingShader_DBS_proj.u_NormalMap, 1);
	qglUniform1iARB(tr.forwardLightingShader_DBS_proj.u_SpecularMap, 2);
	qglUniform1iARB(tr.forwardLightingShader_DBS_proj.u_AttenuationMapXY, 3);
	qglUniform1iARB(tr.forwardLightingShader_DBS_proj.u_AttenuationMapZ, 4);
	if(r_shadows->integer >= 4)
	{
		qglUniform1iARB(tr.forwardLightingShader_DBS_proj.u_ShadowMap, 5);
	}
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.forwardLightingShader_DBS_proj.program);
	GLSL_ShowProgramUniforms(tr.forwardLightingShader_DBS_proj.program);
	GL_CheckErrors();

#ifdef VOLUMETRIC_LIGHTING
	// volumetric lighting
	GLSL_InitGPUShader(&tr.lightVolumeShader_omni, "lightVolume_omni", ATTR_POSITION, qtrue);

	tr.lightVolumeShader_omni.u_AttenuationMapXY =
		qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_AttenuationMapXY");
	tr.lightVolumeShader_omni.u_AttenuationMapZ =
		qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_AttenuationMapZ");
	tr.lightVolumeShader_omni.u_ShadowMap = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_ShadowMap");
	tr.lightVolumeShader_omni.u_ViewOrigin = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_ViewOrigin");
	tr.lightVolumeShader_omni.u_LightOrigin = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_LightOrigin");
	tr.lightVolumeShader_omni.u_LightColor = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_LightColor");
	tr.lightVolumeShader_omni.u_LightRadius = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_LightRadius");
	tr.lightVolumeShader_omni.u_LightScale = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_LightScale");
	tr.lightVolumeShader_omni.u_LightAttenuationMatrix =
		qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_LightAttenuationMatrix");
	tr.lightVolumeShader_omni.u_ShadowCompare = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_ShadowCompare");
	tr.lightVolumeShader_omni.u_ModelMatrix = qglGetUniformLocationARB(tr.lightVolumeShader_omni.program, "u_ModelMatrix");
	tr.lightVolumeShader_omni.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.lightVolumeShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.lightVolumeShader_omni.program);
	qglUniform1iARB(tr.lightVolumeShader_omni.u_AttenuationMapXY, 0);
	qglUniform1iARB(tr.lightVolumeShader_omni.u_AttenuationMapZ, 1);
	qglUniform1iARB(tr.lightVolumeShader_omni.u_ShadowMap, 2);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.lightVolumeShader_omni.program);
	GLSL_ShowProgramUniforms(tr.lightVolumeShader_omni.program);
	GL_CheckErrors();
#endif

	// UT3 style player shadowing
	GLSL_InitGPUShader(&tr.deferredShadowingShader_proj, "deferredShadowing_proj", ATTR_POSITION, qtrue);

	tr.deferredShadowingShader_proj.u_DepthMap =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_DepthMap");
	tr.deferredShadowingShader_proj.u_AttenuationMapXY =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_AttenuationMapXY");
	tr.deferredShadowingShader_proj.u_AttenuationMapZ =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_AttenuationMapZ");
	tr.deferredShadowingShader_proj.u_ShadowMap =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_ShadowMap");
	tr.deferredShadowingShader_proj.u_LightOrigin =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_LightOrigin");
	tr.deferredShadowingShader_proj.u_LightColor =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_LightColor");
	tr.deferredShadowingShader_proj.u_LightRadius =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_LightRadius");
	tr.deferredShadowingShader_proj.u_LightAttenuationMatrix =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_LightAttenuationMatrix");
	tr.deferredShadowingShader_proj.u_LightFrustum =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_LightFrustum");
	tr.deferredShadowingShader_proj.u_ShadowMatrix =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_ShadowMatrix");
	tr.deferredShadowingShader_proj.u_ShadowCompare =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_ShadowCompare");
	tr.deferredShadowingShader_proj.u_PortalClipping =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_PortalClipping");
	tr.deferredShadowingShader_proj.u_PortalPlane =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_PortalPlane");
	tr.deferredShadowingShader_proj.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_ModelViewProjectionMatrix");
	tr.deferredShadowingShader_proj.u_UnprojectMatrix =
		qglGetUniformLocationARB(tr.deferredShadowingShader_proj.program, "u_UnprojectMatrix");

	qglUseProgramObjectARB(tr.deferredShadowingShader_proj.program);
	qglUniform1iARB(tr.deferredShadowingShader_proj.u_DepthMap, 0);
	qglUniform1iARB(tr.deferredShadowingShader_proj.u_AttenuationMapXY, 1);
	qglUniform1iARB(tr.deferredShadowingShader_proj.u_AttenuationMapZ, 2);
	qglUniform1iARB(tr.deferredShadowingShader_proj.u_ShadowMap, 3);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.deferredShadowingShader_proj.program);
	GLSL_ShowProgramUniforms(tr.deferredShadowingShader_proj.program);
	GL_CheckErrors();

	// cubemap reflection for abitrary polygons
	GLSL_InitGPUShader(&tr.reflectionShader_C, "reflection_C", ATTR_POSITION | ATTR_NORMAL, qtrue);

	tr.reflectionShader_C.u_ColorMap = qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_ColorMap");
	tr.reflectionShader_C.u_ViewOrigin = qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_ViewOrigin");
	tr.reflectionShader_C.u_ModelMatrix = qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_ModelMatrix");
	tr.reflectionShader_C.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.reflectionShader_C.u_VertexSkinning = qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_VertexSkinning");
		tr.reflectionShader_C.u_BoneMatrix = qglGetUniformLocationARB(tr.reflectionShader_C.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.reflectionShader_C.program);
	qglUniform1iARB(tr.reflectionShader_C.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.reflectionShader_C.program);
	GLSL_ShowProgramUniforms(tr.reflectionShader_C.program);
	GL_CheckErrors();

	// bumped cubemap reflection for abitrary polygons ( EMBM )
	GLSL_InitGPUShader(&tr.reflectionShader_CB,
					   "reflection_CB", ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL, qtrue);

	tr.reflectionShader_CB.u_ColorMap = qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_ColorMap");
	tr.reflectionShader_CB.u_NormalMap = qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_NormalMap");
	tr.reflectionShader_CB.u_NormalTextureMatrix =
		qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_NormalTextureMatrix");
	tr.reflectionShader_CB.u_ViewOrigin = qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_ViewOrigin");
	tr.reflectionShader_CB.u_ModelMatrix = qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_ModelMatrix");
	tr.reflectionShader_CB.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.reflectionShader_CB.u_VertexSkinning = qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_VertexSkinning");
		tr.reflectionShader_CB.u_BoneMatrix = qglGetUniformLocationARB(tr.reflectionShader_CB.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.reflectionShader_CB.program);
	qglUniform1iARB(tr.reflectionShader_CB.u_ColorMap, 0);
	qglUniform1iARB(tr.reflectionShader_CB.u_NormalMap, 1);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.reflectionShader_CB.program);
	GLSL_ShowProgramUniforms(tr.reflectionShader_CB.program);
	GL_CheckErrors();

	// cubemap refraction for abitrary polygons
	GLSL_InitGPUShader(&tr.refractionShader_C, "refraction_C", ATTR_POSITION | ATTR_NORMAL, qtrue);

	tr.refractionShader_C.u_ColorMap = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_ColorMap");
	tr.refractionShader_C.u_ViewOrigin = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_ViewOrigin");
	tr.refractionShader_C.u_RefractionIndex = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_RefractionIndex");
	tr.refractionShader_C.u_FresnelPower = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_FresnelPower");
	tr.refractionShader_C.u_FresnelScale = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_FresnelScale");
	tr.refractionShader_C.u_FresnelBias = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_FresnelBias");
	tr.refractionShader_C.u_ModelMatrix = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_ModelMatrix");
	tr.refractionShader_C.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.refractionShader_C.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.refractionShader_C.u_VertexSkinning = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_VertexSkinning");
		tr.refractionShader_C.u_BoneMatrix = qglGetUniformLocationARB(tr.refractionShader_C.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.refractionShader_C.program);
	qglUniform1iARB(tr.refractionShader_C.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.refractionShader_C.program);
	GLSL_ShowProgramUniforms(tr.refractionShader_C.program);
	GL_CheckErrors();

	// cubemap dispersion for abitrary polygons
	GLSL_InitGPUShader(&tr.dispersionShader_C, "dispersion_C", ATTR_POSITION | ATTR_NORMAL, qtrue);

	tr.dispersionShader_C.u_ColorMap = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_ColorMap");
	tr.dispersionShader_C.u_ViewOrigin = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_ViewOrigin");
	tr.dispersionShader_C.u_EtaRatio = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_EtaRatio");
	tr.dispersionShader_C.u_FresnelPower = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_FresnelPower");
	tr.dispersionShader_C.u_FresnelScale = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_FresnelScale");
	tr.dispersionShader_C.u_FresnelBias = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_FresnelBias");
	tr.dispersionShader_C.u_ModelMatrix = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_ModelMatrix");
	tr.dispersionShader_C.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_ModelViewProjectionMatrix");
	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.dispersionShader_C.u_VertexSkinning = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_VertexSkinning");
		tr.dispersionShader_C.u_BoneMatrix = qglGetUniformLocationARB(tr.dispersionShader_C.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.dispersionShader_C.program);
	qglUniform1iARB(tr.dispersionShader_C.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.dispersionShader_C.program);
	GLSL_ShowProgramUniforms(tr.dispersionShader_C.program);
	GL_CheckErrors();

	// skybox drawing for abitrary polygons
	GLSL_InitGPUShader(&tr.skyBoxShader, "skybox", ATTR_POSITION, qtrue);

	tr.skyBoxShader.u_ColorMap = qglGetUniformLocationARB(tr.skyBoxShader.program, "u_ColorMap");
	tr.skyBoxShader.u_ViewOrigin = qglGetUniformLocationARB(tr.skyBoxShader.program, "u_ViewOrigin");
	tr.skyBoxShader.u_ModelMatrix = qglGetUniformLocationARB(tr.skyBoxShader.program, "u_ModelMatrix");
	tr.skyBoxShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.skyBoxShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.skyBoxShader.program);
	qglUniform1iARB(tr.skyBoxShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.skyBoxShader.program);
	GLSL_ShowProgramUniforms(tr.skyBoxShader.program);
	GL_CheckErrors();

	// heatHaze post process effect
	GLSL_InitGPUShader(&tr.heatHazeShader, "heatHaze", ATTR_POSITION | ATTR_TEXCOORD, qtrue);

	tr.heatHazeShader.u_DeformMagnitude = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_DeformMagnitude");
	tr.heatHazeShader.u_NormalMap = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_NormalMap");
	tr.heatHazeShader.u_CurrentMap = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_CurrentMap");
	tr.heatHazeShader.u_NormalTextureMatrix = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_NormalTextureMatrix");
	if(r_heatHazeFix->integer && glConfig.framebufferBlitAvailable)
	{
		tr.heatHazeShader.u_ContrastMap = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_ContrastMap");
	}
	tr.heatHazeShader.u_AlphaTest = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_AlphaTest");

	tr.heatHazeShader.u_ModelViewMatrixTranspose =
		qglGetUniformLocationARB(tr.heatHazeShader.program, "u_ModelViewMatrixTranspose");
	tr.heatHazeShader.u_ProjectionMatrixTranspose =
		qglGetUniformLocationARB(tr.heatHazeShader.program, "u_ProjectionMatrixTranspose");
	tr.heatHazeShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.heatHazeShader.program, "u_ModelViewProjectionMatrix");

	if(glConfig.vboVertexSkinningAvailable)
	{
		tr.heatHazeShader.u_VertexSkinning = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_VertexSkinning");
		tr.heatHazeShader.u_BoneMatrix = qglGetUniformLocationARB(tr.heatHazeShader.program, "u_BoneMatrix");
	}

	qglUseProgramObjectARB(tr.heatHazeShader.program);
	qglUniform1iARB(tr.heatHazeShader.u_NormalMap, 0);
	qglUniform1iARB(tr.heatHazeShader.u_CurrentMap, 1);
	if(r_heatHazeFix->integer && glConfig.framebufferBlitAvailable)
	{
		qglUniform1iARB(tr.heatHazeShader.u_ContrastMap, 2);
	}
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.heatHazeShader.program);
	GLSL_ShowProgramUniforms(tr.heatHazeShader.program);
	GL_CheckErrors();

	// bloom post process effect
	GLSL_InitGPUShader(&tr.bloomShader, "bloom", ATTR_POSITION, qtrue);

	tr.bloomShader.u_ColorMap = qglGetUniformLocationARB(tr.bloomShader.program, "u_ColorMap");
	tr.bloomShader.u_ContrastMap = qglGetUniformLocationARB(tr.bloomShader.program, "u_ContrastMap");
	tr.bloomShader.u_BlurMagnitude = qglGetUniformLocationARB(tr.bloomShader.program, "u_BlurMagnitude");
	tr.bloomShader.u_ModelViewProjectionMatrix = qglGetUniformLocationARB(tr.bloomShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.bloomShader.program);
	qglUniform1iARB(tr.bloomShader.u_ColorMap, 0);
	qglUniform1iARB(tr.bloomShader.u_ContrastMap, 1);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.bloomShader.program);
	GLSL_ShowProgramUniforms(tr.bloomShader.program);
	GL_CheckErrors();

	// contrast post process effect
	GLSL_InitGPUShader(&tr.contrastShader, "contrast", ATTR_POSITION, qtrue);

	tr.contrastShader.u_ColorMap = qglGetUniformLocationARB(tr.contrastShader.program, "u_ColorMap");
	if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		tr.contrastShader.u_HDRKey = qglGetUniformLocationARB(tr.contrastShader.program, "u_HDRKey");
		tr.contrastShader.u_HDRAverageLuminance = qglGetUniformLocationARB(tr.contrastShader.program, "u_HDRAverageLuminance");
		tr.contrastShader.u_HDRMaxLuminance = qglGetUniformLocationARB(tr.contrastShader.program, "u_HDRMaxLuminance");
	}
	tr.contrastShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.contrastShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.contrastShader.program);
	qglUniform1iARB(tr.contrastShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.contrastShader.program);
	GLSL_ShowProgramUniforms(tr.contrastShader.program);
	GL_CheckErrors();

	// blurX post process effect
	GLSL_InitGPUShader(&tr.blurXShader, "blurX", ATTR_POSITION, qtrue);

	tr.blurXShader.u_ColorMap = qglGetUniformLocationARB(tr.blurXShader.program, "u_ColorMap");
	tr.blurXShader.u_ModelViewProjectionMatrix = qglGetUniformLocationARB(tr.blurXShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.blurXShader.program);
	qglUniform1iARB(tr.blurXShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.blurXShader.program);
	GLSL_ShowProgramUniforms(tr.blurXShader.program);
	GL_CheckErrors();

	// blurY post process effect
	GLSL_InitGPUShader(&tr.blurYShader, "blurY", ATTR_POSITION, qtrue);

	tr.blurYShader.u_ColorMap = qglGetUniformLocationARB(tr.blurYShader.program, "u_ColorMap");
	tr.blurYShader.u_ModelViewProjectionMatrix = qglGetUniformLocationARB(tr.blurYShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.blurYShader.program);
	qglUniform1iARB(tr.blurYShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.blurYShader.program);
	GLSL_ShowProgramUniforms(tr.blurYShader.program);
	GL_CheckErrors();

	// rotoscope post process effect
	GLSL_InitGPUShader(&tr.rotoscopeShader, "rotoscope", ATTR_POSITION | ATTR_TEXCOORD, qtrue);

	tr.rotoscopeShader.u_ColorMap = qglGetUniformLocationARB(tr.rotoscopeShader.program, "u_ColorMap");
	tr.rotoscopeShader.u_BlurMagnitude = qglGetUniformLocationARB(tr.rotoscopeShader.program, "u_BlurMagnitude");
	tr.rotoscopeShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.rotoscopeShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.rotoscopeShader.program);
	qglUniform1iARB(tr.rotoscopeShader.u_ColorMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.rotoscopeShader.program);
	GLSL_ShowProgramUniforms(tr.rotoscopeShader.program);
	GL_CheckErrors();

	// screen post process effect
	GLSL_InitGPUShader(&tr.screenShader, "screen", ATTR_POSITION | ATTR_COLOR, qtrue);

	tr.screenShader.u_CurrentMap = qglGetUniformLocationARB(tr.screenShader.program, "u_CurrentMap");
	tr.screenShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.screenShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.screenShader.program);
	qglUniform1iARB(tr.screenShader.u_CurrentMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.screenShader.program);
	GLSL_ShowProgramUniforms(tr.screenShader.program);
	GL_CheckErrors();

	// portal process effect
	GLSL_InitGPUShader(&tr.portalShader, "portal", ATTR_POSITION | ATTR_COLOR, qtrue);

	tr.portalShader.u_CurrentMap = qglGetUniformLocationARB(tr.portalShader.program, "u_CurrentMap");
	tr.portalShader.u_PortalRange = qglGetUniformLocationARB(tr.portalShader.program, "u_PortalRange");
	tr.portalShader.u_ModelViewMatrix = qglGetUniformLocationARB(tr.portalShader.program, "u_ModelViewMatrix");
	tr.portalShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.portalShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.portalShader.program);
	qglUniform1iARB(tr.portalShader.u_CurrentMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.portalShader.program);
	GLSL_ShowProgramUniforms(tr.portalShader.program);
	GL_CheckErrors();

	// liquid post process effect
	GLSL_InitGPUShader(&tr.liquidShader, "liquid",
			ATTR_POSITION | ATTR_TEXCOORD | ATTR_TANGENT | ATTR_BINORMAL | ATTR_NORMAL | ATTR_COLOR | ATTR_LIGHTDIRECTION, qtrue);

	tr.liquidShader.u_CurrentMap = qglGetUniformLocationARB(tr.liquidShader.program, "u_CurrentMap");
	tr.liquidShader.u_PortalMap = qglGetUniformLocationARB(tr.liquidShader.program, "u_PortalMap");
	tr.liquidShader.u_DepthMap = qglGetUniformLocationARB(tr.liquidShader.program, "u_DepthMap");
	tr.liquidShader.u_NormalMap = qglGetUniformLocationARB(tr.liquidShader.program, "u_NormalMap");
	tr.liquidShader.u_NormalTextureMatrix = qglGetUniformLocationARB(tr.liquidShader.program, "u_NormalTextureMatrix");
	tr.liquidShader.u_ViewOrigin = qglGetUniformLocationARB(tr.liquidShader.program, "u_ViewOrigin");
	tr.liquidShader.u_RefractionIndex = qglGetUniformLocationARB(tr.liquidShader.program, "u_RefractionIndex");
	tr.liquidShader.u_FresnelPower = qglGetUniformLocationARB(tr.liquidShader.program, "u_FresnelPower");
	tr.liquidShader.u_FresnelScale = qglGetUniformLocationARB(tr.liquidShader.program, "u_FresnelScale");
	tr.liquidShader.u_FresnelBias = qglGetUniformLocationARB(tr.liquidShader.program, "u_FresnelBias");
	tr.liquidShader.u_NormalScale = qglGetUniformLocationARB(tr.liquidShader.program, "u_NormalScale");
	tr.liquidShader.u_FogDensity = qglGetUniformLocationARB(tr.liquidShader.program, "u_FogDensity");
	tr.liquidShader.u_FogColor = qglGetUniformLocationARB(tr.liquidShader.program, "u_FogColor");
	tr.liquidShader.u_ModelMatrix = qglGetUniformLocationARB(tr.liquidShader.program, "u_ModelMatrix");
	tr.liquidShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.liquidShader.program, "u_ModelViewProjectionMatrix");
	tr.liquidShader.u_UnprojectMatrix = qglGetUniformLocationARB(tr.liquidShader.program, "u_UnprojectMatrix");

	qglUseProgramObjectARB(tr.liquidShader.program);
	qglUniform1iARB(tr.liquidShader.u_CurrentMap, 0);
	qglUniform1iARB(tr.liquidShader.u_PortalMap, 1);
	qglUniform1iARB(tr.liquidShader.u_DepthMap, 2);
	qglUniform1iARB(tr.liquidShader.u_NormalMap, 3);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.liquidShader.program);
	GLSL_ShowProgramUniforms(tr.liquidShader.program);
	GL_CheckErrors();

	// uniform fog post process effect
	GLSL_InitGPUShader(&tr.uniformFogShader, "uniformFog", ATTR_POSITION, qtrue);

	tr.uniformFogShader.u_DepthMap = qglGetUniformLocationARB(tr.uniformFogShader.program, "u_DepthMap");
	tr.uniformFogShader.u_ViewOrigin = qglGetUniformLocationARB(tr.uniformFogShader.program, "u_ViewOrigin");
	tr.uniformFogShader.u_FogDensity = qglGetUniformLocationARB(tr.uniformFogShader.program, "u_FogDensity");
	tr.uniformFogShader.u_FogColor = qglGetUniformLocationARB(tr.uniformFogShader.program, "u_FogColor");
	tr.uniformFogShader.u_UnprojectMatrix = qglGetUniformLocationARB(tr.uniformFogShader.program, "u_UnprojectMatrix");
	tr.uniformFogShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.uniformFogShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.uniformFogShader.program);
	qglUniform1iARB(tr.uniformFogShader.u_DepthMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.uniformFogShader.program);
	GLSL_ShowProgramUniforms(tr.uniformFogShader.program);
	GL_CheckErrors();

	// volumetric fog post process effect
	GLSL_InitGPUShader(&tr.volumetricFogShader, "volumetricFog", ATTR_POSITION, qtrue);

	tr.volumetricFogShader.u_DepthMap = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_DepthMap");
	tr.volumetricFogShader.u_DepthMapBack = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_DepthMapBack");
	tr.volumetricFogShader.u_DepthMapFront = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_DepthMapFront");
	tr.volumetricFogShader.u_ViewOrigin = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_ViewOrigin");
	tr.volumetricFogShader.u_FogDensity = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_FogDensity");
	tr.volumetricFogShader.u_FogColor = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_FogColor");
	tr.volumetricFogShader.u_UnprojectMatrix = qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_UnprojectMatrix");
	tr.volumetricFogShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.volumetricFogShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.volumetricFogShader.program);
	qglUniform1iARB(tr.volumetricFogShader.u_DepthMap, 0);
	qglUniform1iARB(tr.volumetricFogShader.u_DepthMapBack, 1);
	qglUniform1iARB(tr.volumetricFogShader.u_DepthMapFront, 2);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.volumetricFogShader.program);
	GLSL_ShowProgramUniforms(tr.volumetricFogShader.program);
	GL_CheckErrors();

	// screen space ambien occlusion post process effect
	GLSL_InitGPUShader(&tr.screenSpaceAmbientOcclusionShader, "screenSpaceAmbientOcclusion", ATTR_POSITION, qtrue);

	tr.screenSpaceAmbientOcclusionShader.u_CurrentMap =
		qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_CurrentMap");
	tr.screenSpaceAmbientOcclusionShader.u_DepthMap =
		qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_DepthMap");
	tr.screenSpaceAmbientOcclusionShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_ModelViewProjectionMatrix");
	//tr.screenSpaceAmbientOcclusionShader.u_ViewOrigin = qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_ViewOrigin");
	//tr.screenSpaceAmbientOcclusionShader.u_SSAOJitter = qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_SSAOJitter");
	//tr.screenSpaceAmbientOcclusionShader.u_SSAORadius = qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_SSAORadius");
	//tr.screenSpaceAmbientOcclusionShader.u_UnprojectMatrix = qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_UnprojectMatrix");
	//tr.screenSpaceAmbientOcclusionShader.u_ProjectMatrix = qglGetUniformLocationARB(tr.screenSpaceAmbientOcclusionShader.program, "u_ProjectMatrix");

	qglUseProgramObjectARB(tr.screenSpaceAmbientOcclusionShader.program);
	qglUniform1iARB(tr.screenSpaceAmbientOcclusionShader.u_CurrentMap, 0);
	qglUniform1iARB(tr.screenSpaceAmbientOcclusionShader.u_DepthMap, 1);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.screenSpaceAmbientOcclusionShader.program);
	GLSL_ShowProgramUniforms(tr.screenSpaceAmbientOcclusionShader.program);
	GL_CheckErrors();

	// depth of field post process effect
	GLSL_InitGPUShader(&tr.depthOfFieldShader, "depthOfField", ATTR_POSITION, qtrue);

	tr.depthOfFieldShader.u_CurrentMap = qglGetUniformLocationARB(tr.depthOfFieldShader.program, "u_CurrentMap");
	tr.depthOfFieldShader.u_DepthMap = qglGetUniformLocationARB(tr.depthOfFieldShader.program, "u_DepthMap");
	tr.depthOfFieldShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.depthOfFieldShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.depthOfFieldShader.program);
	qglUniform1iARB(tr.depthOfFieldShader.u_CurrentMap, 0);
	qglUniform1iARB(tr.depthOfFieldShader.u_DepthMap, 1);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.depthOfFieldShader.program);
	GLSL_ShowProgramUniforms(tr.depthOfFieldShader.program);
	GL_CheckErrors();

	// HDR tone mapping post process effect
	GLSL_InitGPUShader(&tr.toneMappingShader, "toneMapping", ATTR_POSITION, qtrue);

	tr.toneMappingShader.u_CurrentMap = qglGetUniformLocationARB(tr.toneMappingShader.program, "u_CurrentMap");
	tr.toneMappingShader.u_HDRKey = qglGetUniformLocationARB(tr.toneMappingShader.program, "u_HDRKey");
	tr.toneMappingShader.u_HDRAverageLuminance = qglGetUniformLocationARB(tr.toneMappingShader.program, "u_HDRAverageLuminance");
	tr.toneMappingShader.u_HDRMaxLuminance = qglGetUniformLocationARB(tr.toneMappingShader.program, "u_HDRMaxLuminance");
	tr.toneMappingShader.u_ModelViewProjectionMatrix =
		qglGetUniformLocationARB(tr.toneMappingShader.program, "u_ModelViewProjectionMatrix");

	qglUseProgramObjectARB(tr.toneMappingShader.program);
	qglUniform1iARB(tr.toneMappingShader.u_CurrentMap, 0);
	qglUseProgramObjectARB(0);

	GLSL_ValidateProgram(tr.toneMappingShader.program);
	GLSL_ShowProgramUniforms(tr.toneMappingShader.program);
	GL_CheckErrors();

	endTime = ri.Milliseconds();

	ri.Printf(PRINT_ALL, "GLSL shaders load time = %5.2f seconds\n", (endTime - startTime) / 1000.0);
}

void GLSL_ShutdownGPUShaders(void)
{
	ri.Printf(PRINT_ALL, "------- GLSL_ShutdownGPUShaders -------\n");

	if(tr.genericSingleShader.program)
	{
		qglDeleteObjectARB(tr.genericSingleShader.program);
		tr.genericSingleShader.program = 0;
	}

	if(tr.vertexLightingShader_DBS_entity.program)
	{
		qglDeleteObjectARB(tr.vertexLightingShader_DBS_entity.program);
		tr.vertexLightingShader_DBS_entity.program = 0;
	}

	if(tr.vertexLightingShader_DBS_world.program)
	{
		qglDeleteObjectARB(tr.vertexLightingShader_DBS_world.program);
		tr.vertexLightingShader_DBS_world.program = 0;
	}

	if(tr.lightMappingShader.program)
	{
		qglDeleteObjectARB(tr.lightMappingShader.program);
		tr.lightMappingShader.program = 0;
	}

	if(tr.deluxeMappingShader.program)
	{
		qglDeleteObjectARB(tr.deluxeMappingShader.program);
		tr.deluxeMappingShader.program = 0;
	}

	if(tr.geometricFillShader_DBS.program)
	{
		qglDeleteObjectARB(tr.geometricFillShader_DBS.program);
		tr.geometricFillShader_DBS.program = 0;
	}

	if(tr.deferredLightingShader_DBS_omni.program)
	{
		qglDeleteObjectARB(tr.deferredLightingShader_DBS_omni.program);
		tr.deferredLightingShader_DBS_omni.program = 0;
	}

	if(tr.deferredLightingShader_DBS_proj.program)
	{
		qglDeleteObjectARB(tr.deferredLightingShader_DBS_proj.program);
		tr.deferredLightingShader_DBS_proj.program = 0;
	}

	if(tr.depthFillShader.program)
	{
		qglDeleteObjectARB(tr.depthFillShader.program);
		tr.depthFillShader.program = 0;
	}

	if(tr.depthTestShader.program)
	{
		qglDeleteObjectARB(tr.depthTestShader.program);
		tr.depthTestShader.program = 0;
	}

	if(tr.depthToColorShader.program)
	{
		qglDeleteObjectARB(tr.depthToColorShader.program);
		tr.depthToColorShader.program = 0;
	}

	if(tr.shadowExtrudeShader.program)
	{
		qglDeleteObjectARB(tr.shadowExtrudeShader.program);
		tr.shadowExtrudeShader.program = 0;
	}

	if(tr.shadowFillShader.program)
	{
		qglDeleteObjectARB(tr.shadowFillShader.program);
		tr.shadowFillShader.program = 0;
	}

	if(tr.forwardLightingShader_DBS_omni.program)
	{
		qglDeleteObjectARB(tr.forwardLightingShader_DBS_omni.program);
		tr.forwardLightingShader_DBS_omni.program = 0;
	}

	if(tr.forwardLightingShader_DBS_proj.program)
	{
		qglDeleteObjectARB(tr.forwardLightingShader_DBS_proj.program);
		tr.forwardLightingShader_DBS_proj.program = 0;
	}

#ifdef VOLUMETRIC_LIGHTING
	if(tr.lightVolumeShader_omni.program)
	{
		qglDeleteObjectARB(tr.lightVolumeShader_omni.program);
		tr.lightVolumeShader_omni.program = 0;
	}
#endif

	if(tr.deferredShadowingShader_proj.program)
	{
		qglDeleteObjectARB(tr.deferredShadowingShader_proj.program);
		tr.deferredShadowingShader_proj.program = 0;
	}

	if(tr.reflectionShader_C.program)
	{
		qglDeleteObjectARB(tr.reflectionShader_C.program);
		tr.reflectionShader_C.program = 0;
	}

	if(tr.reflectionShader_CB.program)
	{
		qglDeleteObjectARB(tr.reflectionShader_CB.program);
		tr.reflectionShader_CB.program = 0;
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

	if(tr.rotoscopeShader.program)
	{
		qglDeleteObjectARB(tr.rotoscopeShader.program);
		tr.rotoscopeShader.program = 0;
	}

	if(tr.screenShader.program)
	{
		qglDeleteObjectARB(tr.screenShader.program);
		tr.screenShader.program = 0;
	}

	if(tr.portalShader.program)
	{
		qglDeleteObjectARB(tr.portalShader.program);
		tr.portalShader.program = 0;
	}

	if(tr.liquidShader.program)
	{
		qglDeleteObjectARB(tr.liquidShader.program);
		tr.liquidShader.program = 0;
	}

	if(tr.uniformFogShader.program)
	{
		qglDeleteObjectARB(tr.uniformFogShader.program);
		tr.uniformFogShader.program = 0;
	}

	if(tr.volumetricFogShader.program)
	{
		qglDeleteObjectARB(tr.volumetricFogShader.program);
		tr.volumetricFogShader.program = 0;
	}

	if(tr.screenSpaceAmbientOcclusionShader.program)
	{
		qglDeleteObjectARB(tr.screenSpaceAmbientOcclusionShader.program);
		tr.screenSpaceAmbientOcclusionShader.program = 0;
	}

	if(tr.depthOfFieldShader.program)
	{
		qglDeleteObjectARB(tr.depthOfFieldShader.program);
		tr.depthOfFieldShader.program = 0;
	}

	if(tr.toneMappingShader.program)
	{
		qglDeleteObjectARB(tr.toneMappingShader.program);
		tr.toneMappingShader.program = 0;
	}

	glState.currentProgram = 0;
	qglUseProgramObjectARB(0);
}




/*
==================
Tess_DrawElements
==================
*/
void Tess_DrawElements()
{
	if(tess.numIndexes == 0 || tess.numVertexes == 0)
	{
		return;
	}

	// move tess data through the GPU, finally
	if(glState.currentVBO && glState.currentIBO)
	{
		//qglDrawRangeElementsEXT(GL_TRIANGLES, 0, tessmesh->vertexes.size(), mesh->indexes.size(), GL_UNSIGNED_INT, VBO_BUFFER_OFFSET(mesh->vbo_indexes_ofs));

		//qglDrawElements(GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, BUFFER_OFFSET(glState.currentIBO->ofsIndexes));
		qglDrawElements(GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, BUFFER_OFFSET(0));

		backEnd.pc.c_vboVertexes += tess.numVertexes;
		backEnd.pc.c_vboIndexes += tess.numIndexes;
	}
	else
	{
		qglDrawElements(GL_TRIANGLES, tess.numIndexes, GL_INDEX_TYPE, tess.indexes);
	}

	// update performance counters
	backEnd.pc.c_drawElements++;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_vertexes += tess.numVertexes;
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t tess;





/*
=================
BindLightMap
=================
*/
static void BindLightMap()
{
	image_t        *lightmap;

	if(tess.lightmapNum >= 0 && (tess.lightmapNum / 2) < tr.lightmaps.currentElements)
	{
#if defined(COMPAT_Q3A)
		lightmap = tr.fatLightmap;
#else
		lightmap = Com_GrowListElement(&tr.lightmaps, tess.lightmapNum / 2);
#endif
	}
	else
	{
		lightmap = NULL;
	}

	if(!tr.lightmaps.currentElements || !lightmap)
	{
		GL_Bind(tr.whiteImage);
		return;
	}

	GL_Bind(lightmap);
}

/*
=================
BindDeluxeMap
=================
*/
static void BindDeluxeMap()
{
	image_t        *deluxemap;

	if(tess.lightmapNum >= 0 && (tess.lightmapNum / 2) < tr.deluxemaps.currentElements)
	{
		deluxemap = Com_GrowListElement(&tr.deluxemaps, tess.lightmapNum / 2);
	}
	else
	{
		deluxemap = NULL;
	}

	if(!tr.deluxemaps.currentElements || !deluxemap)
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
	GLimp_LogComment("--- DrawTris ---\n");

	GL_BindProgram(&tr.genericSingleShader);
	GL_State(GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE);
	GL_VertexAttribsState(tr.genericSingleShader.attribs);

	if(r_showBatches->integer || r_showLightBatches->integer)
	{
		GLSL_SetUniform_Color(&tr.genericSingleShader, g_color_table[backEnd.pc.c_batches % 8]);
	}
	else if(glState.currentVBO == tess.vbo)
	{
		GLSL_SetUniform_Color(&tr.genericSingleShader, colorRed);
	}
	else if(glState.currentVBO)
	{
		GLSL_SetUniform_Color(&tr.genericSingleShader, colorBlue);
	}
	else
	{
		GLSL_SetUniform_Color(&tr.genericSingleShader, colorWhite);
	}

	GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader,  qfalse);
	GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_CONST);
	GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_CONST);

	GLSL_SetUniform_ModelMatrix(&tr.genericSingleShader, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.genericSingleShader.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}
	GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, 0);

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(tr.whiteImage);
	GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, tess.svars.texMatrices[TB_COLORMAP]);

	qglDepthRange(0, 0);

	Tess_DrawElements();

	qglDepthRange(0, 1);
}





/*
==============
Tess_Begin

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a Tess_End due
to overflow.
==============
*/
// *INDENT-OFF*
void Tess_Begin(	 void (*stageIteratorFunc)(),
					 shader_t * surfaceShader, shader_t * lightShader,
					 qboolean skipTangentSpaces,
					 qboolean shadowVolume,
					 int lightmapNum)
{
	shader_t       *state = (surfaceShader->remappedShader) ? surfaceShader->remappedShader : surfaceShader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.surfaceShader = state;

	tess.surfaceStages = state->stages;
	tess.numSurfaceStages = state->numStages;

	tess.lightShader = lightShader;

	tess.stageIteratorFunc = stageIteratorFunc;
	tess.stageIteratorFunc2 = NULL;

	if(!tess.stageIteratorFunc)
	{
		tess.stageIteratorFunc = Tess_StageIteratorGeneric;
	}

	if(tess.stageIteratorFunc == Tess_StageIteratorGeneric)
	{
		if(state->isSky)
		{
			tess.stageIteratorFunc = Tess_StageIteratorSky;
			tess.stageIteratorFunc2 = Tess_StageIteratorGeneric;
		}
	}

	if(tess.stageIteratorFunc == Tess_StageIteratorGBuffer)
	{
		if(state->isSky)
		{
			tess.stageIteratorFunc = Tess_StageIteratorSky;
			tess.stageIteratorFunc2 = Tess_StageIteratorGBuffer;
		}
	}


	tess.skipTangentSpaces = skipTangentSpaces;
	tess.shadowVolume = shadowVolume;
	tess.lightmapNum = lightmapNum;

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va("--- Tess_Begin( %s, %s, %i, %i, %i ) ---\n", tess.surfaceShader->name, tess.lightShader ? tess.lightShader->name : NULL, tess.skipTangentSpaces, tess.shadowVolume, tess.lightmapNum));
	}
}
// *INDENT-ON*

static void Render_genericSingle(int stage)
{
	shaderStage_t  *pStage;
	unsigned int 	attribBits = ATTR_POSITION | ATTR_TEXCOORD;

	GLimp_LogComment("--- Render_genericSingle ---\n");

	pStage = tess.surfaceStages[stage];

	GL_State(pStage->stateBits);
	GL_BindProgram(&tr.genericSingleShader);

	// set uniforms
	GLSL_SetUniform_TCGen_Environment(&tr.genericSingleShader, pStage->tcGen_Environment);
	if(pStage->tcGen_Environment)
	{
		// calculate the environment texcoords in object space
		GLSL_SetUniform_ViewOrigin(&tr.genericSingleShader, backEnd.orientation.viewOrigin);

		attribBits |= ATTR_NORMAL;
	}

	// u_ColorGen
	switch (pStage->rgbGen)
	{
		case CGEN_VERTEX:
		case CGEN_ONE_MINUS_VERTEX:
			GLSL_SetUniform_ColorGen(&tr.genericSingleShader, pStage->rgbGen);
			attribBits |= ATTR_COLOR;
			break;

		default:
			GLSL_SetUniform_ColorGen(&tr.genericSingleShader, CGEN_CONST);
			break;
	}

	// u_AlphaGen
	switch (pStage->alphaGen)
	{
		case AGEN_VERTEX:
		case AGEN_ONE_MINUS_VERTEX:
			GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, pStage->alphaGen);
			attribBits |= ATTR_COLOR;
			break;

		default:
			GLSL_SetUniform_AlphaGen(&tr.genericSingleShader, AGEN_CONST);
			break;
	}

	// u_Color
	GLSL_SetUniform_Color(&tr.genericSingleShader, tess.svars.color);

	GLSL_SetUniform_ModelMatrix(&tr.genericSingleShader, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.genericSingleShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.genericSingleShader, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.genericSingleShader.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	GLSL_SetUniform_AlphaTest(&tr.genericSingleShader, pStage->stateBits);

	GLSL_SetUniform_PortalClipping(&tr.genericSingleShader, backEnd.viewParms.isPortal);
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];

		// clipping plane in world space
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		GLSL_SetUniform_PortalPlane(&tr.genericSingleShader, plane);
	}

	// bind u_ColorMap
	GL_SelectTexture(0);
	BindAnimatedImage(&pStage->bundle[TB_COLORMAP]);
	GLSL_SetUniform_ColorTextureMatrix(&tr.genericSingleShader, tess.svars.texMatrices[TB_COLORMAP]);

	GL_VertexAttribsState(attribBits);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_vertexLighting_DBS_entity(int stage)
{
	vec3_t          viewOrigin;
	vec3_t          ambientColor;
	vec3_t          lightDir;
	vec4_t          lightColor;
	unsigned int 	attribBits = ATTR_POSITION | ATTR_TEXCOORD | ATTR_NORMAL;

	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.vertexLightingShader_DBS_entity);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
	VectorCopy(backEnd.currentEntity->ambientLight, ambientColor);
	//ClampColor(ambientColor);
	VectorCopy(backEnd.currentEntity->directedLight, lightColor);
	//ClampColor(directedLight);
	VectorCopy(backEnd.currentEntity->lightDir, lightDir);

	GLSL_SetUniform_AmbientColor(&tr.vertexLightingShader_DBS_entity, ambientColor);
	GLSL_SetUniform_ViewOrigin(&tr.vertexLightingShader_DBS_entity, viewOrigin);
	GLSL_SetUniform_LightDir(&tr.vertexLightingShader_DBS_entity, lightDir);
	GLSL_SetUniform_LightColor(&tr.vertexLightingShader_DBS_entity, lightColor);

	GLSL_SetUniform_ModelMatrix(&tr.vertexLightingShader_DBS_entity, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.vertexLightingShader_DBS_entity, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.vertexLightingShader_DBS_entity, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
		{
			qglUniformMatrix4fvARB(tr.vertexLightingShader_DBS_entity.u_BoneMatrix, MAX_BONES, GL_FALSE,
								   &tess.boneMatrices[0][0]);

			attribBits |= (ATTR_BONE_INDEXES | ATTR_BONE_WEIGHTS);
		}
	}

	GLSL_SetUniform_AlphaTest(&tr.vertexLightingShader_DBS_entity, pStage->stateBits);

	if(r_parallaxMapping->integer)
	{
		float           depthScale;

		GLSL_SetUniform_ParallaxMapping(&tr.vertexLightingShader_DBS_entity, tess.surfaceShader->parallax);

		depthScale = RB_EvalExpression(&pStage->depthScaleExp, r_parallaxDepthScale->value);
		GLSL_SetUniform_DepthScale(&tr.vertexLightingShader_DBS_entity, depthScale);
	}

	GLSL_SetUniform_PortalClipping(&tr.vertexLightingShader_DBS_entity, backEnd.viewParms.isPortal);
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];

		// clipping plane in world space
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		GLSL_SetUniform_PortalPlane(&tr.vertexLightingShader_DBS_entity, plane);
	}

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.vertexLightingShader_DBS_entity, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	if(r_normalMapping->integer)
	{
		attribBits |= ATTR_TANGENT | ATTR_BINORMAL;

		// bind u_NormalMap
		GL_SelectTexture(1);
		if(pStage->bundle[TB_NORMALMAP].image[0])
		{
			GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.flatImage);
		}
		GLSL_SetUniform_NormalTextureMatrix(&tr.vertexLightingShader_DBS_entity, tess.svars.texMatrices[TB_NORMALMAP]);

		// bind u_SpecularMap
		GL_SelectTexture(2);
		if(pStage->bundle[TB_SPECULARMAP].image[0])
		{
			GL_Bind(pStage->bundle[TB_SPECULARMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.blackImage);
		}
		GLSL_SetUniform_SpecularTextureMatrix(&tr.vertexLightingShader_DBS_entity, tess.svars.texMatrices[TB_SPECULARMAP]);
	}

	GL_VertexAttribsState(attribBits);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_vertexLighting_DBS_world(int stage)
{
	vec3_t          viewOrigin;

	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.vertexLightingShader_DBS_world);
	GL_VertexAttribsState(tr.vertexLightingShader_DBS_world.attribs);

	// set uniforms
	VectorCopy(backEnd.orientation.viewOrigin, viewOrigin);

#if defined(COMPAT_Q3A)
	// u_ColorGen
	switch (pStage->rgbGen)
	{
		case CGEN_VERTEX:
		case CGEN_ONE_MINUS_VERTEX:
			GLSL_SetUniform_ColorGen(&tr.vertexLightingShader_DBS_world, pStage->rgbGen);
			break;

		default:
			GLSL_SetUniform_ColorGen(&tr.vertexLightingShader_DBS_world, CGEN_CONST);
			break;
	}

	// u_AlphaGen
	switch (pStage->alphaGen)
	{
		case AGEN_VERTEX:
		case AGEN_ONE_MINUS_VERTEX:
			GLSL_SetUniform_AlphaGen(&tr.vertexLightingShader_DBS_world, pStage->alphaGen);
			break;

		default:
			GLSL_SetUniform_AlphaGen(&tr.vertexLightingShader_DBS_world, AGEN_CONST);
			break;
	}

#else
	// u_ColorGen
	switch (pStage->rgbGen)
	{
		//case CGEN_IDENTITY_LIGHTING:
		//case CGEN_IDENTITY:
		//case CGEN_ENTITY:
		case CGEN_WAVEFORM:
		case CGEN_CONST:
		case CGEN_CUSTOM_RGB:
		case CGEN_CUSTOM_RGBs:
			GLSL_SetUniform_ColorGen(&tr.vertexLightingShader_DBS_world, CGEN_CONST);
			break;

		case CGEN_ONE_MINUS_VERTEX:
			GLSL_SetUniform_ColorGen(&tr.vertexLightingShader_DBS_world, CGEN_ONE_MINUS_VERTEX);
			break;

		default:
			GLSL_SetUniform_ColorGen(&tr.vertexLightingShader_DBS_world, CGEN_VERTEX);
			break;
	}

	// u_AlphaGen
	switch (pStage->alphaGen)
	{
		//case AGEN_IDENTITY:
		//case AGEN_ENTITY:
		//case AGEN_ONE_MINUS_ENTITY:
		case AGEN_WAVEFORM:
		case AGEN_CONST:
		case AGEN_CUSTOM:
			GLSL_SetUniform_AlphaGen(&tr.vertexLightingShader_DBS_world, AGEN_CONST);
			break;

		case AGEN_ONE_MINUS_VERTEX:
			GLSL_SetUniform_AlphaGen(&tr.vertexLightingShader_DBS_world, AGEN_ONE_MINUS_VERTEX);
			break;

		default:
			GLSL_SetUniform_AlphaGen(&tr.vertexLightingShader_DBS_world, AGEN_VERTEX);
			break;
	}
#endif

	// u_Color
	GLSL_SetUniform_Color(&tr.vertexLightingShader_DBS_world, tess.svars.color);

//	qglUniform1iARB(tr.vertexLightingShader_DBS_world.u_InverseVertexColor, pStage->inverseVertexColor);
	GLSL_SetUniform_ViewOrigin(&tr.vertexLightingShader_DBS_world, viewOrigin);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.vertexLightingShader_DBS_world, glState.modelViewProjectionMatrix[glState.stackIndex]);

	GLSL_SetUniform_AlphaTest(&tr.vertexLightingShader_DBS_world, pStage->stateBits);

	if(r_parallaxMapping->integer)
	{
		float           depthScale;

		GLSL_SetUniform_ParallaxMapping(&tr.vertexLightingShader_DBS_world, tess.surfaceShader->parallax);

		depthScale = RB_EvalExpression(&pStage->depthScaleExp, r_parallaxDepthScale->value);
		GLSL_SetUniform_DepthScale(&tr.vertexLightingShader_DBS_world, depthScale);
	}

	GLSL_SetUniform_PortalClipping(&tr.vertexLightingShader_DBS_world, backEnd.viewParms.isPortal);
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];

		// clipping plane in world space
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		GLSL_SetUniform_PortalPlane(&tr.vertexLightingShader_DBS_world, plane);
	}

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.vertexLightingShader_DBS_world, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	if(r_normalMapping->integer)
	{
		// bind u_NormalMap
		GL_SelectTexture(1);
		if(pStage->bundle[TB_NORMALMAP].image[0])
		{
			GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.flatImage);
		}
		GLSL_SetUniform_NormalTextureMatrix(&tr.vertexLightingShader_DBS_world, tess.svars.texMatrices[TB_NORMALMAP]);

		// bind u_SpecularMap
		GL_SelectTexture(2);
		if(pStage->bundle[TB_SPECULARMAP].image[0])
		{
			GL_Bind(pStage->bundle[TB_SPECULARMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.blackImage);
		}
		GLSL_SetUniform_SpecularTextureMatrix(&tr.vertexLightingShader_DBS_world, tess.svars.texMatrices[TB_SPECULARMAP]);
	}

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_lightMapping(int stage, qboolean asColorMap)
{
	shaderStage_t  *pStage;

	GLimp_LogComment("--- Render_lightMapping ---\n");

	pStage = tess.surfaceStages[stage];

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.lightMappingShader);
	GL_VertexAttribsState(tr.lightMappingShader.attribs);

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.lightMappingShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	GLSL_SetUniform_AlphaTest(&tr.lightMappingShader, pStage->stateBits);

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	if(asColorMap)
	{
		GL_Bind(tr.whiteImage);
	}
	else
	{
		GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	}
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.lightMappingShader, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	// bind u_LightMap
	GL_SelectTexture(1);
	BindLightMap();

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_deluxeMapping(int stage)
{
	vec3_t          viewOrigin;
	shaderStage_t  *pStage;

	GLimp_LogComment("--- Render_deluxeMapping ---\n");

	pStage = tess.surfaceStages[stage];

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.deluxeMappingShader);
	GL_VertexAttribsState(tr.deluxeMappingShader.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space

	GLSL_SetUniform_ViewOrigin(&tr.deluxeMappingShader, viewOrigin);

	GLSL_SetUniform_ModelMatrix(&tr.deluxeMappingShader, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.deluxeMappingShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	GLSL_SetUniform_AlphaTest(&tr.deluxeMappingShader, pStage->stateBits);

	if(r_parallaxMapping->integer)
	{
		float           depthScale;

		GLSL_SetUniform_ParallaxMapping(&tr.deluxeMappingShader, tess.surfaceShader->parallax);

		depthScale = RB_EvalExpression(&pStage->depthScaleExp, r_parallaxDepthScale->value);
		GLSL_SetUniform_DepthScale(&tr.deluxeMappingShader, depthScale);
	}

	GLSL_SetUniform_PortalClipping(&tr.deluxeMappingShader, backEnd.viewParms.isPortal);
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];

		// clipping plane in world space
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		GLSL_SetUniform_PortalPlane(&tr.deluxeMappingShader, plane);
	}

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.deluxeMappingShader, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	// bind u_NormalMap
	GL_SelectTexture(1);
	if(pStage->bundle[TB_NORMALMAP].image[0])
	{
		GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
	}
	else
	{
		GL_Bind(tr.flatImage);
	}
	GLSL_SetUniform_NormalTextureMatrix(&tr.deluxeMappingShader, tess.svars.texMatrices[TB_NORMALMAP]);

	// bind u_SpecularMap
	GL_SelectTexture(2);
	if(pStage->bundle[TB_SPECULARMAP].image[0])
	{
		GL_Bind(pStage->bundle[TB_SPECULARMAP].image[0]);
	}
	else
	{
		GL_Bind(tr.blackImage);
	}
	GLSL_SetUniform_SpecularTextureMatrix(&tr.deluxeMappingShader, tess.svars.texMatrices[TB_SPECULARMAP]);

	// bind u_LightMap
	GL_SelectTexture(3);
	BindLightMap();

	// bind u_DeluxeMap
	GL_SelectTexture(4);
	BindDeluxeMap();

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_geometricFill_DBS(int stage, qboolean cmap2black)
{
	shaderStage_t  *pStage;
	unsigned        stateBits;
	vec3_t          viewOrigin;
	vec4_t          ambientColor;

	GLimp_LogComment("--- Render_geometricFill_DBS ---\n");

	pStage = tess.surfaceStages[stage];

	// remove blend mode
	stateBits = pStage->stateBits;
	stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_ATEST_BITS);

	GL_State(stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.geometricFillShader_DBS);
	GL_VertexAttribsState(tr.geometricFillShader_DBS.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space

	if(r_precomputedLighting->integer)
	{
		VectorCopy(backEnd.currentEntity->ambientLight, ambientColor);
		ClampColor(ambientColor);
	}
	else if(r_forceAmbient->integer)
	{
		ambientColor[0] = r_forceAmbient->value;
		ambientColor[1] = r_forceAmbient->value;
		ambientColor[2] = r_forceAmbient->value;
	}
	else
	{
		VectorClear(ambientColor);
	}

	GLSL_SetUniform_AlphaTest(&tr.geometricFillShader_DBS, pStage->stateBits);
	GLSL_SetUniform_ViewOrigin(&tr.geometricFillShader_DBS, viewOrigin);
	GLSL_SetUniform_AmbientColor(&tr.geometricFillShader_DBS, ambientColor);

	GLSL_SetUniform_ModelMatrix(&tr.geometricFillShader_DBS, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewMatrix(&tr.geometricFillShader_DBS, glState.modelViewMatrix[glState.stackIndex]);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.geometricFillShader_DBS, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.geometricFillShader_DBS, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.geometricFillShader_DBS.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	if(r_parallaxMapping->integer)
	{
		float           depthScale;

		GLSL_SetUniform_ParallaxMapping(&tr.geometricFillShader_DBS, tess.surfaceShader->parallax);

		depthScale = RB_EvalExpression(&pStage->depthScaleExp, r_parallaxDepthScale->value);
		GLSL_SetUniform_DepthScale(&tr.geometricFillShader_DBS, depthScale);
	}

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	if(cmap2black)
	{
		GL_Bind(tr.blackImage);
	}
	else
	{
		GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	}
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.geometricFillShader_DBS, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	if(r_normalMapping->integer)
	{
		// bind u_NormalMap
		GL_SelectTexture(1);
		if(pStage->bundle[TB_NORMALMAP].image[0])
		{
			GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.flatImage);
		}
		GLSL_SetUniform_NormalTextureMatrix(&tr.geometricFillShader_DBS, tess.svars.texMatrices[TB_NORMALMAP]);

		// bind u_SpecularMap
		GL_SelectTexture(2);
		if(r_forceSpecular->integer)
		{
			GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
		}
		else if(pStage->bundle[TB_SPECULARMAP].image[0])
		{
			GL_Bind(pStage->bundle[TB_SPECULARMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.blackImage);
		}
		GLSL_SetUniform_SpecularTextureMatrix(&tr.geometricFillShader_DBS, tess.svars.texMatrices[TB_SPECULARMAP]);
	}

	Tess_DrawElements();

	GL_CheckErrors();
}


static void Render_depthFill(int stage, qboolean cmap2black)
{
	shaderStage_t  *pStage;
	unsigned        stateBits;
	vec4_t          ambientColor;

	GLimp_LogComment("--- Render_depthFill ---\n");

	pStage = tess.surfaceStages[stage];

	// remove alpha test
	stateBits = pStage->stateBits;
	stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_ATEST_BITS);

	GL_State(stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.depthFillShader);
	GL_VertexAttribsState(tr.depthFillShader.attribs);

	if(r_precomputedLighting->integer)
	{
		GL_VertexAttribsState(tr.depthFillShader.attribs);
	}
	else
	{
		GL_VertexAttribsState(tr.depthFillShader.attribs & ~(ATTR_COLOR));
		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, tess.svars.color);
	}

	// set uniforms
	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.depthFillShader, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.depthFillShader.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	GLSL_SetUniform_AlphaTest(&tr.depthFillShader, pStage->stateBits);


	if(r_precomputedLighting->integer)
	{
		VectorCopy(backEnd.currentEntity->ambientLight, ambientColor);
		ClampColor(ambientColor);
	}
	else if(r_forceAmbient->integer)
	{
		ambientColor[0] = r_forceAmbient->value;
		ambientColor[1] = r_forceAmbient->value;
		ambientColor[2] = r_forceAmbient->value;
	}
	else
	{
		VectorClear(ambientColor);
	}
	GLSL_SetUniform_AmbientColor(&tr.depthFillShader, ambientColor);

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.depthFillShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// bind u_ColorMap
	GL_SelectTexture(0);
	if(cmap2black)
	{
		GL_Bind(tr.blackImage);
	}
	else
	{
		GL_Bind(pStage->bundle[TB_DIFFUSEMAP].image[0]);
	}
	GLSL_SetUniform_ColorTextureMatrix(&tr.depthFillShader, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_shadowFill(int stage)
{
	shaderStage_t  *pStage;
	unsigned        stateBits;
	vec3_t          lightOrigin;


	GLimp_LogComment("--- Render_shadowFill ---\n");

	pStage = tess.surfaceStages[stage];

	// remove alpha test
	stateBits = pStage->stateBits;
	stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_ATEST_BITS);

	GL_State(stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.shadowFillShader);
	GL_VertexAttribsState(tr.shadowFillShader.attribs & ~(ATTR_COLOR));

	if(r_debugShadowMaps->integer)
	{
		vec4_t          shadowMapColor;

		VectorCopy4(g_color_table[backEnd.pc.c_batches % 8], shadowMapColor);

		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, shadowMapColor);
	}

	// set uniforms
	GLSL_SetUniform_AlphaTest(&tr.shadowFillShader, pStage->stateBits);

	VectorCopy(backEnd.currentLight->origin, lightOrigin);	// in world space

	GLSL_SetUniform_LightRadius(&tr.shadowFillShader, backEnd.currentLight->sphereRadius);
	GLSL_SetUniform_LightOrigin(&tr.shadowFillShader, lightOrigin);

	GLSL_SetUniform_ModelMatrix(&tr.shadowFillShader, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.shadowFillShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.shadowFillShader, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.shadowFillShader.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	// bind u_ColorMap
	GL_SelectTexture(0);

	if(pStage->stateBits & GLS_ATEST_BITS)
	{
		GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
		GLSL_SetUniform_ColorTextureMatrix(&tr.shadowFillShader, tess.svars.texMatrices[TB_COLORMAP]);
	}
	else
	{
		GL_Bind(tr.whiteImage);
	}

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_forwardLighting_DBS_omni(shaderStage_t * diffuseStage,
											shaderStage_t * attenuationXYStage,
											shaderStage_t * attenuationZStage, trRefLight_t * light)
{
	vec3_t          viewOrigin;
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	float           shadowTexelSize;
	qboolean        shadowCompare;

	GLimp_LogComment("--- Render_forwardLighting_DBS_omni ---\n");

	// enable shader, set arrays
	GL_BindProgram(&tr.forwardLightingShader_DBS_omni);

	/*
	if(diffuseStage->vertexColor || diffuseStage->inverseVertexColor)
	{
		GL_VertexAttribsState(tr.forwardLightingShader_DBS_omni.attribs);
	}
	else
	*/
	{
		GL_VertexAttribsState(tr.forwardLightingShader_DBS_omni.attribs & ~(ATTR_COLOR));
		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);
	}

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);
	VectorCopy(light->origin, lightOrigin);
	VectorCopy(tess.svars.color, lightColor);

	shadowCompare = r_shadows->integer >= 4 && !light->l.noShadows && light->shadowLOD >= 0;

	if(shadowCompare)
		shadowTexelSize = 1.0f / shadowMapResolutions[light->shadowLOD];
	else
		shadowTexelSize = 1.0f;

	GLSL_SetUniform_ViewOrigin(&tr.forwardLightingShader_DBS_omni, viewOrigin);
//	GLSL_SetUniform_InverseVertexColor(&tr.forwardLightingShader_DBS_omni, diffuseStage->inverseVertexColor);
	GLSL_SetUniform_LightOrigin(&tr.forwardLightingShader_DBS_omni, lightOrigin);
	GLSL_SetUniform_LightColor(&tr.forwardLightingShader_DBS_omni, lightColor);
	GLSL_SetUniform_LightRadius(&tr.forwardLightingShader_DBS_omni, light->sphereRadius);
	GLSL_SetUniform_LightScale(&tr.forwardLightingShader_DBS_omni, light->l.scale);
	GLSL_SetUniform_LightAttenuationMatrix(&tr.forwardLightingShader_DBS_omni, light->attenuationMatrix2);

	GLSL_SetUniform_ShadowCompare(&tr.forwardLightingShader_DBS_omni, shadowCompare);
	if(shadowCompare)
	{
		GLSL_SetUniform_ShadowTexelSize(&tr.forwardLightingShader_DBS_omni, shadowTexelSize);
		GLSL_SetUniform_ShadowBlur(&tr.forwardLightingShader_DBS_omni, r_shadowBlur->value);
	}

	GLSL_SetUniform_ModelMatrix(&tr.forwardLightingShader_DBS_omni, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.forwardLightingShader_DBS_omni, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.forwardLightingShader_DBS_omni, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.forwardLightingShader_DBS_omni.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	GLSL_SetUniform_PortalClipping(&tr.forwardLightingShader_DBS_omni, backEnd.viewParms.isPortal);
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];

		// clipping plane in world space
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		GLSL_SetUniform_PortalPlane(&tr.forwardLightingShader_DBS_omni, plane);
	}

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.forwardLightingShader_DBS_omni, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	if(r_normalMapping->integer)
	{
		// bind u_NormalMap
		GL_SelectTexture(1);
		if(diffuseStage->bundle[TB_NORMALMAP].image[0])
		{
			GL_Bind(diffuseStage->bundle[TB_NORMALMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.flatImage);
		}
		GLSL_SetUniform_NormalTextureMatrix(&tr.forwardLightingShader_DBS_omni, tess.svars.texMatrices[TB_NORMALMAP]);

		// bind u_SpecularMap
		GL_SelectTexture(2);
		if(r_forceSpecular->integer)
		{
			GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
		}
		else if(diffuseStage->bundle[TB_SPECULARMAP].image[0])
		{
			GL_Bind(diffuseStage->bundle[TB_SPECULARMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.blackImage);
		}
		GLSL_SetUniform_SpecularTextureMatrix(&tr.forwardLightingShader_DBS_omni, tess.svars.texMatrices[TB_SPECULARMAP]);
	}

	// bind u_AttenuationMapXY
	GL_SelectTexture(3);
	BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

	// bind u_AttenuationMapZ
	GL_SelectTexture(4);
	BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

	// bind u_ShadowMap
	if(shadowCompare)
	{
		GL_SelectTexture(5);
		GL_Bind(tr.shadowCubeFBOImage[light->shadowLOD]);
	}

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_forwardLighting_DBS_proj(shaderStage_t * diffuseStage,
											shaderStage_t * attenuationXYStage,
											shaderStage_t * attenuationZStage, trRefLight_t * light)
{
	vec3_t          viewOrigin;
	vec3_t          lightOrigin;
	vec4_t          lightColor;
	float           shadowTexelSize;
	qboolean        shadowCompare;

	GLimp_LogComment("--- Render_fowardLighting_DBS_proj ---\n");

	// enable shader, set arrays
	GL_BindProgram(&tr.forwardLightingShader_DBS_proj);

	/*
	if(diffuseStage->vertexColor || diffuseStage->inverseVertexColor)
	{
		GL_VertexAttribsState(tr.forwardLightingShader_DBS_proj.attribs);
	}
	else
	*/
	{
		GL_VertexAttribsState(tr.forwardLightingShader_DBS_proj.attribs & ~(ATTR_COLOR));
		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);
	}

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);
	VectorCopy(light->origin, lightOrigin);
	VectorCopy(tess.svars.color, lightColor);

	shadowCompare = r_shadows->integer >= 4 && !light->l.noShadows && light->shadowLOD >= 0;

	if(shadowCompare)
		shadowTexelSize = 1.0f / shadowMapResolutions[light->shadowLOD];
	else
		shadowTexelSize = 1.0f;

	GLSL_SetUniform_ViewOrigin(&tr.forwardLightingShader_DBS_proj, viewOrigin);
//	GLSL_SetUniform_InverseVertexColor(&tr.forwardLightingShader_DBS_proj, diffuseStage->inverseVertexColor);
	GLSL_SetUniform_LightOrigin(&tr.forwardLightingShader_DBS_proj, lightOrigin);
	GLSL_SetUniform_LightColor(&tr.forwardLightingShader_DBS_proj, lightColor);
	GLSL_SetUniform_LightRadius(&tr.forwardLightingShader_DBS_proj, light->sphereRadius);
	GLSL_SetUniform_LightScale(&tr.forwardLightingShader_DBS_proj, light->l.scale);
	GLSL_SetUniform_LightAttenuationMatrix(&tr.forwardLightingShader_DBS_proj, light->attenuationMatrix2);

	GLSL_SetUniform_ShadowCompare(&tr.forwardLightingShader_DBS_proj, shadowCompare);
	if(shadowCompare)
	{
		GLSL_SetUniform_ShadowMatrix(&tr.forwardLightingShader_DBS_proj, light->attenuationMatrix);
		GLSL_SetUniform_ShadowTexelSize(&tr.forwardLightingShader_DBS_proj, shadowTexelSize);
		GLSL_SetUniform_ShadowBlur(&tr.forwardLightingShader_DBS_proj, r_shadowBlur->value);
	}

	GLSL_SetUniform_ModelMatrix(&tr.forwardLightingShader_DBS_proj, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.forwardLightingShader_DBS_proj, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.forwardLightingShader_DBS_proj, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.forwardLightingShader_DBS_proj.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	GLSL_SetUniform_PortalClipping(&tr.forwardLightingShader_DBS_proj, backEnd.viewParms.isPortal);
	if(backEnd.viewParms.isPortal)
	{
		float           plane[4];

		// clipping plane in world space
		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		GLSL_SetUniform_PortalPlane(&tr.forwardLightingShader_DBS_proj, plane);
	}

	// bind u_DiffuseMap
	GL_SelectTexture(0);
	GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
	GLSL_SetUniform_DiffuseTextureMatrix(&tr.forwardLightingShader_DBS_proj, tess.svars.texMatrices[TB_DIFFUSEMAP]);

	if(r_normalMapping->integer)
	{
		// bind u_NormalMap
		GL_SelectTexture(1);
		if(diffuseStage->bundle[TB_NORMALMAP].image[0])
		{
			GL_Bind(diffuseStage->bundle[TB_NORMALMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.flatImage);
		}
		GLSL_SetUniform_NormalTextureMatrix(&tr.forwardLightingShader_DBS_proj, tess.svars.texMatrices[TB_NORMALMAP]);

		// bind u_SpecularMap
		GL_SelectTexture(2);
		if(r_forceSpecular->integer)
		{
			GL_Bind(diffuseStage->bundle[TB_DIFFUSEMAP].image[0]);
		}
		else if(diffuseStage->bundle[TB_SPECULARMAP].image[0])
		{
			GL_Bind(diffuseStage->bundle[TB_SPECULARMAP].image[0]);
		}
		else
		{
			GL_Bind(tr.blackImage);
		}
		GLSL_SetUniform_SpecularTextureMatrix(&tr.forwardLightingShader_DBS_proj, tess.svars.texMatrices[TB_SPECULARMAP]);
	}

	// bind u_AttenuationMapXY
	GL_SelectTexture(3);
	BindAnimatedImage(&attenuationXYStage->bundle[TB_COLORMAP]);

	// bind u_AttenuationMapZ
	GL_SelectTexture(4);
	BindAnimatedImage(&attenuationZStage->bundle[TB_COLORMAP]);

	// bind u_ShadowMap
	if(shadowCompare)
	{
		GL_SelectTexture(5);
		GL_Bind(tr.shadowMapFBOImage[light->shadowLOD]);
	}

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_reflection_C(int stage)
{
	vec3_t          viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_reflection_C ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.reflectionShader_C);
	GL_VertexAttribsState(tr.reflectionShader_C.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
	GLSL_SetUniform_ViewOrigin(&tr.reflectionShader_C, viewOrigin);

	GLSL_SetUniform_ModelMatrix(&tr.reflectionShader_C, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.reflectionShader_C, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.reflectionShader_C, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.reflectionShader_C.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_reflection_CB(int stage)
{
	vec3_t          viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_reflection_CB ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.reflectionShader_CB);
	GL_VertexAttribsState(tr.reflectionShader_CB.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
	GLSL_SetUniform_ViewOrigin(&tr.reflectionShader_CB, viewOrigin);

	GLSL_SetUniform_ModelMatrix(&tr.reflectionShader_CB, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.reflectionShader_CB, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.reflectionShader_CB, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.reflectionShader_CB.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	// bind u_ColorMap
	GL_SelectTexture(0);
#if 0
	if(backEnd.currentEntity && (backEnd.currentEntity != &tr.worldEntity))
	{
		GL_BindNearestCubeMap(backEnd.currentEntity->e.origin);
	}
	else
	{
		GL_BindNearestCubeMap(viewOrigin);
	}
#else
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
#endif

	// bind u_NormalMap
	GL_SelectTexture(1);
	GL_Bind(pStage->bundle[TB_NORMALMAP].image[0]);
	GLSL_SetUniform_NormalTextureMatrix(&tr.reflectionShader_CB, tess.svars.texMatrices[TB_NORMALMAP]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_refraction_C(int stage)
{
	vec3_t          viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_refraction_C ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.refractionShader_C);
	GL_VertexAttribsState(tr.refractionShader_C.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
	GLSL_SetUniform_ViewOrigin(&tr.refractionShader_C, viewOrigin);
	GLSL_SetUniform_RefractionIndex(&tr.refractionShader_C, RB_EvalExpression(&pStage->refractionIndexExp, 1.0));
	qglUniform1fARB(tr.refractionShader_C.u_FresnelPower, RB_EvalExpression(&pStage->fresnelPowerExp, 2.0));
	qglUniform1fARB(tr.refractionShader_C.u_FresnelScale, RB_EvalExpression(&pStage->fresnelScaleExp, 2.0));
	qglUniform1fARB(tr.refractionShader_C.u_FresnelBias, RB_EvalExpression(&pStage->fresnelBiasExp, 1.0));

	GLSL_SetUniform_ModelMatrix(&tr.refractionShader_C, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.refractionShader_C, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.refractionShader_C, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.refractionShader_C.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_dispersion_C(int stage)
{
	vec3_t          viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];
	float           eta;
	float           etaDelta;

	GLimp_LogComment("--- Render_dispersion_C ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.dispersionShader_C);
	GL_VertexAttribsState(tr.dispersionShader_C.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space
	eta = RB_EvalExpression(&pStage->etaExp, (float)1.1);
	etaDelta = RB_EvalExpression(&pStage->etaDeltaExp, (float)-0.02);

	GLSL_SetUniform_ViewOrigin(&tr.dispersionShader_C, viewOrigin);
	qglUniform3fARB(tr.dispersionShader_C.u_EtaRatio, eta, eta + etaDelta, eta + (etaDelta * 2));
	qglUniform1fARB(tr.dispersionShader_C.u_FresnelPower, RB_EvalExpression(&pStage->fresnelPowerExp, 2.0f));
	qglUniform1fARB(tr.dispersionShader_C.u_FresnelScale, RB_EvalExpression(&pStage->fresnelScaleExp, 2.0f));
	qglUniform1fARB(tr.dispersionShader_C.u_FresnelBias, RB_EvalExpression(&pStage->fresnelBiasExp, 1.0f));

	GLSL_SetUniform_ModelMatrix(&tr.dispersionShader_C, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.dispersionShader_C, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.dispersionShader_C, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.dispersionShader_C.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_skybox(int stage)
{
	vec3_t          viewOrigin;
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_skybox ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.skyBoxShader);
	GL_VertexAttribsState(tr.skyBoxShader.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space

	GLSL_SetUniform_ViewOrigin(&tr.skyBoxShader, viewOrigin);

	GLSL_SetUniform_ModelMatrix(&tr.skyBoxShader, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.skyBoxShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// bind u_ColorMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_screen(int stage)
{
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_screen ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.screenShader);

	/*
	if(pStage->vertexColor || pStage->inverseVertexColor)
	{
		GL_VertexAttribsState(tr.screenShader.attribs);
	}
	else
	*/
	{
		GL_VertexAttribsState(tr.screenShader.attribs & ~(ATTR_COLOR));
		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, tess.svars.color);
	}

	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.screenShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// bind u_CurrentMap
	GL_SelectTexture(0);
	BindAnimatedImage(&pStage->bundle[TB_COLORMAP]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_portal(int stage)
{
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_portal ---\n");

	GL_State(pStage->stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.portalShader);

	/*
	if(pStage->vertexColor || pStage->inverseVertexColor)
	{
		GL_VertexAttribsState(tr.portalShader.attribs);
	}
	else
	*/
	{
		GL_VertexAttribsState(tr.portalShader.attribs & ~(ATTR_COLOR));
		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, tess.svars.color);
	}

	qglUniform1fARB(tr.portalShader.u_PortalRange, tess.surfaceShader->portalRange);

	GLSL_SetUniform_ModelViewMatrix(&tr.portalShader, glState.modelViewMatrix[glState.stackIndex]);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.portalShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// bind u_CurrentMap
	GL_SelectTexture(0);
	BindAnimatedImage(&pStage->bundle[TB_COLORMAP]);

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_heatHaze(int stage)
{
	unsigned        stateBits;
	float           deformMagnitude;
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_heatHaze ---\n");

	if(r_heatHazeFix->integer && glConfig.framebufferBlitAvailable)
	{
		FBO_t          *previousFBO;
		unsigned        stateBits;

		// capture current color buffer for u_CurrentMap
		/*
		GL_SelectTexture(0);
		GL_Bind(tr.currentRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth,
							 tr.currentRenderImage->uploadHeight);

		*/

		previousFBO = glState.currentFBO;

		if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
			   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
		{
			// copy deferredRenderFBO to portalRenderFBO
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.occlusionRenderFBO->frameBuffer);
			qglBlitFramebufferEXT(0, 0, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height,
								   0, 0, tr.occlusionRenderFBO->width, tr.occlusionRenderFBO->height,
								   GL_DEPTH_BUFFER_BIT,
								   GL_NEAREST);
		}
		else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
		{
			// copy deferredRenderFBO to portalRenderFBO
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.occlusionRenderFBO->frameBuffer);
			qglBlitFramebufferEXT(0, 0, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height,
								   0, 0, tr.occlusionRenderFBO->width, tr.occlusionRenderFBO->height,
								   GL_DEPTH_BUFFER_BIT,
								   GL_NEAREST);
		}
		else
		{
			// copy depth of the main context to deferredRenderFBO
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.occlusionRenderFBO->frameBuffer);
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
								   0, 0, glConfig.vidWidth, glConfig.vidHeight,
								   GL_DEPTH_BUFFER_BIT,
								   GL_NEAREST);
		}

		R_BindFBO(tr.occlusionRenderFBO);
		R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.occlusionRenderFBOImage->texnum, 0);

		// clear color buffer
		qglClear(GL_COLOR_BUFFER_BIT);

		// remove blend mode
		stateBits = pStage->stateBits;
		stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_DEPTHMASK_TRUE);

		GL_State(stateBits);

		// enable shader, set arrays
		GL_BindProgram(&tr.depthTestShader);
		GL_VertexAttribsState(tr.depthTestShader.attribs);

		// set uniforms
		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.depthTestShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

		// bind u_ColorMap
		GL_SelectTexture(0);
		GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
		GLSL_SetUniform_ColorTextureMatrix(&tr.depthTestShader, tess.svars.texMatrices[TB_COLORMAP]);

		// bind u_CurrentMap
		GL_SelectTexture(1);
		GL_Bind(tr.currentRenderImage);

		Tess_DrawElements();

		R_BindFBO(previousFBO);

		GL_CheckErrors();
	}

	// remove alpha test
	stateBits = pStage->stateBits;
	stateBits &= ~GLS_ATEST_BITS;
	stateBits &= ~GLS_DEPTHMASK_TRUE;

	GL_State(stateBits);

	// enable shader, set arrays
	GL_BindProgram(&tr.heatHazeShader);
	GL_VertexAttribsState(tr.heatHazeShader.attribs);

	// set uniforms
	GLSL_SetUniform_AlphaTest(&tr.heatHazeShader, pStage->stateBits);

	deformMagnitude = RB_EvalExpression(&pStage->deformMagnitudeExp, 1.0);

	qglUniform1fARB(tr.heatHazeShader.u_DeformMagnitude, deformMagnitude);

	GLSL_SetUniform_ModelViewMatrixTranspose(&tr.heatHazeShader, glState.modelViewMatrix[glState.stackIndex]);
	GLSL_SetUniform_ProjectionMatrixTranspose(&tr.heatHazeShader, glState.projectionMatrix[glState.stackIndex]);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.heatHazeShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	if(glConfig.vboVertexSkinningAvailable)
	{
		GLSL_SetUniform_VertexSkinning(&tr.heatHazeShader, tess.vboVertexSkinning);

		if(tess.vboVertexSkinning)
			qglUniformMatrix4fvARB(tr.heatHazeShader.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
	}

	// bind u_NormalMap
	GL_SelectTexture(0);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	GLSL_SetUniform_NormalTextureMatrix(&tr.heatHazeShader, tess.svars.texMatrices[TB_COLORMAP]);

	// bind u_CurrentMap
	GL_SelectTexture(1);
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
				   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GL_Bind(tr.deferredRenderFBOImage);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		GL_Bind(tr.deferredRenderFBOImage);
	}
	else
	{
		GL_Bind(tr.currentRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);
	}


	// bind u_ContrastMap
	if(r_heatHazeFix->integer && glConfig.framebufferBlitAvailable)
	{
		GL_SelectTexture(2);
		GL_Bind(tr.occlusionRenderFBOImage);
	}

	Tess_DrawElements();

	GL_CheckErrors();
}

static void Render_liquid(int stage)
{
	vec3_t          viewOrigin;
	float           fogDensity;
	vec3_t          fogColor;
	shaderStage_t  *pStage = tess.surfaceStages[stage];

	GLimp_LogComment("--- Render_liquid ---\n");

	// Tr3B: don't allow blend effects
	GL_State(pStage->stateBits & ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_DEPTHMASK_TRUE));

	// enable shader, set arrays
	GL_BindProgram(&tr.liquidShader);
	GL_VertexAttribsState(tr.liquidShader.attribs);

	// set uniforms
	VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space

	fogDensity = RB_EvalExpression(&pStage->fogDensityExp, 0.001);
	VectorCopy(tess.svars.color, fogColor);

	GLSL_SetUniform_ViewOrigin(&tr.liquidShader, viewOrigin);
	GLSL_SetUniform_RefractionIndex(&tr.liquidShader, RB_EvalExpression(&pStage->refractionIndexExp, 1.0));
	qglUniform1fARB(tr.liquidShader.u_FresnelPower, RB_EvalExpression(&pStage->fresnelPowerExp, 2.0));
	qglUniform1fARB(tr.liquidShader.u_FresnelScale, RB_EvalExpression(&pStage->fresnelScaleExp, 1.0));
	qglUniform1fARB(tr.liquidShader.u_FresnelBias, RB_EvalExpression(&pStage->fresnelBiasExp, 0.05));
	qglUniform1fARB(tr.liquidShader.u_NormalScale, RB_EvalExpression(&pStage->normalScaleExp, 0.05));
	qglUniform1fARB(tr.liquidShader.u_FogDensity, fogDensity);
	qglUniform3fARB(tr.liquidShader.u_FogColor, fogColor[0], fogColor[1], fogColor[2]);

	GLSL_SetUniform_UnprojectMatrix(&tr.liquidShader, backEnd.viewParms.unprojectionMatrix);
	GLSL_SetUniform_ModelMatrix(&tr.liquidShader, backEnd.orientation.transformMatrix);
	GLSL_SetUniform_ModelViewProjectionMatrix(&tr.liquidShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

	// capture current color buffer for u_CurrentMap
	GL_SelectTexture(0);
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
					   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GL_Bind(tr.deferredRenderFBOImage);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		GL_Bind(tr.deferredRenderFBOImage);
	}
	else
	{
		GL_Bind(tr.currentRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.currentRenderImage->uploadWidth, tr.currentRenderImage->uploadHeight);
	}

	// bind u_PortalMap
	GL_SelectTexture(1);
	GL_Bind(tr.portalRenderImage);

	// bind u_DepthMap
	GL_SelectTexture(2);
	if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
			   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
	{
		GL_Bind(tr.depthRenderImage);
	}
	else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
	{
		GL_Bind(tr.depthRenderImage);
	}
	else
	{
		// depth texture is not bound to a FBO
		GL_Bind(tr.depthRenderImage);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);
	}

	// bind u_NormalMap
	GL_SelectTexture(3);
	GL_Bind(pStage->bundle[TB_COLORMAP].image[0]);
	GLSL_SetUniform_NormalTextureMatrix(&tr.liquidShader, tess.svars.texMatrices[TB_COLORMAP]);

	Tess_DrawElements();

	GL_CheckErrors();
}



// see Fog Polygon Volumes documentation by Nvidia for further information
static void Render_volumetricFog()
{
#if 0
	vec3_t          viewOrigin;
	float           fogDensity;
	vec3_t          fogColor;

	GLimp_LogComment("--- Render_volumetricFog---\n");

	if(glConfig.framebufferBlitAvailable)
	{
		FBO_t          *previousFBO;

		previousFBO = glState.currentFBO;

		if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
			   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
		{
			// copy deferredRenderFBO to occlusionRenderFBO
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.occlusionRenderFBO->frameBuffer);
			qglBlitFramebufferEXT(0, 0, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height,
								   0, 0, tr.occlusionRenderFBO->width, tr.occlusionRenderFBO->height,
								   GL_DEPTH_BUFFER_BIT,
								   GL_NEAREST);
		}
		else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
		{
			// copy deferredRenderFBO to occlusionRenderFBO
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, tr.deferredRenderFBO->frameBuffer);
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.occlusionRenderFBO->frameBuffer);
			qglBlitFramebufferEXT(0, 0, tr.deferredRenderFBO->width, tr.deferredRenderFBO->height,
								   0, 0, tr.occlusionRenderFBO->width, tr.occlusionRenderFBO->height,
								   GL_DEPTH_BUFFER_BIT,
								   GL_NEAREST);
		}
		else
		{
			// copy depth of the main context to occlusionRenderFBO
			qglBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
			qglBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, tr.occlusionRenderFBO->frameBuffer);
			qglBlitFramebufferEXT(0, 0, glConfig.vidWidth, glConfig.vidHeight,
								   0, 0, glConfig.vidWidth, glConfig.vidHeight,
								   GL_DEPTH_BUFFER_BIT,
								   GL_NEAREST);
		}

		// setup shader with uniforms
		GL_BindProgram(&tr.depthToColorShader);
		GL_VertexAttribsState(tr.depthToColorShader.attribs);
		GL_State(0);//GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.depthToColorShader, glState.modelViewProjectionMatrix[glState.stackIndex]);

		// Tr3B: might be cool for ghost player effects
		if(glConfig.vboVertexSkinningAvailable)
		{
			GLSL_SetUniform_VertexSkinning(&tr.depthToColorShader, tess.vboVertexSkinning);

			if(tess.vboVertexSkinning)
				qglUniformMatrix4fvARB(tr.depthToColorShader.u_BoneMatrix, MAX_BONES, GL_FALSE, &tess.boneMatrices[0][0]);
		}


		// render back faces
		R_BindFBO(tr.occlusionRenderFBO);
		R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.depthToColorBackFacesFBOImage->texnum, 0);

		GL_ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		qglClear(GL_COLOR_BUFFER_BIT);
		GL_Cull(CT_BACK_SIDED);
		Tess_DrawElements();

		// render front faces
		R_AttachFBOTexture2D(GL_TEXTURE_2D, tr.depthToColorFrontFacesFBOImage->texnum, 0);

		qglClear(GL_COLOR_BUFFER_BIT);
		GL_Cull(CT_FRONT_SIDED);
		Tess_DrawElements();

		R_BindFBO(previousFBO);






		// enable shader, set arrays
		GL_BindProgram(&tr.volumetricFogShader);
		GL_VertexAttribsState(tr.volumetricFogShader.attribs);

		//GL_State(GLS_DEPTHTEST_DISABLE);	// | GLS_DEPTHMASK_TRUE);
		//GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR);
		GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA | GLS_DSTBLEND_SRC_ALPHA);
		GL_Cull(CT_TWO_SIDED);

		qglVertexAttrib4fvARB(ATTR_INDEX_COLOR, colorWhite);

		// set uniforms
		VectorCopy(backEnd.viewParms.orientation.origin, viewOrigin);	// in world space

		{
			fogDensity = tess.surfaceShader->fogParms.density;
			VectorCopy(tess.surfaceShader->fogParms.color, fogColor);
		}

		GLSL_SetUniform_ModelViewProjectionMatrix(&tr.volumetricFogShader, glState.modelViewProjectionMatrix[glState.stackIndex]);
		GLSL_SetUniform_UnprojectMatrix(&tr.volumetricFogShader, backEnd.viewParms.unprojectionMatrix);

		GLSL_SetUniform_ViewOrigin(&tr.volumetricFogShader, viewOrigin);
		qglUniform1fARB(tr.volumetricFogShader.u_FogDensity, fogDensity);
		qglUniform3fARB(tr.volumetricFogShader.u_FogColor, fogColor[0], fogColor[1], fogColor[2]);

		// bind u_DepthMap
		GL_SelectTexture(0);
		if(r_deferredShading->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable &&
				   glConfig.drawBuffersAvailable && glConfig.maxDrawBuffers >= 4)
		{
			GL_Bind(tr.depthRenderImage);
		}
		else if(r_hdrRendering->integer && glConfig.framebufferObjectAvailable && glConfig.textureFloatAvailable)
		{
			GL_Bind(tr.depthRenderImage);
		}
		else
		{
			// depth texture is not bound to a FBO
			GL_Bind(tr.depthRenderImage);
			qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, tr.depthRenderImage->uploadWidth, tr.depthRenderImage->uploadHeight);
		}

		// bind u_DepthMapBack
		GL_SelectTexture(1);
		GL_Bind(tr.depthToColorBackFacesFBOImage);

		// bind u_DepthMapFront
		GL_SelectTexture(2);
		GL_Bind(tr.depthToColorFrontFacesFBOImage);

		Tess_DrawElements();
	}

	GL_CheckErrors();
#endif
}

/*
===============
Tess_ComputeColor
===============
*/
void Tess_ComputeColor(shaderStage_t * pStage)
{
	float           rgb;
	float           red;
	float           green;
	float           blue;
	float           alpha;

	GLimp_LogComment("--- Tess_ComputeColor ---\n");

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
			tess.svars.color[0] = tr.identityLight;
			tess.svars.color[1] = tr.identityLight;
			tess.svars.color[2] = tr.identityLight;
			tess.svars.color[3] = tr.identityLight;
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

		case CGEN_WAVEFORM:
		{
			float           glow;
			waveForm_t     *wf;

			wf = &pStage->rgbWave;

			if(wf->func == GF_NOISE)
			{
				glow = wf->base + R_NoiseGet4f(0, 0, 0, (backEnd.refdef.floatTime + wf->phase) * wf->frequency) * wf->amplitude;
			}
			else
			{
				glow = RB_EvalWaveForm(wf) * tr.identityLight;
			}

			if(glow < 0)
			{
				glow = 0;
			}
			else if(glow > 1)
			{
				glow = 1;
			}

			tess.svars.color[0] = glow;
			tess.svars.color[1] = glow;
			tess.svars.color[2] = glow;
			tess.svars.color[3] = 1.0;
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
				red =
					Q_bound(0.0, RB_EvalExpression(&pStage->redExp, backEnd.currentEntity->e.shaderRGBA[0] * (1.0 / 255.0)), 1.0);
				green =
					Q_bound(0.0, RB_EvalExpression(&pStage->greenExp, backEnd.currentEntity->e.shaderRGBA[1] * (1.0 / 255.0)),
							1.0);
				blue =
					Q_bound(0.0, RB_EvalExpression(&pStage->blueExp, backEnd.currentEntity->e.shaderRGBA[2] * (1.0 / 255.0)),
							1.0);
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
		default:
		case AGEN_IDENTITY:
		{
			if(pStage->rgbGen != CGEN_IDENTITY)
			{
				tess.svars.color[3] = 1.0;
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
				tess.svars.color[3] = 1.0;	// FIXME ?
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
				tess.svars.color[3] = 0.0;	// FIXME ?
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

		case AGEN_WAVEFORM:
		{
			float           glow;
			waveForm_t     *wf;

			wf = &pStage->alphaWave;

			glow = RB_EvalWaveFormClamped(wf);

			tess.svars.color[3] = glow;
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
Tess_ComputeTexMatrices
===============
*/
static void Tess_ComputeTexMatrices(shaderStage_t * pStage)
{
	int             i;
	vec_t          *matrix;

	GLimp_LogComment("--- Tess_ComputeTexMatrices ---\n");

	for(i = 0; i < MAX_TEXTURE_BUNDLES; i++)
	{
		matrix = tess.svars.texMatrices[i];

		RB_CalcTexMatrix(&pStage->bundle[i], matrix);
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
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorGeneric( %s, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name,
						  tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	Tess_DeformGeometry();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		// Tr3B: FIXME analyze required vertex attribs by the current material
		Tess_UpdateVBOs(0);
	}

	if(tess.surfaceShader->fogVolume)
	{
		Render_volumetricFog();
		return;
	}

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
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

		Tess_ComputeColor(pStage);
		Tess_ComputeTexMatrices(pStage);

		switch (pStage->type)
		{
			case ST_COLORMAP:
			{
				Render_genericSingle(stage);
				break;
			}

#if defined(COMPAT_Q3A)
			case ST_LIGHTMAP:
			{
				Render_lightMapping(stage, qtrue);
				break;
			}
#endif

			case ST_DIFFUSEMAP:
			case ST_COLLAPSE_lighting_DB:
			case ST_COLLAPSE_lighting_DBS:
			{
				//if(tess.surfaceShader->sort <= SS_OPAQUE)
				{
					if(r_precomputedLighting->integer || r_vertexLighting->integer)
					{
						if(!r_vertexLighting->integer && tess.lightmapNum >= 0 && (tess.lightmapNum / 2) < tr.lightmaps.currentElements)
						{
							if(tr.worldDeluxeMapping && r_normalMapping->integer)
							{
								Render_deluxeMapping(stage);
							}
							else
							{
								Render_lightMapping(stage, qfalse);
							}
						}
						else if(backEnd.currentEntity != &tr.worldEntity)
						{
							Render_vertexLighting_DBS_entity(stage);
						}
						else
						{
							Render_vertexLighting_DBS_world(stage);
						}
					}
					else
					{
						Render_depthFill(stage, qfalse);
					}
				}
				break;
			}

			case ST_COLLAPSE_reflection_CB:
			{
				Render_reflection_CB(stage);
				break;
			}

			case ST_REFLECTIONMAP:
			{
				Render_reflection_C(stage);
				break;
			}

			case ST_REFRACTIONMAP:
			{
				Render_refraction_C(stage);
				break;
			}

			case ST_DISPERSIONMAP:
			{
				Render_dispersion_C(stage);
				break;
			}

			case ST_SKYBOXMAP:
			{
				Render_skybox(stage);
				break;
			}

			case ST_SCREENMAP:
			{
				Render_screen(stage);
				break;
			}

			case ST_PORTALMAP:
			{
				Render_portal(stage);
				break;
			}

			case ST_HEATHAZEMAP:
			{
				Render_heatHaze(stage);
				break;
			}

			case ST_LIQUIDMAP:
			{
				Render_liquid(stage);
				break;
			}

			default:
				break;
		}
	}

	// reset polygon offset
	if(tess.surfaceShader->polygonOffset)
	{
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}
}

void Tess_StageIteratorGBuffer()
{
	int             stage;

	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorGBuffer( %s, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name,
						  tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	Tess_DeformGeometry();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		// Tr3B: FIXME analyze required vertex attribs by the current material
		Tess_UpdateVBOs(0);
	}

#if 0
	if(tess.surfaceShader->fogVolume)
	{
		//R_BindFBO(tr.deferredRenderFBO);
		Render_volumetricFog();
		return;
	}
#endif

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
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

		Tess_ComputeColor(pStage);
		Tess_ComputeTexMatrices(pStage);

		switch (pStage->type)
		{
			case ST_COLORMAP:
			{
#if 0
				R_BindFBO(tr.deferredRenderFBO);
				Render_genericSingle(stage);
#endif

#if 1
				if(tess.surfaceShader->sort <= SS_OPAQUE && !(pStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)))
				{
					R_BindFBO(tr.geometricRenderFBO);
					Render_geometricFill_DBS(stage, qtrue);
				}
#endif
				break;
			}

			case ST_DIFFUSEMAP:
			case ST_COLLAPSE_lighting_DB:
			case ST_COLLAPSE_lighting_DBS:
			{
#if 0
				R_BindFBO(tr.deferredRenderFBO);
				if(r_precomputedLighting->integer || r_vertexLighting->integer)
				{
					if(!r_vertexLighting->integer && tess.lightmapNum >= 0 && tess.lightmapNum < tr.numLightmaps)
					{
						if(tr.worldDeluxeMapping && r_normalMapping->integer)
						{
							Render_deluxeMapping(stage);
						}
						else
						{
							Render_lightMapping(stage, qfalse);
						}
					}
					else if(backEnd.currentEntity != &tr.worldEntity)
					{
						Render_vertexLighting_DBS_entity(stage);
					}
					else
					{
						Render_vertexLighting_DBS_world(stage);
					}
				}
				else
				{
					Render_depthFill(stage, qfalse);
				}
#endif

				R_BindFBO(tr.geometricRenderFBO);
				Render_geometricFill_DBS(stage, qfalse);
				break;
			}

#if 0
			case ST_COLLAPSE_reflection_CB:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_reflection_CB(stage);
				break;
			}

			case ST_REFLECTIONMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_reflection_C(stage);
				break;
			}

			case ST_REFRACTIONMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_refraction_C(stage);
				break;
			}

			case ST_DISPERSIONMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_dispersion_C(stage);
				break;
			}

			case ST_SKYBOXMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_skybox(stage);
				break;
			}

			case ST_SCREENMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_screen(stage);
				break;
			}

			case ST_PORTALMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_portal(stage);
				break;
			}

			case ST_HEATHAZEMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_heatHaze(stage);
				break;
			}

			case ST_LIQUIDMAP:
			{
				R_BindFBO(tr.deferredRenderFBO);
				Render_liquid(stage);
				break;
			}
#endif

			default:
				break;
		}
	}

	// reset polygon offset
	qglDisable(GL_POLYGON_OFFSET_FILL);
}

void Tess_StageIteratorDepthFill()
{
	int             stage;

	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorDepthFill( %s, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name,
						  tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	Tess_DeformGeometry();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		Tess_UpdateVBOs(ATTR_POSITION | ATTR_TEXCOORD);
	}

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
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

		Tess_ComputeTexMatrices(pStage);

		switch (pStage->type)
		{
			case ST_COLORMAP:
			{
				if(tess.surfaceShader->sort <= SS_OPAQUE)
				{
					Render_depthFill(stage, qfalse);
				}
				break;
			}

#if defined(COMPAT_Q3A)
			case ST_LIGHTMAP:
			{
				Render_depthFill(stage, qtrue);
				break;
			}
#endif
			case ST_DIFFUSEMAP:
			case ST_COLLAPSE_lighting_DB:
			case ST_COLLAPSE_lighting_DBS:
			{
				Render_depthFill(stage, qfalse);
				break;
			}

			default:
				break;
		}
	}

	// reset polygon offset
	qglDisable(GL_POLYGON_OFFSET_FILL);
}

void Tess_StageIteratorShadowFill()
{
	int             stage;

	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorShadowFill( %s, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name,
						  tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	Tess_DeformGeometry();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		Tess_UpdateVBOs(ATTR_POSITION | ATTR_TEXCOORD);
	}

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
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

		Tess_ComputeTexMatrices(pStage);

		switch (pStage->type)
		{
			case ST_COLORMAP:
			{
				if(tess.surfaceShader->sort <= SS_OPAQUE)
				{
					Render_shadowFill(stage);
				}
				break;
			}

#if defined(COMPAT_Q3A)
			case ST_LIGHTMAP:
#endif
			case ST_DIFFUSEMAP:
			case ST_COLLAPSE_lighting_DB:
			case ST_COLLAPSE_lighting_DBS:
			{
				Render_shadowFill(stage);
				break;
			}

			default:
				break;
		}
	}

	// reset polygon offset
	qglDisable(GL_POLYGON_OFFSET_FILL);
}

void Tess_StageIteratorStencilShadowVolume()
{
	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorStencilShadowVolume( %s, %i vertices, %i triangles ) ---\n",
						  tess.surfaceShader->name, tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		Tess_UpdateVBOs(ATTR_POSITION);
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
		qglVertexAttrib4fARB(ATTR_INDEX_COLOR, 1.0f, 0.0f, 0.0f, 0.05f);
		Tess_DrawElements();
#endif

#if 1
		GL_Cull(CT_BACK_SIDED);
		qglVertexAttrib4fARB(ATTR_INDEX_COLOR, 0.0f, 1.0f, 0.0f, 0.05f);
		Tess_DrawElements();
#endif

#if 1
		GL_State(GLS_DEPTHFUNC_LESS | GLS_POLYMODE_LINE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
		GL_Cull(CT_TWO_SIDED);
		qglVertexAttrib4fARB(ATTR_INDEX_COLOR, 0.0f, 0.0f, 1.0f, 0.05f);
		Tess_DrawElements();
#endif
	}
	else
	{
		if(backEnd.currentEntity->needZFail)
		{
			// mirrors have the culling order reversed
			//if(backEnd.viewParms.isMirror)
			//  GL_FrontFace(GL_CW);

			if(qglStencilFuncSeparateATI && qglStencilOpSeparateATI && glConfig.stencilWrapAvailable)
			{
				GL_Cull(CT_TWO_SIDED);

				qglStencilFuncSeparateATI(GL_ALWAYS, GL_ALWAYS, 0, (GLuint) ~ 0);

				qglStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
				qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);

				Tess_DrawElements();
			}
			else if(qglActiveStencilFaceEXT)
			{
				// render both sides at once
				GL_Cull(CT_TWO_SIDED);

				qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

				qglActiveStencilFaceEXT(GL_BACK);
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
				}

				qglActiveStencilFaceEXT(GL_FRONT);
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
				}

				Tess_DrawElements();

				qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			}
			else
			{
				// draw only the front faces of the shadow volume
				GL_Cull(CT_FRONT_SIDED);

				// increment the stencil value on zfail
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
				}

				Tess_DrawElements();

				// draw only the back faces of the shadow volume
				GL_Cull(CT_BACK_SIDED);

				// decrement the stencil value on zfail
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
				}

				Tess_DrawElements();
			}

			//if(backEnd.viewParms.isMirror)
			//  GL_FrontFace(GL_CCW);
		}
		else
		{
			// Tr3B - zpass rendering is cheaper because we can skip the lightcap and darkcap
			// see GPU Gems1 9.5.4

			// mirrors have the culling order reversed
			//if(backEnd.viewParms.isMirror)
			//  GL_FrontFace(GL_CW);

			if(qglStencilFuncSeparateATI && qglStencilOpSeparateATI && glConfig.stencilWrapAvailable)
			{
				GL_Cull(CT_TWO_SIDED);

				qglStencilFuncSeparateATI(GL_ALWAYS, GL_ALWAYS, 0, (GLuint) ~ 0);

				qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
				qglStencilOpSeparateATI(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);

				Tess_DrawElements();
			}
			else if(qglActiveStencilFaceEXT)
			{
				// render both sides at once
				GL_Cull(CT_TWO_SIDED);

				qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

				qglActiveStencilFaceEXT(GL_BACK);
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				}

				qglActiveStencilFaceEXT(GL_FRONT);
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
				}

				Tess_DrawElements();

				qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
			}
			else
			{
				// draw only the back faces of the shadow volume
				GL_Cull(CT_BACK_SIDED);

				// increment the stencil value on zpass
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				}

				Tess_DrawElements();

				// draw only the front faces of the shadow volume
				GL_Cull(CT_FRONT_SIDED);

				// decrement the stencil value on zpass
				if(glConfig.stencilWrapAvailable)
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
				}
				else
				{
					qglStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
				}

				Tess_DrawElements();
			}

			//if(backEnd.viewParms.isMirror)
			//  GL_FrontFace(GL_CCW);
		}
	}
}

void Tess_StageIteratorStencilLighting()
{
	int             i, j;
	trRefLight_t   *light;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;

	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorStencilLighting( %s, %s, %i vertices, %i triangles ) ---\n",
						  tess.surfaceShader->name, tess.lightShader->name, tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	light = backEnd.currentLight;

	Tess_DeformGeometry();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		// Tr3B: FIXME analyze required vertex attribs by the current material
		Tess_UpdateVBOs(0);
	}

	// set OpenGL state for lighting
#if 0
	if(!light->additive)
	{
		GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL | GLS_STENCILTEST_ENABLE);
	}
	else
#endif
	{
		if(tess.surfaceShader->sort > SS_OPAQUE)
		{
			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_STENCILTEST_ENABLE);
		}
		else
		{
			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL | GLS_STENCILTEST_ENABLE);
		}
	}

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
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

		Tess_ComputeTexMatrices(diffuseStage);

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

			Tess_ComputeColor(attenuationXYStage);
			R_ComputeFinalAttenuation(attenuationXYStage, light);

			switch (diffuseStage->type)
			{
				case ST_DIFFUSEMAP:
				case ST_COLLAPSE_lighting_DB:
				case ST_COLLAPSE_lighting_DBS:
					if(light->l.rlType == RL_OMNI)
					{
						Render_forwardLighting_DBS_omni(diffuseStage, attenuationXYStage, attenuationZStage, light);
					}
					else if(light->l.rlType == RL_PROJ)
					{
						Render_forwardLighting_DBS_proj(diffuseStage, attenuationXYStage, attenuationZStage, light);
					}
					else
					{
						// TODO ?
					}
					break;

				default:
					break;
			}
		}
	}

	// reset polygon offset
	if(tess.surfaceShader->polygonOffset)
	{
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}
}

void Tess_StageIteratorLighting()
{
	int             i, j;
	trRefLight_t   *light;
	shaderStage_t  *attenuationXYStage;
	shaderStage_t  *attenuationZStage;

	// log this call
	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment(va
						 ("--- Tess_StageIteratorLighting( %s, %s, %i vertices, %i triangles ) ---\n", tess.surfaceShader->name,
						  tess.lightShader->name, tess.numVertexes, tess.numIndexes / 3));
	}

	GL_CheckErrors();

	light = backEnd.currentLight;

	Tess_DeformGeometry();

	if(!glState.currentVBO || !glState.currentIBO || glState.currentVBO == tess.vbo || glState.currentIBO == tess.ibo)
	{
		// Tr3B: FIXME analyze required vertex attribs by the current material
		Tess_UpdateVBOs(0);
	}

	// set OpenGL state for lighting
	if(light->l.inverseShadows)
	{
		GL_State(GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR);
	}
	else
	{
		if(tess.surfaceShader->sort > SS_OPAQUE)
		{
			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
		}
		else
		{
			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);
		}
	}

	// set face culling appropriately
	GL_Cull(tess.surfaceShader->cullType);

	// set polygon offset if necessary
	if(tess.surfaceShader->polygonOffset)
	{
		qglEnable(GL_POLYGON_OFFSET_FILL);
		qglPolygonOffset(r_offsetFactor->value, r_offsetUnits->value);
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

		Tess_ComputeTexMatrices(diffuseStage);

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

			Tess_ComputeColor(attenuationXYStage);
			R_ComputeFinalAttenuation(attenuationXYStage, light);

			switch (diffuseStage->type)
			{
				case ST_DIFFUSEMAP:
				case ST_COLLAPSE_lighting_DB:
				case ST_COLLAPSE_lighting_DBS:
					if(light->l.rlType == RL_OMNI)
					{
						Render_forwardLighting_DBS_omni(diffuseStage, attenuationXYStage, attenuationZStage, light);
					}
					else if(light->l.rlType == RL_PROJ)
					{
						if(!light->l.inverseShadows)
						{
							Render_forwardLighting_DBS_proj(diffuseStage, attenuationXYStage, attenuationZStage, light);
						}
					}
					else
					{
						// TODO ?
					}
					break;

				default:
					break;
			}
		}
	}

	// reset polygon offset
	if(tess.surfaceShader->polygonOffset)
	{
		qglDisable(GL_POLYGON_OFFSET_FILL);
	}
}



/*
=================
Tess_End

Render tesselated data
=================
*/
void Tess_End()
{
	if(tess.numIndexes == 0 || tess.numVertexes == 0)
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

	GL_CheckErrors();

	// call off to shader specific tess end function
	tess.stageIteratorFunc();

	if(!tess.shadowVolume)
	{
		// draw debugging stuff
		if(r_showTris->integer || r_showBatches->integer ||
		   (r_showLightBatches->integer && (tess.stageIteratorFunc == Tess_StageIteratorLighting)))
		{
			DrawTris();
		}
	}

	tess.vboVertexSkinning = qfalse;

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	GLimp_LogComment("--- Tess_End ---\n");

	GL_CheckErrors();
}
