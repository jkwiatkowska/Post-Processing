//--------------------------------------------------------------------------------------
// Selection Post-Processing Pixel Shader
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
Texture2D FocusMap : register(t2);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
    float4 colour = SceneTexture.Sample(PointSample, input.sceneUV);
    
    if (FocusMap.Sample(PointSample, input.sceneUV).a > EPSILON)
    {
        return colour;
    }
    
    float depth = DepthMap.Sample(PointSample, input.sceneUV).a;
    
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
    
    for (int i = 0; i < 8; i++)
    {
        float sampledValue = FocusMap.Sample(PointSample, input.sceneUV + offsets[i] * gOutlineThickness).a;
        if (sampledValue > EPSILON && sampledValue < depth)
        {
            return 1.0f;
        }
    }
    
    return colour;
}