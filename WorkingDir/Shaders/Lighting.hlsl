#include "Include/Common.hlsl"
#include "Include/LightingCommon.hlsl"

#if PIXEL_SHADER

struct Light
{
	float3 m_Color;
	uint m_Type; // Use LIGHT_TYPE_* from ShaderConstants.h
	
    // LIGHT_TYPE_DIRECTIONAL: direction to light (view space)
    // LIGHT_TYPE_POINT: position (view space)
	float3 m_DirectionToLight_Position;
	uint _padding0;
};
ConstantBuffer<Light> g_Light : register(b1);

#if 0
// a = roughness
// H = half between normal and light
float DistributionGGX(float3 N, float3 H, float a)
{
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
  
float GeometrySmith(float3 N, float3 V, float3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
#endif

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
float3 EnvBRDFApprox(float3 specularColor, float roughness, float ndotv)
{
	const float4 c0 = float4(-1, -0.0275, -0.572, 0.022);
	const float4 c1 = float4(1, 0.0425, 1.04, -0.04);
	float4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
	float2 AB = float2(-1.04, 1.04) * a004 + r.zw;
	return specularColor * AB.x + AB.y;
}

// St. Peter's Basilica SH
// https://www.shadertoy.com/view/lt2GRD
struct SHCoefficients
{
	float3 l00, l1m1, l10, l11, l2m2, l2m1, l20, l21, l22;
};

float3 EnvRemap(float3 c)
{
	return pow(2. * c, 2.2.xxx);
}

float3 SHIrradiance(float3 nrm)
{
	const SHCoefficients SH_STPETER = {
		float3(0.3623915, 0.2624130, 0.2326261),
		float3(0.1759131, 0.1436266, 0.1260569),
		float3(-0.0247311, -0.0101254, -0.0010745),
		float3(0.0346500, 0.0223184, 0.0101350),
		float3(0.0198140, 0.0144073, 0.0043987),
		float3(-0.0469596, -0.0254485, -0.0117786),
		float3(-0.0898667, -0.0760911, -0.0740964),
		float3(0.0050194, 0.0038841, 0.0001374),
		float3(-0.0818750, -0.0321501, 0.0033399)
	};
	const SHCoefficients c = SH_STPETER;
	const float c1 = 0.429043;
	const float c2 = 0.511664;
	const float c3 = 0.743125;
	const float c4 = 0.886227;
	const float c5 = 0.247708;
	return (
		c1 * c.l22 * (nrm.x * nrm.x - nrm.y * nrm.y) +
		c3 * c.l20 * nrm.z * nrm.z +
		c4 * c.l00 -
		c5 * c.l20 +
		2.0 * c1 * c.l2m2 * nrm.x * nrm.y +
		2.0 * c1 * c.l21  * nrm.x * nrm.z +
		2.0 * c1 * c.l2m1 * nrm.y * nrm.z +
		2.0 * c2 * c.l11  * nrm.x +
		2.0 * c2 * c.l1m1 * nrm.y +
		2.0 * c2 * c.l10  * nrm.z
		).xxx;
}

float VisibilityTerm(float roughness, float ndotv, float ndotl)
{
	float r2 = roughness * roughness;
	float gv = ndotl * sqrt(ndotv * (ndotv - ndotv * r2) + r2);
	float gl = ndotv * sqrt(ndotl * (ndotl - ndotl * r2) + r2);
	return 0.5 / max(gv + gl, 0.00001);
}

float DistributionTerm(float roughness, float ndoth)
{
	float r2 = roughness * roughness;
	float d = (ndoth * r2 - ndoth) * ndoth + 1.0;
	return r2 / (d * d * PI);
}

float3 FresnelTerm(float3 specularColor, float vdoth)
{
	float3 fresnel = specularColor + (1. - specularColor) * pow((1. - vdoth), 5.);
	return fresnel;
}

float4 MainPS(float4 pos : SV_Position) : SV_Target
{
	int3 loadPos = int3(pos.xy, 0);
	float depth = Depth.Load(loadPos).r;
	float3 albedo = GBufferAlbedo.Load(loadPos).rgb;
	float3 normal_View = normalize(GBufferNormal.Load(loadPos).rgb);
	
	float3 pos_Clip;
	pos_Clip.x = pos.x * perFrameConstants.RenderResolutionInv.x * 2. - 1.;
	pos_Clip.y = 1. - pos.y * perFrameConstants.RenderResolutionInv.y * 2.;
	pos_Clip.z = depth;
	float4 pos_ViewHomo = mul(perFrameConstants.ProjInv, float4(pos_Clip, 1.0));
	float3 pos_View = pos_ViewHomo.xyz / pos_ViewHomo.w;
	
	float3 dirToLight = g_Light.m_DirectionToLight_Position;
	float diffuseTerm = max(0.0, dot(normal_View, dirToLight));
	float3 reflected_View = reflect(-perFrameConstants.DirToLight_View, normal_View);
	float3 dirToCam_View = normalize(-pos_View);
	float specularTerm = pow(max(0.0, dot(dirToCam_View, reflected_View)), 15.0);
	
	//float3 color = g_Light.m_Color * diffuseTerm * (albedo + specularTerm);
	float3 color = g_Light.m_Color * diffuseTerm * (lerp(albedo, float3(1.0,1.0,1.0), specularTerm));

	// TEMP
	float3 l = normalize(dirToLight);
	float3 v = normalize(dirToCam_View);
	float3 n = normal_View;
	float3 h = normalize(l + n);
	// Fresnel term / specular color
	//const float3 F0 = float3(0.955, 0.638, 0.538);// Copper
	//const float3 F0 = float3(0.040, 0.040, 0.040);// Plastic
	const float3 F0 = albedo;
	const float roughness = 0.05;
	float isMetal = 0.;
	
#if 0
	float3 F_Schlick = F0 + (float3(1.0, 1.0, 1.0) - F0) * pow(1.0 - dot(n, l), 5.0);

	color = F_Schlick / 20.0;
	//color = h * 0.5 + 0.5;
#endif

#if 0
	float3 fLambert = albedo / PI;
	float D = DistributionGGX(n, h, roughness);
	float k_Direct = (roughness + 1.0) * (roughness + 1.0) / 8.0;
	float G = GeometrySmith(n, v, l, k_Direct);
	float3 F = fresnelSchlick(dot(n, h), F0);
	float3 kS = F; //specular
	float3 kD = 1.0.xxx - kS;//diffuse
	//float3 fCookTorrance = 1.0;
	//color = diffuseTerm * (kD * fLambert + kS * fCookTorrance);
	//color = F;
	color = ( kD * fLambert + D*F*G/( 4.0*dot(l,n)*dot(v,n) ) ) * g_Light.m_Color * max(0.0, dot(normal_View, dirToLight));
	//color = kS;
#endif

	float3 baseColor = albedo;
	float3 diffuseColor = isMetal == 1. ? 0.0.xxx : baseColor;
	float3 specularColor = isMetal == 1. ? baseColor : 0.02.xxx;
	float roughnessE = roughness * roughness;
	float roughnessL = max(.01, roughnessE);

	float3 refl = reflect(-dirToCam_View, normal_View);
	float3 diffuse = {0.,0.,0,};
	float3 specular = {0.,0.,0,};

	float3 halfVec = normalize(dirToCam_View + dirToLight);
	float vdoth = saturate(dot(dirToCam_View, halfVec));
	float ndoth = saturate(dot(normal_View, halfVec));
	float ndotv = saturate(dot(normal_View, dirToCam_View));
	float ndotl = saturate(dot(normal_View, dirToLight));
	float3 envSpecularColor = EnvBRDFApprox(specularColor, roughnessE, ndotv);

	float3 env1 = EnvRemap(float3(0.4, 0.4, 0.4));//texture(iChannel2, refl).xyz);
	float3 env2 = EnvRemap(float3(0.4, 0.4, 0.4));//texture(iChannel1, refl).xyz);
	float3 env3 = EnvRemap(float3(0.4, 0.4, 0.4));//SHIrradiance(refl));
	float3 env = lerp(env1, env2, saturate(roughnessE * 4.));
	env = lerp(env, env3, saturate((roughnessE - 0.25) / 0.75));

	diffuse += diffuseColor * EnvRemap(SHIrradiance(normal_View));
	specular += envSpecularColor * env;

	diffuse += diffuseColor * g_Light.m_Color * saturate(dot(normal_View, dirToLight));

	float3 lightF = FresnelTerm(specularColor, vdoth);
	float lightD = DistributionTerm(roughnessL, ndoth);
	float lightV = VisibilityTerm(roughnessL, ndotv, ndotl);
	specular += g_Light.m_Color * lightF * (lightD * lightV * PI * ndotl);

	float ao = 1.;//SceneAO(pos, normal_View, localToWorld);
	diffuse *= ao;
	specular *= saturate(pow(ndotv + ao, roughnessE) - 1. + ao);

	color = diffuse + specular;
	if (false)
	{
		color = diffuse;
	}
	if (false)
	{
		color = specular;
	}
	if (false)
	{
		color = lightD.xxx;
	}
	if (false)
	{
		color = envSpecularColor;
	}
	if (false)
	{
		color = lightV.xxx * (4.0f * ndotv * ndotl);
	}
	color = color * .4;

	return float4(color, 1.0);
}

#endif
