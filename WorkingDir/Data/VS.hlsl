#include "Include/Common.hlsl"

struct PerObjectConstants
{
	float4x4 worldViewProj;
};
ConstantBuffer<PerObjectConstants> perObjectConstants : register(b1);

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
	output.pos = mul(perObjectConstants.worldViewProj, float4(pos, 1));
	output.texCoord = input.texCoord;
	output.color = input.color;
	return output;
}
