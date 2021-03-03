#include "DeferredRenderer.hpp"
#include "Lighting/LightMeshes.hpp"
#include "RenderSettings.hpp"
#include "../Settings.hpp"
#include "../Game.hpp"

const eg::FramebufferFormatHint DeferredRenderer::GEOMETRY_FB_FORMAT =
{
	1, GB_DEPTH_FORMAT, { GB_COLOR_FORMAT, GB_COLOR_FORMAT }
};

static eg::SamplerDescription s_attachmentSamplerDesc;

static bool unlit = false;

static void OnInit()
{
	s_attachmentSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.minFilter = eg::TextureFilter::Nearest;
	s_attachmentSamplerDesc.magFilter = eg::TextureFilter::Nearest;
	s_attachmentSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
	
	eg::console::AddCommand("unlit", 0, [&] (eg::Span<const std::string_view>, eg::console::Writer&) {
		unlit = !unlit;
	});
}

EG_ON_INIT(OnInit)

constexpr uint32_t SSAO_SAMPLES_HQ = 24;
constexpr uint32_t SSAO_SAMPLES_LQ = 16;
constexpr uint32_t SSAO_ROTATIONS_RES = 32;

DeferredRenderer::DeferredRenderer()
{
	eg::SamplerDescription shadowMapSamplerDesc;
	shadowMapSamplerDesc.wrapU = eg::WrapMode::Repeat;
	shadowMapSamplerDesc.wrapV = eg::WrapMode::Repeat;
	shadowMapSamplerDesc.wrapW = eg::WrapMode::Repeat;
	shadowMapSamplerDesc.minFilter = eg::TextureFilter::Linear;
	shadowMapSamplerDesc.magFilter = eg::TextureFilter::Linear;
	shadowMapSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
	shadowMapSamplerDesc.enableCompare = true;
	shadowMapSamplerDesc.compareOp = eg::CompareOp::Less;
	m_shadowMapSampler = eg::Sampler(shadowMapSamplerDesc);
	
	CreatePipelines();
	
	//Initializes the SSAO samples buffer
	std::uniform_real_distribution<float> distNeg1To1(-1.0f, 1.0f);
	std::uniform_real_distribution<float> dist0To1(0.0f, 1.0f);
	glm::vec4 ssaoSamples[SSAO_SAMPLES_HQ];
	for (uint32_t i = 0; i < SSAO_SAMPLES_HQ; i++)
	{
		float scale = std::min((float)i / (float)SSAO_SAMPLES_LQ, 1.0f);
		glm::vec3 sample(distNeg1To1(globalRNG), distNeg1To1(globalRNG), dist0To1(globalRNG));
		sample = glm::normalize(sample) * dist0To1(globalRNG) * glm::mix(0.1f, 1.0f, scale * scale);
		ssaoSamples[i] = glm::vec4(sample, 0);
	}
	m_ssaoSamplesBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(ssaoSamples), ssaoSamples);
	m_ssaoSamplesBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
	
	//Initializes SSAO rotation data
	eg::UploadBuffer rotationDataUploadBuffer = eg::GetTemporaryUploadBuffer(SSAO_ROTATIONS_RES * SSAO_ROTATIONS_RES * sizeof(float), 8);
	volatile float* rotationDataPtr = reinterpret_cast<float*>(rotationDataUploadBuffer.Map());
	for (uint32_t i = 0; i < SSAO_ROTATIONS_RES * SSAO_ROTATIONS_RES; i++)
	{
		float t = std::uniform_real_distribution<float>(0.0f, eg::TWO_PI)(globalRNG);
		*(rotationDataPtr++) = std::sin(t);
		*(rotationDataPtr++) = std::cos(t);
	}
	rotationDataUploadBuffer.Flush();
	
	eg::SamplerDescription ssaoRotationsSampler;
	ssaoRotationsSampler.wrapU = eg::WrapMode::Repeat;
	ssaoRotationsSampler.wrapV = eg::WrapMode::Repeat;
	ssaoRotationsSampler.wrapW = eg::WrapMode::Repeat;
	ssaoRotationsSampler.minFilter = eg::TextureFilter::Linear;
	ssaoRotationsSampler.magFilter = eg::TextureFilter::Linear;
	
	//Creates the SSAO rotation texture
	eg::TextureCreateInfo textureCI;
	textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;
	textureCI.width = SSAO_ROTATIONS_RES;
	textureCI.height = SSAO_ROTATIONS_RES;
	textureCI.format = eg::Format::R32G32_Float;
	textureCI.mipLevels = 1;
	textureCI.defaultSamplerDescription = &ssaoRotationsSampler;
	m_ssaoRotationsTexture = eg::Texture::Create2D(textureCI);
	
	//Uploads SSAO rotation data
	eg::TextureRange textureDataRange = {};
	textureDataRange.sizeX = SSAO_ROTATIONS_RES;
	textureDataRange.sizeY = SSAO_ROTATIONS_RES;
	textureDataRange.sizeZ = 1;
	eg::DC.SetTextureData(m_ssaoRotationsTexture, textureDataRange, rotationDataUploadBuffer.buffer, rotationDataUploadBuffer.offset);
	m_ssaoRotationsTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

static inline eg::Pipeline CreateSSAOPipeline(uint32_t samples)
{
	struct SpecConstantData
	{
		uint32_t _samples;
		float rotationsTexScale;
	};
	SpecConstantData specConstantData;
	specConstantData._samples = samples;
	specConstantData.rotationsTexScale = 1.0f / SSAO_ROTATIONS_RES;
	
	eg::SpecializationConstantEntry specConstantEntries[2] =
		{
			{ 0, offsetof(SpecConstantData, _samples), sizeof(uint32_t) },
			{ 1, offsetof(SpecConstantData, rotationsTexScale), sizeof(float) }
		};
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAO.fs.glsl").DefaultVariant();
	pipelineCI.fragmentShader.specConstantsData = &specConstantData;
	pipelineCI.fragmentShader.specConstantsDataSize = sizeof(SpecConstantData);
	pipelineCI.fragmentShader.specConstants = specConstantEntries;
	eg::Pipeline pipeline = eg::Pipeline::Create(pipelineCI);
	pipeline.FramebufferFormatHint(eg::Format::R8_UNorm);
	return pipeline;
}

void DeferredRenderer::CreatePipelines()
{
	const eg::ShaderModuleAsset& ambientFragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/Ambient.fs.glsl");
	eg::GraphicsPipelineCreateInfo ambientPipelineCI;
	ambientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ambientPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	
	ambientPipelineCI.fragmentShader = ambientFragmentShader.GetVariant("VSSAO");
	m_ambientPipelineWithSSAO = eg::Pipeline::Create(ambientPipelineCI);
	m_ambientPipelineWithSSAO.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_ambientPipelineWithSSAO.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	ambientPipelineCI.fragmentShader = ambientFragmentShader.GetVariant("VNoSSAO");
	m_ambientPipelineWithoutSSAO = eg::Pipeline::Create(ambientPipelineCI);
	m_ambientPipelineWithoutSSAO.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_ambientPipelineWithoutSSAO.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	eg::GraphicsPipelineCreateInfo slPipelineCI;
	slPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SpotLight.vs.glsl").DefaultVariant();
	slPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SpotLight.fs.glsl").DefaultVariant();
	slPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	slPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	slPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	slPipelineCI.cullMode = eg::CullMode::Front;
	m_spotLightPipeline = eg::Pipeline::Create(slPipelineCI);
	m_spotLightPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_spotLightPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	eg::SpecializationConstantEntry pointLightSpecConstEntries[1];
	pointLightSpecConstEntries[0].constantID = 0;
	pointLightSpecConstEntries[0].size = sizeof(uint32_t);
	pointLightSpecConstEntries[0].offset = 0;
	uint32_t PL_SPEC_CONST_DATA_SOFT_SHADOWS = 1;
	uint32_t PL_SPEC_CONST_DATA_HARD_SHADOWS = 0;
	
	eg::GraphicsPipelineCreateInfo plPipelineCI;
	plPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.vs.glsl").DefaultVariant();
	plPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.fs.glsl").DefaultVariant();
	plPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	plPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	plPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	plPipelineCI.cullMode = eg::CullMode::Back;
	plPipelineCI.fragmentShader.specConstants = pointLightSpecConstEntries;
	plPipelineCI.fragmentShader.specConstantsData = &PL_SPEC_CONST_DATA_SOFT_SHADOWS;
	plPipelineCI.fragmentShader.specConstantsDataSize = sizeof(uint32_t);
	m_pointLightPipelineSoftShadows = eg::Pipeline::Create(plPipelineCI);
	m_pointLightPipelineSoftShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pointLightPipelineSoftShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	plPipelineCI.fragmentShader.specConstantsData = &PL_SPEC_CONST_DATA_HARD_SHADOWS;
	m_pointLightPipelineHardShadows = eg::Pipeline::Create(plPipelineCI);
	m_pointLightPipelineHardShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pointLightPipelineHardShadows.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
	eg::GraphicsPipelineCreateInfo ssaoBlurPipelineCI;
	ssaoBlurPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ssaoBlurPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAOBlur.fs.glsl").DefaultVariant();
	m_ssaoBlurPipeline = eg::Pipeline::Create(ssaoBlurPipelineCI);
	m_ssaoBlurPipeline.FramebufferFormatHint(eg::Format::R8_UNorm);
	
	m_ssaoPipelineLQ = CreateSSAOPipeline(SSAO_SAMPLES_LQ);
	m_ssaoPipelineHQ = CreateSSAOPipeline(SSAO_SAMPLES_HQ);
}

void DeferredRenderer::BeginGeometry(RenderTexManager& rtManager) const
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::GBColor1, RenderTex::GBColor2, RenderTex::GBDepth, "Geometry");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB(0.3f, 0.1f, 0.1f, 1);
	rpBeginInfo.colorAttachments[1].clearValue = eg::ColorLin(0, 0, 1, 1);
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::BeginGeometryFlags(RenderTexManager& rtManager) const
{
	eg::DC.EndRenderPass();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::Flags, {}, RenderTex::GBDepth, "GeometryFlags");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB(0, 0, 0, 0);
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::EndGeometry(RenderTexManager& rtManager) const
{
	eg::DC.EndRenderPass();
	
	rtManager.RenderTextureUsageHintFS(RenderTex::GBColor1);
	rtManager.RenderTextureUsageHintFS(RenderTex::GBColor2);
	rtManager.RenderTextureUsageHintFS(RenderTex::GBDepth);
	rtManager.RenderTextureUsageHintFS(RenderTex::Flags);
}

void DeferredRenderer::BeginTransparent(RenderTex destinationTexture, RenderTexManager& rtManager)
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(destinationTexture, {}, RenderTex::GBDepth, "Transparent");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::EndTransparent()
{
	eg::DC.EndRenderPass();
}

static float* ssaoRadius = eg::TweakVarFloat("ssao_radius", 0.7f);
static float* ssaoPower = eg::TweakVarFloat("ssao_power", 5.0f);
static float* ssaoDepthFadeMin = eg::TweakVarFloat("ssao_df_min", 0.2f);
static float* ssaoDepthFadeRate = eg::TweakVarFloat("ssao_df_rate", 0.05f);

void DeferredRenderer::PrepareSSAO(RenderTexManager& rtManager) const
{
	// ** Initial SSAO Pass **
	
	eg::MultiStageGPUTimer timer;
	timer.StartStage("SSAO - Initial");
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAOUnblurred, {}, {}, "SSAO");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(settings.lightingQuality == QualityLevel::VeryHigh ? m_ssaoPipelineHQ : m_ssaoPipelineLQ);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindUniformBuffer(m_ssaoSamplesBuffer, 0, 3, 0, sizeof(float) * 4 * SSAO_SAMPLES_HQ);
	eg::DC.BindTexture(m_ssaoRotationsTexture, 0, 4);
	
	float pcData[] = {
		*ssaoRadius,
		*ssaoDepthFadeMin,
		1.0f / *ssaoDepthFadeRate,
		*ssaoPower
	};
	eg::DC.PushConstants(0, sizeof(pcData), pcData);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSAOUnblurred);
	
	// ** Blur passes **
	
	timer.StartStage("SSAO - Blur");
	
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAOTempBlur, {}, {}, "SSAOBlur1");
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_ssaoBlurPipeline);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAOUnblurred), 0, 0);
	eg::DC.PushConstants(0, glm::vec2(1.0f / rtManager.ResX(), 0));
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSAOTempBlur);
	
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAO, {}, {}, "SSAOBlur2");
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_ssaoBlurPipeline);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAOTempBlur), 0, 0);
	eg::DC.PushConstants(0, glm::vec2(0, 1.0f / rtManager.ResY()));
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSAO);
}

static float* ambientIntensity = eg::TweakVarFloat("ambient_intensity", 0.1f);

void DeferredRenderer::BeginLighting(RenderTexManager& rtManager)
{
	if (settings.SSAOEnabled())
	{
		PrepareSSAO(rtManager);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::LitWithoutWater, {}, {}, "Lighting");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::ColorLin ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc));
	if (!unlit)
	{
		ambientColor = ambientColor.ScaleRGB(*ambientIntensity);
	}
	
	if (settings.SSAOEnabled())
	{
		eg::DC.BindPipeline(m_ambientPipelineWithSSAO);
		eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAO), 0, 5);
	}
	else
	{
		eg::DC.BindPipeline(m_ambientPipelineWithoutSSAO);
	}
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor1), 0, 1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 3);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::Flags), 0, 4);
	
	eg::DC.PushConstants(0, sizeof(float) * 3, &ambientColor.r);
	
	eg::DC.Draw(0, 3, 0, 1);
}

void DeferredRenderer::End() const
{
	eg::DC.EndRenderPass();
}

static float* causticsIntensity = eg::TweakVarFloat("cau_intensity", 10);
static float* causticsColorOffset = eg::TweakVarFloat("cau_clr_offset", 1.5f);
static float* causticsPanSpeed = eg::TweakVarFloat("cau_pan_speed", 0.05f);
static float* causticsTexScale = eg::TweakVarFloat("cau_tex_scale", 0.5f);

static const float shadowSoftnessByQualityLevel[] =
{
	0.0f,
	0.0f,
	1.0f,
	1.3f,
	1.6f
};

void DeferredRenderer::DrawPointLights(const std::vector<std::shared_ptr<PointLight>>& pointLights,
	eg::TextureRef waterDepthTexture, RenderTexManager& rtManager, uint32_t shadowResolution) const
{
	if (pointLights.empty() || unlit)
		return;
	
	auto gpuTimer = eg::StartGPUTimer("Point Lights");
	auto cpuTimer = eg::StartCPUTimer("Point Lights");
	
	bool softShadows = settings.shadowQuality >= QualityLevel::Medium;
	
	eg::DC.BindPipeline(softShadows ? m_pointLightPipelineSoftShadows : m_pointLightPipelineHardShadows);
	
	BindPointLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor1), 0, 1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 3);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::Flags), 0, 4);
	eg::DC.BindTexture(waterDepthTexture, 0, 5);
	eg::DC.BindTexture(eg::GetAsset<eg::Texture>("Caustics"), 0, 6);
	
	struct PointLightPC
	{
		glm::vec3 position;
		float range;
		glm::vec3 radiance;
		float invRange;
		float causticsScale;
		float causticsColorOffset;
		float causticsPanSpeed;
		float causticsTexScale;
		float shadowSampleDist;
		float specularIntensity;
	};
	
	PointLightPC pc;
	pc.causticsScale = *causticsIntensity;
	pc.causticsColorOffset = *causticsColorOffset;
	pc.causticsPanSpeed = *causticsPanSpeed;
	pc.causticsTexScale = *causticsTexScale;
	pc.shadowSampleDist = shadowSoftnessByQualityLevel[(int)settings.shadowQuality] / shadowResolution;
	
	for (const std::shared_ptr<PointLight>& light : pointLights)
	{
		if (!light->enabled || light->shadowMap.handle == nullptr)
			continue;
		
		pc.position = light->position;
		pc.range = light->Range();
		pc.radiance = light->Radiance();
		pc.invRange = 1.0f / pc.range;
		pc.specularIntensity = light->enableSpecularHighlights ? 1 : 0;
		eg::DC.PushConstants(0, pc);
		
		eg::DC.BindTexture(light->shadowMap, 0, 7, &m_shadowMapSampler);
		
		eg::DC.DrawIndexed(0, POINT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
