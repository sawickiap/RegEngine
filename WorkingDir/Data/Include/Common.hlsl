#define PI 3.14159265358979323846264338327950288

struct PerFrameConstants
{
	uint FrameIndex;
	float SceneTime;
	float2 RenderResolution;
	
	float2 RenderResolutionInv;
	uint _padding0;

	float4x4 Proj;
	float4x4 ProjInv;

	float3 DirToLight_View; // Normalized
	uint _padding1;

	float3 LightColor;
	uint _padding2;

	float3 AmbientColor;
	uint _padding3;
};
ConstantBuffer<PerFrameConstants> perFrameConstants : register(b0);

#ifdef VERTEX_SHADER

float4 FullScreenQuadVS(uint vertexID : SV_VertexID) : SV_Position
{
	return float4(
		vertexID == 1 ? 3.0 : -1.0,
		vertexID == 2 ? 3.0 : -1.0,
		0.0,
		1.0);
}

#endif // #ifdef VERTEX_SHADER
