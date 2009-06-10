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
uniform sampler2D	u_ShadowMap;
uniform vec3		u_ViewOrigin;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightColor;
uniform float		u_LightRadius;
uniform float		u_LightScale;
uniform int			u_ShadowCompare;
uniform float       u_ShadowTexelSize;
uniform float       u_ShadowBlur;
uniform mat4		u_ModelMatrix;
uniform int         u_PortalClipping;
uniform vec4		u_PortalPlane;

varying vec4		var_Position;
varying vec4		var_TexDiffuse;
varying vec4		var_TexNormal;
#if defined(r_NormalMapping)
varying vec2		var_TexSpecular;
#endif
varying vec4		var_TexAtten;
varying vec4		var_Tangent;
varying vec4		var_Binormal;
varying vec4		var_Normal;


#if defined(PCSS)
float SumBlocker(vec4 SP, float vertexDistance, float filterWidth, float samples)
{
	float stepSize = 2.0 * filterWidth / samples;
	
	float blockerCount = 0.0;
    float blockerSum = 0.0;
    
	for(float i = -filterWidth; i < filterWidth; i += stepSize)
	{
		for(float j = -filterWidth; j < filterWidth; j += stepSize)
		{
			float shadowDistance = texture2DProj(u_ShadowMap, vec3(SP.xy + vec2(i, j), SP.w)).x;
			//float shadowDistance = texture2D(u_ShadowMap, SP.xy / SP.w + vec2(i, j)).x;
			
			// FIXME VSM_CLAMP
			
			if(vertexDistance > shadowDistance)
			{
				blockerCount += 1.0;
				blockerSum += shadowDistance;
			}
		}
	}
	
	float result;
	if(blockerCount > 0.0)
		result = blockerSum / blockerCount;
	else
		result = 0.0;
	
	return result;
}

float EstimatePenumbra(float vertexDistance, float blocker)
{
	float penumbra;
	
	if(blocker == 0.0)
		penumbra = 0.0;
	else
		penumbra = ((vertexDistance - blocker) * u_LightRadius) / blocker;
	
	return penumbra;
}
#endif

#if defined(VSM) || defined(ESM)
vec4 PCF(vec4 SP, float filterWidth, float samples)
{
	// compute step size for iterating through the kernel
	float stepSize = 2.0 * filterWidth / samples;
	
	vec4 moments = vec4(0.0, 0.0, 0.0, 0.0);
	for(float i = -filterWidth; i < filterWidth; i += stepSize)
	{
		for(float j = -filterWidth; j < filterWidth; j += stepSize)
		{
			//moments += texture2DProj(u_ShadowMap, vec3(SP.xy + vec2(i, j), SP.w));
			moments += texture2D(u_ShadowMap, SP.xy / SP.w + vec2(i, j));
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

	if(var_TexAtten.q <= 0.0)
	{
		discard;
	}
	
#if defined(VSM)
	if(bool(u_ShadowCompare))
	{
		vec4 SP;	// shadow point in shadow space
		SP.x = var_Position.w;
		SP.y = var_Tangent.w;
		SP.z = var_Binormal.w;
		SP.w = var_Normal.w;
		
		// compute incident ray
		vec3 I = var_Position.xyz - u_LightOrigin;
		
		const float	SHADOW_BIAS = 0.001;
		float vertexDistance = length(I) / u_LightRadius - SHADOW_BIAS;
		
		#if defined(PCF_2X2)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 2.0);
		#elif defined(PCF_3X3)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 3.0);
		#elif defined(PCF_4X4)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 4.0);
		#elif defined(PCF_5X5)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 5.0);
		#elif defined(PCF_6X6)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 6.0);
		#elif defined(PCSS)
		
		// step 1: find blocker estimate
		float blockerSearchWidth = u_ShadowTexelSize * u_LightRadius / vertexDistance;
		float blockerSamples = 6.0; // how many samples to use for blocker search
		float blocker = SumBlocker(SP, vertexDistance, blockerSearchWidth, blockerSamples);
		
		#if 0
		// uncomment to visualize blockers
		gl_FragColor = vec4(blocker * 0.3, 0.0, 0.0, 1.0);
		return;
		#endif
		
		// step 2: estimate penumbra using parallel planes approximation
		float penumbra = EstimatePenumbra(vertexDistance, blocker);
		
		#if 0
		// uncomment to visualize penumbrae
		gl_FragColor = vec4(0.0, 0.0, penumbra, 1.0);
		return;
		#endif
		
		// step 3: compute percentage-closer filter
		vec4 shadowMoments;
		if(penumbra > 0.0)
		{
			const float PCFsamples = 4.0;
			
			/*
			float maxpen = PCFsamples * (1.0 / u_ShadowTexelSize);
			if(penumbra > maxpen)
				penumbra = maxpen;
			*/
		
			shadowMoments = PCF(SP, penumbra, PCFsamples);
		}
		else
		{
			shadowMoments = texture2DProj(u_ShadowMap, SP.xyw);
		}
		#else
		
		// no filter
		vec4 shadowMoments = texture2DProj(u_ShadowMap, SP.xyw);
		#endif
	
		#if defined(VSM_CLAMP)
		// convert to [-1, 1] vector space
		shadowMoments = 2.0 * (shadowMoments - 0.5);
		#endif
		
		float shadowDistance = shadowMoments.r;
		float shadowDistanceSquared = shadowMoments.a;
	
		// standard shadow map comparison
		shadow = vertexDistance <= shadowDistance ? 1.0 : 0.0;
	
		// variance shadow mapping
		float E_x2 = shadowDistanceSquared;
		float Ex_2 = shadowDistance * shadowDistance;
	
		// AndyTX: VSM_EPSILON is there to avoid some ugly numeric instability with fp16
		float variance = max(E_x2 - Ex_2, VSM_EPSILON);
		//float variance = smoothstep(VSM_EPSILON, 1.0, max(E_x2 - Ex_2, 0.0));
	
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
		vec4 SP;	// shadow point in shadow space
		SP.x = var_Position.w;
		SP.y = var_Tangent.w;
		SP.z = var_Binormal.w;
		SP.w = var_Normal.w;
		
		// compute incident ray
		vec3 I = var_Position.xyz - u_LightOrigin;
		
		const float	SHADOW_BIAS = 0.001;
		float vertexDistance = (length(I) / u_LightRadius) * r_ShadowMapDepthScale; // - SHADOW_BIAS;
		
		#if defined(PCF_2X2)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 2.0);
		#elif defined(PCF_3X3)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 3.0);
		#elif defined(PCF_4X4)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 4.0);
		#elif defined(PCF_5X5)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 5.0);
		#elif defined(PCF_6X6)
		vec4 shadowMoments = PCF(SP, u_ShadowTexelSize * u_ShadowBlur, 6.0);
		#else
		// no filter
		vec4 shadowMoments = texture2DProj(u_ShadowMap, SP.xyw);
		#endif
		
		float shadowDistance = shadowMoments.a;
		
		// exponential shadow mapping
		//shadow = vertexDistance <= shadowDistance ? 1.0 : 0.0;
		shadow = clamp(exp(r_OverDarkeningFactor * (shadowDistance - vertexDistance)), 0.0, 1.0);
		//shadow = smoothstep(0.0, 1.0, shadow);
		
		#if defined(DEBUG_ESM)
		gl_FragColor.r = DEBUG_ESM & 1 ? shadowDistance : 0.0;
		gl_FragColor.g = DEBUG_ESM & 2 ? -(shadowDistance - vertexDistance) : 0.0;
		gl_FragColor.b = DEBUG_ESM & 4 ? shadow : 0.0;
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
		vec3 L = normalize(u_LightOrigin - var_Position.xyz);
	
#if defined(r_NormalMapping)
		// compute view direction in world space
		vec3 V = normalize(u_ViewOrigin - var_Position.xyz);
	
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
		#if 0
		vec3 texAtten = var_TexAtten.xyz / var_TexAtten.w;
		vec3 attenuationXY = texture2D(u_AttenuationMapXY, texAtten.xy).rgb;
		vec3 attenuationZ  = texture2D(u_AttenuationMapZ, vec2(texAtten.z, 0.0)).rgb;
		#else
		vec3 attenuationXY = texture2DProj(u_AttenuationMapXY, var_TexAtten.xyw).rgb;
		vec3 attenuationZ  = texture2D(u_AttenuationMapZ, vec2(clamp(var_TexAtten.z, 0.0, 1.0), 0.0)).rgb;
		//vec3 attenuationZ = var_TexAtten.z;
		#endif

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
