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
uniform vec3		u_AmbientColor;
uniform vec3		u_LightDir;
uniform vec3		u_LightColor;

varying vec3		var_Normal;
varying vec2		var_TexDiffuse;

void	main()
{		
	// compute normal
	vec3 N = normalize(var_Normal);
		
	// compute lightdir
	vec3 L = normalize(u_LightDir);
	
	// compute the diffuse term
	vec4 diffuse = texture2D(u_DiffuseMap, var_TexDiffuse);
	
	// compute the light term
	vec3 light = u_AmbientColor + u_LightColor * clamp(dot(N, L), 0.0, 1.0);
	clamp(light, 0.0, 1.0);
	
	// compute final color
	gl_FragColor.rgba = diffuse;
	gl_FragColor.rgb *= light;
}
