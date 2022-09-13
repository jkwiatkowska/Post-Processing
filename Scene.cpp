//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here
#include "ColourRGBA.h" 

#include <array>
#include <sstream>
#include <memory>

//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------

//********************
// Available post-processes
enum class PostProcessType
{
	None,
	Copy,
	Tint,
	GreyNoise,
	Burn,
	Distort,
	Spiral,
	HeatHaze,
	Gradient,
	BlurX,
	BlurY,
	Underwater,
	DepthOfField,
	Retro,
	Bloom,
	Brightness,
	DirectionalBlur,
	HueShift,
	ChromaticAberration,
	Outline,
	Dilation,
	FrostedGlass,
	Selection,
};

enum class PostProcessMode
{
	Fullscreen,
	Area,
	Polygon,
};

class PolygonData
{
public:
	std::array<CVector3, 4> Points;
	CMatrix4x4 Matrix;

	PolygonData(std::array<CVector3, 4> points, CMatrix4x4 matrix)
	{
		Points = points;
		Matrix = matrix;
	}
};

class PostProcess
{
public:
	PostProcessType Type;
	PostProcessMode Mode;
	PolygonData* PolyData;

	PostProcess(PostProcessType type, PostProcessMode mode = PostProcessMode::Fullscreen, PolygonData* polyData = nullptr)
	{
		Type = type;
		Mode = mode;
		PolyData = polyData;
	}

	~PostProcess()
	{
		if (PolyData) delete PolyData;
	}
};

std::vector<PostProcess*> gFullScreenPostProcesses;
std::vector<PostProcess*> gPolygonPostProcesses;

//********************


// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 1.5f;  // Radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // Units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)

// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;


// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
Mesh* gStarsMesh;
Mesh* gGroundMesh;
Mesh* gCubeMesh;
Mesh* gCrateMesh;
Mesh* gLightMesh;
Mesh* gWall1Mesh;
Mesh* gWall2Mesh;
Mesh* gTeapotMesh;
Mesh* gTrollMesh;

Model* gStars;

class Object
{
public:
	Model* mModel;
	ID3D11ShaderResourceView* mTexture;

	Object(Mesh* mesh, CVector3 position, CVector3 rotation, float scale, ID3D11ShaderResourceView* texture)
	{
		mModel = new Model(mesh);
		mModel->SetPosition(position);
		mModel->SetRotation(rotation);
		mModel->SetScale(scale);
		mTexture = texture;
	}

	~Object()
	{
		delete mModel;
		mModel = nullptr;

		mTexture = nullptr;
	}
};
std::vector<Object*> gObjects;
int gFocusedObject = 0;

Camera* gCamera;


// Store lights in an array in this exercise
const int NUM_LIGHTS = 2;
struct Light
{
	Model*   model;
	CVector3 colour;
	float    strength;
};
Light gLights[NUM_LIGHTS];


// Additional light information
CVector3 gAmbientColour = { 0.3f, 0.3f, 0.4f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

ColourRGBA gBackgroundColor = { 0.3f, 0.3f, 0.4f, 1.0f };
ColourRGBA gNDBackgroundColor = { 0.3f, 0.3f, 0.4f, 0.0f };

// Variables controlling light1's orbiting of the cube
const float gLightOrbitRadius = 20.0f;
const float gLightOrbitSpeed = 0.7f;

// Bloom variables
float gTempTimer = 0.0f;
int gTempDiagonalBlurs = 3;

// Motion blur
float gCopyAlpha = 1.0f;

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants (settings) that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer*     gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constants (settings) that change per-model (e.g. world matrix)
ID3D11Buffer*     gPerModelConstantBuffer; // --"--

//**************************
PostProcessingConstants gPostProcessingConstants;       // As above, but constants (settings) for each post-process
ID3D11Buffer*           gPostProcessingConstantBuffer; // --"--
//**************************


//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// DirectX objects controlling textures used in this lab
ID3D11Resource*           gStarsDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gStarsDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gGroundDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gCrateDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gCrateDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gCubeDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gCubeDiffuseSpecularMapSRV = nullptr;
ID3D11Resource*           gWallMap = nullptr;
ID3D11ShaderResourceView* gWallMapSRV = nullptr;
ID3D11Resource*           gTeapotMap = nullptr;
ID3D11ShaderResourceView* gTeapotMapSRV = nullptr;
ID3D11Resource*           gTrollDiffuseSpecularMap = nullptr;
ID3D11ShaderResourceView* gTrollDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gLightDiffuseMap = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;



//****************************
// Post processing textures

// This texture will have the scene renderered on it. Then the texture is then used for post-processing
ID3D11Texture2D*          gSceneTexture				= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gSceneRenderTarget		= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gSceneTextureSRV			= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Texture2D*		  gSceneTexture2			= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*	  gSceneRenderTarget2		= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gSceneTextureSRV2			= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Texture2D*		  gTempTexture				= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gTempRenderTarget			= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gTempTextureSRV			= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Texture2D*		  gTempTexture2				= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gTempRenderTarget2		= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gTempTextureSRV2			= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11ShaderResourceView* gCurrentBloomTextureSRV	= nullptr;

ID3D11Texture2D*		  gNormalDepthTexture		= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gNormalDepthRenderTarget	= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gNormalDepthTextureSRV	= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Texture2D*		  gNormalDepthTexture2		= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gNormalDepthRenderTarget2	= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gNormalDepthTextureSRV2	= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11ShaderResourceView* gCurrentNormalDepthTextureSRV = nullptr;

ID3D11Texture2D*		  gFocusedObjectTexture				= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gFocusedObjectRenderTarget		= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gFocusedObjectTextureSRV			= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Texture2D*		  gFocusedObjectTexture2			= nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gFocusedObjectRenderTarget2		= nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gFocusedObjectTextureSRV2			= nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11ShaderResourceView* gCurrentFocusedObjectTextureSRV = nullptr;

// Additional textures used for specific post-processes
ID3D11Resource*           gNoiseMap = nullptr;
ID3D11ShaderResourceView* gNoiseMapSRV = nullptr;
ID3D11Resource*           gBurnMap = nullptr;
ID3D11ShaderResourceView* gBurnMapSRV = nullptr;
ID3D11Resource*           gDistortMap = nullptr;
ID3D11ShaderResourceView* gDistortMapSRV = nullptr;
ID3D11Resource*           gNoiseMap2 = nullptr;
ID3D11ShaderResourceView* gNoiseMapSRV2 = nullptr;

//****************************



//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
	////--------------- Load meshes ---------------////

	// Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
	try
	{
		gStarsMesh  = new Mesh("Stars.x");
		gGroundMesh = new Mesh("Hills.x");
		gCubeMesh   = new Mesh("Cube.x");
		gCrateMesh  = new Mesh("CargoContainer.x");
		gLightMesh  = new Mesh("Light.x");
		gWall1Mesh  = new Mesh("Wall1.x");
		gWall2Mesh  = new Mesh("Wall2.x");
		gTeapotMesh = new Mesh("Teapot.x");
		gTrollMesh  = new Mesh("Troll.x");
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}


	////--------------- Load / prepare textures & GPU states ---------------////

	// Load textures and create DirectX objects for them
	// The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
	// texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
	// The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.
	if (!LoadTexture("Stars.jpg",                &gStarsDiffuseSpecularMap,  &gStarsDiffuseSpecularMapSRV) ||
		!LoadTexture("GrassDiffuseSpecular.dds", &gGroundDiffuseSpecularMap, &gGroundDiffuseSpecularMapSRV) ||
		!LoadTexture("StoneDiffuseSpecular.dds", &gCubeDiffuseSpecularMap,   &gCubeDiffuseSpecularMapSRV) ||
		!LoadTexture("CargoA.dds",               &gCrateDiffuseSpecularMap,  &gCrateDiffuseSpecularMapSRV) ||
		!LoadTexture("Flare.jpg",                &gLightDiffuseMap,          &gLightDiffuseMapSRV) ||
		!LoadTexture("Noise.png",				 &gNoiseMap,				 &gNoiseMapSRV) ||
		!LoadTexture("Burn.png",                 &gBurnMap,					 &gBurnMapSRV) ||
		!LoadTexture("Distort.png",              &gDistortMap,				 &gDistortMapSRV) ||
		!LoadTexture("Noise2.png",				 &gNoiseMap2,				 &gNoiseMapSRV2) ||
		!LoadTexture("brick_35.jpg",			 &gWallMap,					 &gWallMapSRV) ||
		!LoadTexture("TrollDiffuseSpecular.dds", &gTrollDiffuseSpecularMap,	 &gTrollDiffuseSpecularMapSRV) ||
		!LoadTexture("Saturn.jpg",				 &gTeapotMap,				 &gTeapotMapSRV))
	{
		gLastError = "Error loading textures";
		return false;
	}


	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}


	////--------------- Prepare shaders and constant buffers to communicate with them ---------------////

	// Load the shaders required for the geometry we will use (see Shader.cpp / .h)
	if (!LoadShaders())
	{
		gLastError = "Error loading shaders";
		return false;
	}

	// Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
	// These allow us to pass data from CPU to shaders such as lighting information or matrices
	// See the comments above where these variable are declared and also the UpdateScene function
	gPerFrameConstantBuffer       = CreateConstantBuffer(sizeof(gPerFrameConstants));
	gPerModelConstantBuffer       = CreateConstantBuffer(sizeof(gPerModelConstants));
	gPostProcessingConstantBuffer = CreateConstantBuffer(sizeof(gPostProcessingConstants));
	if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr || gPostProcessingConstantBuffer == nullptr)
	{
		gLastError = "Error creating constant buffers";
		return false;
	}



	//********************************************
	//**** Create Scene Texture

	// We will render the scene to this texture instead of the back-buffer (screen), then we post-process the texture onto the screen
	// This is exactly the same code we used in the graphics module when we were rendering the scene onto a cube using a texture

	// Using a helper function to load textures from files above. Here we create the scene texture manually
	// as we are creating a special kind of texture (one that we can render to). Many settings to prepare:
	D3D11_TEXTURE2D_DESC sceneTextureDesc = {};
	sceneTextureDesc.Width = gViewportWidth;  // Full-screen post-processing - use full screen size for texture
	sceneTextureDesc.Height = gViewportHeight;
	sceneTextureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	sceneTextureDesc.ArraySize = 1;
	sceneTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	sceneTextureDesc.SampleDesc.Count = 1;
	sceneTextureDesc.SampleDesc.Quality = 0;
	sceneTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	sceneTextureDesc.CPUAccessFlags = 0;
	sceneTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &gSceneTexture)) ||
		FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &gSceneTexture2)))
	{
		gLastError = "Error creating scene texture";
		return false;
	}

	// We created the scene texture above, now we get a "view" of it as a render target, i.e. get a special pointer to the texture that
	// we use when rendering to it (see RenderScene function below)
	if (FAILED(gD3DDevice->CreateRenderTargetView(gSceneTexture, NULL, &gSceneRenderTarget)) ||
		FAILED(gD3DDevice->CreateRenderTargetView(gSceneTexture2, NULL, &gSceneRenderTarget2)))
	{
		gLastError = "Error creating scene render target view";
		return false;
	}

	// We also need to send this texture (resource) to the shaders. To do that we must create a shader-resource "view"
	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc = {};
	srDesc.Format = sceneTextureDesc.Format;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(gSceneTexture, &srDesc, &gSceneTextureSRV)) || 
		FAILED(gD3DDevice->CreateShaderResourceView(gSceneTexture2, &srDesc, &gSceneTextureSRV2)))
	{
		gLastError = "Error creating scene shader resource view";
		return false;
	}

	//********************************************
	//**** Create Temp Texture for bloom and depth of field effects
	D3D11_TEXTURE2D_DESC tempTextureDesc = {};
	tempTextureDesc.Width = gViewportWidth;  // Full-screen post-processing - use full screen size for texture
	tempTextureDesc.Height = gViewportHeight;
	tempTextureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	tempTextureDesc.ArraySize = 1;
	tempTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	tempTextureDesc.SampleDesc.Count = 1;
	tempTextureDesc.SampleDesc.Quality = 0;
	tempTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	tempTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	tempTextureDesc.CPUAccessFlags = 0;
	tempTextureDesc.MiscFlags = 0;

	if (FAILED(gD3DDevice->CreateTexture2D(&tempTextureDesc, NULL, &gTempTexture)) || 
		FAILED(gD3DDevice->CreateTexture2D(&tempTextureDesc, NULL, &gTempTexture2)))
	{
		gLastError = "Error creating bloom texture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gTempTexture, NULL, &gTempRenderTarget)) ||
		FAILED(gD3DDevice->CreateRenderTargetView(gTempTexture2, NULL, &gTempRenderTarget2)))
	{
		gLastError = "Error creating bloom render target view";
		return false;
	}

	if (FAILED(gD3DDevice->CreateShaderResourceView(gTempTexture, &srDesc, &gTempTextureSRV)) ||
		FAILED(gD3DDevice->CreateShaderResourceView(gTempTexture2, &srDesc, &gTempTextureSRV2)))
	{
		gLastError = "Error creating bloom shader resource view";
		return false;
	}

	//********************************************
	//**** Create Normal and Depth Map
	D3D11_TEXTURE2D_DESC ndTextureDesc = {};
	ndTextureDesc.Width = gViewportWidth;  // Full-screen post-processing - use full screen size for texture
	ndTextureDesc.Height = gViewportHeight;
	ndTextureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	ndTextureDesc.ArraySize = 1;
	ndTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	ndTextureDesc.SampleDesc.Count = 1;
	ndTextureDesc.SampleDesc.Quality = 0;
	ndTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	ndTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	ndTextureDesc.CPUAccessFlags = 0;
	ndTextureDesc.MiscFlags = 0;

	if (FAILED(gD3DDevice->CreateTexture2D(&ndTextureDesc, NULL, &gNormalDepthTexture))||
		FAILED(gD3DDevice->CreateTexture2D(&ndTextureDesc, NULL, &gNormalDepthTexture2)))
	{
		gLastError = "Error creating normal depth texture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gNormalDepthTexture, NULL,  &gNormalDepthRenderTarget))||
		FAILED(gD3DDevice->CreateRenderTargetView(gNormalDepthTexture2, NULL, &gNormalDepthRenderTarget2)))
	{
		gLastError = "Error creating normal depth render target view";
		return false;
	}

	if (FAILED(gD3DDevice->CreateShaderResourceView(gNormalDepthTexture, &srDesc,  &gNormalDepthTextureSRV))||
		FAILED(gD3DDevice->CreateShaderResourceView(gNormalDepthTexture2, &srDesc, &gNormalDepthTextureSRV2)))
	{
		gLastError = "Error creating normal depth shader resource view";
		return false;
	}

	// Focused object
	D3D11_TEXTURE2D_DESC foTextureDesc = {};
	foTextureDesc.Width = gViewportWidth;  // Full-screen post-processing - use full screen size for texture
	foTextureDesc.Height = gViewportHeight;
	foTextureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	foTextureDesc.ArraySize = 1;
	foTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	foTextureDesc.SampleDesc.Count = 1;
	foTextureDesc.SampleDesc.Quality = 0;
	foTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	foTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	foTextureDesc.CPUAccessFlags = 0;
	foTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&foTextureDesc, NULL, &gFocusedObjectTexture)) ||
		FAILED(gD3DDevice->CreateTexture2D(&foTextureDesc, NULL, &gFocusedObjectTexture2)))
	{
		gLastError = "Error creating normal depth texture";
		return false;
	}

	if (FAILED(gD3DDevice->CreateRenderTargetView(gFocusedObjectTexture, NULL, &gFocusedObjectRenderTarget)) ||
		FAILED(gD3DDevice->CreateRenderTargetView(gFocusedObjectTexture2, NULL, &gFocusedObjectRenderTarget2)))
	{
		gLastError = "Error creating normal depth render target view";
		return false;
	}

	if (FAILED(gD3DDevice->CreateShaderResourceView(gFocusedObjectTexture, &srDesc, &gFocusedObjectTextureSRV)) ||
		FAILED(gD3DDevice->CreateShaderResourceView(gFocusedObjectTexture2, &srDesc, &gFocusedObjectTextureSRV2)))
	{
		gLastError = "Error creating normal depth shader resource view";
		return false;
	}

	return true;
}


// Prepare the scene
// Returns true on success
bool InitScene()
{
	////--------------- Set up scene ---------------////
	gStars = new Model(gStarsMesh);
	gStars->SetScale(8000.0f);

	gObjects.push_back(new Object(gGroundMesh, CVector3(0.0f, 0.0f, 0.0f), CVector3(0.0f, 0.0f, 0.0f), 1.0f, gGroundDiffuseSpecularMapSRV));		//0
	gObjects.push_back(new Object(gCubeMesh, CVector3(42, 5, -10), CVector3(0.0f, ToRadians(-110.0f), 0.0f), 1.5f, gCubeDiffuseSpecularMapSRV));	//1
	gObjects.push_back(new Object(gCrateMesh, CVector3(-10, 0, 90), CVector3(0.0f, ToRadians(40.0f), 0.0f), 6.0f, gCrateDiffuseSpecularMapSRV));	//2
	gObjects.push_back(new Object(gWall1Mesh, CVector3(15, 0, -5), CVector3(0, 3, 0), 30, gWallMapSRV));											//3
	gObjects.push_back(new Object(gWall2Mesh, CVector3(15, 15, -5), CVector3(0, 3, 0), 30, gWallMapSRV));											//4
	gObjects.push_back(new Object(gTeapotMesh, CVector3(35, 0, 65), CVector3(0, 2, 0), 1.6f, gTeapotMapSRV));										//5
	gObjects.push_back(new Object(gTrollMesh, CVector3(-20, 5, 55), CVector3(0.3f, 2, 0.1f), 10.0f, gTrollDiffuseSpecularMapSRV));					//6

	// Polygon postprocesses

	// An array of four points in world space - a tapered square centred at the origin
	const std::array<CVector3, 4> points = { { {-5,13,0}, {-5,3,0}, {5,13,0}, {5,3,0} } }; // C++ strangely needs an extra pair of {} here... only for std:array...

	// A rotating matrix placing the model above in the scene
	static CMatrix4x4 polyMatrix = MatrixTranslation(gObjects[3]->mModel->Position());
	polyMatrix = MatrixRotationY(3) * polyMatrix;

	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::Underwater, PostProcessMode::Polygon, new PolygonData(points, polyMatrix)));

	const std::array<CVector3, 4> points2 = { { {20,28,0}, {20,18,0}, {10,28,0}, {10,18,0} } };
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::HueShift, PostProcessMode::Polygon, new PolygonData(points2, polyMatrix)));
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::Retro, PostProcessMode::Polygon, new PolygonData(points2, polyMatrix)));

	const std::array<CVector3, 4> points3 = { { {10,28,0}, {10,18,0}, {0,28,0}, {0,18,0} } };
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::Gradient, PostProcessMode::Polygon, new PolygonData(points3, polyMatrix)));
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::FrostedGlass, PostProcessMode::Polygon, new PolygonData(points3, polyMatrix)));

	const std::array<CVector3, 4> points4 = { { {0,28,0}, {0,18,0}, {-10,28,0}, {-10,18,0} } };
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::ChromaticAberration, PostProcessMode::Polygon, new PolygonData(points4, polyMatrix)));

	const std::array<CVector3, 4> points5 = { { {-10,28,0}, {-10,18,0}, {-20,28,0}, {-20,18,0} } };
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::HueShift, PostProcessMode::Polygon, new PolygonData(points5, polyMatrix)));
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::Retro, PostProcessMode::Polygon, new PolygonData(points5, polyMatrix)));
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::Spiral, PostProcessMode::Polygon, new PolygonData(points5, polyMatrix)));
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::Distort, PostProcessMode::Polygon, new PolygonData(points5, polyMatrix)));
	gPolygonPostProcesses.push_back(new PostProcess(PostProcessType::ChromaticAberration, PostProcessMode::Polygon, new PolygonData(points5, polyMatrix)));

	// Light set-up - using an array this time
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		gLights[i].model = new Model(gLightMesh);
	}

	gLights[0].colour = { 0.8f, 0.8f, 1.0f };
	gLights[0].strength = 10;
	gLights[0].model->SetPosition({ 30, 10, 0 });
	gLights[0].model->SetScale(pow(gLights[0].strength, 1.0f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.

	gLights[1].colour = { 1.0f, 0.8f, 0.2f };
	gLights[1].strength = 40;
	gLights[1].model->SetPosition({ -70, 30, 100 });
	gLights[1].model->SetScale(pow(gLights[1].strength, 1.0f));


	////--------------- Set up camera ---------------////

	gCamera = new Camera();
	gCamera->SetPosition({ 85, 40, -25 });
	gCamera->SetRotation({ ToRadians(20.0f), ToRadians(-50.0f), 0.0f });

	return true;
}


// Release the geometry and scene resources created above
void ReleaseResources()
{
	ReleaseStates();

	if (gSceneTextureSRV)              gSceneTextureSRV->Release();
	if (gSceneRenderTarget)            gSceneRenderTarget->Release();
	if (gSceneTexture)                 gSceneTexture->Release();

	if (gSceneTextureSRV2)             gSceneTextureSRV2->Release();
	if (gSceneRenderTarget2)           gSceneRenderTarget2->Release();
	if (gSceneTexture2)                gSceneTexture2->Release();
									   
	if (gTempTextureSRV)               gTempTextureSRV->Release();
	if (gTempRenderTarget)             gTempRenderTarget->Release();
	if (gTempTexture)                  gTempTexture->Release();
									   
	if (gTempTextureSRV2)              gTempTextureSRV2->Release();
	if (gTempRenderTarget2)            gTempRenderTarget2->Release();
	if (gTempTexture2)                 gTempTexture2->Release();
									   
	if (gNormalDepthTextureSRV)        gNormalDepthTextureSRV->Release();
	if (gNormalDepthRenderTarget)      gNormalDepthRenderTarget->Release();
	if (gNormalDepthTexture)           gNormalDepthTexture->Release();
							   		   
	if (gNormalDepthTextureSRV2)       gNormalDepthTextureSRV2->Release();
	if (gNormalDepthRenderTarget2)     gNormalDepthRenderTarget2->Release();
	if (gNormalDepthTexture2)          gNormalDepthTexture2->Release();
									   
	if (gFocusedObjectTextureSRV)      gFocusedObjectTextureSRV->Release();
	if (gFocusedObjectRenderTarget)    gFocusedObjectRenderTarget->Release();
	if (gFocusedObjectTexture)         gFocusedObjectTexture->Release();
									   
	if (gFocusedObjectTextureSRV2)     gFocusedObjectTextureSRV2->Release();
	if (gFocusedObjectRenderTarget2)   gFocusedObjectRenderTarget2->Release();
	if (gFocusedObjectTexture2)        gFocusedObjectTexture2->Release();

	if (gDistortMapSRV)                gDistortMapSRV->Release();
	if (gDistortMap)                   gDistortMap->Release();
	if (gNoiseMap)					   gNoiseMap->Release();
	if (gNoiseMapSRV)                  gNoiseMapSRV->Release();
	if (gBurnMapSRV)                   gBurnMapSRV->Release();
	if (gBurnMap)                      gBurnMap->Release();
	if (gNoiseMap2)                    gNoiseMap2->Release();
	if (gNoiseMapSRV2)                 gNoiseMapSRV2->Release();

	if (gLightDiffuseMapSRV)           gLightDiffuseMapSRV->Release();
	if (gLightDiffuseMap)              gLightDiffuseMap->Release();
	if (gCrateDiffuseSpecularMapSRV)   gCrateDiffuseSpecularMapSRV->Release();
	if (gCrateDiffuseSpecularMap)      gCrateDiffuseSpecularMap->Release();
	if (gCubeDiffuseSpecularMapSRV)    gCubeDiffuseSpecularMapSRV->Release();
	if (gCubeDiffuseSpecularMap)       gCubeDiffuseSpecularMap->Release();
	if (gGroundDiffuseSpecularMapSRV)  gGroundDiffuseSpecularMapSRV->Release();
	if (gGroundDiffuseSpecularMap)     gGroundDiffuseSpecularMap->Release();
	if (gStarsDiffuseSpecularMapSRV)   gStarsDiffuseSpecularMapSRV->Release();
	if (gStarsDiffuseSpecularMap)      gStarsDiffuseSpecularMap->Release();

	if (gPostProcessingConstantBuffer) gPostProcessingConstantBuffer->Release();
	if (gPerModelConstantBuffer)       gPerModelConstantBuffer->Release();
	if (gPerFrameConstantBuffer)       gPerFrameConstantBuffer->Release();

	ReleaseShaders();

	// See note in InitGeometry about why we're not using unique_ptr and having to manually delete
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		delete gLights[i].model;  gLights[i].model = nullptr;
	}
	for (int i = gObjects.size() - 1; i >= 0; i--)
	{
		delete gObjects[i]; 
		gObjects[i] = nullptr;
	}
	gObjects.clear();

	delete gLightMesh;   gLightMesh = nullptr;
	delete gCrateMesh;   gCrateMesh = nullptr;
	delete gCubeMesh;    gCubeMesh = nullptr;
	delete gGroundMesh;  gGroundMesh = nullptr;
	delete gStarsMesh;   gStarsMesh = nullptr;

	for (int i = gFullScreenPostProcesses.size() - 1; i >= 0; i--)
	{
		delete gFullScreenPostProcesses[i];
		gFullScreenPostProcesses[i] = nullptr;
	}
	gFullScreenPostProcesses.clear();

	for (int i = gPolygonPostProcesses.size() - 1; i >= 0; i--)
	{
		delete gPolygonPostProcesses[i];
		gPolygonPostProcesses[i] = nullptr;
	}
	gPolygonPostProcesses.clear();
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

// Render everything in the scene from the given camera
void RenderSceneFromCamera(Camera* camera)
{
	// Set camera matrices in the constant buffer and send over to GPU
	gPerFrameConstants.cameraMatrix = camera->WorldMatrix();
	gPerFrameConstants.viewMatrix = camera->ViewMatrix();
	gPerFrameConstants.projectionMatrix = camera->ProjectionMatrix();
	gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
	UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

	// Indicate that the constant buffer we just updated is for use in the vertex shader (VS), geometry shader (GS) and pixel shader (PS)
	gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
	gD3DContext->GSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);
	gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);

	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);


	////--------------- Render ordinary models ---------------///

	// Select which shaders to use next
	gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)

	// States - no blending, normal depth buffer and back-face culling (standard set-up for opaque models)
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
	gD3DContext->RSSetState(gCullBackState);

	// Render lit models, only change textures for each one
	gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

	for (int i = 0; i < gObjects.size(); i++)
	{
		gD3DContext->PSSetShaderResources(0, 1, &gObjects[i]->mTexture); // First parameter must match texture slot number in the shader
		gObjects[i]->mModel->Render();
	}

	////--------------- Render sky ---------------////

	// Select which shaders to use next
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gTintedTexturePixelShader, nullptr, 0);

	// Using a pixel shader that tints the texture - don't need a tint on the sky so set it to white
	gPerModelConstants.objectColour = { 1, 1, 1 };

	// Stars point inwards
	gD3DContext->RSSetState(gCullNoneState);

	// Render sky
	gD3DContext->PSSetShaderResources(0, 1, &gStarsDiffuseSpecularMapSRV);
	gStars->Render();



	////--------------- Render lights ---------------////

	// Select which shaders to use next (actually same as before, so we could skip this)
	gD3DContext->VSSetShader(gBasicTransformVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gTintedTexturePixelShader, nullptr, 0);

	// Select the texture and sampler to use in the pixel shader
	gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV); // First parameter must match texture slot number in the shaer

	// States - additive blending, read-only depth buffer and no culling (standard set-up for blending)
	gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);

	// Render all the lights in the array
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		gPerModelConstants.objectColour = gLights[i].colour; // Set any per-model constants apart from the world matrix just before calling render (light colour here)
		gLights[i].model->Render();
	}
}

void RenderSceneNormalsAndDepth(ID3D11RenderTargetView* renderTarget)
{
	////--------------- Render depths and normals of each pixel to use in post-processing ---------------////
	
	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all
	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);
	gD3DContext->ClearRenderTargetView(renderTarget, &gBackgroundColor.r);
	gD3DContext->OMSetRenderTargets(1, &renderTarget, gDepthStencil);
	
	// Shaders
	gD3DContext->VSSetShader(gNormalDepthVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gNormalDepthPixelShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)
	
	// States
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
	gD3DContext->RSSetState(gCullBackState);

	// Render the models with depth
	for (int i = 0; i < gObjects.size(); i++)
	{
		gObjects[i]->mModel->Render();
	}
}

void RenderFocusedObject()
{
	if (gFocusedObject == 0)
	{
		return;
	}

	////--------------- Render depths and normals of each pixel to use in post-processing ---------------////

	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all
	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);
	gD3DContext->ClearRenderTargetView(gFocusedObjectRenderTarget, &gNDBackgroundColor.r);
	gD3DContext->OMSetRenderTargets(1, &gFocusedObjectRenderTarget, gDepthStencil);

	// Shaders
	gD3DContext->VSSetShader(gNormalDepthVertexShader, nullptr, 0);
	gD3DContext->PSSetShader(gNormalDepthPixelShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)

	// States
	gD3DContext->OMSetDepthStencilState(gNoDepthBufferState, 0);
	gD3DContext->RSSetState(gCullNoneState);

	// Render the models with depth
	gObjects[gFocusedObject]->mModel->Render();
}

//**************************

// Select the appropriate shader plus any additional textures required for a given post-process
// Helper function shared by full-screen, area and polygon post-processing functions below
void SelectPostProcessShaderAndTextures(PostProcessType postProcess)
{
	if (postProcess == PostProcessType::Copy)
	{
		gD3DContext->PSSetShader(gCopyPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::Gradient)
	{
		gD3DContext->PSSetShader(gGradientPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::BlurY)
	{
		gD3DContext->PSSetShader(gBlurYPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::BlurX)
	{
		gD3DContext->PSSetShader(gBlurXPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::Underwater)
	{
		gD3DContext->PSSetShader(gUnderwaterPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::DepthOfField)
	{
		gD3DContext->PSSetShader(gDepthOfFieldPostProcess, nullptr, 0);

		gD3DContext->PSSetShaderResources(1, 1, &gCurrentNormalDepthTextureSRV);
	}

	else if (postProcess == PostProcessType::Retro)
	{
		gD3DContext->PSSetShader(gRetroPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::Bloom)
	{
		gD3DContext->PSSetShader(gBloomPostProcess, nullptr, 0);

		gD3DContext->PSSetShaderResources(1, 1, &gCurrentBloomTextureSRV);
	}

	else if (postProcess == PostProcessType::Brightness)
	{
		gD3DContext->PSSetShader(gBrightnessPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::DirectionalBlur)
	{
		gD3DContext->PSSetShader(gDirectionalBlurPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::HueShift)
	{
		gD3DContext->PSSetShader(gHueShiftPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::ChromaticAberration)
	{
		gD3DContext->PSSetShader(gChromaticAberrationPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::Outline)
	{
		gD3DContext->PSSetShader(gOutlinePostProcess, nullptr, 0);

		gD3DContext->PSSetShaderResources(1, 1, &gCurrentNormalDepthTextureSRV);
	}

	else if (postProcess == PostProcessType::Dilation)
	{
		gD3DContext->PSSetShader(gDilationPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::FrostedGlass)
	{
		gD3DContext->PSSetShader(gFrostedGlassPostProcess, nullptr, 0);

		gD3DContext->PSSetShaderResources(1, 1, &gNoiseMapSRV2);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcessType::Selection)
	{
		gD3DContext->PSSetShader(gSelectionPostProcess, nullptr, 0);

		gD3DContext->PSSetShaderResources(1, 1, &gCurrentNormalDepthTextureSRV);
		gD3DContext->PSSetShaderResources(2, 1, &gCurrentFocusedObjectTextureSRV);
	}

	else if (postProcess == PostProcessType::Tint)
	{
		gD3DContext->PSSetShader(gTintPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::GreyNoise)
	{
		gD3DContext->PSSetShader(gGreyNoisePostProcess, nullptr, 0);

		// Give pixel shader access to the noise texture
		gD3DContext->PSSetShaderResources(1, 1, &gNoiseMapSRV);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcessType::Burn)
	{
		gD3DContext->PSSetShader(gBurnPostProcess, nullptr, 0);

		// Give pixel shader access to the burn texture (basically a height map that the burn level ascends)
		gD3DContext->PSSetShaderResources(1, 1, &gBurnMapSRV);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcessType::Distort)
	{
		gD3DContext->PSSetShader(gDistortPostProcess, nullptr, 0);

		// Give pixel shader access to the distortion texture (containts 2D vectors (in R & G) to shift the texture UVs to give a cut-glass impression)
		gD3DContext->PSSetShaderResources(1, 1, &gDistortMapSRV);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcessType::Spiral)
	{
		gD3DContext->PSSetShader(gSpiralPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcessType::HeatHaze)
	{
		gD3DContext->PSSetShader(gHeatHazePostProcess, nullptr, 0);
	}
}


void PostProcessSetup(ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* renderTarget, ID3D11BlendState* blendState)
{
	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all
	gD3DContext->OMSetRenderTargets(1, &renderTarget, gDepthStencil);

	// Give the pixel shader (post-processing shader) access to the scene texture 
	gD3DContext->PSSetShaderResources(0, 1, &srv);

	gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)


	// Using special vertex shader that creates its own data for a 2D screen quad
	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


	// States - no blending, don't write to depth buffer and ignore back-face culling
	gD3DContext->OMSetBlendState(blendState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);


	// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
	gD3DContext->IASetInputLayout(NULL); // No vertex data
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}


// Perform a full-screen post process from "scene texture" to back buffer
void FullScreenPostProcess(PostProcessType postProcess, ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* renderTarget, ID3D11BlendState* blendState)
{
	PostProcessSetup(srv, renderTarget, blendState);

	// Select shader and textures needed for the required post-processes (helper function above)
	SelectPostProcessShaderAndTextures(postProcess);


	// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
	gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
	gPostProcessingConstants.area2DSize    = { 1, 1 }; // Full size of screen
	gPostProcessingConstants.area2DDepth   = 0;        // Depth buffer value for full screen is as close as possible


	// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);


	// Draw a quad
	gD3DContext->Draw(4, 0);
}


// Perform an area post process from "scene texture" to back buffer at a given point in the world, with a given size (world units)
void AreaPostProcess(PostProcessType postProcess, ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* renderTarget, 
					 ID3D11BlendState* blendState, CVector3 worldPoint, CVector2 areaSize)
{
	PostProcessSetup(srv, renderTarget, blendState);

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess);

	// Use picking methods to find the 2D position of the 3D point at the centre of the area effect
	auto worldPointTo2D = gCamera->PixelFromWorldPt(worldPoint, gViewportWidth, gViewportHeight);
	CVector2 area2DCentre = { worldPointTo2D.x, worldPointTo2D.y };
	float areaDistance = worldPointTo2D.z;
	
	// Nothing to do if given 3D point is behind the camera
	if (areaDistance < gCamera->NearClip())  return;
	
	// Convert pixel coordinates to 0->1 coordinates as used by the shader
	area2DCentre.x /= gViewportWidth;
	area2DCentre.y /= gViewportHeight;



	// Using new helper function here - it calculates the world space units covered by a pixel at a certain distance from the camera.
	// Use this to find the size of the 2D area we need to cover the world space size requested
	CVector2 pixelSizeAtPoint = gCamera->PixelSizeInWorldSpace(areaDistance, gViewportWidth, gViewportHeight);
	CVector2 area2DSize = { areaSize.x / pixelSizeAtPoint.x, areaSize.y / pixelSizeAtPoint.y };

	// Again convert the result in pixels to a result to 0->1 coordinates
	area2DSize.x /= gViewportWidth;
	area2DSize.y /= gViewportHeight;



	// Send the area top-left and size into the constant buffer - the 2DQuad vertex shader will use this to create a quad in the right place
	gPostProcessingConstants.area2DTopLeft = area2DCentre - 0.5f * area2DSize; // Top-left of area is centre - half the size
	gPostProcessingConstants.area2DSize = area2DSize;

	// Manually calculate depth buffer value from Z distance to the 3D point and camera near/far clip values. Result is 0->1 depth value
	// We've never seen this full calculation before, it's occasionally useful. It is derived from the material in the Picking lecture
	// Having the depth allows us to have area effects behind normal objects
	gPostProcessingConstants.area2DDepth = gCamera->FarClip() * (areaDistance - gCamera->NearClip()) / (gCamera->FarClip() - gCamera->NearClip());
	gPostProcessingConstants.area2DDepth /= areaDistance;

	// Pass over this post-processing area to shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);


	// Draw a quad
	gD3DContext->Draw(4, 0);
}


// Perform an post process from "scene texture" to back buffer within the given four-point polygon and a world matrix to position/rotate/scale the polygon
void PolygonPostProcess(PostProcessType postProcess, ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* renderTarget, ID3D11BlendState* blendState, 
						const std::array<CVector3, 4>& points, const CMatrix4x4& worldMatrix)
{
	PostProcessSetup(srv, renderTarget, blendState);

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < points.size(); ++i)
	{
		CVector4 modelPosition = CVector4(points[i], 1);
		CVector4 worldPosition = modelPosition * worldMatrix;
		CVector4 viewportPosition = worldPosition * gCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the polygon points to the shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(gPostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &gPostProcessingConstantBuffer);

	// Select the special 2D polygon post-processing vertex shader and draw the polygon
	gD3DContext->VSSetShader(g2DPolygonVertexShader, nullptr, 0);
	gD3DContext->Draw(4, 0);
}


//**************************
void UpdateBloomEffectDirection(float directionOffset)
{
	gPostProcessingConstants.directionalBlurX = cos(gTempTimer + directionOffset);
	gPostProcessingConstants.directionalBlurY = sin(gTempTimer + directionOffset);
}

void RenderBloomTexture(ID3D11ShaderResourceView* srv)
{
	auto bloomSRV = srv;
	auto bloomRT = gTempRenderTarget2;

	FullScreenPostProcess(PostProcessType::Brightness, bloomSRV, bloomRT, gNoBlendingState);

	bloomSRV = gTempTextureSRV2;
	bloomRT = gTempRenderTarget;

	FullScreenPostProcess(PostProcessType::BlurY, bloomSRV, bloomRT, gNoBlendingState);

	bloomSRV = gTempTextureSRV;
	bloomRT = gTempRenderTarget2;

	FullScreenPostProcess(PostProcessType::BlurX, bloomSRV, bloomRT, gNoBlendingState);

	for (int j = 0; j < gTempDiagonalBlurs; j++)
	{
		UpdateBloomEffectDirection((float)j * (PI / gTempDiagonalBlurs));

		FullScreenPostProcess(PostProcessType::DirectionalBlur, bloomSRV, bloomRT, gAdditiveBlendingState);
	}

	bloomSRV = gTempTextureSRV2;
	bloomRT = gTempRenderTarget;

	gCurrentBloomTextureSRV = bloomSRV;
}

void ApplyPostProcess(PostProcess* postProcess, ID3D11ShaderResourceView* srv, ID3D11RenderTargetView* renderTarget)
{
	if (postProcess->Type == PostProcessType::Selection && gFocusedObject <= 0)
	{
		return;
	}

	if (postProcess->Type == PostProcessType::Bloom)
	{
		// Render a texture that shows blurred bright areas to use in the bloom post-process.
		RenderBloomTexture(srv);
	}

	if (postProcess->Mode == PostProcessMode::Fullscreen)
	{
		FullScreenPostProcess(postProcess->Type, srv, renderTarget, gNoBlendingState);
	}

	else if (postProcess->Mode == PostProcessMode::Area)
	{
		// Pass a 3D point for the centre of the affected area and the size of the (rectangular) area in world units
		AreaPostProcess(postProcess->Type, srv, renderTarget, gAlphaBlendingState, gLights[0].model->Position(), { 10, 10 });
	}

	else if (postProcess->Mode == PostProcessMode::Polygon)
	{
		// Pass an array of 4 points and a matrix. Only supports 4 points.
		if (postProcess->PolyData != nullptr)
		{
			PolygonPostProcess(postProcess->Type, srv, renderTarget, gNoBlendingState, postProcess->PolyData->Points, postProcess->PolyData->Matrix);
		}
	}
}

// Rendering the scene
void RenderScene()
{
	//// Common settings ////

	// Set up the light information in the constant buffer
	// Don't send to the GPU yet, the function RenderSceneFromCamera will do that
	gPerFrameConstants.light1Colour   = gLights[0].colour * gLights[0].strength;
	gPerFrameConstants.light1Position = gLights[0].model->Position();
	gPerFrameConstants.light2Colour   = gLights[1].colour * gLights[1].strength;
	gPerFrameConstants.light2Position = gLights[1].model->Position();

	gPerFrameConstants.ambientColour  = gAmbientColour;
	gPerFrameConstants.specularPower  = gSpecularPower;
	gPerFrameConstants.cameraPosition = gCamera->Position();

	gPerFrameConstants.viewportWidth  = static_cast<float>(gViewportWidth);
	gPerFrameConstants.viewportHeight = static_cast<float>(gViewportHeight);



	////--------------- Main scene rendering ---------------////

	// Set the target for rendering and select the main depth buffer.
	// If using post-processing then render to the scene texture, otherwise to the usual back buffer
	// Also clear the render target to a fixed colour and the depth buffer to the far distance
	gD3DContext->OMSetRenderTargets(1, &gSceneRenderTarget, gDepthStencil);
	gD3DContext->ClearRenderTargetView(gSceneRenderTarget, &gNDBackgroundColor.r);


	// Setup the viewport to the size of the main window
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(gViewportWidth);
	vp.Height = static_cast<FLOAT>(gViewportHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	gD3DContext->RSSetViewports(1, &vp);

	// Render the scene from the main camera
	RenderSceneFromCamera(gCamera);

	////--------------- Scene completion ---------------////

	// Run any post-processing steps
	RenderFocusedObject();
	gCurrentFocusedObjectTextureSRV = gFocusedObjectTextureSRV;
	auto foRenderTarget = gFocusedObjectRenderTarget2;

	RenderSceneNormalsAndDepth(gNormalDepthRenderTarget);
	gCurrentNormalDepthTextureSRV = gNormalDepthTextureSRV;
	auto ndRenderTarget = gNormalDepthRenderTarget2;

	auto srv = gSceneTextureSRV;
	auto renderTarget = gSceneRenderTarget2;

	gPostProcessingConstants.copyAlpha = 1.0f;

	FullScreenPostProcess(PostProcessType::Copy, srv, renderTarget, gNoBlendingState);
	
	FullScreenPostProcess(PostProcessType::Copy, gCurrentNormalDepthTextureSRV, ndRenderTarget, gNoBlendingState);

	if (gFocusedObject > 0)
	{
		FullScreenPostProcess(PostProcessType::Copy, gCurrentFocusedObjectTextureSRV, foRenderTarget, gNoBlendingState);
	}

	auto itA = gPolygonPostProcesses.begin();
	auto itB = gFullScreenPostProcesses.begin();
	int i = 0;
	int j = 0;

	while (itA != gPolygonPostProcesses.end() || itB != gFullScreenPostProcesses.end())
	{
		PostProcess* postProcess = *((itA != gPolygonPostProcesses.end()) ? itA : itB);

		ApplyPostProcess(postProcess, srv, renderTarget);

		// Switch between textures and render targets to apply multiple postprocesses.
		if (i % 2 == 0)
		{
			srv = gSceneTextureSRV2;
			renderTarget = gSceneRenderTarget;
		}
		else
		{
			srv = gSceneTextureSRV;
			renderTarget = gSceneRenderTarget2;
		}

		if (postProcess->Mode == PostProcessMode::Polygon || postProcess->Mode == PostProcessMode::Area)
		{
			FullScreenPostProcess(PostProcessType::Copy, srv, renderTarget, gNoBlendingState);
		}

		// If the post process distorts the image, apply that distortion to the normal/depth map as well. 
		if (postProcess->Type == PostProcessType::Retro	   ||
			postProcess->Type == PostProcessType::Spiral   || postProcess->Type == PostProcessType::Underwater ||
			postProcess->Type == PostProcessType::BlurX    || postProcess->Type == PostProcessType::BlurY      ||
			postProcess->Type == PostProcessType::Dilation || postProcess->Type == PostProcessType::FrostedGlass)
		{
			ApplyPostProcess(postProcess, gCurrentNormalDepthTextureSRV, ndRenderTarget);
			if (gFocusedObject != 0)
			{
				ApplyPostProcess(postProcess, gCurrentFocusedObjectTextureSRV, foRenderTarget);
			}

			// Switch between textures and render targets to apply multiple postprocesses.
			if (j % 2 == 0)
			{
				gCurrentNormalDepthTextureSRV = gNormalDepthTextureSRV2;
				ndRenderTarget = gNormalDepthRenderTarget;
				if (gFocusedObject != 0)
				{
					gCurrentFocusedObjectTextureSRV = gFocusedObjectTextureSRV2;
					foRenderTarget = gFocusedObjectRenderTarget;
				}
			}
			else
			{
				gCurrentNormalDepthTextureSRV = gNormalDepthTextureSRV;
				ndRenderTarget = gNormalDepthRenderTarget2;
				if (gFocusedObject != 0)
				{
					gCurrentFocusedObjectTextureSRV = gFocusedObjectTextureSRV;
					foRenderTarget = gFocusedObjectRenderTarget2;
				}
			}

			if (j == 0 && (postProcess->Mode == PostProcessMode::Polygon || postProcess->Mode == PostProcessMode::Area))
			{
				FullScreenPostProcess(PostProcessType::Copy, gCurrentNormalDepthTextureSRV, ndRenderTarget, gNoBlendingState);
				if (gFocusedObject != 0)
				{
					FullScreenPostProcess(PostProcessType::Copy, gCurrentFocusedObjectTextureSRV, foRenderTarget, gNoBlendingState);
				}
			}

			j++;
		}

		if (itA != gPolygonPostProcesses.end())
		{
			++itA;
		}
		else if (itB != gFullScreenPostProcesses.end())
		{
			++itB;
		}

		++i;
	}

	if (gCopyAlpha < 1.0f)
	{
		gPostProcessingConstants.copyAlpha = gCopyAlpha;
		FullScreenPostProcess(PostProcessType::Copy, srv, gBackBufferRenderTarget, gAlphaBlendingState);
	}
	else
	{
		FullScreenPostProcess(PostProcessType::Copy, srv, gBackBufferRenderTarget, gNoBlendingState);
	}

	// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
	ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);

	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
	// Set first parameter to 1 to lock to vsync
	gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------

// Update models and camera. frameTime is the time passed since the last frame
void UpdateScene(float frameTime)
{
	//***********

	// Select post process on keys
	if (KeyHit(Key_1))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Gradient));
	}
	else if (KeyHit(Key_2))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::BlurX));
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::BlurY));
	}
	else if (KeyHit(Key_3))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Underwater));
	}
	else if (KeyHit(Key_4))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::DepthOfField));
	}
	else if (KeyHit(Key_5))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Retro));
	}
	else if (KeyHit(Key_6))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Bloom));
	}
	else if (KeyHit(Key_7))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Dilation));
	}
	else if (KeyHit(Key_8))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::ChromaticAberration));
	}
	else if (KeyHit(Key_9))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Outline));
	}
	else if (KeyHit(Key_F1))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::HueShift));
	}
	else if (KeyHit(Key_F2))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::FrostedGlass));
	}
	else if (KeyHit(Key_F7))
	{
		gFullScreenPostProcesses.push_back(new PostProcess(PostProcessType::Selection));
	}
	else if (KeyHit(Key_0))
	{
		for (int i = gFullScreenPostProcesses.size() - 1; i >= 0; i--)
		{
			delete gFullScreenPostProcesses[i];
		}
		gFullScreenPostProcesses.clear();
	}
	else if (KeyHit(Key_Z))
	{
		if (gFullScreenPostProcesses.size() > 0)
		{
			gFullScreenPostProcesses.erase(gFullScreenPostProcesses.end() - 1);
		}
	}

	// Motion blur
	const float copyAlphaChange = 0.25f;
	if (KeyHeld(Key_F3))
	{
		gCopyAlpha += copyAlphaChange * frameTime;
	}
	else if (KeyHeld(Key_F4))
	{
		gCopyAlpha -= copyAlphaChange * frameTime;
	}
	gCopyAlpha = Clamp(gCopyAlpha, 0.05f, 1.0f);

	// Post processing settings - all data for post-processes is updated every frame whether in use or not (minimal cost)
	
	// Colour for tint shader
	gPostProcessingConstants.tintColour = { 1, 0, 0 };

	// Gradient shader hues
	static float hue = 0.5f;
	static float hue2 = 0.0f;
	const float hue1ChangeSpeed = 0.2f;
	const float hue2ChangeSpeed = 0.2f;
	static float hue1ChangeSpeedMult = 1.0f;
	static float hue2ChangeSpeedMult = 1.0f;
	const float min = -EPSILON;
	const float max = 1.0f + EPSILON;

	gPostProcessingConstants.gradientHue = { hue, hue2 };

	hue += hue1ChangeSpeed * hue1ChangeSpeedMult * frameTime;
	hue2 += hue2ChangeSpeed * hue2ChangeSpeedMult * frameTime;

	if (hue < min || hue > max)
	{
		hue1ChangeSpeedMult = hue1ChangeSpeedMult;
		hue = Clamp(hue);
	}
	
	if (hue2 < min || hue2 > max)
	{
		hue2ChangeSpeedMult = -hue2ChangeSpeedMult;
		hue2 = Clamp(hue2);
	}

	// Hue shift
	static float hueShift = 0.0f;
	hueShift += 0.2f * frameTime;
	gPostProcessingConstants.hueShift = hueShift;

	//Blur level
	const float standardDeviation = 5.2f;
	const float standardDeviationSquared = standardDeviation * standardDeviation;

	static float blurSize = 0.03f;
	const float blurSizeChangeSpeed = 0.1f;
	if (KeyHeld(Key_Comma))
	{
		blurSize -= blurSizeChangeSpeed * frameTime;
		if (blurSize < 0.0f)
		{
			blurSize = 0.0f;
		}
	}
	else if (KeyHeld(Key_Period))
	{
		blurSize += blurSizeChangeSpeed * frameTime;
	}

	gPostProcessingConstants.blurSize.x = blurSize;
	gPostProcessingConstants.blurSize.y = blurSize;
	gPostProcessingConstants.standardDeviationSquared = standardDeviationSquared;

	// Underwater effect
	const float waterHeight = 60.0f;
	gPostProcessingConstants.underwaterHue = Lerp(0.65f, 0.5f, gCamera->Position().y/waterHeight);
	gPostProcessingConstants.underwaterBrightness.y = Lerp(0.5f, 1.0f, gCamera->Position().y / waterHeight);
	gPostProcessingConstants.underwaterBrightness.x = Lerp(0.9f, 1.3f, gCamera->Position().y / waterHeight);

	gPostProcessingConstants.wobbleStrength = 0.005f;
	static float wobbleTimer = 0.0f;
	wobbleTimer += frameTime;
	gPostProcessingConstants.wobbleTimer = wobbleTimer;

	// Retro effect
	static float pixelSize = 8.0f;
	const float pixelSizeChangeSpeed = 10.0f;
	if (KeyHeld(Key_N))
	{
		pixelSize -= pixelSizeChangeSpeed * frameTime;
		if (pixelSize < 1.0f)
		{
			pixelSize = 1.0f;
		}
	}
	else if (KeyHeld(Key_M))
	{
		pixelSize += pixelSizeChangeSpeed * frameTime;
	}

	const CVector2 pixels = CVector2(gViewportWidth / floor(pixelSize), gViewportHeight / floor(pixelSize));
	gPostProcessingConstants.pixelNumber = pixels;

	gPostProcessingConstants.pixelBrightnessHueShift = 0.3f;
	gPostProcessingConstants.pixelBrightnessLevels = 12.0f;

	gPostProcessingConstants.pixelSaturationMin = 0.8f;
	gPostProcessingConstants.pixelSaturationLevels = 2.0f;

	const CVector2 pixelHueRange = CVector2(160.0f / 360.0f, 305.0f / 360.0f);
	gPostProcessingConstants.pixelHueRange = pixelHueRange;
	gPostProcessingConstants.pixelHueLevels = 7.0f;

	// Bloom effect
	static float bloomThreshold = 0.9f;
	const float bloomThresholdChangeSpeed = 0.3f;

	if (KeyHeld(Key_V))
	{
		bloomThreshold = Clamp(bloomThreshold + bloomThresholdChangeSpeed * frameTime);
	}
	else if (KeyHeld(Key_B))
	{
		bloomThreshold = Clamp(bloomThreshold - bloomThresholdChangeSpeed * frameTime);
	}

	gPostProcessingConstants.bloomThreshold = bloomThreshold;
	gPostProcessingConstants.bloomIntensity = 1.2f;

	static float bloomTimerChange = 1.0f;
	float bloomTimerMax = 1.0f;
	float directionalBlurSize = 0.15f + (1.0f - cos(gTempTimer)) * 0.4f;
	gPostProcessingConstants.directionalBlurSize = directionalBlurSize;
	gPostProcessingConstants.directionalBlurIntensity = 0.6f;

	UpdateBloomEffectDirection(0.0f);

	gTempTimer += bloomTimerChange * frameTime;

	if (gTempTimer > bloomTimerMax)
	{
		gTempTimer = bloomTimerMax - EPSILON;
		bloomTimerChange = -1.0f;
	}
	if (gTempTimer < 0.0f)
	{
		gTempTimer = EPSILON;
		bloomTimerChange = 1.0f;
	}

	if (KeyHit(Key_X))
	{
		gTempDiagonalBlurs = Clamp(gTempDiagonalBlurs - 1, 0, 20);
	}

	if (KeyHit(Key_C))
	{
		gTempDiagonalBlurs = Clamp(gTempDiagonalBlurs + 1, 0, 20);
	}

	// Chromatic aberration
	static float aberrationTimer = 0.0f;
	float colourOffset = cos(aberrationTimer) * 0.011f;
	CVector3 colourOffsets = CVector3(colourOffset, 0.0f, -colourOffset);
	gPostProcessingConstants.colourOffset = colourOffsets;
	aberrationTimer += frameTime;

	// Outline effect
	static float outlineThreshold = 0.12f;
	const float outlineThresholdChange = 0.5f;
	if (KeyHeld(Key_K))
	{
		outlineThreshold = Clamp(outlineThreshold + outlineThresholdChange * frameTime, 0.001f, 10.0f);
	}

	if (KeyHeld(Key_L))
	{
		outlineThreshold = Clamp(outlineThreshold - outlineThresholdChange * frameTime, 0.001f, 10.0f);
	}

	gPostProcessingConstants.outlineThreshold = outlineThreshold;
	gPostProcessingConstants.outlineThickness = 0.0012f;

	// Dilation effect
	static float dilationSize = 0.01f;
	const float dilationSizeChange = 0.01f;
	const float maxDilation = 0.05f;
	static float dilationType = 1;

	if (KeyHeld(Key_O))
	{
		dilationSize = Clamp(dilationSize - dilationSizeChange * frameTime, 0.0f, maxDilation);
	}

	if (KeyHeld(Key_P))
	{
		dilationSize = Clamp(dilationSize + dilationSizeChange * frameTime, 0.0f, maxDilation);
	}

	if (KeyHit(Key_Q))
	{
		dilationType++;
		if (dilationType > 2.0f)
		{
			dilationType = 0.0f;
		}
	}
	gPostProcessingConstants.dilationType = dilationType;

	const float aspectRatio = gViewportWidth / gViewportHeight;
	CVector2 dilationSizes = CVector2(dilationSize * aspectRatio, dilationSize);
	gPostProcessingConstants.dilationSize = dilationSizes;

	const CVector2 dilationThreshold = CVector2(0.05f, 0.5f);
	gPostProcessingConstants.dilationThreshold = dilationThreshold;

	// Depth of field effect
	static float focalPlane = 0.2f;
	static float planeDist = 0.15f;
	const float planeChangeSpeed = 0.1f;
	const float planeDistChangeSpeed = 0.2f;

	if (KeyHeld(Key_T))
	{
		focalPlane = Clamp(focalPlane - planeChangeSpeed * frameTime, -1.0f, 1.0f);
		gFocusedObject = 0;
	}
	else if (KeyHeld(Key_Y))
	{
		focalPlane = Clamp(focalPlane + planeChangeSpeed * frameTime, -1.0f, 1.0f);
		gFocusedObject = 0;
	}

	if (KeyHeld(Key_U))
	{
		planeDist = Clamp(planeDist - planeDistChangeSpeed * frameTime, 0.02f, 0.5f);
	}
	else if (KeyHeld(Key_I))
	{
		planeDist = Clamp(planeDist + planeDistChangeSpeed * frameTime, 0.02f, 0.5f);
	}

	if (KeyHit(Key_F6))
	{
		gFocusedObject++;
		if (gFocusedObject >= gObjects.size())
		{
			gFocusedObject = 0;
		}
	}
	if (KeyHit(Key_F5))
	{
		gFocusedObject--;
		if (gFocusedObject <= 0)
		{
			gFocusedObject = gObjects.size() - 1;
		}
	}

	if (gFocusedObject > 0)
	{
		CVector4 viewportPosition = CVector4(gObjects[gFocusedObject]->mModel->Position(), 1) * gCamera->ViewProjectionMatrix();
		float depth = viewportPosition.z / 500.0f;

		focalPlane = depth;
	}


	gPostProcessingConstants.nearPlane = Clamp(focalPlane - planeDist);
	gPostProcessingConstants.focalPlane = focalPlane;
	gPostProcessingConstants.farPlane = Clamp(focalPlane + planeDist);

	// Frosted glass
	gPostProcessingConstants.frostedGlassFrequency = 0.1f;
	gPostProcessingConstants.frostedGlassoffsetSize = CVector2(0.01f, 0.01f);

	// Noise scaling adjusts how fine the grey noise is.
	const float grainSize = 140; // Fineness of the noise grain
	gPostProcessingConstants.noiseScale  = { gViewportWidth / grainSize, gViewportHeight / grainSize };

	// The noise offset is randomised to give a constantly changing noise effect (like tv static)
	gPostProcessingConstants.noiseOffset = { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };

	// Set and increase the burn level (cycling back to 0 when it reaches 1.0f)
	const float burnSpeed = 0.2f;
	gPostProcessingConstants.burnHeight = fmod(gPostProcessingConstants.burnHeight + burnSpeed * frameTime, 1.0f);

	// Set the level of distortion
	gPostProcessingConstants.distortLevel = 0.03f;

	// Set and increase the amount of spiral - use a tweaked cos wave to animate
	static float wiggle = 0.0f;
	const float wiggleSpeed = 1.0f;
	gPostProcessingConstants.spiralLevel = ((1.0f - cos(wiggle)) * 4.0f );
	wiggle += wiggleSpeed * frameTime;

	// Update heat haze timer
	gPostProcessingConstants.heatHazeTimer += frameTime;

	//***********


	// Orbit one light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float lightRotate = 0.0f;
	static bool go = true;
	gLights[0].model->SetPosition({ 20 + cos(lightRotate) * gLightOrbitRadius, 10, 20 + sin(lightRotate) * gLightOrbitRadius });
	if (go)  lightRotate -= gLightOrbitSpeed * frameTime;
	if (KeyHit(Key_L))  go = !go;

	// Control of camera
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);

	// Toggle FPS limiting
	if (KeyHit(Key_P))  lockFPS = !lockFPS;

	// Show frame time / FPS in the window title //
	const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
	static float totalFrameTime = 0;
	static int frameCount = 0;
	totalFrameTime += frameTime;
	++frameCount;
	if (totalFrameTime > fpsUpdateTime)
	{
		// Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
		float avgFrameTime = totalFrameTime / frameCount;
		std::ostringstream frameTimeMs;
		frameTimeMs.precision(2);
		frameTimeMs << std::fixed << avgFrameTime * 1000;
		std::string windowTitle = "CO3303 Week 14: Area Post Processing - Frame Time: " + frameTimeMs.str() +
			"ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
		SetWindowTextA(gHWnd, windowTitle.c_str());
		totalFrameTime = 0;
		frameCount = 0;
	}
}
