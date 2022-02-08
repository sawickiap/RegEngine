#include "Include/Common.hlsl"
#include "Include/LightingCommon.hlsl"

float4 main(float4 pos : SV_Position) : SV_Target
{
	int3 loadPos = int3(pos.xy, 0);
	float depth = Depth.Load(loadPos).r;
	float3 albedo = GBufferAlbedo.Load(loadPos).rgb;
	float3 normal_View = normalize(GBufferNormal.Load(loadPos).rgb);
	
	float3 pos_Clip;
	pos_Clip.x = pos.x * perFrameConstants.RenderResolutionInv.x * 2. - 1.;
	pos_Clip.y = 1. - pos.y * perFrameConstants.RenderResolutionInv.y * 2.;
	pos_Clip.z = depth;
	float4 pos_ViewHomo = mul(perFrameConstants.ProjInv, float4(pos_Clip, 1.0));
	float3 pos_View = pos_ViewHomo.xyz / pos_ViewHomo.w;
	
	float diffuseTerm = max(0.0, dot(normal_View, perFrameConstants.DirToLight_View));
	float3 reflected_View = reflect(-perFrameConstants.DirToLight_View, normal_View);
	float3 dirToCam_View = float3(0., 0., -1.);
	float specularTerm = pow(max(0.0, dot(dirToCam_View, reflected_View)), 15.0);
	
	float3 color = diffuseTerm * albedo * perFrameConstants.LightColor;
	color += specularTerm * diffuseTerm * perFrameConstants.LightColor;

	return float4(color, 1.0);
}
