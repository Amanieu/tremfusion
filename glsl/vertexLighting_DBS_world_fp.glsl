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

uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_SpecularMap;
uniform int			u_AlphaTest;
uniform vec3		u_ViewOrigin;
uniform int			u_ParallaxMapping;
uniform float		u_DepthScale;
uniform int         u_PortalClipping;
uniform vec4		u_PortalPlane;

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

#if defined(r_ParallaxMapping)
float RayIntersectDisplaceMap(vec2 dp, vec2 ds)
{
	const int linearSearchSteps = 16;
	const int binarySearchSteps = 6;

	float depthStep = 1.0 / float(linearSearchSteps);

	// current size of search window
	float size = depthStep;

	// current depth position
	float depth = 0.0;

	// best match found (starts with last position 1.0)
	float bestDepth = 1.0;

	// search front to back for first point inside object
	for(int i = 0; i < linearSearchSteps - 1; ++i)
	{
		depth += size;
		
		vec4 t = texture2D(u_NormalMap, dp + ds * depth);

		if(bestDepth > 0.996)		// if no depth found yet
			if(depth >= t.w)
				bestDepth = depth;	// store best depth
	}

	depth = bestDepth;
	
	// recurse around first point (depth) for closest match
	for(int i = 0; i < binarySearchSteps; ++i)
	{
		size *= 0.5;

		vec4 t = texture2D(u_NormalMap, dp + ds * depth);
		
		if(depth >= t.w)
		#ifdef RM_DOUBLEDEPTH
			if(depth <= t.z)
		#endif
			{
				bestDepth = depth;
				depth -= 2.0 * size;
			}

		depth += size;
	}

	return bestDepth;
}
#endif

void	main()
{
	if(bool(u_PortalClipping))
	{
		float dist = dot(var_Position.xyz, u_PortalPlane.xyz) - u_PortalPlane.w;
		if(dist < 0.0)
		{
			discard;
			return;
		}
	}

#if defined(r_NormalMapping)
	// construct object-space-to-tangent-space 3x3 matrix
	mat3 objectToTangentMatrix;
	if(gl_FrontFacing)
	{
		objectToTangentMatrix = mat3( -var_Tangent.x, -var_Binormal.x, -var_Normal.x,
							-var_Tangent.y, -var_Binormal.y, -var_Normal.y,
							-var_Tangent.z, -var_Binormal.z, -var_Normal.z	);
	}
	else
	{
		objectToTangentMatrix = mat3(	var_Tangent.x, var_Binormal.x, var_Normal.x,
							var_Tangent.y, var_Binormal.y, var_Normal.y,
							var_Tangent.z, var_Binormal.z, var_Normal.z	);
	}
	
	// compute view direction in tangent space
	vec3 V = normalize(objectToTangentMatrix * (u_ViewOrigin - var_Position));
#endif
	
	vec2 texDiffuse = var_TexDiffuseNormal.st;

#if defined(r_NormalMapping)
	vec2 texNormal = var_TexDiffuseNormal.pq;
	vec2 texSpecular = var_TexSpecular.st;
#endif

#if defined(r_ParallaxMapping)
	if(bool(u_ParallaxMapping))
	{
		// ray intersect in view direction
		
		// size and start position of search in texture space
		vec2 S = V.xy * -u_DepthScale / V.z;
			
		float depth = RayIntersectDisplaceMap(texNormal, S);
		
		// compute texcoords offset
		vec2 texOffset = S * depth;
		
		texDiffuse.st += texOffset;
		texNormal.st += texOffset;
		texSpecular.st += texOffset;
	}
#endif

	// compute the diffuse term
	vec4 diffuse = texture2D(u_DiffuseMap, texDiffuse);
	if(u_AlphaTest == ATEST_GT_0 && diffuse.a <= 0.0)
	{
		discard;
		return;
	}
	else if(u_AlphaTest == ATEST_LT_128 && diffuse.a >= 0.5)
	{
		discard;
		return;
	}
	else if(u_AlphaTest == ATEST_GE_128 && diffuse.a < 0.5)
	{
		discard;
		return;
	}

#if defined(r_NormalMapping)

	// compute normal in tangent space from normalmap
	vec3 N = 2.0 * (texture2D(u_NormalMap, texNormal).xyz - 0.5);
	#if defined(r_NormalScale)
	N.z *= r_NormalScale;
	normalize(N);
	#endif

#if defined(COMPAT_Q3A)
	// fake bump mapping
 	vec3 L = N;
#else
	// compute light direction in tangent space
	vec3 L = normalize(objectToTangentMatrix * var_LightDirection);
#endif
 
 	// compute half angle in tangent space
	vec3 H = normalize(L + V);
	
	// compute the light term
	vec3 light = var_LightColor.rgb * clamp(dot(N, L), 0.0, 1.0);
	
	// compute the specular term
	vec3 specular = texture2D(u_SpecularMap, texSpecular).rgb * var_LightColor.rgb * pow(clamp(dot(N, H), 0.0, 1.0), r_SpecularExponent) * r_SpecularScale;
	
	// compute final color
	vec4 color = diffuse;
	color.rgb *= light;
	color.rgb += specular;
	
//	color.rgb *= var_Color.rgb;	// apply model paint color for terrain blending
	
//	color.rgb = var_LightDirection.rgb;
	gl_FragColor = color;

#elif defined(COMPAT_Q3A)

	gl_FragColor = vec4(diffuse.rgb * var_LightColor.rgb, diffuse.a);

#else
	vec3 N;
	if(gl_FrontFacing)
		N = -normalize(var_Normal);
	else
		N = normalize(var_Normal);
	
	vec3 L = normalize(var_LightDirection);
	
	gl_FragColor = vec4(diffuse.rgb * var_LightColor.rgb * clamp(dot(N, L), 0.0, 1.0), diffuse.a);
#endif
}
