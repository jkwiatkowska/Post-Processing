//--------------------------------------------------------------------------------------
// Normal Depth Pixel Shader
//--------------------------------------------------------------------------------------
// Returns the normal vector and depth of the current pixel to be used in post-processing.

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Pixel shader entry point - each shader has a "main" function
float4 main(NormalDepthPixelShaderInput input) : SV_Target
{
    // Normal might have been scaled by model scaling or interpolation so renormalise
    float depth = input.depthPosition.z / 500.0f;
    return float4(normalize(input.worldNormal), depth);
}