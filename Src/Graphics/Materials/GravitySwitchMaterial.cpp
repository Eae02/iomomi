#include "GravitySwitchMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../../Utils.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"

static eg::Pipeline gravitySwitchPipelineEditor;
static eg::Pipeline gravitySwitchPipelineGame;
static eg::DescriptorSet gravitySwitchDescriptorSet;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchLight.fs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = false; // TODO: Is this ok?
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.label = "GravSwitchGame";
	gravitySwitchPipelineGame = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "GravSwitchEditor";
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.enableDepthWrite = true;
	pipelineCI.depthStencilUsage = eg::TextureUsage::FramebufferAttachment;
	gravitySwitchPipelineEditor = eg::Pipeline::Create(pipelineCI);

	gravitySwitchDescriptorSet = eg::DescriptorSet(gravitySwitchPipelineGame, 0);
	gravitySwitchDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	gravitySwitchDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 1);
	gravitySwitchDescriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 2);
	gravitySwitchDescriptorSet.BindUniformBuffer(
		frameDataUniformBuffer, 3, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 2
	);
}

static void OnShutdown()
{
	gravitySwitchPipelineEditor.Destroy();
	gravitySwitchPipelineGame.Destroy();
	gravitySwitchDescriptorSet.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

size_t GravitySwitchMaterial::PipelineHash() const
{
	return typeid(GravitySwitchMaterial).hash_code();
}

inline static eg::PipelineRef GetPipeline(const MeshDrawArgs& drawArgs)
{
	switch (drawArgs.drawMode)
	{
	case MeshDrawMode::Emissive:
		return gravitySwitchPipelineGame;
	case MeshDrawMode::Editor:
		return gravitySwitchPipelineEditor;
	default:
		return eg::PipelineRef();
	}
}

bool GravitySwitchMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	eg::PipelineRef pipeline = GetPipeline(*mDrawArgs);
	if (pipeline.handle == nullptr)
		return false;

	cmdCtx.BindPipeline(pipeline);

	return true;
}

bool GravitySwitchMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	const float parameters[2] = { intensity, timeOffset * 10 };
	uint32_t parametersOffset = PushFrameUniformData(ToCharSpan<float>(parameters));

	cmdCtx.BindDescriptorSet(gravitySwitchDescriptorSet, 0, { &parametersOffset, 1 });

	return true;
}

eg::IMaterial::VertexInputConfiguration GravitySwitchMaterial::GetVertexInputConfiguration(const void*) const
{
	return VertexInputConfig_SoaPXNTI;
}
