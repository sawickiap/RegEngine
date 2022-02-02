struct PerFrameConstants
{
	uint FrameIndex;
	float SceneTime;
	uint2 _padding0;
	float3 DirToLight_View;
	uint _padding1;
	float3 LightColor;
	uint _padding2;
	float3 AmbientColor;
	uint _padding3;
};
ConstantBuffer<PerFrameConstants> perFrameConstants : register(b0);
