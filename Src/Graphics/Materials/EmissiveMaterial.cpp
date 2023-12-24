#include "EmissiveMaterial.hpp"

#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"

static eg::Pipeline emissiveGeometryPipeline;
static eg::Pipeline emissivePipeline;

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
	emissiveGeometryPipeline = eg::Pipeline::Create(pipelineCI);
	emissiveGeometryPipeline.FramebufferFormatHint(DeferredRenderer::GEOMETRY_FB_FORMAT);

	pipelineCI.enableDepthWrite = false;
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Emissive.fs.glsl").DefaultVariant();
	pipelineCI.numColorAttachments = 1;
	pipelineCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One, eg::BlendFactor::SrcAlpha,
		eg::BlendFactor::One);
	pipelineCI.label = "Emissive";
	emissivePipeline = eg::Pipeline::Create(pipelineCI);
	emissivePipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	emissivePipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	emissivePipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
}

static void OnShutdown()
{
	emissiveGeometryPipeline.Destroy();
	emissivePipeline.Destroy();
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
		cmdCtx.BindPipeline(emissivePipeline);
		cmdCtx.PushConstants(0, 1.0f);
		break;
	case MeshDrawMode::Editor:
		cmdCtx.BindPipeline(emissivePipeline);
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
