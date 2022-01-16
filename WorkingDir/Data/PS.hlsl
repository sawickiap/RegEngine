#include "Include/Common.hlsl"

Texture2D<float4> t : register(t0);
SamplerState s : register(s0);

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target
{
	return input.color * t.Sample(s, input.texCoord);
}
