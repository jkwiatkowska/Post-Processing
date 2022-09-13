//--------------------------------------------------------------------------------------
// Blur Y Post-Processing Pixel Shader
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

#define SAMPLES 150

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
    float4 colour = 0;
    float sum = 0;
	
	[unroll(SAMPLES)]
    for (float i = 0; i < SAMPLES; i++)
    {
		// Offset of the sample
        float offset = (i / (SAMPLES - 1) - 0.5) * gBlurSize.y;
		
		// UV coordinate of sample
        float2 uv = input.sceneUV + float2(0, offset);
		
		// Gaussian function
        float gauss = Gauss(offset);
		
		// Multiply colour at offset point with influence from gaussian function and add it to colour
        sum += gauss;
        colour += SceneTexture.Sample(PointSample, uv) * gauss;
    }
	
    colour = colour / sum;
	
	// Got the RGB from the scene texture, set alpha to 1 for final output
    return colour;
}