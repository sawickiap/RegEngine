float4 main(uint vertexID : SV_VertexID) : SV_Position
{
	return float4(
		vertexID == 1 ? 3.0 : -1.0,
		vertexID == 2 ? 3.0 : -1.0,
		0.5,
		1.0);
}
