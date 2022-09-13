//--------------------------------------------------------------------------------------
// Frosted Glass Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering

Texture2D NoiseMap : register(t1);
SamplerState TrilinearWrap : register(s1);

//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------


float4 main(PostProcessingInput input) : SV_Target
{
    float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    
    float2 ox = float2(gFrostedGlassoffsetSize.x, 0.0);
    float2 oy = float2(0.0, gFrostedGlassoffsetSize.y);
    float2 PP = input.sceneUV - oy;
    float4 c00 = SceneTexture.Sample(PointSample, PP - ox);
    float4 c01 = SceneTexture.Sample(PointSample, PP);
    float4 c02 = SceneTexture.Sample(PointSample, PP + ox);
    PP = input.sceneUV;
    float4 c10 = SceneTexture.Sample(PointSample, PP - ox);
    float4 c11 = SceneTexture.Sample(PointSample, PP);
    float4 c12 = SceneTexture.Sample(PointSample, PP + ox);
    PP = input.sceneUV + oy;
    float4 c20 = SceneTexture.Sample(PointSample, PP - ox);
    float4 c21 = SceneTexture.Sample(PointSample, PP);
    float4 c22 = SceneTexture.Sample(PointSample, PP + ox);

    float n = NoiseMap.Sample(TrilinearWrap, gFrostedGlassFrequency * input.areaUV).x;
    n = fmod(n, 0.111111) / 0.111111;
    float4 finalColour = Spline(n, c00, c01, c02, c10, c11, c12, c20, c21, c22);
    return finalColour;
}