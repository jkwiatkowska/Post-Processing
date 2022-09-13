//--------------------------------------------------------------------------------------
// Bloom Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------
// Just samples a pixel from the scene texture and multiplies it by a fixed colour to tint the scene

#include "Common.hlsli"


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D SceneTexture : register(t0);
SamplerState PointSample : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering

// This shader also uses a bloom texture, which contains information about the brightness of each pixel in the scene.
Texture2D BloomMap : register(t1);



//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
	// Get brightness
    float3 bloom = BloomMap.Sample(PointSample, input.areaUV) * gBloomIntensity;
    
    float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb + bloom;
    
    return float4(colour, GetAreaAlpha(input.areaUV));
}