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
	float4 pos_Clip : SV_POSITION;
	float3 pos_View : POS_VIEW;
	float3 normal_View : NORMAL;
	float2 texCoord : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos_Clip = mul(perObjectConstants.WorldViewProj, float4(input.pos_Local, 1));
	output.pos_View = mul(perObjectConstants.WorldView, float4(input.pos_Local, 1)).xyz;
	output.normal_View = mul((float3x3)perObjectConstants.WorldView, input.normal_Local);
	output.texCoord = input.texCoord;
	return output;
}
