#include "EmissiveMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../DeferredRenderer.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline emissivePipelineEditor;
static eg::Pipeline emissivePipelineGame;
static eg::Pipeline emissivePipelinePlanarRefl;

static void OnInit()
{
	const eg::ShaderModuleAsset& vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.vs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = vertexShader.GetVariant("VDefault");
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
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
	emissivePipelineGame.FramebufferFormatHint(DeferredRenderer::LIGHT_COLOR_FORMAT, DeferredRenderer::DEPTH_FORMAT);
	
	pipelineCI.label = "EmissiveEditor";
	emissivePipelineEditor = eg::Pipeline::Create(pipelineCI);
	emissivePipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	pipelineCI.label = "EmissivePlanarRefl";
	pipelineCI.vertexShader = vertexShader.GetVariant("VPlanarRefl");
	pipelineCI.numClipDistances = 1;
	emissivePipelinePlanarRefl = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	emissivePipelineEditor.Destroy();
	emissivePipelineGame.Destroy();
	emissivePipelinePlanarRefl.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

EmissiveMaterial EmissiveMaterial::instance;

size_t EmissiveMaterial::PipelineHash() const
{
	return typeid(EmissiveMaterial).hash_code();
}

inline static eg::PipelineRef GetPipeline(const MeshDrawArgs& drawArgs)
{
	switch (drawArgs.drawMode)
	{
	case MeshDrawMode::Emissive: return emissivePipelineGame;
	case MeshDrawMode::Editor: return emissivePipelineEditor;
	case MeshDrawMode::PlanarReflection: return emissivePipelinePlanarRefl;
	default: return eg::PipelineRef();
	}
}

bool EmissiveMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	eg::PipelineRef pipeline = GetPipeline(*mDrawArgs);
	if (pipeline.handle == nullptr)
		return false;
	
	cmdCtx.BindPipeline(pipeline);
	
	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	if (mDrawArgs->drawMode == MeshDrawMode::PlanarReflection)
	{
		glm::vec4 pc(mDrawArgs->reflectionPlane.GetNormal(), -mDrawArgs->reflectionPlane.GetDistance());
		eg::DC.PushConstants(0, pc);
	}
	
	return true;
}

bool EmissiveMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
