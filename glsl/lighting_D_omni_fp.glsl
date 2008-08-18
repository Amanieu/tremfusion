/*
===========================================================================
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

uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_AttenuationMapXY;
uniform sampler2D	u_AttenuationMapZ;
//uniform samplerCube	u_AttenuationMapCube;
//uniform samplerCube	u_ShadowMap;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightColor;
uniform float       u_LightScale;

varying vec3		var_Vertex;
varying vec3		var_Normal;
varying vec2		var_TexDiffuse;
varying vec3		var_TexAttenXYZ;
//varying vec3		var_TexAttenCube;
//varying vec3		var_TexShadow;

void	main()
{
	// compute normal
	vec3 N = normalize(var_Normal);
		
	// compute lightdir
	vec3 L = normalize(u_LightOrigin - var_Vertex);
	
	// compute the diffuse term
	vec4 diffuse = texture2D(u_DiffuseMap, var_TexDiffuse);
	diffuse.rgb *= u_LightColor * clamp(dot(N, L), 0.0, 1.0);
	
	// compute attenuation	
	vec3 attenuationXY		= texture2D(u_AttenuationMapXY, var_TexAttenXYZ.xy).rgb;
	vec3 attenuationZ		= texture2D(u_AttenuationMapZ, vec2(var_TexAttenXYZ.z, 0)).rgb;
//	vec3 attenuationCube	= textureCube(u_AttenuationMapCube, var_TexAttenCube).rgb;

	// compute shadow
//	vec3 shadow = textureCube(u_ShadowMap, var_TexShadow).rgb;
	
	// compute final color
	vec4 color = diffuse;
	color.rgb *= attenuationXY;
	color.rgb *= attenuationZ;
//	color.rgb *= attenuationCube;
//  color.rgb *= shadow;
	color.rgb *= u_LightScale;

	gl_FragColor = color;
}
