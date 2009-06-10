/*
===========================================================================
Copyright (C) 2008-2009 Robert Beckebans <trebor_7@users.sourceforge.net>

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
attribute vec3		attr_Tangent;
attribute vec3		attr_Binormal;
attribute vec3		attr_Normal;
attribute vec4		attr_Color;
#if !defined(COMPAT_Q3A)
attribute vec3		attr_LightDirection;
#endif

uniform mat4		u_DiffuseTextureMatrix;
uniform mat4		u_NormalTextureMatrix;
uniform mat4		u_SpecularTextureMatrix;
uniform int			u_InverseVertexColor;
uniform mat4		u_ModelViewProjectionMatrix;
uniform int			u_ColorGen;
uniform int			u_AlphaGen;
uniform vec4		u_Color;

varying vec3		var_Position;
varying vec4		var_TexDiffuseNormal;
varying vec2		var_TexSpecular;
varying vec4		var_LightColor;
#if !defined(COMPAT_Q3A)
varying vec3		var_LightDirection;
#endif
varying vec3		var_Tangent;
varying vec3		var_Binormal;
varying vec3		var_Normal;

void	main()
{
	// transform vertex position into homogenous clip-space
	gl_Position = u_ModelViewProjectionMatrix * attr_Position;
	
	// assign position in object space
	var_Position = attr_Position.xyz;
	
	// transform diffusemap texcoords
	var_TexDiffuseNormal.st = (u_DiffuseTextureMatrix * attr_TexCoord0).st;
	
#if defined(r_NormalMapping)
	// transform normalmap texcoords
	var_TexDiffuseNormal.pq = (u_NormalTextureMatrix * attr_TexCoord0).st;
	
	// transform specularmap texture coords
	var_TexSpecular = (u_SpecularTextureMatrix * attr_TexCoord0).st;
#endif
	
#if !defined(COMPAT_Q3A)
	// assign vertex to light vector in object space
	var_LightDirection = attr_LightDirection;
#endif
	
	// assign color
	if(u_ColorGen == CGEN_VERTEX)
	{
		var_LightColor.r = attr_Color.r;
		var_LightColor.g = attr_Color.g;
		var_LightColor.b = attr_Color.b;
	}
	else if(u_ColorGen == CGEN_ONE_MINUS_VERTEX)
	{
		var_LightColor.r = 1.0 - attr_Color.r;
		var_LightColor.g = 1.0 - attr_Color.g;
		var_LightColor.b = 1.0 - attr_Color.b;
	}
	else
	{
		var_LightColor.rgb = u_Color.rgb;
	}
	
#if defined(r_NormalMapping)
	var_Tangent = attr_Tangent;
	var_Binormal = attr_Binormal;
#endif

	var_Normal = attr_Normal;
}
