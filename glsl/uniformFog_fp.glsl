/*
===========================================================================
Copyright (C) 2007 Robert Beckebans <trebor_7@users.sourceforge.net>

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

uniform sampler2D	u_DepthMap;
uniform vec3		u_ViewOrigin;
uniform float		u_FogDensity;
uniform vec3		u_FogColor;
uniform mat4		u_UnprojectMatrix;

//varying vec3		var_Position;

void	main()
{
	// calculate the screen texcoord in the 0.0 to 1.0 range
	vec2 st = gl_FragCoord.st * r_FBufScale;
	
	// scale by the screen non-power-of-two-adjust
	st *= r_NPOTScale;
	
	// reconstruct vertex position in world space
	float depth = texture2D(u_DepthMap, st).r;
	vec4 P = u_UnprojectMatrix * vec4(gl_FragCoord.xy, depth, 1.0);
	P.xyz /= P.w;

	// calculate fog distance
	float fogDistance = distance(P.xyz, u_ViewOrigin);
	
	// calculate fog exponent
	float fogExponent = fogDistance * u_FogDensity;
	
	// calculate fog factor
	float fogFactor = exp2(-abs(fogExponent));
	
	// lerp between FBO color and fog color with GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA. GLS_DSTBLEND_SRC_ALPHA
	gl_FragColor = vec4(u_FogColor, fogFactor);
}
