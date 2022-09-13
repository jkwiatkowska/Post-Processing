//--------------------------------------------------------------------------------------
// Chromatic Aberration Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
    float2 uv = input.sceneUV;
    
    uv.x = input.sceneUV.x + gColourOffset.r;
    float r = SceneTexture.Sample(PointSample, uv).r;
    
    uv.x = input.sceneUV.x + gColourOffset.g;
    float g = SceneTexture.Sample(PointSample, uv).g;
    
    uv.x = input.sceneUV.x + gColourOffset.b;
    float b = SceneTexture.Sample(PointSample, uv).b;
    
	// Got the RGB from the scene texture, set alpha to 1 for final output
    return float4(r, g, b, 1.0f);
}