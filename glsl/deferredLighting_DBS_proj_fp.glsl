/*
===========================================================================
Copyright (C) 2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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
uniform sampler2D	u_DepthMap;
uniform sampler2D	u_AttenuationMapXY;
uniform sampler2D	u_AttenuationMapZ;
uniform sampler2D	u_ShadowMap;
uniform vec3		u_ViewOrigin;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightColor;
uniform float		u_LightRadius;
uniform float       u_LightScale;
uniform mat4		u_LightAttenuationMatrix;
#if !defined(GLHW_ATI) && !defined(GLHW_ATI_DX10)
uniform vec4		u_LightFrustum[6];
#endif
uniform mat4		u_ShadowMatrix;
uniform int			u_ShadowCompare;
uniform int         u_PortalClipping;
uniform vec4		u_PortalPlane;
uniform mat4		u_UnprojectMatrix;

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
	
	if(bool(u_PortalClipping))
	{
		float dist = dot(P.xyz, u_PortalPlane.xyz) - u_PortalPlane.w;
		if(dist < 0.0)
		{
			discard;
			return;
		}
	}
	
	// transform vertex position into light space
	vec4 texAtten			= u_LightAttenuationMatrix * vec4(P.xyz, 1.0);
	if(texAtten.q <= 0.0)
	{
		// point is behind the near clip plane
		discard;
		return;
	}
	
#if !defined(GLHW_ATI) && !defined(GLHW_ATI_DX10)
	// make sure that the vertex position is inside the light frustum
	for(int i = 0; i < 6; ++i)
	{
		vec4 plane = u_LightFrustum[i];

		float dist = dot(P.xyz, plane.xyz) - plane.w;
		if(dist < 0.0)
		{
			discard;
			return;
		}
	}
#endif

	float shadow = 1.0;
	
#if defined(VSM)
	if(bool(u_ShadowCompare))
	{
		// compute incident ray
		vec3 I = P.xyz - u_LightOrigin;
		
		const float	SHADOW_BIAS = 0.001;
		float vertexDistance = length(I) / u_LightRadius - SHADOW_BIAS;
		
		// no filter
		vec4 texShadow = u_ShadowMatrix * vec4(P.xyz, 1.0);
		vec4 shadowMoments = texture2DProj(u_ShadowMap, texShadow.xyw);
		//vec4 shadowMoments = texture2DProj(u_ShadowMap, SP.xyw);
	
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
		float variance = min(max(E_x2 - Ex_2, 0.0) + VSM_EPSILON, 1.0);
		//float variance = smoothstep(VSM_EPSILON, 1.0, max(E_x2 - Ex_2, 0.0));
	
		float mD = shadowDistance - vertexDistance;
		float mD_2 = mD * mD;
		float p = variance / (variance + mD_2);
		p = smoothstep(0.0, 1.0, p);
	
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
		vec3 I = P.xyz - u_LightOrigin;
		
		// no filter
		vec4 texShadow = u_ShadowMatrix * vec4(P.xyz, 1.0);
		vec4 shadowMoments = texture2DProj(u_ShadowMap, texShadow.xyw);
		
		const float	SHADOW_BIAS = 0.001;
		float vertexDistance = (length(I) / u_LightRadius) * r_ShadowMapDepthScale; // - SHADOW_BIAS;
		
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
		// compute the diffuse term
		vec4 diffuse = texture2D(u_DiffuseMap, st);
	
		// compute normal in world space
		vec3 N = 2.0 * (texture2D(u_NormalMap, st).xyz - 0.5);
		
		//vec3 N;
		//N.x = diffuse.a;
		//N.y = S.a;
		//N.z = P.w;
		//N.xyz = 2.0 * (N.xyz - 0.5);
		//N.z = sqrt(1.0 - dot(N.xy, N.xy));
	
		// compute light direction in world space
		vec3 L = normalize(u_LightOrigin - P.xyz);
	
#if defined(r_NormalMapping)
		// compute view direction in world space
		vec3 V = normalize(u_ViewOrigin - P.xyz);
	
		// compute half angle in world space
		vec3 H = normalize(L + V);
		
		// compute the specular term
		vec4 S = texture2D(u_SpecularMap, st);
#endif
	
		// compute attenuation
		vec3 attenuationXY = texture2DProj(u_AttenuationMapXY, texAtten.xyw).rgb;
		vec3 attenuationZ  = texture2D(u_AttenuationMapZ, vec2(clamp(texAtten.z, 0.0, 1.0), 0.0)).rgb;
	
		// compute final color
		vec4 color = diffuse;
		color.rgb *= u_LightColor * clamp(dot(N, L), 0.0, 1.0);
#if defined(r_NormalMapping)
		color.rgb += S.rgb * u_LightColor * pow(clamp(dot(N, H), 0.0, 1.0), r_SpecularExponent) * r_SpecularScale;
#endif
		color.rgb *= attenuationXY;
		color.rgb *= attenuationZ;
		color.rgb *= u_LightScale;
		color.rgb *= shadow;
		
		gl_FragColor = color;
	}
}
