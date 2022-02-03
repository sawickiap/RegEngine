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
	float3 albedoColor = t.Sample(s, input.texCoord).rgb;
	
	float3 normal_View = normalize(input.normal_View);
	float diffuseTerm = max(0.0, dot(normal_View, perFrameConstants.DirToLight_View));
	float3 reflected_View = reflect(-perFrameConstants.DirToLight_View, normal_View);
	float3 dirToCam_View = float3(0., 0., -1.);
	float specularTerm = pow(max(0.0, dot(dirToCam_View, reflected_View)), 15.0);
	
	float3 color = perFrameConstants.AmbientColor * albedoColor;
	color += diffuseTerm * albedoColor * perFrameConstants.LightColor;
	color += specularTerm * diffuseTerm * perFrameConstants.LightColor;

	return float4(color, 1);
}
