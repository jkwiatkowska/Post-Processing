//--------------------------------------------------------------------------------------
// Outline/edge detection Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
										  // post-processing so this sampler will use "point sampling" - no filtering

Texture2D NormalDepthMap : register(t1);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
    // Depth has a bigger weight than normals
    const float depthWeight = 12.0f;
    
	// Get pixel from scene texture
    float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    float4 normalDepthValue = NormalDepthMap.Sample(PointSample, input.sceneUV);
    normalDepthValue.a *= depthWeight;
    
    const float2 offsets[8] =
    {
        float2(-1, -1),
        float2(-1, 0),
        float2(-1, 1),
        float2(0, -1),
        float2(0, 1),
        float2(1, -1),
        float2(1, 0),
        float2(1, 1)
    };
    
    float4 sampledValue = float4(0, 0, 0, 0);
    for (int i = 0; i < 8; i++)
    {
        sampledValue += NormalDepthMap.Sample(PointSample, input.sceneUV + offsets[i] * gOutlineThickness);
    }
    sampledValue.a *= depthWeight;
    sampledValue /= 8;

    float edgeValue = length(normalDepthValue - sampledValue);
    
    if (edgeValue >= gOutlineThreshold)
    {
        colour *= 0.1f;
    }
    
    return float4(colour, 1.0f);
}