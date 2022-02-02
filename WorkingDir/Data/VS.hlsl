#include "Include/Common.hlsl"

struct PerObjectConstants
{
	float4x4 WorldViewProj;
	float4x4 WorldView;
};
ConstantBuffer<PerObjectConstants> perObjectConstants : register(b1);

struct VS_INPUT
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float3 normal_View : NORMAL;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	float3 pos = input.pos;
	//pos.z += sin(g_SceneTime);

	VS_OUTPUT output;
	output.pos = mul(perObjectConstants.WorldViewProj, float4(pos, 1));
	output.normal_View = mul((float3x3)perObjectConstants.WorldView, input.normal);
	output.texCoord = input.texCoord;
	output.color = input.color;
	return output;
}
