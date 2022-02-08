#include "Include/Common.hlsl"

Texture2D<float4> t : register(t0);
SamplerState s : register(s0);

struct PS_INPUT
{
	float4 pos_Screen : SV_POSITION;
	float3 normal_View : NORMAL;
	float2 texCoord : TEXCOORD;
};

void main(
	PS_INPUT input,
	out float4 outAlbedo : SV_Target0,
	out float4 outNormal : SV_Target1)
{
	outAlbedo.rgb = t.Sample(s, input.texCoord).rgb;
	outAlbedo.a = 1.0;

	outNormal.rgb = input.normal_View;
	outNormal.a = 1.0;
}
