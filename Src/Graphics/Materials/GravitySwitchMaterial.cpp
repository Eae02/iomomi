#include "GravitySwitchMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"
#include "../RenderSettings.hpp"
#include "../DeferredRenderer.hpp"

static eg::Pipeline gravitySwitchPipelineEditor;
static eg::Pipeline gravitySwitchPipelineGame;
static eg::DescriptorSet gravitySwitchDescriptorSet;

static void OnInit()
{
	const eg::ShaderModuleAsset& fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravitySwitchLight.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = fragmentShader.GetVariant("VDefault");
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 2, offsetof(StaticPropMaterial::InstanceData, textureScale) };
	pipelineCI.numColorAttachments = 2;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.label = "GravSwitchGame";
	gravitySwitchPipelineGame = eg::Pipeline::Create(pipelineCI);
	gravitySwitchPipelineGame.FramebufferFormatHint(DeferredRenderer::GEOMETRY_FB_FORMAT);
	
	pipelineCI.label = "GravSwitchEditor";
	pipelineCI.fragmentShader = fragmentShader.GetVariant("VEditor");
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.numColorAttachments = 1;
	gravitySwitchPipelineEditor = eg::Pipeline::Create(pipelineCI);
	gravitySwitchPipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	gravitySwitchDescriptorSet = eg::DescriptorSet(gravitySwitchPipelineGame, 0);
	gravitySwitchDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	gravitySwitchDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 1);
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
	case MeshDrawMode::Game: return gravitySwitchPipelineGame;
	case MeshDrawMode::Editor: return gravitySwitchPipelineEditor;
	default: return eg::PipelineRef();
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
