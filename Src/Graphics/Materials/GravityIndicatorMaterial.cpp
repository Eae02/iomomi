#include "GravityIndicatorMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline pipeline;
static eg::DescriptorSet descriptorSet;

GravityIndicatorMaterial GravityIndicatorMaterial::instance;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityIndicator.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityIndicator.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(GravityIndicatorMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, offsetof(GravityIndicatorMaterial::InstanceData, transform) + 0 };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, offsetof(GravityIndicatorMaterial::InstanceData, transform) + 16 };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(GravityIndicatorMaterial::InstanceData, transform) + 32 };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 3, offsetof(GravityIndicatorMaterial::InstanceData, down) };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 2, offsetof(GravityIndicatorMaterial::InstanceData, minIntensity) };
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	pipelineCI.label = "GravityIndicator";
	pipeline = eg::Pipeline::Create(pipelineCI);
	pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
	pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	descriptorSet = eg::DescriptorSet(pipeline, 0);
	descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 1);
}

static void OnShutdown()
{
	pipeline.Destroy();
	descriptorSet.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

size_t GravityIndicatorMaterial::PipelineHash() const
{
	return typeid(GravityIndicatorMaterial).hash_code();
}

bool GravityIndicatorMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	if (mDrawArgs->drawMode != MeshDrawMode::Emissive)
		return false;
	
	cmdCtx.BindPipeline(pipeline);
	cmdCtx.BindDescriptorSet(descriptorSet, 0);
	return true;
}

bool GravityIndicatorMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}

eg::IMaterial::OrderRequirement GravityIndicatorMaterial::GetOrderRequirement() const
{
	return OrderRequirement::OnlyUnordered;
}

bool GravityIndicatorMaterial::CheckInstanceDataType(const std::type_info* instanceDataType) const
{
	return instanceDataType == &typeid(InstanceData);
}
