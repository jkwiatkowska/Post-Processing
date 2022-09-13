//--------------------------------------------------------------------------------------
// Copy Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------
// Just copies pixels to the back-buffer - no processing

#include "Common.hlsli"


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// The scene has been rendered to a texture, these variables allow access to that texture
Texture2D    SceneTexture : register(t0);
SamplerState PointSample  : register(s0); // We don't usually want to filter (bilinear, trilinear etc.) the scene texture when
                                          // post-processing so this sampler will use "point sampling" - no filtering


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Post-processing shader that tints the scene texture to a given colour
float4 main(PostProcessingInput input) : SV_Target
{
	// Sample a pixel from the scene texture
	float4 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgba;
	
	// Lower alpha can be set to create a motion blur effect.
	if (gCopyAlpha < 1.0f - EPSILON)
    {
		colour.a = gCopyAlpha;
    }
	// Got the RGB from the scene texture, set alpha to 1 for final output
	return colour;
}