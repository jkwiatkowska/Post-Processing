//--------------------------------------------------------------------------------------
// Hue Shift Post-Processing Pixel Shader
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
	// Sample a pixel from the scene texture.
    float3 colour = SceneTexture.Sample(PointSample, input.sceneUV).rgb;
    
    // Convert the colour of the pixel to HSL.
    colour = RGBtoHSL(colour);
    
    // Change the hue of the pixel
    colour.r += gHueShift;
    while (colour.r > 1.0f)
    {
        colour.r -= 1.0f;
    }
    
    // Convert back to RGB.
    colour = HSLtoRGB(colour);
    
    return float4(colour, GetAreaAlpha(input.areaUV));
}