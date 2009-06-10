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

uniform sampler2D	u_ColorMap;
uniform int			u_AlphaTest;
uniform vec3		u_LightOrigin;
uniform float       u_LightRadius;

varying vec3		var_Position;
varying vec2		var_Tex;
varying vec4		var_Color;

void	main()
{
	vec4 color = texture2D(u_ColorMap, var_Tex);
	if(u_AlphaTest == ATEST_GT_0 && color.a <= 0.0)
	{
		discard;
		return;
	}
	else if(u_AlphaTest == ATEST_LT_128 && color.a >= 0.5)
	{
		discard;
		return;
	}
	else if(u_AlphaTest == ATEST_GE_128 && color.a < 0.5)
	{
		discard;
		return;
	}
	
#if defined(VSM)
	
	float distance = length(var_Position - u_LightOrigin) / u_LightRadius;
	float distanceSquared = distance * distance;

	// shadowmap can be float RGBA or luminance alpha so store distanceSquared into alpha
	
#if defined(VSM_CLAMP)
	// convert to [0,1] color space
	gl_FragColor = vec4(distance, 0.0 , 0.0, distanceSquared) * 0.5 + 0.5;
#else
	gl_FragColor = vec4(distance, 0.0, 0.0, distanceSquared);
#endif

#elif defined(ESM)
	
	float distance = (length(var_Position - u_LightOrigin) / u_LightRadius) * r_ShadowMapDepthScale;// ;
	
	gl_FragColor = vec4(0.0, 0.0, 0.0, distance);
#else
	gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
#endif
}
