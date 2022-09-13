//--------------------------------------------------------------------------------------
// Normal Depth Pixel Shader
//--------------------------------------------------------------------------------------
// Returns the float value of currently rendered objects, to distinguish between objects.

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Pixel shader entry point - each shader has a "main" function
float4 main(LightingPixelShaderInput input) : SV_Target
{
    return float4(input.worldPosition, 1);
}