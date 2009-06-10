/*
===========================================================================
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

attribute vec4		attr_Position;
attribute vec4		attr_TexCoord0;
attribute vec3		attr_Normal;
attribute vec4		attr_Color;
#if defined(r_VertexSkinning)
attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;
uniform int			u_VertexSkinning;
uniform mat4		u_BoneMatrix[MAX_GLSL_BONES];
#endif

uniform mat4		u_ColorTextureMatrix;
uniform vec3		u_ViewOrigin;
uniform int			u_TCGen_Environment;
uniform int			u_ColorGen;
uniform int			u_AlphaGen;
uniform vec4		u_Color;
uniform mat4		u_ModelMatrix;
//uniform mat4		u_ProjectionMatrix;
uniform mat4		u_ModelViewProjectionMatrix;

varying vec3		var_Position;
varying vec2		var_Tex;
varying vec4		var_Color;

void	main()
{
	vec4 position = vec4(0.0);

#if defined(r_VertexSkinning)
	if(bool(u_VertexSkinning))
	{
		vec4 vertex = vec4(0.0);
		
		for(int i = 0; i < 4; i++)
		{
			int boneIndex = int(attr_BoneIndexes[i]);
			float boneWeight = attr_BoneWeights[i];
			mat4  boneMatrix = u_BoneMatrix[boneIndex];
			
			position += (boneMatrix * attr_Position) * boneWeight;
		}

		// transform vertex position into homogenous clip-space
		gl_Position = u_ModelViewProjectionMatrix * position;
		
		// transform position into world space
		var_Position = (u_ModelMatrix * vertex).xyz;
	}
	else
#endif
	{
		// transform vertex position into homogenous clip-space
		gl_Position = u_ModelViewProjectionMatrix * attr_Position;
		//gl_Position = u_ProjectionMatrix * u_ModelViewMatrix * attr_Position;
		//gl_Position = u_ProjectionMatrix * attr_Position;
		
		position = attr_Position;
		
		// transform position into world space
		var_Position = (u_ModelMatrix * attr_Position).xyz;
	}
	
	// transform texcoords
	if(bool(u_TCGen_Environment))
	{
		vec3 viewer = normalize(u_ViewOrigin - position.xyz);

		float d = dot(attr_Normal, viewer);

		vec3 reflected = attr_Normal * 2.0 * d - viewer;
		
		var_Tex.s = 0.5 + reflected.y * 0.5;
		var_Tex.t = 0.5 - reflected.z * 0.5;
	}
	else
	{
		var_Tex = (u_ColorTextureMatrix * attr_TexCoord0).st;
	}
	
	// assign color
	if(u_ColorGen == CGEN_VERTEX)
	{
		var_Color.r = attr_Color.r;
		var_Color.g = attr_Color.g;
		var_Color.b = attr_Color.b;
	}
	else if(u_ColorGen == CGEN_ONE_MINUS_VERTEX)
	{
		var_Color.r = 1.0 - attr_Color.r;
		var_Color.g = 1.0 - attr_Color.g;
		var_Color.b = 1.0 - attr_Color.b;
	}
	else
	{
		var_Color.rgb = u_Color.rgb;
	}
	
	if(u_AlphaGen == AGEN_VERTEX)
	{
		var_Color.a = attr_Color.a;
	}
	else if(u_AlphaGen == AGEN_ONE_MINUS_VERTEX)
	{
		var_Color.a = 1.0 - attr_Color.a;
	}
	else
	{
		var_Color.a = u_Color.a;
	}
}
