#include "LightStripMaterial.hpp"
#include "../../Editor/EditorGraphics.hpp"

#include "../DeferredRenderer.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"

static eg::Pipeline lightStripPipelineEditor;
static eg::Pipeline lightStripPipelineGame;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LightStrip.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LightStrip.fs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = false; // TODO: Is this OK?
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.vertexBindings[VERTEX_BINDING_POSITION] = { VERTEX_STRIDE_POSITION, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[VERTEX_BINDING_TEXCOORD] = { VERTEX_STRIDE_TEXCOORD, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[LightStripMaterial::INSTANCE_DATA_BINDING] = {
		sizeof(LightStripMaterial::InstanceData),
		eg::InputRate::Instance,
	};
	pipelineCI.vertexAttributes[0] = { VERTEX_BINDING_POSITION, eg::DataType::Float32, 3, 0 };
	pipelineCI.vertexAttributes[1] = { VERTEX_BINDING_TEXCOORD, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[2] = { LightStripMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 0 };
	pipelineCI.vertexAttributes[3] = { LightStripMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 16 };
	pipelineCI.vertexAttributes[4] = { LightStripMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 32 };
	pipelineCI.vertexAttributes[5] = { LightStripMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 48 };
	pipelineCI.vertexAttributes[6] = { LightStripMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 1, 64 };
	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.label = "LightStripGame";
	lightStripPipelineGame = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "LightStripEditor";
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.enableDepthWrite = true;
	pipelineCI.depthStencilUsage = eg::TextureUsage::FramebufferAttachment;
	lightStripPipelineEditor = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	lightStripPipelineEditor.Destroy();
	lightStripPipelineGame.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

LightStripMaterial::LightStripMaterial()
	: m_parametersBuffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, sizeof(float) * 8, nullptr),
	  m_parametersDescriptorSet(lightStripPipelineGame, 1)
{
	m_parametersDescriptorSet.BindUniformBuffer(m_parametersBuffer, 0);
}

void LightStripMaterial::SetParameters(const Parameters& parameters)
{
	if (m_currentParameters.has_value() && *m_currentParameters == parameters)
		return;

	const float bufferData[8] = {
		parameters.color1.r, parameters.color1.g, parameters.color1.b, 0.0f,
		parameters.color2.r, parameters.color2.g, parameters.color2.b, parameters.transitionProgress,
	};
	m_parametersBuffer.DCUpdateData<float>(0, bufferData);
	m_parametersBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);

	m_currentParameters = parameters;
}

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

	RenderSettings::instance->BindVertexShaderDescriptorSet();

	return true;
}

bool LightStripMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	cmdCtx.BindDescriptorSet(m_parametersDescriptorSet, 1);
	return true;
}

bool LightStripMaterial::CheckInstanceDataType(const std::type_info* instanceDataType) const
{
	return instanceDataType == &typeid(InstanceData);
}

eg::IMaterial::VertexInputConfiguration LightStripMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfiguration{
		.vertexBindingsMask = 0b11,
		.instanceDataBindingIndex = INSTANCE_DATA_BINDING,
	};
}
