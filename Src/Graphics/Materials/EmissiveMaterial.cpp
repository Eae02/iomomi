#include "EmissiveMaterial.hpp"

#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"

static eg::Pipeline emissiveGeometryPipeline;
static eg::Pipeline emissivePipelineGame;
static eg::Pipeline emissivePipelineEditor;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.vs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(EmissiveMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 4 * sizeof(float) * 4 };

	pipelineCI.label = "EmissiveGeometry";
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EmissiveGeom.fs.glsl").DefaultVariant();
	pipelineCI.numColorAttachments = 2;
	pipelineCI.colorAttachmentFormats[0] = GB_COLOR_FORMAT;
	pipelineCI.colorAttachmentFormats[1] = GB_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	emissiveGeometryPipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.enableDepthWrite = false;
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.fs.glsl").DefaultVariant();
	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One, eg::BlendFactor::SrcAlpha,
		eg::BlendFactor::One);
	pipelineCI.label = "EmissiveGameLight";
	emissivePipelineGame = eg::Pipeline::Create(pipelineCI);

	pipelineCI.colorAttachmentFormats[0] = eg::Format::DefaultColor;
	pipelineCI.depthAttachmentFormat = eg::Format::DefaultDepthStencil;
	pipelineCI.label = "EmissiveEditor";
	emissivePipelineEditor = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	emissiveGeometryPipeline.Destroy();
	emissivePipelineGame.Destroy();
	emissivePipelineEditor.Destroy();
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

	switch (mDrawArgs->drawMode)
	{
	case MeshDrawMode::Emissive:
		cmdCtx.BindPipeline(emissivePipelineGame);
		cmdCtx.PushConstants(0, 1.0f);
		break;
	case MeshDrawMode::Editor:
		cmdCtx.BindPipeline(emissivePipelineEditor);
		cmdCtx.PushConstants(0, 0.0f);
		break;
	case MeshDrawMode::Game:
		cmdCtx.BindPipeline(emissiveGeometryPipeline);
		break;
	default:
		return false;
	}

	RenderSettings::instance->BindVertexShaderDescriptorSet();

	return true;
}

bool EmissiveMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
