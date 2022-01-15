cbuffer constants : register(b0)
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
	VS_OUTPUT output;
	output.pos = mul(g_worldViewProj, float4(input.pos, 1));
	output.texCoord = input.texCoord;
	output.color = input.color;
	return output;
}
