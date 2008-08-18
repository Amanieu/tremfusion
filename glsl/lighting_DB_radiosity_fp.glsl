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
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_LightMap;
uniform sampler2D	u_DeluxeMap;

varying vec2		var_TexDiffuse;
varying vec2		var_TexNormal;
varying vec2		var_TexLight;
varying mat3		var_OS2TSMatrix;

void	main()
{
	// compute normal in tangent space from normalmap
	vec3 N = 2.0 * (texture2D(u_NormalMap, var_TexNormal).xyz - 0.5);
	N = normalize(N);
	
	// compute light direction from object space deluxe map into tangent space
	vec3 L = normalize(var_OS2TSMatrix * (2.0 * (texture2D(u_DeluxeMap, var_TexLight).xyz - 0.5)));
	
	// compute light color from object space lightmap
	vec3 C = texture2D(u_LightMap, var_TexLight).rgb;
	
	// compute the diffuse term
	vec4 diffuse = texture2D(u_DiffuseMap, var_TexDiffuse);
	diffuse.rgb *= C * clamp(dot(N, L), 0.0, 1.0);
	
	// compute final color
	gl_FragColor = diffuse;
}
