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

static int* unlit = eg::TweakVarInt("unlit", 0, 0, 1);

static void OnInit()
{
	s_attachmentSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	s_attachmentSamplerDesc.minFilter = eg::TextureFilter::Nearest;
	s_attachmentSamplerDesc.magFilter = eg::TextureFilter::Nearest;
	s_attachmentSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
}

EG_ON_INIT(OnInit)

static const uint32_t SSAO_SAMPLES[3] = 
{
	/* Low    */ 10,
	/* Medium */ 16,
	/* High   */ 24,
};

constexpr uint32_t SSAO_ROTATIONS_RES = 32;

static inline std::vector<glm::vec4> GenerateSSAOSamples(uint32_t count)
{
	std::uniform_real_distribution<float> distNeg1To1(-1.0f, 1.0f);
	std::uniform_real_distribution<float> dist0To1(0.0f, 1.0f);
	std::vector<glm::vec4> ssaoSamples(count);
	for (uint32_t i = 0; i < count; i++)
	{
		float scale = (float)i / (float)count;
		glm::vec3 sample(distNeg1To1(globalRNG), distNeg1To1(globalRNG), dist0To1(globalRNG));
		sample = glm::normalize(sample) * dist0To1(globalRNG) * glm::mix(0.1f, 1.0f, scale * scale);
		ssaoSamples[i] = glm::vec4(sample, 0);
	}
	return ssaoSamples;
}

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
	
	m_ssaoSamplesBuffer = eg::Buffer(
		eg::BufferFlags::UniformBuffer | eg::BufferFlags::Update,
		sizeof(float) * 4 * SSAO_SAMPLES[2], nullptr);
	
	//Initializes SSAO rotation data
	const size_t rotationDataSize = SSAO_ROTATIONS_RES * SSAO_ROTATIONS_RES * sizeof(float);
	eg::UploadBuffer rotationDataUploadBuffer = eg::GetTemporaryUploadBuffer(rotationDataSize, 8);
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
	pipeline.FramebufferFormatHint(GetFormatForRenderTexture(RenderTex::SSAO));
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
	
	eg::SpecializationConstantEntry pointLightSpecConstEntries[2];
	pointLightSpecConstEntries[0].constantID = 0;
	pointLightSpecConstEntries[0].size = sizeof(uint32_t);
	pointLightSpecConstEntries[0].offset = 0;
	pointLightSpecConstEntries[1].constantID = 1;
	pointLightSpecConstEntries[1].size = sizeof(uint32_t);
	pointLightSpecConstEntries[1].offset = sizeof(uint32_t);
	uint32_t pointLightSpecConstants[2];
	
	eg::GraphicsPipelineCreateInfo plPipelineCI;
	plPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.vs.glsl").DefaultVariant();
	plPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.fs.glsl").DefaultVariant();
	plPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	plPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	plPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	plPipelineCI.cullMode = eg::CullMode::Back;
	plPipelineCI.fragmentShader.specConstants = pointLightSpecConstEntries;
	plPipelineCI.fragmentShader.specConstantsData = pointLightSpecConstants;
	plPipelineCI.fragmentShader.specConstantsDataSize = sizeof(pointLightSpecConstants);
	
	for (uint32_t shadowMode = 0; shadowMode <= 2; shadowMode++)
	{
#ifdef IOMOMI_NO_WATER
		const uint32_t waterMode = 0;
#else
		for (uint32_t waterMode = 0; waterMode <= 1; waterMode++)
#endif
		{
			pointLightSpecConstants[0] = shadowMode;
			pointLightSpecConstants[1] = waterMode;
			m_pointLightPipelines[shadowMode][waterMode] = eg::Pipeline::Create(plPipelineCI);
			m_pointLightPipelines[shadowMode][waterMode].FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
			m_pointLightPipelines[shadowMode][waterMode].FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
		}
	}
	
	eg::GraphicsPipelineCreateInfo ssaoBlurPipelineCI;
	ssaoBlurPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ssaoBlurPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAOBlur.fs.glsl").DefaultVariant();
	m_ssaoBlurPipeline = eg::Pipeline::Create(ssaoBlurPipelineCI);
	m_ssaoBlurPipeline.FramebufferFormatHint(eg::Format::R8_UNorm);
	
	for (size_t i = 0; i < std::size(SSAO_SAMPLES); i++)
	{
		m_ssaoPipelines[i] = CreateSSAOPipeline(SSAO_SAMPLES[i]);
	}
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

void DeferredRenderer::EndGeometry(RenderTexManager& rtManager) const
{
	eg::DC.EndRenderPass();
	
	rtManager.RenderTextureUsageHintFS(RenderTex::GBColor1);
	rtManager.RenderTextureUsageHintFS(RenderTex::GBColor2);
	rtManager.RenderTextureUsageHintFS(RenderTex::GBDepth);
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

void DeferredRenderer::PrepareSSAO(RenderTexManager& rtManager)
{
	// ** Initial SSAO Pass **
	
	const uint32_t numSSAOSamples = SSAO_SAMPLES[(int)settings.ssaoQuality - 1];
	if (numSSAOSamples != m_ssaoSamplesBufferCurrentSamples)
	{
		m_ssaoSamplesBuffer.UsageHint(eg::BufferUsage::CopyDst);
		
		std::vector<glm::vec4> samples = GenerateSSAOSamples(numSSAOSamples);
		eg::DC.UpdateBuffer(m_ssaoSamplesBuffer, 0, samples.size() * sizeof(float) * 4, samples.data());
		
		m_ssaoSamplesBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
		m_ssaoSamplesBufferCurrentSamples = numSSAOSamples;
	}
	
	eg::MultiStageGPUTimer timer;
	timer.StartStage("SSAO - Initial");
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAOUnblurred, {}, {}, "SSAO");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_ssaoPipelines[(int)settings.ssaoQuality - 1]);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindUniformBuffer(m_ssaoSamplesBuffer, 0, 3, 0, sizeof(float) * 4 * SSAO_SAMPLES[2]);
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
	if (settings.ssaoQuality != SSAOQuality::Off)
	{
		PrepareSSAO(rtManager);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::LitWithoutWater, {}, {}, "Lighting");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::ColorLin ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc));
	if (*unlit == 0)
	{
		ambientColor = ambientColor.ScaleRGB(*ambientIntensity);
	}
	
	if (settings.ssaoQuality != SSAOQuality::Off)
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
static int* drawShadows = eg::TweakVarInt("draw_shadows", 1, 0, 1);

void DeferredRenderer::DrawPointLights(const std::vector<std::shared_ptr<PointLight>>& pointLights,
	bool hasWater, eg::TextureRef waterDepthTexture, RenderTexManager& rtManager, uint32_t shadowResolution) const
{
	if (pointLights.empty() || *unlit == 1)
		return;
	
	auto gpuTimer = eg::StartGPUTimer("Point Lights");
	auto cpuTimer = eg::StartCPUTimer("Point Lights");
	
	float shadowSoftness = qvar::shadowSoftness(settings.shadowQuality);
	
	int shadowMode;
	if (*drawShadows == 0)
		shadowMode = SHADOW_MODE_NONE;
	else if (shadowSoftness > 0)
		shadowMode = SHADOW_MODE_SOFT;
	else
		shadowMode = SHADOW_MODE_HARD;
	
	int renderCaustics = hasWater && qvar::waterRenderCaustics(settings.waterQuality);
	
	eg::DC.BindPipeline(m_pointLightPipelines[shadowMode][renderCaustics]);
	
	BindPointLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor1), 0, 1);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 3);
	
	if (hasWater)
	{
		eg::DC.BindTexture(waterDepthTexture, 0, 4);
		eg::DC.BindTexture(eg::GetAsset<eg::Texture>("Caustics"), 0, 5);
	}
	
	struct __attribute__((packed)) PointLightPC
	{
		float positionX;
		float positionY;
		float positionZ;
		float range;
		float radianceX;
		float radianceY;
		float radianceZ;
		float _radiancePad;
		float causticsScale;
		float causticsColorOffset;
		float causticsPanSpeed;
		float causticsTexScale;
		float shadowSampleDist;
		float specularIntensity;
	};
	
	PointLightPC pc = { };
	pc.causticsScale = *causticsIntensity;
	pc.causticsColorOffset = *causticsColorOffset;
	pc.causticsPanSpeed = *causticsPanSpeed;
	pc.causticsTexScale = *causticsTexScale;
	pc.shadowSampleDist = shadowSoftness / shadowResolution;
	
	for (const std::shared_ptr<PointLight>& light : pointLights)
	{
		if (!light->enabled || light->shadowMap.handle == nullptr)
			continue;
		
		pc.positionX = light->position.x;
		pc.positionY = light->position.y;
		pc.positionZ = light->position.z;
		pc.radianceX = light->Radiance().x;
		pc.radianceY = light->Radiance().y;
		pc.radianceZ = light->Radiance().z;
		pc.range = light->Range();
		pc.specularIntensity = light->enableSpecularHighlights ? 1 : 0;
		eg::DC.PushConstants(0, pc);
		
		if (*drawShadows)
			eg::DC.BindTexture(light->shadowMap, 0, 6, &m_shadowMapSampler);
		
		eg::DC.DrawIndexed(0, POINT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
