/*
===========================================================================
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>
Copyright (C) 2006 defconx          <defcon-x@ns-co.net>

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

// ATI bugfix, set by renderer at compile time
//#define ATI

attribute vec4		attr_TexCoord0;

#if defined(ATI)
uniform mat4		u_ProjectionMatrixTranspose;
#endif

uniform float		u_DeformMagnitude;

varying vec2		var_TexNormal;
varying float		var_Deform;

void	main()
{
	// transform vertex position into homogenous clip-space
	gl_Position = ftransform();
	
	// transform normalmap texcoords
	var_TexNormal = (gl_TextureMatrix[0] * attr_TexCoord0).st;
	
	// take the deform magnitude and scale it by the projection distance
	vec4 tmp0 = vec4(1, 0, 0, 1);
	tmp0.z = dot(gl_ModelViewMatrixTranspose[2], gl_Vertex);

#if defined(ATI)
	float tmp1 = dot(u_ProjectionMatrixTranspose[0],  tmp0);
#else
	float tmp1 = dot(gl_ProjectionMatrixTranspose[0],  tmp0);
#endif
	
	// clamp the distance so the the deformations don't get too wacky near the view
	tmp1 = min(tmp1, 0.02);
	
	var_Deform = tmp1 * u_DeformMagnitude;
}
