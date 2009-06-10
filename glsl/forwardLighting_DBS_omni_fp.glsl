/*
===========================================================================
Copyright (C) 2007-2009 Robert Beckebans <trebor_7@users.sourceforge.net>

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
uniform sampler2D	u_AttenuationMapXY;
uniform sampler2D	u_AttenuationMapZ;
#if defined(VSM) || defined(ESM)
uniform samplerCube	u_ShadowMap;
#endif
uniform vec3		u_ViewOrigin;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightColor;
uniform float		u_LightRadius;
uniform float       u_LightScale;
uniform int			u_ShadowCompare;
uniform float       u_ShadowTexelSize;
uniform float       u_ShadowBlur;
uniform int         u_PortalClipping;
uniform vec4		u_PortalPlane;

varying vec3		var_Position;
varying vec4		var_TexDiffuse;
varying vec4		var_TexNormal;
#if defined(r_NormalMapping)
varying vec2		var_TexSpecular;
#endif
varying vec3		var_TexAttenXYZ;
#if defined(r_NormalMapping)
varying vec4		var_Tangent;
varying vec4		var_Binormal;
#endif
varying vec4		var_Normal;
//varying vec4		var_Color;


#if defined(VSM) || defined(ESM)

/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors(const vec3 forward, inout vec3 right, inout vec3 up)
{
	// this rotate and negate guarantees a vector
	// not colinear with the original
	right.y = -forward.x;
	right.z = forward.y;
	right.x = forward.z;

	float d = dot(right, forward);
	right += forward * -d;
	normalize(right);
	up = cross(right, forward);	// GLSL cross product is the same as in Q3A
}

vec4 PCF(vec3 I, float filterWidth, float samples)
{
	vec3 forward, right, up;
	
	forward = normalize(I);
	MakeNormalVectors(forward, right, up);

	// compute step size for iterating through the kernel
	float stepSize = 2.0 * filterWidth / samples;
	
	vec4 moments = vec4(0.0, 0.0, 0.0, 0.0);
	for(float i = -filterWidth; i < filterWidth; i += stepSize)
	{
		for(float j = -filterWidth; j < filterWidth; j += stepSize)
		{
			moments += textureCube(u_ShadowMap, I + right * i + up * j);
		}
	}
	
	// return average of the samples
	moments *= (1.0 / (samples * samples));
	return moments;
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

	float shadow = 1.0;

#if defined(VSM)
	if(bool(u_ShadowCompare))
	{
		// compute incident ray
		vec3 I = var_Position - u_LightOrigin;
	
		#if defined(PCF_2X2)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 2.0);
		#elif defined(PCF_3X3)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 3.0);
		#elif defined(PCF_4X4)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 4.0);
		#elif defined(PCF_5X5)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 5.0);
		#elif defined(PCF_6X6)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 6.0);
		#else
		vec4 shadowMoments = textureCube(u_ShadowMap, I);
		#endif
		
		#if defined(VSM_CLAMP)
		// convert to [-1, 1] vector space
		shadowMoments = 2.0 * (shadowMoments - 0.5);
		#endif
	
		float shadowDistance = shadowMoments.r;
		float shadowDistanceSquared = shadowMoments.a;
		
		const float	SHADOW_BIAS = 0.001;
		float vertexDistance = length(I) / u_LightRadius - SHADOW_BIAS;
	
		// standard shadow map comparison
		shadow = vertexDistance <= shadowDistance ? 1.0 : 0.0;
	
		// variance shadow mapping
		float E_x2 = shadowDistanceSquared;
		float Ex_2 = shadowDistance * shadowDistance;
	
		// AndyTX: VSM_EPSILON is there to avoid some ugly numeric instability with fp16
		float variance = max(E_x2 - Ex_2, VSM_EPSILON);
	
		// compute probabilistic upper bound
		float mD = shadowDistance - vertexDistance;
		float mD_2 = mD * mD;
		float p = variance / (variance + mD_2);
		
		#if defined(r_LightBleedReduction)
		p = smoothstep(r_LightBleedReduction, 1.0, p);
		#endif
	
		#if defined(DEBUG_VSM)
		gl_FragColor.r = DEBUG_VSM & 1 ? variance : 0.0;
		gl_FragColor.g = DEBUG_VSM & 2 ? mD_2 : 0.0;
		gl_FragColor.b = DEBUG_VSM & 4 ? p : 0.0;
		gl_FragColor.a = 1.0;
		return;
		#else
		shadow = max(shadow, p);
		#endif
	}
	
	if(shadow <= 0.0)
	{
		discard;
	}
	else
#elif defined(ESM)
	if(bool(u_ShadowCompare))
	{
		// compute incident ray
		vec3 I = var_Position - u_LightOrigin;
	
		#if defined(PCF_2X2)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 2.0);
		#elif defined(PCF_3X3)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 3.0);
		#elif defined(PCF_4X4)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 4.0);
		#elif defined(PCF_5X5)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 5.0);
		#elif defined(PCF_6X6)
		vec4 shadowMoments = PCF(I, u_ShadowTexelSize * u_ShadowBlur * length(I), 6.0);
		#else
		vec4 shadowMoments = textureCube(u_ShadowMap, I);
		#endif
		
		const float	SHADOW_BIAS = 0.001;
		float vertexDistance = (length(I) / u_LightRadius) * r_ShadowMapDepthScale;// - SHADOW_BIAS;
		
		float shadowDistance = shadowMoments.a;
		
		// exponential shadow mapping
		shadow = clamp(exp(r_OverDarkeningFactor * (shadowDistance - vertexDistance)), 0.0, 1.0);
		
		#if defined(DEBUG_ESM)
		gl_FragColor.r = DEBUG_ESM & 1 ? shadowDistance : 0.0;
		gl_FragColor.g = DEBUG_ESM & 2 ? -(shadowDistance - vertexDistance) : 0.0;
		gl_FragColor.b = DEBUG_ESM & 4 ? 1.0 - shadow : 0.0;
		gl_FragColor.a = 1.0;
		return;
		#endif
	}
	
	if(shadow <= 0.0)
	{
		discard;
	}
	else
#endif
	{
		// compute light direction in world space
		vec3 L = normalize(u_LightOrigin - var_Position);
	
#if defined(r_NormalMapping)
		// compute view direction in world space
		vec3 V = normalize(u_ViewOrigin - var_Position);
	
		// compute half angle in world space
		vec3 H = normalize(L + V);
	
		// compute normal in tangent space from normalmap
		vec3 N = 2.0 * (texture2D(u_NormalMap, var_TexNormal.st).xyz - 0.5);
		#if defined(r_NormalScale)
		N.z *= r_NormalScale;
		normalize(N);
		#endif
		
		// invert tangent space for twosided surfaces
		mat3 tangentToWorldMatrix;
		if(gl_FrontFacing)
			tangentToWorldMatrix = mat3(-var_Tangent.xyz, -var_Binormal.xyz, -var_Normal.xyz);
		else
			tangentToWorldMatrix = mat3(var_Tangent.xyz, var_Binormal.xyz, var_Normal.xyz);
	
		// transform normal into world space
		N = tangentToWorldMatrix * N;
#else
		vec3 N;
		if(gl_FrontFacing)
			N = -normalize(var_Normal.xyz);
		else
			N = normalize(var_Normal.xyz);
#endif
	
		// compute the diffuse term
		vec4 diffuse = texture2D(u_DiffuseMap, var_TexDiffuse.st);
		diffuse.rgb *= u_LightColor * clamp(dot(N, L), 0.0, 1.0);
	
#if defined(r_NormalMapping)
		// compute the specular term
		vec3 specular = texture2D(u_SpecularMap, var_TexSpecular).rgb * u_LightColor * pow(clamp(dot(N, H), 0.0, 1.0), r_SpecularExponent) * r_SpecularScale;
#endif
	
		// compute attenuation
		vec3 attenuationXY		= texture2D(u_AttenuationMapXY, var_TexAttenXYZ.xy).rgb;
		vec3 attenuationZ		= texture2D(u_AttenuationMapZ, vec2(var_TexAttenXYZ.z, 0)).rgb;
					
		// compute final color
		vec4 color = diffuse;
#if defined(r_NormalMapping)
		color.rgb += specular;
#endif
		color.rgb *= attenuationXY;
		color.rgb *= attenuationZ;
		color.rgb *= u_LightScale;
		color.rgb *= shadow;
	
		color.r *= var_TexDiffuse.p;
		color.gb *= var_TexNormal.pq;

		gl_FragColor = color;
	}
}
