//--------------------------------------------------------------------------------------
// Retro Game Mode Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

#define SAMPLES 50

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
	// Limit the UV and get the colour at that position
    float2 uv = float2(floor(input.sceneUV.x * gPixelNumber.x) / gPixelNumber.x, floor(input.sceneUV.y * gPixelNumber.y) / gPixelNumber.y);
    
    float4 sampledColour = SceneTexture.Sample(PointSample, uv).rgba;
    float3 colour = sampledColour.rgb;
    
    // Convert to HSL
    colour = RGBtoHSL(colour);
    
    // Ensure minimum brightness and limit possiblebrightness levels.
    colour.b = clamp(ceil(colour.b * gPixelBrightnessLevels) / gPixelBrightnessLevels, 0.0f, 1.0f);

    // Ensure minimum saturation and limit possible saturation levels.
    if (colour.g < gPixelSaturationMin)
    {
        if (colour.g < EPSILON)
        {
            colour.r = 0.6f;
        }
        colour.g = gPixelSaturationMin;
    }
    colour.g = clamp(gPixelSaturationMin + ceil(colour.g * gPixelSaturationLevels) / gPixelSaturationLevels * gPixelSaturationMin / 1.0f, 0.0f, 1.0f);
    
    // Limit hue
    float range = gPixelHueRange.y - gPixelHueRange.x;
    colour.r = gPixelHueRange.x + ceil(colour.r * gPixelHueLevels) / gPixelHueLevels * (range / 1.0f) + colour.b * gPixelBrightnessHueShift;
    while (colour.r > 1.0f)
    {
        colour.r -= 1.0f;
    }
    while (colour.r < 0.0f)
    {
        colour.r += 1.0f;
    }
    
    // Convert back to RGB
    colour = HSLtoRGB(colour);
    
    return float4(colour, sampledColour.a);
}