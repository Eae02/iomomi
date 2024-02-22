#include "GravityCornerLightMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"

GravityCornerLightMaterial GravityCornerLightMaterial::instance;

static eg::Pipeline gravityCornerPipeline;
static eg::Pipeline gravityCornerPipelineEditor;
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
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.label = "GravityCornerLight";
	gravityCornerPipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.label = "GravityCornerLightEditor";
	gravityCornerPipelineEditor = eg::Pipeline::Create(pipelineCI);

	gravityCornerDescriptorSet = eg::DescriptorSet(gravityCornerPipeline, 0);
	gravityCornerDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	gravityCornerDescriptorSet.BindTexture(
		eg::GetAsset<eg::Texture>("Textures/GravityCornerLightDist.png"), 1, &commonTextureSampler);
	gravityCornerDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 2, &commonTextureSampler);
}

static void OnShutdown()
{
	gravityCornerPipeline.Destroy();
	gravityCornerPipelineEditor.Destroy();
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

	if (mDrawArgs->drawMode == MeshDrawMode::Emissive)
		cmdCtx.BindPipeline(gravityCornerPipeline);
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
		cmdCtx.BindPipeline(gravityCornerPipelineEditor);
	else
		return false;

	cmdCtx.BindDescriptorSet(gravityCornerDescriptorSet, 0);
	return true;
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

eg::IMaterial::VertexInputConfiguration GravityCornerLightMaterial::GetVertexInputConfiguration(const void*) const
{
	return VertexInputConfig_SoaPXNTI;
}
