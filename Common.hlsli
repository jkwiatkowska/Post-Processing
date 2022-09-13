//--------------------------------------------------------------------------------------
// Common include file for all shaders
//--------------------------------------------------------------------------------------
// Using include files to define the type of data passed between the shaders

#define EPSILON 1e-10
#define PI 3.14159265359
#define TWO_PI PI*2
#define EULER 2.71828182846

//--------------------------------------------------------------------------------------
// Shader input / output
//--------------------------------------------------------------------------------------

// The structure below describes the vertex data to be sent into the vertex shader for ordinary non-skinned models
struct BasicVertex
{
    float3 position : position;
    float3 normal   : normal;
    float2 uv       : uv;
};



// This structure describes what data the lighting pixel shader receives from the vertex shader.
// The projected position is a required output from all vertex shaders - where the vertex is on the screen
// The world position and normal at the vertex are sent to the pixel shader for the lighting equations.
// The texture coordinates (uv) are passed from vertex shader to pixel shader unchanged to allow textures to be sampled
struct LightingPixelShaderInput
{
    float4 projectedPosition : SV_Position; // This is the position of the pixel to render, this is a required input
                                            // to the pixel shader and so it uses the special semantic "SV_Position"
                                            // because the shader needs to identify this important information
    
    float3 worldPosition : worldPosition;   // The world position and normal of each vertex is passed to the pixel...
    float3 worldNormal   : worldNormal;     //...shader to calculate per-pixel lighting. These will be interpolated
                                            // automatically by the GPU (rasterizer stage) so each pixel will know
                                            // its position and normal in the world - required for lighting equations
    
    float2 uv : uv; // UVs are texture coordinates. The artist specifies for every vertex which point on the texture is "pinned" to that vertex.
};


// This structure is similar to the one above but for the light models, which aren't themselves lit
struct SimplePixelShaderInput
{
    float4 projectedPosition : SV_Position;
    float2 uv                : uv;
};

struct NormalDepthPixelShaderInput
{
    float4 projectedPosition : SV_Position;
    float3 worldNormal       : worldNormal;
    float4 depthPosition     : depthPosition;
};


//**************************

// The vertex data received by each post-process shader. Just the 2d projected position (pixel coordinate on screen), 
// and two sets of UVs - one for accessing the texture showing the scene, one refering to the area being affected (see the 2DQuad_pp.hlsl file for diagram & details)
struct PostProcessingInput
{
	float4 projectedPosition     : SV_Position;
	noperspective float2 sceneUV : sceneUV;      // "noperspective" is needed for polygon processing or the sampling of the scene texture doesn't work correctly (ask tutor if you are interested)
	float2 areaUV                : areaUV;
};

//**************************



//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------

// These structures are "constant buffers" - a way of passing variables over from C++ to the GPU
// They are called constants but that only means they are constant for the duration of a single GPU draw call.
// These "constants" correspond to variables in C++ that we will change per-model, or per-frame etc.

// In this exercise the matrices used to position the camera are updated from C++ to GPU every frame along with lighting information
// These variables must match exactly the gPerFrameConstants structure in Scene.cpp
cbuffer PerFrameConstants : register(b0) // The b0 gives this constant buffer the number 0 - used in the C++ code
{
	float4x4 gCameraMatrix;         // World matrix for the camera (inverse of the ViewMatrix below)
	float4x4 gViewMatrix;
    float4x4 gProjectionMatrix;
    float4x4 gViewProjectionMatrix; // The above two matrices multiplied together to combine their effects

    float3   gLight1Position; // 3 floats: x, y z
    float    gViewportWidth;  // Using viewport width and height as padding - see this structure in earlier labs to read about padding here
    float3   gLight1Colour;
    float    gViewportHeight;

    float3   gLight2Position;
    float    padding1;
    float3   gLight2Colour;
    float    padding2;

    float3   gAmbientColour;
    float    gSpecularPower;

    float3   gCameraPosition;
    float    padding3;
}
// Note constant buffers are not structs: we don't use the name of the constant buffer, these are really just a collection of global variables (hence the 'g')



static const int MAX_BONES = 64;

// If we have multiple models then we need to update the world matrix from C++ to GPU multiple times per frame because we
// only have one world matrix here. Because this data is updated more frequently it is kept in a different buffer for better performance.
// We also keep other data that changes per-model here
// These variables must match exactly the gPerModelConstants structure in Scene.cpp
cbuffer PerModelConstants : register(b1) // The b1 gives this constant buffer the number 1 - used in the C++ code
{
    float4x4 gWorldMatrix;

    float3   gObjectColour;  // Useed for tinting light models
	float    gExplodeAmount; // Used in the geometry shader to control how much the polygons are exploded outwards

	float4x4 gBoneMatrices[MAX_BONES];
}


//**************************

// This is where we receive post-processing settings from the C++ side
// These variables must match exactly the gPostProcessingConstants structure in Scene.cpp
// Note that this buffer reuses the same index (register) as the per-model buffer above since they won't be used together
cbuffer PostProcessingConstants : register(b1) 
{
	float2 gArea2DTopLeft; // Top-left of post-process area on screen, provided as coordinate from 0.0->1.0 not as a pixel coordinate
	float2 gArea2DSize;    // Size of post-process area on screen, provided as sizes from 0.0->1.0 (1 = full screen) not as a size in pixels
	float  gArea2DDepth;   // Depth buffer value for area (0.0 nearest to 1.0 furthest). Full screen post-processing uses 0.0f
	float3 paddingA;       // Pad things to collections of 4 floats (see notes in earlier labs to read about padding)

  	float4 gPolygon2DPoints[4]; // Four points of a polygon in 2D viewport space for polygon post-processing. Matrix transformations already done on C++ side

	// Tint post-process settings
	float3 gTintColour;
    
	// Copy post-process setting (alpha setting for motion blur)
    float  gCopyAlpha;

	// Grey noise post-process settings
    float2 gNoiseScale;
	float2 gNoiseOffset;

	// Burn post-process settings
	float  gBurnHeight;
	float3 paddingC;

	// Distort post-process settings
	float  gDistortLevel;
	float3 paddingD;

	// Spiral post-process settings
	float  gSpiralLevel;
	float3 paddingE;

	// Heat haze post-process settings
	float  gHeatHazeTimer;
	float3 paddingF;
    
    // Gradient and hue shift post-process settings
    float2 gGradientHue;
    float  gHueShift;
    float  paddingG;
    
    // Blur post-process settings
    float2 gBlurSize;
    float  gStandardDeviationSquared;
    float  paddingH;
    
    // Underwater post-process settings
    float  gUnderwaterHue;
    float2 gUnderwaterBrightness;
    
    float  gWobbleStrength;
    float  gWobbleTimer;
    float  paddingI;
    
    // Retro post-process settings
    float2 gPixelNumber;
    float  gPixelBrightnessHueShift;
    
    float  gPixelBrightnessLevels;
    float  gPixelSaturationMin;
    float  gPixelSaturationLevels;
    
    float2 gPixelHueRange;
    float  gPixelHueLevels;
    
    // Bloom post-process settings
    float  gBloomThreshold;
    float  gBloomIntensity;
    float  paddingJ;
    
	// Directional blur post-process settings
    float  gDirectionalBlurSize;
    float  gDirectionalBlurX;
    float  gDirectionalBlurY;
    
    float  gDirectionalBlurIntensity;
    float2 paggingK;
    
    // Chromatic aberration post-process settings
    float3 gColourOffset;
    
    // Outline post-process settings
    float  gOutlineThreshold;
    float  gOutlineThickness;
    float  paddingL;
    
    // Dilation post-process settings
    float2 gDilationSize;
    float  gDilationType;
    
    float2 gDilationThreshold;
    float  paddingM;
    
    // Depth of field post-process settings
    float  gFocalPlane;
    float  gNearPlane;
    float  gFarPlane;
    
    float  gFocusedObject;
    float2 paddingN;
    
    // Frosted glass post-process settings
    float  gFrostedGlassFrequency;
    float2 gFrostedGlassoffsetSize;
}


//**************************************

// Common post-processing functions
float GetAreaAlpha(float2 uv)
{
    // Calculate alpha to display the effect in a softened circle, could use a texture rather than calculations for the same task.
	// Uses the second set of area texture coordinates, which range from (0,0) to (1,1) over the area being processed
    float softEdge = 0.20f; // Softness of the edge of the circle - range 0.001 (hard edge) to 0.25 (very soft)
    float2 centreVector = uv - float2(0.5f, 0.5f);
    float centreLengthSq = dot(centreVector, centreVector);
    float alpha = 1.0f - saturate((centreLengthSq - 0.25f + softEdge) / softEdge); // Soft circle calculation based on fact that this circle has a radius of 0.5 (as area UVs go from 0->1)
    
    return alpha;
}

float Gauss(float x)
{
    if (gStandardDeviationSquared < EPSILON)
    {
        return 1.0f;
    }
    return (1.0f / sqrt(TWO_PI * gStandardDeviationSquared)) * pow(EPSILON, -(x * x) / (2.0f * gStandardDeviationSquared));
}

float DilationForDepth(float depth)
{
    float f = 0.0f;
    
    if (depth < gFocalPlane)
    {
        f = -(depth - gFocalPlane) / (gFocalPlane - gNearPlane);
    }
    else
    {
        f = (depth - gFocalPlane) / (gFarPlane - gFocalPlane);
        f = clamp(f, 0.0f, 1.0f);
    }
    
    return f;
}

float4 Spline(float x, float4 c1, float4 c2, float4 c3, float4 c4, float4 c5, float4 c6, float4 c7, float4 c8, float4 c9)
{
    float w1, w2, w3, w4, w5, w6, w7, w8, w9;
    w1 = 0;
    w2 = 0;
    w3 = 0;
    w4 = 0;
    w5 = 0;
    w6 = 0;
    w7 = 0;
    w8 = 0;
    w9 = 0;
    float tmp = x * 8.0;
    if (tmp <= 1.0)
    {
        w1 = 1.0 - tmp;
        w2 = tmp;
    }
    else if (tmp <= 2.0)
    {
        tmp = tmp - 1.0;
        w2 = 1.0 - tmp;
        w3 = tmp;
    }
    else if (tmp <= 3.0)
    {
        tmp = tmp - 2.0;
        w3 = 1.0 - tmp;
        w4 = tmp;
    }
    else if (tmp <= 4.0)
    {
        tmp = tmp - 3.0;
        w4 = 1.0 - tmp;
        w5 = tmp;
    }
    else if (tmp <= 5.0)
    {
        tmp = tmp - 4.0;
        w5 = 1.0 - tmp;
        w6 = tmp;
    }
    else if (tmp <= 6.0)
    {
        tmp = tmp - 5.0;
        w6 = 1.0 - tmp;
        w7 = tmp;
    }
    else if (tmp <= 7.0)
    {
        tmp = tmp - 6.0;
        w7 = 1.0 - tmp;
        w8 = tmp;
    }
    else
    {
        tmp = saturate(tmp - 7.0);
        w8 = 1.0 - tmp;
        w9 = tmp;
    }
    return w1 * c1 + w2 * c2 + w3 * c3 + w4 * c4 + w5 * c5 + w6 * c6 + w7 * c7 + w8 * c8 + w9 * c9;
}


//**************************************

// Colour conversion functions
// Source: https://www.chilliant.com/rgb2hsv.html
 
float3 HUEtoRGB(in float H)
{
    float R = abs(H * 6 - 3) - 1;
    float G = 2 - abs(H * 6 - 2);
    float B = 2 - abs(H * 6 - 4);
    return saturate(float3(R, G, B));
}

float3 RGBtoHCV(in float3 RGB)
{
    float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0 / 3.0) : float4(RGB.gb, 0.0, -1.0 / 3.0);
    float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6 * C + EPSILON) + Q.z);
    return float3(H, C, Q.x);
}

float3 HSVtoRGB(in float3 HSV)
{
    float3 RGB = HUEtoRGB(HSV.x);
    return ((RGB - 1) * HSV.y + 1) * HSV.z;
}

float3 RGBtoHSL(in float3 RGB)
{
    float3 HCV = RGBtoHCV(RGB);
    float z = HCV.z - HCV.y * 0.5;
    float s = clamp(HCV.y / (1. - abs(z * 2. - 1.) + EPSILON), 0.0f, 1.0f);
    return float3(HCV.x, s, z);
}

float RGBToBrightness(in float3 RGB)
{
    return dot(RGB, float3(0.2126, 0.7152, 0.0722));
}

float3 HSLtoRGB(in float3 HSL)
{
    float3 RGB = HUEtoRGB(HSL.x);
    float c = (1. - abs(2. * HSL.z - 1.)) * HSL.y;
    return (RGB - 0.5) * c + HSL.z;
}

//**************************