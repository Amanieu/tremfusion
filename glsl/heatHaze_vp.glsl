/*
===========================================================================
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>
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

attribute vec4		attr_Position;
attribute vec4		attr_TexCoord0;
#if defined(r_VertexSkinning)
attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;
uniform int			u_VertexSkinning;
uniform mat4		u_BoneMatrix[MAX_GLSL_BONES];
#endif

uniform mat4		u_NormalTextureMatrix;
uniform mat4		u_ProjectionMatrixTranspose;
uniform mat4		u_ModelViewMatrixTranspose;
uniform mat4		u_ModelViewProjectionMatrix;

uniform float		u_DeformMagnitude;

varying vec2		var_TexNormal;
varying float		var_Deform;

void	main()
{
	vec4            deformVec;
    float           d1, d2;	

#if defined(r_VertexSkinning)
	if(bool(u_VertexSkinning))
	{
		vec4 vertex = vec4(0.0);
		
		for(int i = 0; i < 4; i++)
		{
			int boneIndex = int(attr_BoneIndexes[i]);
			float boneWeight = attr_BoneWeights[i];
			mat4  boneMatrix = u_BoneMatrix[boneIndex];
			
			vertex += (boneMatrix * attr_Position) * boneWeight;
		}

		// transform vertex position into homogenous clip-space
		gl_Position = u_ModelViewProjectionMatrix * vertex;
		
		// take the deform magnitude and scale it by the projection distance
		deformVec = vec4(1, 0, 0, 1);
		deformVec.z = dot(u_ModelViewMatrixTranspose[2], vertex);
	}
	else
#endif
	{
		// transform vertex position into homogenous clip-space
		gl_Position = u_ModelViewProjectionMatrix * attr_Position;
		
		// take the deform magnitude and scale it by the projection distance
		deformVec = vec4(1, 0, 0, 1);
		deformVec.z = dot(u_ModelViewMatrixTranspose[2], attr_Position);
	}
	
	// transform normalmap texcoords
	var_TexNormal = (u_NormalTextureMatrix * attr_TexCoord0).st;

	d1 = dot(u_ProjectionMatrixTranspose[0],  deformVec);
    d2 = dot(u_ProjectionMatrixTranspose[3],  deformVec);
	
	// clamp the distance so the the deformations don't get too wacky near the view
	var_Deform = min(d1 * (1.0 / max(d2, 1.0)), 0.02) * u_DeformMagnitude;
}
