//--------------------------------------------------------------------------------------
// Underwater Post-Processing Pixel Shader
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
    const float effectStrength = gWobbleStrength;
	
	// The wobble effect is a combination of sine waves in x and y dimensions
    float SinX = sin(input.sceneUV.x * 35.0f + gWobbleTimer * 1.0f);
    float SinY = sin(input.sceneUV.y * 20.0f + gWobbleTimer * 0.8f);
	
	// Offset for scene texture UV based on effect
	// Adjust size of UV offset based on the constant EffectStrength and the overall size of area being processed
    float2 offset = float2(SinY, SinX) * effectStrength;

	// Get pixel from scene texture, offset using the wobble effect
    float4 sampledColour = SceneTexture.Sample(PointSample, input.sceneUV + offset);
    float3 colour = sampledColour.rgb;
    
    // Get the water hue and brightness
    float hue = gUnderwaterHue;
    float brightness = lerp(gUnderwaterBrightness.x, gUnderwaterBrightness.y, input.sceneUV.y);
    
    // Convert the colour of the pixel to HSL.
    colour = RGBtoHSL(colour);
    
    // Change the hue and brightness of the pixel and increase its saturation.
    colour.r = hue;
    
    colour.b = clamp(colour.b * brightness, 0.0, 1.0);
    colour.g = clamp(colour.g + 0.4, 0.0, 1.0);
    
    // Convert back to RGB.
    colour = HSLtoRGB(colour);
    
    return float4(colour, sampledColour.a);
}