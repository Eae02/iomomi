#include "ForceFieldMaterial.hpp"

#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
#include "MeshDrawArgs.hpp"

ForceFieldMaterial::ForceFieldMaterial()
{
	eg::SpecializationConstantEntry specConstants[1] = { { .constantID = WATER_MODE_CONST_ID } };

	eg::GraphicsPipelineCreateInfo pipelineCI = {
		.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.vs.glsl").ToStageInfo(),
		.fragmentShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.fs.glsl").DefaultVariant(),
			.specConstants = specConstants,
		},
		.enableDepthTest = true,
		.enableDepthWrite = false,
		.topology = eg::Topology::TriangleStrip,
		.cullMode = eg::CullMode::None,
		.colorAttachmentFormats = {lightColorAttachmentFormat},
		.blendStates = { eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha) },
		.depthAttachmentFormat = GB_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly,
		.vertexAttributes = {
			{ POSITION_BINDING, eg::DataType::Float32, 2, 0 },
			{ INSTANCE_DATA_BINDING, eg::DataType::UInt32, 1, offsetof(ForceFieldInstance, mode) },
			{ INSTANCE_DATA_BINDING, eg::DataType::Float32, 3, offsetof(ForceFieldInstance, offset) },
			{ INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformX) },
			{ INSTANCE_DATA_BINDING, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformY) },
		},
	};
	pipelineCI.vertexBindings[POSITION_BINDING] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[INSTANCE_DATA_BINDING] = { sizeof(ForceFieldInstance), eg::InputRate::Instance };

	pipelineCI.label = "ForceField[BeforeWater]";
	specConstants[0].value = WATER_MODE_BEFORE;
	m_pipelineBeforeWater = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "ForceField[Final]";
	specConstants[0].value = WATER_MODE_AFTER;
	m_pipelineFinal = eg::Pipeline::Create(pipelineCI);

	m_descriptorSet0 = eg::DescriptorSet(m_pipelineFinal, 0);
	m_descriptorSet0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet0.BindTexture(eg::GetAsset<eg::Texture>("Textures/ForceFieldParticle.png"), 1);
	m_descriptorSet0.BindSampler(samplers::linearClampAnisotropic, 2);
}

size_t ForceFieldMaterial::PipelineHash() const
{
	return typeid(ForceFieldMaterial).hash_code();
}

bool ForceFieldMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	eg::PipelineRef pipeline;
	eg::DescriptorSetRef descriptorSet2;

	switch (mDrawArgs->drawMode)
	{
	case MeshDrawMode::TransparentBeforeWater:
		pipeline = m_pipelineBeforeWater;
		descriptorSet2 = blackPixelTextureDescriptorSet;
		break;
	case MeshDrawMode::TransparentBeforeBlur:
		pipeline = m_pipelineFinal;
		descriptorSet2 = mDrawArgs->renderTargets->blurredGlassDepthTextureDescriptorSet.value();
		break;
	case MeshDrawMode::TransparentFinal:
		pipeline = m_pipelineFinal;
		descriptorSet2 = blackPixelTextureDescriptorSet;
		break;
	default:
		return false;
	}

	cmdCtx.BindPipeline(pipeline);

	cmdCtx.BindDescriptorSet(m_descriptorSet0, 0);
	cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->GetWaterDepthTextureDescriptorSetOrDummy(), 1);
	cmdCtx.BindDescriptorSet(descriptorSet2, 2);

	return true;
}

bool ForceFieldMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}

eg::IMaterial::VertexInputConfiguration ForceFieldMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfiguration{
		.vertexBindingsMask = 0b1,
		.instanceDataBindingIndex = INSTANCE_DATA_BINDING,
	};
}
