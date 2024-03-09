#include "ForceFieldMaterial.hpp"

#include "../../Settings.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"

ForceFieldMaterial::ForceFieldMaterial()
{
	eg::SpecializationConstantEntry specConstants[1];
	specConstants[0].constantID = WATER_MODE_CONST_ID;

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = {
		.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.fs.glsl").DefaultVariant(),
		.specConstants = specConstants,
	};
	pipelineCI.vertexBindings[POSITION_BINDING] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[INSTANCE_DATA_BINDING] = { sizeof(ForceFieldInstance), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { POSITION_BINDING, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { INSTANCE_DATA_BINDING, eg::DataType::UInt32, 1,
		                               offsetof(ForceFieldInstance, mode) };
	pipelineCI.vertexAttributes[2] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 3,
		                               offsetof(ForceFieldInstance, offset) };
	pipelineCI.vertexAttributes[3] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 4,
		                               offsetof(ForceFieldInstance, transformX) };
	pipelineCI.vertexAttributes[4] = { INSTANCE_DATA_BINDING, eg::DataType::Float32, 4,
		                               offsetof(ForceFieldInstance, transformY) };
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.blendStates[0] =
		eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;

	pipelineCI.label = "ForceField[BeforeWater]";
	specConstants[0].value = WATER_MODE_BEFORE;
	m_pipelineBeforeWater = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "ForceField[Final]";
	specConstants[0].value = WATER_MODE_AFTER;
	m_pipelineFinal = eg::Pipeline::Create(pipelineCI);

	m_particleSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.maxAnistropy = settings.anisotropicFiltering,
	});
}

size_t ForceFieldMaterial::PipelineHash() const
{
	return typeid(ForceFieldMaterial).hash_code();
}

bool ForceFieldMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);

	auto CommonBind = [&]()
	{
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/ForceFieldParticle.png"), 0, 1, &m_particleSampler);
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 2, &framebufferNearestSampler);
	};

	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeWater)
	{
		cmdCtx.BindPipeline(m_pipelineBeforeWater);
		CommonBind();
		cmdCtx.BindTexture(blackPixelTexture, 0, 3, &framebufferNearestSampler);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindPipeline(m_pipelineFinal);
		CommonBind();
		cmdCtx.BindTexture(
			mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 0, 3, &framebufferNearestSampler
		);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
	{
		cmdCtx.BindPipeline(m_pipelineFinal);
		CommonBind();
		cmdCtx.BindTexture(blackPixelTexture, 0, 3, &framebufferNearestSampler);
		return true;
	}

	return false;
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
