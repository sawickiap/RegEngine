#include "Include/Common.hlsl"
#include "Include/LightingCommon.hlsl"

#if PIXEL_SHADER

float4 MainPS(float4 pos : SV_Position) : SV_Target
{
	int3 loadPos = int3(pos.xy, 0);
	float3 albedo = GBufferAlbedo.Load(loadPos).rgb;
	float3 color = albedo * perFrameConstants.AmbientColor;
	return float4(color, 1.0);
}

#endif
