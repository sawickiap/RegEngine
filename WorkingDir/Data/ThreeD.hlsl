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
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float3 normal_View : NORMAL;
	float2 texCoord : TEXCOORD;
};

////////////////////////////////////////////////////////////////////////////////
#if VERTEX_SHADER

VS_OUTPUT MainVS(VS_INPUT input)
{
	float4 pos_Clip = mul(perObjectConstants.WorldViewProj, float4(input.pos_Local, 1));
	
	VS_OUTPUT output;
	output.pos = pos_Clip;
	output.normal_View = mul((float3x3)perObjectConstants.WorldView, input.normal_Local);
	output.texCoord = input.texCoord;
	return output;
}

////////////////////////////////////////////////////////////////////////////////
#elif PIXEL_SHADER

Texture2D<float4> t : register(t0);
SamplerState s : register(s0);

void MainPS(
	VS_OUTPUT input,
	out float4 outAlbedo : SV_Target0,
	out float4 outNormal : SV_Target1)
{
	outAlbedo.rgb = t.Sample(s, input.texCoord).rgb;
	outAlbedo.a = 1.0;

	outNormal.rgb = input.normal_View;
	outNormal.a = 1.0;
}

#endif
