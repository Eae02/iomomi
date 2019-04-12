#include "LightStripMaterial.hpp"
#include "../DeferredRenderer.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline lightStripPipelineEditor;
static eg::Pipeline lightStripPipelineGame;
static eg::Pipeline lightStripPipelinePlanarRefl;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LightStrip.vs.glsl").GetVariant("VDefault");
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LightStrip.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(LightStripMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 1, 4 * sizeof(float) * 4 };
	pipelineCI.numColorAttachments = 1;
	pipelineCI.label = "LightStripGame";
	lightStripPipelineGame = eg::Pipeline::Create(pipelineCI);
	lightStripPipelineGame.FramebufferFormatHint(DeferredRenderer::LIGHT_COLOR_FORMAT, DeferredRenderer::DEPTH_FORMAT);
	
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.label = "LightStripEditor";
	lightStripPipelineEditor = eg::Pipeline::Create(pipelineCI);
	lightStripPipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LightStrip.vs.glsl").GetVariant("VPlanarRefl");
	pipelineCI.cullMode = eg::CullMode::Front;
	pipelineCI.numClipDistances = 1;
	pipelineCI.label = "LightStripPlanarRefl";
	lightStripPipelinePlanarRefl = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	lightStripPipelineEditor.Destroy();
	lightStripPipelineGame.Destroy();
	lightStripPipelinePlanarRefl.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

size_t LightStripMaterial::PipelineHash() const
{
	return typeid(LightStripMaterial).hash_code();
}

bool LightStripMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
	switch (mDrawArgs->drawMode)
	{
	case MeshDrawMode::Emissive:
		cmdCtx.BindPipeline(lightStripPipelineGame);
		break;
	case MeshDrawMode::Editor:
		cmdCtx.BindPipeline(lightStripPipelineEditor);
		break;
	case MeshDrawMode::PlanarReflection:
		cmdCtx.BindPipeline(lightStripPipelinePlanarRefl);
		break;
	default:
		return false;
	}
	
	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	return true;
}

bool LightStripMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	float pc[12] = 
	{
		color1.r, color1.g, color1.b, 0.0f,
		color2.r, color2.g, color2.b, transitionProgress
	};
	
	size_t pcSize = 8 * sizeof(float);
	
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
	if (mDrawArgs->drawMode == MeshDrawMode::PlanarReflection)
	{
		pc[8] = mDrawArgs->reflectionPlane.GetNormal().x;
		pc[9] = mDrawArgs->reflectionPlane.GetNormal().y;
		pc[10] = mDrawArgs->reflectionPlane.GetNormal().z;
		pc[11] = -mDrawArgs->reflectionPlane.GetDistance();
		pcSize += 4 * sizeof(float);
	}
	
	cmdCtx.PushConstants(0, pcSize, pc);
	
	return true;
}
