//--------------------------------------------------------------------------------------
// Blur X Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

#define SAMPLES 13

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
    float4 originalColour = SceneTexture.Sample(PointSample, input.sceneUV);
    float4 colour = originalColour;
    float brightness = RGBToBrightness(colour.rgb);
	
	[unroll(SAMPLES * 2)]
    for (float i = -SAMPLES; i <= SAMPLES; i++)
    {
        for (float j = -SAMPLES; j <= SAMPLES; j++)
        {
            // Ignore this position if it's not within the desired shape
            if ((gDilationType >= 2.0f && (abs(i) > SAMPLES - abs(j)) ||      // Diamond shape
                (gDilationType >= 1.0f && (length(float2(i, j)) > SAMPLES)))) // Circle shape
            {
                continue;
            }
            
            // Sample the UV at an offset.
            float2 offset = float2((i / SAMPLES) * gDilationSize.x, (j / SAMPLES) * gDilationSize.y);
            float4 sampledColour = SceneTexture.Sample(PointSample, input.sceneUV + offset);
            
            // The aim is to find the brightest colour in the sampled area.
            float sampleBrightness = RGBToBrightness(sampledColour.rgb);
            if (sampleBrightness > brightness)
            {
                colour = sampledColour;
                brightness = sampleBrightness;
            }
        }
    }
	
    // The brighter the brightest sampled colour is, the more of an effect it has on the final colour. 
    colour = lerp(originalColour, colour, smoothstep(gDilationThreshold.x, gDilationThreshold.y, brightness));
	
    return colour;
}