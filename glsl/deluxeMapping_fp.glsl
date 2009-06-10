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

uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_SpecularMap;
uniform sampler2D	u_LightMap;
uniform sampler2D	u_DeluxeMap;
uniform int			u_AlphaTest;
uniform vec3		u_ViewOrigin;
uniform int			u_ParallaxMapping;
uniform float		u_DepthScale;
uniform int         u_PortalClipping;
uniform vec4		u_PortalPlane;

varying vec3		var_Position;
varying vec2		var_TexDiffuse;
varying vec2		var_TexNormal;
varying vec2		var_TexSpecular;
varying vec2		var_TexLight;
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

#if defined(r_showLightMaps)
	gl_FragColor = texture2D(u_LightMap, var_TexLight);
#elif defined(r_showDeluxeMaps)
	gl_FragColor = texture2D(u_DeluxeMap, var_TexLight);
#else

	// invert tangent space for two sided surfaces
	mat3 tangentToWorldMatrix;
	if(gl_FrontFacing)
		tangentToWorldMatrix = mat3(-var_Tangent.xyz, -var_Binormal.xyz, -var_Normal.xyz);
	else
		tangentToWorldMatrix = mat3(var_Tangent.xyz, var_Binormal.xyz, var_Normal.xyz);
	
	// compute view direction in world space
	vec3 I = normalize(u_ViewOrigin - var_Position);
	
	vec2 texDiffuse = var_TexDiffuse.st;
	vec2 texNormal = var_TexNormal.st;
	vec2 texSpecular = var_TexSpecular.st;

#if defined(r_ParallaxMapping)
	if(bool(u_ParallaxMapping))
	{
		// ray intersect in view direction
		
		mat3 worldToTangentMatrix;
		#if defined(GLHW_ATI) || defined(GLHW_ATI_DX10)
		worldToTangentMatrix = mat3(tangentToWorldMatrix[0][0], tangentToWorldMatrix[1][0], tangentToWorldMatrix[2][0],
									tangentToWorldMatrix[0][1], tangentToWorldMatrix[1][1], tangentToWorldMatrix[2][1], 
									tangentToWorldMatrix[0][2], tangentToWorldMatrix[1][2], tangentToWorldMatrix[2][2]);
		#else
		worldToTangentMatrix = transpose(tangentToWorldMatrix);
		#endif
	
		// compute view direction in tangent space
		vec3 V = worldToTangentMatrix * (u_ViewOrigin - var_Position.xyz);
		V = normalize(V);
		
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

	// compute normal in world space from normalmap
	vec3 N = tangentToWorldMatrix * (2.0 * (texture2D(u_NormalMap, texNormal).xyz - 0.5));
	
	// compute light direction in world space
	vec3 L = 2.0 * (texture2D(u_DeluxeMap, var_TexLight).xyz - 0.5);
	
	// compute half angle in world space
	vec3 H = normalize(L + I);
	
	// compute light color from world space lightmap
	vec3 lightColor = texture2D(u_LightMap, var_TexLight).rgb;
	
	diffuse.rgb *= lightColor.rgb * clamp(dot(N, L), 0.0, 1.0);
	
	// compute the specular term
	vec3 specular = texture2D(u_SpecularMap, texSpecular).rgb * lightColor * pow(clamp(dot(N, H), 0.0, 1.0), r_SpecularExponent) * r_SpecularScale;
	
	// compute final color
	gl_FragColor.rgba = diffuse;
	gl_FragColor.rgb += specular;
#endif
}
