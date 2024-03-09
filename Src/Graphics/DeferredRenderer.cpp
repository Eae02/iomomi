#include "DeferredRenderer.hpp"

#include "../Game.hpp"
#include "../Settings.hpp"
#include "../Water/WaterSimulationShaders.hpp"
#include "Lighting/LightMeshes.hpp"
#include "RenderSettings.hpp"

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

static const uint32_t SSAO_SAMPLES[3] = {
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
		float scale = static_cast<float>(i) / static_cast<float>(count);
		glm::vec3 sample(distNeg1To1(globalRNG), distNeg1To1(globalRNG), dist0To1(globalRNG));
		sample = glm::normalize(sample) * dist0To1(globalRNG) * glm::mix(0.1f, 1.0f, scale * scale);
		ssaoSamples[i] = glm::vec4(sample, 0);
	}
	return ssaoSamples;
}

DeferredRenderer::DeferredRenderer()
{
	m_shadowMapSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::Repeat,
		.wrapV = eg::WrapMode::Repeat,
		.wrapW = eg::WrapMode::Repeat,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Nearest,
		.enableCompare = true,
		.compareOp = eg::CompareOp::Less,
	});

	CreatePipelines();

	m_ssaoSamplesBuffer = eg::Buffer(
		eg::BufferFlags::UniformBuffer | eg::BufferFlags::Update, sizeof(float) * 4 * SSAO_SAMPLES[2], nullptr
	);

	// Initializes SSAO rotation data
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

	m_ssaoRotationsTextureSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::Repeat,
		.wrapV = eg::WrapMode::Repeat,
		.wrapW = eg::WrapMode::Repeat,
		.minFilter = eg::TextureFilter::Nearest,
		.magFilter = eg::TextureFilter::Nearest,
	});

	// Creates the SSAO rotation texture
	eg::TextureCreateInfo textureCI;
	textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;
	textureCI.width = SSAO_ROTATIONS_RES;
	textureCI.height = SSAO_ROTATIONS_RES;
	textureCI.format = eg::Format::R32G32_Float;
	textureCI.mipLevels = 1;
	m_ssaoRotationsTexture = eg::Texture::Create2D(textureCI);

	// Uploads SSAO rotation data
	eg::TextureRange textureDataRange = {};
	textureDataRange.sizeX = SSAO_ROTATIONS_RES;
	textureDataRange.sizeY = SSAO_ROTATIONS_RES;
	textureDataRange.sizeZ = 1;
	eg::DC.SetTextureData(
		m_ssaoRotationsTexture, textureDataRange, rotationDataUploadBuffer.buffer, rotationDataUploadBuffer.offset
	);
	m_ssaoRotationsTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

static inline eg::Pipeline CreateSSAOPipeline(uint32_t samples)
{
	const eg::SpecializationConstantEntry specConstantEntries[] = {
		{ 0, samples },                  // num samples
		{ 1, 1.0f / SSAO_ROTATIONS_RES } // texture scale
	};

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAO.fs.glsl").ToStageInfo();
	pipelineCI.fragmentShader.specConstants = specConstantEntries;
	pipelineCI.colorAttachmentFormats[0] = GetFormatForRenderTexture(RenderTex::SSAO, true);
	pipelineCI.label = "SSAO";
	eg::Pipeline pipeline = eg::Pipeline::Create(pipelineCI);
	return pipeline;
}

void DeferredRenderer::CreatePipelines()
{
	const eg::ShaderModuleAsset& ambientFragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/Ambient.fs.glsl");
	eg::GraphicsPipelineCreateInfo ambientPipelineCI;
	ambientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	ambientPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);

	ambientPipelineCI.fragmentShader = ambientFragmentShader.ToStageInfo("VSSAO");
	ambientPipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	ambientPipelineCI.label = "Ambient[SSAO]";
	m_ambientPipelineWithSSAO = eg::Pipeline::Create(ambientPipelineCI);

	ambientPipelineCI.fragmentShader = ambientFragmentShader.ToStageInfo("VNoSSAO");
	ambientPipelineCI.label = "Ambient[NoSSAO]";
	m_ambientPipelineWithoutSSAO = eg::Pipeline::Create(ambientPipelineCI);

	eg::SpecializationConstantEntry pointLightSpecConstants[2];
	pointLightSpecConstants[0].constantID = 0;
	pointLightSpecConstants[1].constantID = 1;

	eg::GraphicsPipelineCreateInfo plPipelineCI;
	plPipelineCI.vertexShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.vs.glsl").ToStageInfo();
	plPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.fs.glsl").ToStageInfo();
	plPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	plPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	plPipelineCI.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	plPipelineCI.cullMode = eg::CullMode::Back;
	plPipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	plPipelineCI.fragmentShader.specConstants = pointLightSpecConstants;

	for (uint32_t shadowMode = 0; shadowMode <= 2; shadowMode++)
	{
		for (uint32_t waterMode = 0; waterMode <= static_cast<uint32_t>(waterSimShaders.isWaterSupported); waterMode++)
		{
			pointLightSpecConstants[0].value = shadowMode;
			pointLightSpecConstants[1].value = waterMode;

			char label[32];
			snprintf(label, sizeof(label), "PointLight:S%u:W%u", shadowMode, waterMode);
			plPipelineCI.label = label;

			m_pointLightPipelines[shadowMode][waterMode] = eg::Pipeline::Create(plPipelineCI);
		}
	}

	eg::GraphicsPipelineCreateInfo ssaoDepthLinPipelineCI;
	ssaoDepthLinPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	ssaoDepthLinPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAODepthLin.fs.glsl").ToStageInfo();
	ssaoDepthLinPipelineCI.colorAttachmentFormats[0] = eg::Format::R32_Float;
	ssaoDepthLinPipelineCI.label = "SSAODepthLinearize";
	m_ssaoDepthLinPipeline = eg::Pipeline::Create(ssaoDepthLinPipelineCI);

	eg::GraphicsPipelineCreateInfo ssaoBlurPipelineCI;
	ssaoBlurPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	ssaoBlurPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAOBlur.fs.glsl").ToStageInfo();
	ssaoBlurPipelineCI.colorAttachmentFormats[0] = eg::Format::R8_UNorm;
	ssaoBlurPipelineCI.label = "SSAOBlur";
	m_ssaoBlurPipeline = eg::Pipeline::Create(ssaoBlurPipelineCI);

	for (size_t i = 0; i < std::size(SSAO_SAMPLES); i++)
	{
		m_ssaoPipelines[i] = CreateSSAOPipeline(SSAO_SAMPLES[i]);
	}
}

void DeferredRenderer::BeginGeometry(RenderTexManager& rtManager) const
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer =
		rtManager.GetFramebuffer(RenderTex::GBColor1, RenderTex::GBColor2, RenderTex::GBDepth, "Geometry");
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

static float* ssaoRadius = eg::TweakVarFloat("ssao_radius", 0.5f);
static float* ssaoDepthFadeMin = eg::TweakVarFloat("ssao_df_min", 0.2f);
static float* ssaoDepthFadeRate = eg::TweakVarFloat("ssao_df_rate", 0.05f);
static float* ssaoTolerance = eg::TweakVarFloat("ssao_tol", 0.95f);
static float* ssaoBlur = eg::TweakVarFloat("ssao_blur", 1.0f);

static int* ambientOnlyOcc = eg::TweakVarInt("amb_only_occ", 0, 0, 1);

void DeferredRenderer::PrepareSSAO(RenderTexManager& rtManager)
{
	// ** Initial SSAO Pass **

	const uint32_t numSSAOSamples = SSAO_SAMPLES[static_cast<int>(settings.ssaoQuality) - 1];
	if (numSSAOSamples != m_ssaoSamplesBufferCurrentSamples)
	{
		m_ssaoSamplesBuffer.UsageHint(eg::BufferUsage::CopyDst);

		std::vector<glm::vec4> samples = GenerateSSAOSamples(numSSAOSamples);
		m_ssaoSamplesBuffer.DCUpdateData<glm::vec4>(0, samples);

		m_ssaoSamplesBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
		m_ssaoSamplesBufferCurrentSamples = numSSAOSamples;
	}

	eg::MultiStageGPUTimer timer;

	timer.StartStage("SSAO - Depth Linearize");

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAOGBDepthLinear, {}, {}, "SSAO Depth Linearize");
	eg::DC.BeginRenderPass(rpBeginInfo);
	eg::DC.BindPipeline(m_ssaoDepthLinPipeline);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 0, &framebufferNearestSampler);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSAOGBDepthLinear);

	timer.StartStage("SSAO - Main");

	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAOUnblurred, {}, {}, "SSAO");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);

	eg::DC.BindPipeline(m_ssaoPipelines[static_cast<int>(settings.ssaoQuality) - 1]);

	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAOGBDepthLinear), 0, 1, &framebufferLinearSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2, &framebufferNearestSampler);
	eg::DC.BindTexture(m_ssaoRotationsTexture, 0, 3, &m_ssaoRotationsTextureSampler);
	eg::DC.BindUniformBuffer(m_ssaoSamplesBuffer, 0, 4);

	float pcData[] = {
		*ssaoRadius,
		*ssaoDepthFadeMin,
		1.0f / *ssaoDepthFadeRate,
		1.0f / *ssaoTolerance,
	};
	eg::DC.PushConstants(0, sizeof(pcData), pcData);

	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSAOUnblurred);

	// ** Blur passes **

	timer.StartStage("SSAO - Blur");

	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAOTempBlur, {}, {}, "SSAOBlur1");
	eg::DC.BeginRenderPass(rpBeginInfo);

	float pixelW = 2.0f / static_cast<float>(rtManager.ResX());
	float pixelH = 2.0f / static_cast<float>(rtManager.ResY());

	eg::DC.BindPipeline(m_ssaoBlurPipeline);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAOUnblurred), 0, 0, &framebufferLinearSampler);
	float pc1[4] = { 2 * pixelW * *ssaoBlur, 0.0f, pixelW / 2, pixelH / 2 };
	eg::DC.PushConstants(0, sizeof(pc1), pc1);
	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();
	rtManager.RenderTextureUsageHintFS(RenderTex::SSAOTempBlur);

	rpBeginInfo.framebuffer = rtManager.GetFramebuffer(RenderTex::SSAO, {}, {}, "SSAOBlur2");
	eg::DC.BeginRenderPass(rpBeginInfo);

	eg::DC.BindPipeline(m_ssaoBlurPipeline);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAOTempBlur), 0, 0, &framebufferLinearSampler);
	float pc2[4] = { 0.0f, 2 * pixelH * *ssaoBlur, -pixelW / 2, -pixelH / 2 };
	eg::DC.PushConstants(0, sizeof(pc2), pc2);
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
		eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::SSAO), 0, 5, &framebufferLinearSampler);
	}
	else
	{
		eg::DC.BindPipeline(m_ambientPipelineWithoutSSAO);
	}

	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor1), 0, 1, &framebufferNearestSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2, &framebufferNearestSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 3, &framebufferNearestSampler);

	float pc[4] = { ambientColor.r, ambientColor.g, ambientColor.b, static_cast<float>(*ambientOnlyOcc) };
	eg::DC.PushConstants(0, sizeof(pc), pc);

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

void DeferredRenderer::DrawPointLights(
	const std::vector<std::shared_ptr<PointLight>>& pointLights, bool hasWater, eg::TextureRef waterDepthTexture,
	RenderTexManager& rtManager, uint32_t shadowResolution
) const
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

	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0);

	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor1), 0, 1, &framebufferNearestSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBColor2), 0, 2, &framebufferNearestSampler);
	eg::DC.BindTexture(rtManager.GetRenderTexture(RenderTex::GBDepth), 0, 3, &framebufferNearestSampler);
	eg::DC.BindTexture(waterDepthTexture, 0, 4, &framebufferNearestSampler);
	eg::DC.BindTexture(eg::GetAsset<eg::Texture>("Textures/Caustics"), 0, 5, &commonTextureSampler);

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

	PointLightPC pc = {};
	pc.causticsScale = *causticsIntensity;
	pc.causticsColorOffset = *causticsColorOffset;
	pc.causticsPanSpeed = *causticsPanSpeed;
	pc.causticsTexScale = *causticsTexScale;
	pc.shadowSampleDist = shadowSoftness / static_cast<float>(shadowResolution);

	for (const std::shared_ptr<PointLight>& light : pointLights)
	{
		if (!light->enabled || light->shadowMap == nullptr)
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
		{
			eg::DC.BindTextureView(light->shadowMap, 0, 6, &m_shadowMapSampler);
		}

		eg::DC.DrawIndexed(0, POINT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
