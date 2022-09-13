
//--------------------------------------------------------------------------------------
// Depth Dilation Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
										  // post-processing so this sampler will use "point sampling" - no filtering

Texture2D DepthMap : register(t1);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that calculates the dilation for each pixel, to be used in the depth of field buffer.
float4 main(PostProcessingInput input) : SV_Target
{
    float depth = DepthMap.Sample(PointSample, input.sceneUV).a;
    float dilation = DilationForDepth(DepthMap.Sample(PointSample, input.sceneUV).a);
    
    return dilation;
}