#include "EmissiveMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"

static eg::Pipeline emissiveGeometryPipeline;
static eg::Pipeline emissivePipelineGame;
static eg::Pipeline emissivePipelineEditor;

static void OnInit()
{
	// clang-format off
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.vs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexBindings[VERTEX_BINDING_POSITION] = { VERTEX_STRIDE_POSITION, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[EmissiveMaterial::INSTANCE_DATA_BINDING] = { sizeof(EmissiveMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { VERTEX_BINDING_POSITION, eg::DataType::Float32, 3, 0 };
	pipelineCI.vertexAttributes[1] = { EmissiveMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[2] = { EmissiveMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[3] = { EmissiveMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[4] = { EmissiveMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { EmissiveMaterial::INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, 4 * sizeof(float) * 4 };
	// clang-format on

	pipelineCI.label = "EmissiveGeometry";
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EmissiveGeom.fs.glsl").ToStageInfo();
	pipelineCI.numColorAttachments = 2;
	pipelineCI.colorAttachmentFormats[0] = GB_COLOR_FORMAT;
	pipelineCI.colorAttachmentFormats[1] = GB_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	emissiveGeometryPipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.enableDepthWrite = false;
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.fs.glsl").ToStageInfo();
	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One, eg::BlendFactor::SrcAlpha,
		eg::BlendFactor::One
	);
	pipelineCI.label = "EmissiveGameLight";
	emissivePipelineGame = eg::Pipeline::Create(pipelineCI);

	// TODO: Set Alpha to 0
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.enableDepthWrite = true;
	pipelineCI.depthStencilUsage = eg::TextureUsage::FramebufferAttachment;
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
		break;
	case MeshDrawMode::Editor:
		cmdCtx.BindPipeline(emissivePipelineEditor);
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

eg::IMaterial::VertexInputConfiguration EmissiveMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfiguration{
		.vertexBindingsMask = 0b1,
		.instanceDataBindingIndex = INSTANCE_DATA_BINDING,
	};
}
