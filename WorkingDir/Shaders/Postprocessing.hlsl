#include "Include/Common.hlsl"

#if PIXEL_SHADER

Texture2D<float4> t : register(t0);

float4 MainPS(float4 pos : SV_Position) : SV_Target
{
	float3 res = t.Load(int3(pos.xy, 0)).rgb;
	
	float gammaInv = 1/2.2;
	res = pow(res, gammaInv);

	return float4(res, 1);
}

#endif
