#include "ForceFieldMaterial.hpp"

#include "../../Settings.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"

ForceFieldMaterial::ForceFieldMaterial()
{
	eg::SpecializationConstantEntry waterModeSpecConstant;
	waterModeSpecConstant.constantID = WATER_MODE_CONST_ID;
	waterModeSpecConstant.size = sizeof(int32_t);
	waterModeSpecConstant.offset = 0;

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.fs.glsl").DefaultVariant();
	pipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(ForceFieldInstance), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::UInt32, 1, offsetof(ForceFieldInstance, mode) };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 3, offsetof(ForceFieldInstance, offset) };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformX) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformY) };
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.blendStates[0] =
		eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.fragmentShader.specConstants = { &waterModeSpecConstant, 1 };
	pipelineCI.fragmentShader.specConstantsDataSize = sizeof(int32_t);

	pipelineCI.label = "ForceField[BeforeWater]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_BEFORE);
	m_pipelineBeforeWater = eg::Pipeline::Create(pipelineCI);
	m_pipelineBeforeWater.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	m_pipelineBeforeWater.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);

	pipelineCI.label = "ForceField[Final]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_AFTER);
	m_pipelineFinal = eg::Pipeline::Create(pipelineCI);
	m_pipelineFinal.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	m_pipelineFinal.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);

	m_particleSampler = eg::Sampler(eg::SamplerDescription{ .wrapU = eg::WrapMode::ClampToEdge,
	                                                        .wrapV = eg::WrapMode::ClampToEdge,
	                                                        .wrapW = eg::WrapMode::ClampToEdge,
	                                                        .maxAnistropy = settings.anisotropicFiltering });
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
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/ForceFieldParticle.png"), 0, 1, &m_particleSampler);
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 2);
	};

	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeWater)
	{
		cmdCtx.BindPipeline(m_pipelineBeforeWater);
		CommonBind();
		cmdCtx.BindTexture(blackPixelTexture, 0, 3);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindPipeline(m_pipelineFinal);
		CommonBind();
		cmdCtx.BindTexture(mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 0, 3);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
	{
		cmdCtx.BindPipeline(m_pipelineFinal);
		CommonBind();
		cmdCtx.BindTexture(blackPixelTexture, 0, 3);
		return true;
	}

	return false;
}

bool ForceFieldMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
