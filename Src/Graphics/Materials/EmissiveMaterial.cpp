#include "EmissiveMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../DeferredRenderer.hpp"
#include "../RenderSettings.hpp"
#include "../../Settings.hpp"

static eg::Pipeline emissivePipelineEditor;
static eg::Pipeline emissivePipelineGame;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(EmissiveMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 4 * sizeof(float) * 4 };
	pipelineCI.numColorAttachments = 1;
	pipelineCI.label = "EmissiveGame";
	emissivePipelineGame = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.label = "EmissiveEditor";
	emissivePipelineEditor = eg::Pipeline::Create(pipelineCI);
	emissivePipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
}

static void OnShutdown()
{
	emissivePipelineEditor.Destroy();
	emissivePipelineGame.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

EmissiveMaterial EmissiveMaterial::instance;

size_t EmissiveMaterial::PipelineHash() const
{
	return typeid(EmissiveMaterial).hash_code();
}

bool EmissiveMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode == MeshDrawMode::Emissive)
		cmdCtx.BindPipeline(emissivePipelineGame);
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
		cmdCtx.BindPipeline(emissivePipelineEditor);
	else
		return false;
	
	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	return true;
}

bool EmissiveMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
