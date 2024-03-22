#include "GravitySwitchVolLightMaterial.hpp"

#include <ctime>

#include "../../Game.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
#include "MeshDrawArgs.hpp"

static eg::Pipeline gsVolLightPipelineBeforeWater;
static eg::Pipeline gsVolLightPipelineFinal;

// These values define the bounds of the AABB through which the ray will be traced. YMIN/YMAX define the bounds in the
//  dimension that is normal to the gravity switch. SIZE defines the radius of the AABB in the other two dimensions.
static constexpr float YMIN = 0.1f;
static constexpr float YMAX = 2.0f;
static constexpr float SIZE = 1.0f;

static const float cubeVertices[] = {
	-SIZE, YMAX, SIZE,  SIZE, YMAX,  SIZE, -SIZE, YMIN,  SIZE,  SIZE, YMIN,  SIZE, SIZE,  YMIN,
	-SIZE, SIZE, YMAX,  SIZE, SIZE,  YMAX, -SIZE, -SIZE, YMAX,  SIZE, -SIZE, YMAX, -SIZE, -SIZE,
	YMIN,  SIZE, -SIZE, YMIN, -SIZE, SIZE, YMIN,  -SIZE, -SIZE, YMAX, -SIZE, SIZE, YMAX,  -SIZE,
};

static constexpr float MAX_ANGLE = 30.0f;

struct LightDataBuffer
{
	// These two values derive from the previous constants
	float tMax;
	float inverseMaxY;

	float oneOverRaySteps;
	int quarterRaySteps;

	// Stores raySteps points along the ray (in parameter form) where samples should be made. These values are randomly
	//  distributed along the ray and are recalculated if the number of steps changes (as a result of a quality change).
	float samplePoints[48];
};

static QualityLevel currentQualityLevel = QualityLevel::VeryLow;

static eg::Buffer cubeVertexBuffer;
static eg::Buffer lightDataBuffer;

static eg::MeshBuffersDescriptor meshBuffersDescriptor;

static eg::DescriptorSet lightVolDescriptorSet;

static void OnInit()
{
	eg::SpecializationConstantEntry waterModeSpecConstant;
	waterModeSpecConstant.constantID = WATER_MODE_CONST_ID;

	// Creates the volumetric light pipeline
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchVolLight.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchVolLight.fs.glsl").ToStageInfo();
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Front;
	pipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.fragmentShader.specConstants = { &waterModeSpecConstant, 1 };

	pipelineCI.label = "GravSwitchVolLight[BeforeWater]";
	waterModeSpecConstant.value = WATER_MODE_BEFORE;
	gsVolLightPipelineBeforeWater = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "GravSwitchVolLight[Final]";
	waterModeSpecConstant.value = WATER_MODE_AFTER;
	gsVolLightPipelineFinal = eg::Pipeline::Create(pipelineCI);

	// Creates the light data uniform buffer
	eg::BufferCreateInfo lightDataBufferCreateInfo;
	lightDataBufferCreateInfo.label = "GravitySwitchVolUB";
	lightDataBufferCreateInfo.size = sizeof(LightDataBuffer);
	lightDataBufferCreateInfo.initialData = nullptr;
	lightDataBufferCreateInfo.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::Update;
	lightDataBuffer = eg::Buffer(lightDataBufferCreateInfo);

	// Creates the vertex buffer to use when rendering volumetric lighting
	eg::BufferCreateInfo vbCreateInfo;
	vbCreateInfo.label = "CubeVertexBuffer";
	vbCreateInfo.size = sizeof(cubeVertices);
	vbCreateInfo.initialData = cubeVertices;
	vbCreateInfo.flags = eg::BufferFlags::VertexBuffer;
	cubeVertexBuffer = eg::Buffer(vbCreateInfo);

	cubeVertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);

	meshBuffersDescriptor = {
		.vertexBuffer = cubeVertexBuffer,
		.vertexStreamOffsets = { 0 },
	};

	lightVolDescriptorSet = eg::DescriptorSet(gsVolLightPipelineBeforeWater, 0);
	lightVolDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	lightVolDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravitySwitchVolEmi.png"), 1);
	lightVolDescriptorSet.BindSampler(samplers::linearRepeat, 2);
	lightVolDescriptorSet.BindUniformBuffer(lightDataBuffer, 3, 0, sizeof(LightDataBuffer));
}

static void OnShutdown()
{
	gsVolLightPipelineBeforeWater.Destroy();
	gsVolLightPipelineFinal.Destroy();
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

	int raySteps = qvar::gravitySwitchVolLightSamples(qualityLevel);
	if (raySteps == 0)
		return;

	// Initializes the light data uniform buffer data
	LightDataBuffer lightDataBufferStruct;
	lightDataBufferStruct.tMax = std::tan(glm::radians(MAX_ANGLE));
	lightDataBufferStruct.inverseMaxY = 1.0f / (YMAX * 0.9f);
	lightDataBufferStruct.oneOverRaySteps = 1.0f / static_cast<float>(raySteps);
	lightDataBufferStruct.quarterRaySteps = raySteps / 4;

	// Generates sample points
	std::uniform_real_distribution<float> offDist(0.0f, 1.0f);
	for (int i = 0; i < raySteps; i++)
	{
		lightDataBufferStruct.samplePoints[i] =
			(static_cast<float>(i) + offDist(globalRNG)) / static_cast<float>(raySteps);
	}

	lightDataBuffer.DCUpdateData(0, sizeof(LightDataBuffer), &lightDataBufferStruct);
	lightDataBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

bool GravitySwitchVolLightMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	if (qvar::gravitySwitchVolLightSamples(currentQualityLevel) == 0)
		return false;

	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeWater)
		cmdCtx.BindPipeline(gsVolLightPipelineBeforeWater);
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
		cmdCtx.BindPipeline(gsVolLightPipelineFinal);
	else
		return false;

	cmdCtx.BindDescriptorSet(lightVolDescriptorSet, 0);
	cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->GetWaterDepthTextureDescriptorSetOrDummy(), 2);

	return true;
}

bool GravitySwitchVolLightMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	EG_ASSERT(m_descriptorSet.handle != nullptr);
	cmdCtx.BindDescriptorSet(m_descriptorSet, 1);
	return true;
}

eg::MeshBatch::Mesh GravitySwitchVolLightMaterial::GetMesh()
{
	return {
		.buffersDescriptor = &meshBuffersDescriptor,
		.numElements = 14,
	};
}

eg::IMaterial::VertexInputConfiguration GravitySwitchVolLightMaterial::GetVertexInputConfiguration(const void* drawArgs
) const
{
	return VertexInputConfiguration{ .vertexBindingsMask = 1 };
}

void GravitySwitchVolLightMaterial::SetParameters(
	const glm::vec3& position, const glm::mat3& rotationMatrix, float intensity
)
{
	float parametersData[16] = {};
	parametersData[0] = position.x;
	parametersData[1] = position.y;
	parametersData[2] = position.z;
	parametersData[3] = intensity;
	std::copy_n(&rotationMatrix[0].x, 3, parametersData + 4);
	std::copy_n(&rotationMatrix[1].x, 3, parametersData + 8);
	std::copy_n(&rotationMatrix[2].x, 3, parametersData + 12);

	if (m_parametersBuffer.handle == nullptr)
	{
		m_parametersBuffer = eg::Buffer(
			eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst, sizeof(parametersData), parametersData
		);
		m_descriptorSet = eg::DescriptorSet(gsVolLightPipelineFinal, 1);
		m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 0);
	}
	else
	{
		m_parametersBuffer.DCUpdateData<float>(0, parametersData);
	}
}
