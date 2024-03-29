#include "GravityCornerLightMaterial.hpp"

#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"

GravityCornerLightMaterial GravityCornerLightMaterial::instance;

static eg::Pipeline gravityCornerPipeline;
static eg::DescriptorSet gravityCornerDescriptorSet;

static float DEPTH_OFFSET = 0.001f;

static void OnInit()
{
	eg::SpecializationConstantEntry depthOffsetSpecConstant;
	depthOffsetSpecConstant.constantID = COMMON_3D_DEPTH_OFFSET_CONST_ID;
	depthOffsetSpecConstant.size = sizeof(float);
	depthOffsetSpecConstant.offset = 0;

	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityCornerLight.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexShader.specConstants = { &depthOffsetSpecConstant, 1 };
	pipelineCI.vertexShader.specConstantsDataSize = sizeof(float);
	pipelineCI.vertexShader.specConstantsData = &DEPTH_OFFSET;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.label = "GravityCornerLight";
	gravityCornerPipeline = eg::Pipeline::Create(pipelineCI);
	gravityCornerPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	gravityCornerPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
	gravityCornerPipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);

	gravityCornerDescriptorSet = eg::DescriptorSet(gravityCornerPipeline, 0);
	gravityCornerDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	gravityCornerDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravityCornerLightDist.png"), 1);
	gravityCornerDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 2, &commonTextureSampler);
}

static void OnShutdown()
{
	gravityCornerPipeline.Destroy();
	gravityCornerDescriptorSet.Destroy();
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

	if (mDrawArgs->drawMode == MeshDrawMode::Emissive || mDrawArgs->drawMode == MeshDrawMode::Editor)
	{
		cmdCtx.BindPipeline(gravityCornerPipeline);
		cmdCtx.BindDescriptorSet(gravityCornerDescriptorSet, 0);
		return true;
	}

	return false;
}

static constexpr float ACT_ANIMATION_DURATION = 0.5f;
static constexpr float ACT_ANIMATION_INTENSITY = 20.0f;

bool GravityCornerLightMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	float pc[4];
	pc[0] = m_activationPos.x;
	pc[1] = m_activationPos.y;
	pc[2] = m_activationPos.z;
	pc[3] = m_activationIntensity * ACT_ANIMATION_INTENSITY;
	cmdCtx.PushConstants(0, sizeof(pc), pc);

	return true;
}

void GravityCornerLightMaterial::Activate(const glm::vec3& position)
{
	m_activationPos = position;
	m_activationIntensity = 1.0f;
}

void GravityCornerLightMaterial::Update(float dt)
{
	m_activationIntensity = std::max(m_activationIntensity - dt / ACT_ANIMATION_DURATION, 0.0f);
}
