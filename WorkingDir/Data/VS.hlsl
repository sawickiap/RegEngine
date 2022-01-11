cbuffer constants : register(b0)
{
	float4x4 g_worldViewProj;
};

struct VS_INPUT
{
	uint vertexId : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	if(input.vertexId == 0)
	{
		output.pos = float4(0., 0., 0., 1.);
		output.color = float4(1., 0., 0., 1.);
	}
	else if(input.vertexId == 1)
	{
		output.pos = float4(1., 0., 0., 1.);
		output.color = float4(0., 1., 0., 1.);
	}
	else
	{
		output.pos = float4(0., 0., 1., 1.);
		output.color = float4(0., 0., 1., 1.);
	}
	output.pos = mul(g_worldViewProj, output.pos);
	return output;
}
