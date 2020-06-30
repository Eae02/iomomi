#include "GravitySwitchVolLightMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"
#include "../DeferredRenderer.hpp"
#include "../GraphicsCommon.hpp"
#include "../../Settings.hpp"

#include <random>
#include <ctime>

static eg::Pipeline gsVolLightPipelineBeforeWater;
static eg::Pipeline gsVolLightPipelineAfterWater;

//These values define the bounds of the AABB through which the ray will be traced. YMIN/YMAX define the bounds in the
// dimension that is normal to the gravity switch. SIZE defines the radius of the AABB in the other two dimensions.
static constexpr float YMIN = 0.1f;
static constexpr float YMAX = 2.0f;
static constexpr float SIZE = 1.0f;

static const float cubeVertices[] =
{
	-SIZE, YMAX,  SIZE,
	 SIZE, YMAX,  SIZE,
	-SIZE, YMIN,  SIZE,
	 SIZE, YMIN,  SIZE,
	 SIZE, YMIN, -SIZE,
	 SIZE, YMAX,  SIZE,
	 SIZE, YMAX, -SIZE,
	-SIZE, YMAX,  SIZE,
	-SIZE, YMAX, -SIZE,
	-SIZE, YMIN,  SIZE,
	-SIZE, YMIN, -SIZE,
	 SIZE, YMIN, -SIZE,
	-SIZE, YMAX, -SIZE,
	 SIZE, YMAX, -SIZE
};

static constexpr float MAX_ANGLE = 30.0f;

#pragma pack(push, 1)
struct LightDataBuffer
{
	//These two values derive from the previous constants
	float tMax;
	float inverseMaxY;
	
	float oneOverRaySteps;
	int quarterRaySteps;
	
	//Stores raySteps points along the ray (in parameter form) where samples should be made. These values are randomly
	// distributed along the ray and are recalculated if the number of steps changes (as a result of a quality change).
	float samplePoints[48];
};
#pragma pack(pop)

static QualityLevel currentQualityLevel = QualityLevel::VeryLow;

static eg::Buffer cubeVertexBuffer;
static eg::Buffer lightDataBuffer;

static eg::DescriptorSet lightVolDescriptorSet;

static void OnInit()
{
	eg::SpecializationConstantEntry waterModeSpecConstant;
	waterModeSpecConstant.constantID = WATER_MODE_CONST_ID;
	waterModeSpecConstant.size = sizeof(int32_t);
	waterModeSpecConstant.offset = 0;
	
	//Creates the volumetric light pipeline
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchVolLight.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchVolLight.fs.glsl").DefaultVariant();
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Front;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	pipelineCI.fragmentShader.specConstants = { &waterModeSpecConstant, 1 };
	pipelineCI.fragmentShader.specConstantsDataSize = sizeof(int32_t);
	
	pipelineCI.label = "GravSwitchVolLight[BeforeWater]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_BEFORE);
	gsVolLightPipelineBeforeWater = eg::Pipeline::Create(pipelineCI);
	gsVolLightPipelineBeforeWater.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
	gsVolLightPipelineBeforeWater.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	
	pipelineCI.label = "GravSwitchVolLight[AfterWater]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_AFTER);
	gsVolLightPipelineAfterWater = eg::Pipeline::Create(pipelineCI);
	gsVolLightPipelineAfterWater.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
	gsVolLightPipelineAfterWater.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	
	//Creates the light data uniform buffer
	eg::BufferCreateInfo lightDataBufferCreateInfo;
	lightDataBufferCreateInfo.label = "GravitySwitchVolUB";
	lightDataBufferCreateInfo.size = sizeof(LightDataBuffer);
	lightDataBufferCreateInfo.initialData = nullptr;
	lightDataBufferCreateInfo.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::Update;
	lightDataBuffer = eg::Buffer(lightDataBufferCreateInfo);
	
	//Creates the vertex buffer to use when rendering volumetric lighting
	eg::BufferCreateInfo vbCreateInfo;
	vbCreateInfo.label = "CubeVertexBuffer";
	vbCreateInfo.size = sizeof(cubeVertices);
	vbCreateInfo.initialData = cubeVertices;
	vbCreateInfo.flags = eg::BufferFlags::VertexBuffer;
	cubeVertexBuffer = eg::Buffer(vbCreateInfo);
	
	cubeVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	lightVolDescriptorSet = eg::DescriptorSet(gsVolLightPipelineBeforeWater, 0);
	lightVolDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	lightVolDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravitySwitchVolEmi.png"), 1);
	lightVolDescriptorSet.BindUniformBuffer(lightDataBuffer, 2, 0, sizeof(LightDataBuffer));
}

static void OnShutdown()
{
	gsVolLightPipelineBeforeWater.Destroy();
	gsVolLightPipelineAfterWater.Destroy();
	cubeVertexBuffer.Destroy();
	lightDataBuffer.Destroy();
	lightVolDescriptorSet.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

size_t GravitySwitchVolLightMaterial::PipelineHash() const
{
	return typeid(GravitySwitchVolLightMaterial).hash_code();
}

void GravitySwitchVolLightMaterial::SetQuality(QualityLevel qualityLevel)
{
	if (qualityLevel == currentQualityLevel)
		return;
	currentQualityLevel = qualityLevel;
	
	if (qualityLevel < QualityLevel::Medium)
		return;
	
	//Initializes the light data uniform buffer data
	LightDataBuffer lightDataBufferStruct;
	lightDataBufferStruct.tMax = std::tan(glm::radians(MAX_ANGLE));
	lightDataBufferStruct.inverseMaxY = 1.0f / (YMAX * 0.9f);
	
	int raySteps = 20;
	if (settings.lightingQuality == QualityLevel::High)
		raySteps = 32;
	else if (settings.lightingQuality == QualityLevel::VeryHigh)
		raySteps = 40;
	
	lightDataBufferStruct.oneOverRaySteps = 1.0f / raySteps;
	lightDataBufferStruct.quarterRaySteps = raySteps / 4;
	
	//Generates sample points
	std::mt19937 rng(std::time(nullptr));
	std::uniform_real_distribution<float> offDist(0.0f, 1.0f);
	for (int i = 0; i < raySteps; i++)
	{
		lightDataBufferStruct.samplePoints[i] = ((float)i + offDist(rng)) / (float)raySteps;
	}
	
	eg::DC.UpdateBuffer(lightDataBuffer, 0, sizeof(LightDataBuffer), &lightDataBufferStruct);
	lightDataBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

bool GravitySwitchVolLightMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	if (currentQualityLevel < QualityLevel::Medium)
		return false;
	
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeWater)
		cmdCtx.BindPipeline(gsVolLightPipelineBeforeWater);
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentAfterWater)
		cmdCtx.BindPipeline(gsVolLightPipelineAfterWater);
	else
		return false;
	
	cmdCtx.BindDescriptorSet(lightVolDescriptorSet, 0);
	cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 1, 0);
	
	return true;
}

bool GravitySwitchVolLightMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	float pcData[4 + 4 + 4 + 3];
	pcData[0] = switchPosition.x;
	pcData[1] = switchPosition.y;
	pcData[2] = switchPosition.z;
	pcData[3] = intensity;
	std::copy_n(&rotationMatrix[0].x, 3, pcData + 4);
	std::copy_n(&rotationMatrix[1].x, 3, pcData + 8);
	std::copy_n(&rotationMatrix[2].x, 3, pcData + 12);
	
	cmdCtx.PushConstants(0, sizeof(pcData), pcData);
	
	return true;
}

eg::MeshBatch::Mesh GravitySwitchVolLightMaterial::GetMesh()
{
	eg::MeshBatch::Mesh mesh;
	mesh.vertexBuffer = cubeVertexBuffer;
	mesh.numElements = 14;
	mesh.firstIndex = 0;
	mesh.firstVertex = 0;
	return mesh;
}
