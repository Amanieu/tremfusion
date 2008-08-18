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

uniform sampler2D	u_ColorMap;
uniform vec2		u_FBufScale;
uniform vec2		u_NPotScale;

void	main()
{
	vec2 st00 = gl_FragCoord.st;

	// calculate the screen texcoord in the 0.0 to 1.0 range
	st00 *= u_FBufScale;
	
	// scale by the screen non-power-of-two-adjust
	st00 *= u_NPotScale;
	
	// calculate contrast color
#if 1
	vec4 color = texture2D(u_ColorMap, st00);
	vec4 contrast = color * color;
	contrast.x += contrast.y;
	contrast.x += contrast.z;
	contrast.x *= 0.33333333;
	gl_FragColor = contrast;
#else
	vec4 color = texture2D(u_ColorMap, st00);
//	vec3 contrast = dot(color.rgb, vec3(0.11, 0.55, 0.33));
//	vec3 contrast = dot(color.rgb, vec3(0.27, 0.67, 0.06));
	vec3 contrast = dot(color.rgb, vec3(0.33, 0.55, 0.11));
	gl_FragColor.rgb = contrast;
	gl_FragColor.a = color.a;
#endif
}
