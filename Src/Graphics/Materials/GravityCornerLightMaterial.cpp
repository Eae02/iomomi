#include "GravityCornerLightMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"

GravityCornerLightMaterial GravityCornerLightMaterial::instance;

static eg::Pipeline gravityCornerPipeline;
static eg::Pipeline gravityCornerPipelineEditor;

static float DEPTH_OFFSET = 0.001f;

static void OnInit()
{
	const eg::SpecializationConstantEntry depthOffsetSpecConstant = { COMMON_3D_DEPTH_OFFSET_CONST_ID, DEPTH_OFFSET };

	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityCornerLight.fs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexShader.specConstants = { &depthOffsetSpecConstant, 1 };
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.label = "GravityCornerLight";
	gravityCornerPipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.label = "GravityCornerLightEditor";
	pipelineCI.depthStencilUsage = eg::TextureUsage::FramebufferAttachment;
	gravityCornerPipelineEditor = eg::Pipeline::Create(pipelineCI);

	GravityCornerLightMaterial::instance.Initialize();
}

static void OnShutdown()
{
	gravityCornerPipeline.Destroy();
	gravityCornerPipelineEditor.Destroy();
	GravityCornerLightMaterial::instance = {};
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

size_t GravityCornerLightMaterial::PipelineHash() const
{
	return typeid(GravityCornerLightMaterial).hash_code();
}

bool GravityCornerLightMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	if (mDrawArgs->drawMode == MeshDrawMode::Emissive)
		cmdCtx.BindPipeline(gravityCornerPipeline);
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
		cmdCtx.BindPipeline(gravityCornerPipelineEditor);
	else
		return false;

	return true;
}

static constexpr float ACT_ANIMATION_DURATION = 0.5f;
static constexpr float ACT_ANIMATION_INTENSITY = 20.0f;

void GravityCornerLightMaterial::Initialize()
{
	m_parametersBuffer =
		eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, sizeof(float) * 4, nullptr);

	m_descriptorSet = eg::DescriptorSet(gravityCornerPipeline, 0);
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravityCornerLightDist.png"), 1);
	m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 2);
	m_descriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 3);
	m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 4);
}

bool GravityCornerLightMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	return true;
}

void GravityCornerLightMaterial::Activate(const glm::vec3& position)
{
	m_activationPos = position;
	m_activationIntensity = 1.0f;
}

void GravityCornerLightMaterial::Update(float dt)
{
	if (m_activationIntensity > 0.0f || !m_hasUpdatedParametersBuffer)
	{
		m_activationIntensity = std::max(m_activationIntensity - dt / ACT_ANIMATION_DURATION, 0.0f);

		const float parameters[4] = {
			m_activationPos.x,
			m_activationPos.y,
			m_activationPos.z,
			m_activationIntensity * ACT_ANIMATION_INTENSITY,
		};

		eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBufferWith<float>(parameters);
		eg::DC.CopyBuffer(uploadBuffer.buffer, m_parametersBuffer, uploadBuffer.offset, 0, sizeof(parameters));
		m_parametersBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);

		m_hasUpdatedParametersBuffer = true;
	}
}

eg::IMaterial::VertexInputConfiguration GravityCornerLightMaterial::GetVertexInputConfiguration(const void*) const
{
	return VertexInputConfig_SoaPXNTI;
}
