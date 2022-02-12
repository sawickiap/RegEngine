#include "Include/Common.hlsl"

struct PerObjectConstants
{
	float4x4 WorldViewProj;
	float4x4 WorldView;
};
ConstantBuffer<PerObjectConstants> perObjectConstants : register(b1);

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

Texture2D<float4> albedoTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);
SamplerState s : register(s0);

void MainPS(
	VS_OUTPUT input,
	out float4 outAlbedo : SV_Target0,
	out float4 outNormal_View : SV_Target1)
{
	outAlbedo.rgb = albedoTexture.Sample(s, input.texCoord).rgb;
	outAlbedo.a = 1.0;

	float3 normal_Tangent = normalTexture.Sample(s, input.texCoord).rgb * 2.0 - 1.0;
	// TODO check which normalize() are required and which are not.
	normal_Tangent = normalize(normal_Tangent);
	float3x3 TBN = float3x3(
		normalize(input.tangent_Local),
		normalize(input.bitangent_Local),
		normalize(input.normal_Local));
	float3 normal_Local = mul(normal_Tangent, TBN);
	float3 normal_View = mul((float3x3)perObjectConstants.WorldView, normal_Local);
	normal_View = normalize(normal_View);

	//TEMP
	//normal_View = mul((float3x3)perObjectConstants.WorldView, input.normal_Local);

	outNormal_View.rgb = normalize(normal_View);
	outNormal_View.a = 1.0;
}

#endif
