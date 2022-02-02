#include "Include/Common.hlsl"

Texture2D<float4> t : register(t0);
SamplerState s : register(s0);

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float3 normal_View : NORMAL;
	float2 texCoord : TEXCOORD;
	float4 color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target
{
	float3 normal_View = normalize(input.normal_View);
	float3 albedoColor = t.Sample(s, input.texCoord).rgb;
	albedoColor *= input.color.rgb;
	float3 lighting = perFrameConstants.AmbientColor;
	lighting += max(0, dot(normal_View, perFrameConstants.DirToLight_View)) * perFrameConstants.LightColor;
	return float4(albedoColor * lighting, 1);
}
