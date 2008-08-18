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

uniform samplerCube	u_ColorMap;
uniform vec3		u_ViewOrigin;

varying vec3		var_Vertex;
varying vec3		var_Normal;

void	main()
{
	// compute incident ray
	vec3 I = normalize(var_Vertex - u_ViewOrigin);
	
	// compute normal
	vec3 N = normalize(var_Normal);
	
	// compute reflection ray
	vec3 R = reflect(I, N);
	
	// compute reflection color
	vec4 reflect_color = textureCube(u_ColorMap, R).rgba;

	// compute final color
	gl_FragColor = reflect_color;
}
