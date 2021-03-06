#include "Include/Common.hlsl"

/*
Macros:
ALPHA_TEST = 0, 1
HAS_MATERIAL_COLOR = 0, 1
HAS_ALBEDO_TEXTURE = 0, 1
HAS_NORMAL_TEXTURE = 0, 1
*/

struct PerObjectConstants
{
	float4x4 WorldViewProj;
	float4x4 WorldView;
};
ConstantBuffer<PerObjectConstants> perObjectConstants : register(b1);

struct PerMaterialConstants
{
	uint Flags; // Use MATERIAL_FLAG_*
	float AlphaCutoff; // Valid only when (Flags & MATERIAL_FLAG_ALPHA_MASK)
	uint2 _padding0;

	float3 Color; // Valid only when (Flags & MATERIAL_FLAG_HAS_MATERIAL_COLOR)
	uint _padding1;
};
ConstantBuffer<PerMaterialConstants> perMaterialConstants : register(b2);

struct VS_INPUT
{
	float3 pos_Local : POSITION;
	float3 normal_Local : NORMAL;
	float3 tangent_Local : TANGENT;
	float3 bitangent_Local : BITANGENT;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

struct VS_OUTPUT
{
	float4 pos_Clip : SV_POSITION;
	float3 normal_Local : NORMAL;
	float3 tangent_Local : TANGENT;
	float3 bitangent_Local : BITANGENT;
	float2 texCoord : TEXCOORD;
};

////////////////////////////////////////////////////////////////////////////////
#if VERTEX_SHADER

VS_OUTPUT MainVS(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos_Clip = mul(perObjectConstants.WorldViewProj, float4(input.pos_Local, 1.0));
	//output.normal_View = mul((float3x3)perObjectConstants.WorldView, input.normal_Local);
	output.normal_Local = input.normal_Local;
	output.tangent_Local = input.tangent_Local;
	output.bitangent_Local = input.bitangent_Local;
	output.texCoord = input.texCoord;
	return output;
}

////////////////////////////////////////////////////////////////////////////////
#elif PIXEL_SHADER

Texture2D<float4> albedoTexture : register(t0); // Valid only when (Flags & MATERIAL_FLAG_HAS_ALBEDO_TEXTURE)
Texture2D<float4> normalTexture : register(t1); // Valid only when (Flags & MATERIAL_FLAG_HAS_NORMAL_TEXTURE)
SamplerState albedoSampler : register(s0); // Valid only when (Flags & MATERIAL_FLAG_HAS_ALBEDO_TEXTURE)
SamplerState normalSampler : register(s1); // Valid only when (Flags & MATERIAL_FLAG_HAS_NORMAL_TEXTURE)

void MainPS(
	VS_OUTPUT input,
	out float4 outAlbedo : SV_Target0,
	out float4 outNormal_View : SV_Target1)
{
	float4 albedoColor = 1.0.xxxx;
#if HAS_MATERIAL_COLOR
	albedoColor.rgb *= perMaterialConstants.Color;
#endif
#if HAS_ALBEDO_TEXTURE
	albedoColor *= albedoTexture.Sample(albedoSampler, input.texCoord);
#endif
#if ALPHA_TEST
	clip(albedoColor.a - perMaterialConstants.AlphaCutoff);
#endif
	outAlbedo = float4(albedoColor.rgb, 1.0);

#if HAS_NORMAL_TEXTURE
	float3 normal_Tangent = normalTexture.Sample(normalSampler, input.texCoord).rgb * 2.0 - 1.0;
	// TODO check which normalize() are required and which are not.
	normal_Tangent = normalize(normal_Tangent);
	float3x3 TBN = float3x3(
		normalize(input.tangent_Local),
		normalize(input.bitangent_Local),
		normalize(input.normal_Local));
	float3 normal_Local = mul(normal_Tangent, TBN);
#else
	float3 normal_Local = input.normal_Local;
#endif
	float3 normal_View = mul((float3x3)perObjectConstants.WorldView, normal_Local);

	outNormal_View = float4(normalize(normal_View), 1.0);
}

#endif
