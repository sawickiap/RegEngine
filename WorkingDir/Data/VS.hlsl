#include "Include/Common.hlsl"

cbuffer constants : register(b1)
{
	float4x4 g_worldViewProj;
};

struct VS_INPUT
{
	float3 pos : POSITION;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	float3 pos = input.pos;
	//pos.z += sin(g_SceneTime);

	VS_OUTPUT output;
	output.pos = mul(g_worldViewProj, float4(pos, 1));
	output.texCoord = input.texCoord;
	output.color = input.color;
	return output;
}
