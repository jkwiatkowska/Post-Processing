//--------------------------------------------------------------------------------------
// Normal Depth Vertex Shader
//--------------------------------------------------------------------------------------
// Basic matrix transformations only

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Vertex shader gets vertices from the mesh one at a time. It transforms their positions
// from 3D into 2D (see lectures) and passes that position down the pipeline so pixels can be rendered. 
NormalDepthPixelShaderInput main(BasicVertex modelVertex)
{
    NormalDepthPixelShaderInput output; // This is the data the pixel shader requires from this vertex shader

    float4 modelNormal = float4(modelVertex.normal, 0); // For normals add a 0 in the 4th element to indicate it is a vector
    output.worldNormal = mul(gWorldMatrix, modelNormal).xyz; // Only needed the 4th element to do this multiplication by 4x4 matrix...
                                                             //... it is not needed for lighting so discard afterwards with the .xyz
    
    float4 modelPosition = float4(modelVertex.position, 1);
    float4 worldPosition = mul(gWorldMatrix, modelPosition);
    float4 viewPosition = mul(gViewMatrix, worldPosition);
    output.projectedPosition = mul(gProjectionMatrix, viewPosition);
    
    output.depthPosition = output.projectedPosition;

    return output; // Ouput data sent down the pipeline (to the pixel shader)
}
