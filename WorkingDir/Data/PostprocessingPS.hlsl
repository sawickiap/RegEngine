Texture2D<float4> t : register(t0);

float4 main(float4 pos : SV_Position) : SV_Target
{
	float4 res = t.Load(int3(pos.xy, 0));
	return res;
}
