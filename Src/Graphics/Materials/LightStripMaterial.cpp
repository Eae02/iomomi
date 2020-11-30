#include "LightStripMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../DeferredRenderer.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline lightStripPipelineEditor;
static eg::Pipeline lightStripPipelineGame;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LightStrip.vs.glsl").DefaultVariant();
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
	lightStripPipelineGame.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
	lightStripPipelineGame.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.label = "LightStripEditor";
	lightStripPipelineEditor = eg::Pipeline::Create(pipelineCI);
	lightStripPipelineEditor.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
}

static void OnShutdown()
{
	lightStripPipelineEditor.Destroy();
	lightStripPipelineGame.Destroy();
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
	if (mDrawArgs->drawMode == MeshDrawMode::Emissive)
		cmdCtx.BindPipeline(lightStripPipelineGame);
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
		cmdCtx.BindPipeline(lightStripPipelineEditor);
	else
		return false;
	
	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	return true;
}

bool LightStripMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	float pc[8] = 
	{
		color1.r, color1.g, color1.b, 0.0f,
		color2.r, color2.g, color2.b, transitionProgress
	};
	
	cmdCtx.PushConstants(0, sizeof(pc), pc);
	
	return true;
}

bool LightStripMaterial::CheckInstanceDataType(const std::type_info* instanceDataType) const
{
	return instanceDataType == &typeid(InstanceData);
}
