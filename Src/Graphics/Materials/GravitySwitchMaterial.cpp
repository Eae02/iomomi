#include "GravitySwitchMaterial.hpp"

#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"
#include "../../Editor/EditorGraphics.hpp"

static eg::Pipeline gravitySwitchPipelineEditor;
static eg::Pipeline gravitySwitchPipelineGame;
static eg::DescriptorSet gravitySwitchDescriptorSet;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchLight.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.label = "GravSwitchGame";
	gravitySwitchPipelineGame = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "GravSwitchEditor";
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	gravitySwitchPipelineEditor = eg::Pipeline::Create(pipelineCI);

	gravitySwitchDescriptorSet = eg::DescriptorSet(gravitySwitchPipelineGame, 0);
	gravitySwitchDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	gravitySwitchDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 1, &commonTextureSampler);
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
	cmdCtx.BindDescriptorSet(gravitySwitchDescriptorSet, 0);

	return true;
}

bool GravitySwitchMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	float pc[2] = { intensity, timeOffset * 10 };
	cmdCtx.PushConstants(0, sizeof(pc), pc);
	return true;
}
