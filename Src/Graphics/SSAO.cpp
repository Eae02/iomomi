#include "SSAO.hpp"
#include "../Game.hpp"
#include "../Settings.hpp"
#include "RenderSettings.hpp"
#include "RenderTargets.hpp"

static const uint32_t SSAO_SAMPLES[3] = {
	/* Low    */ 10,
	/* Medium */ 16,
	/* High   */ 24,
};

static constexpr eg::Format SSAO_LINEAR_DEPTH_FORMAT = eg::Format::R32_Float;
static constexpr eg::Format SSAO_TEXTURE_FORMAT = eg::Format::R8_UNorm;

constexpr uint32_t SSAO_ROTATIONS_RES = 32;

static inline void GenerateSSAOSamples(std::span<glm::vec4> output)
{
	std::uniform_real_distribution<float> distNeg1To1(-1.0f, 1.0f);
	std::uniform_real_distribution<float> dist0To1(0.0f, 1.0f);
	for (size_t i = 0; i < output.size(); i++)
	{
		float scale = static_cast<float>(i) / static_cast<float>(output.size());
		glm::vec3 sample(distNeg1To1(globalRNG), distNeg1To1(globalRNG), dist0To1(globalRNG));
		sample = glm::normalize(sample) * dist0To1(globalRNG) * glm::mix(0.1f, 1.0f, scale * scale);
		output[i] = glm::vec4(sample, 0);
	}
}

static eg::Pipeline ssaoDepthLinPipeline;
static std::array<eg::Pipeline, 3> ssaoPipelines;
static eg::Pipeline ssaoBlurPipeline;

static void OnInit()
{
	const eg::ShaderModuleHandle postVS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();

	ssaoDepthLinPipeline = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = eg::ShaderStageInfo{ .shaderModule = postVS },
		.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAODepthLin.fs.glsl").ToStageInfo(),
		.colorAttachmentFormats = { SSAO_LINEAR_DEPTH_FORMAT },
		.label = "SSAODepthLinearize",
	});

	ssaoBlurPipeline = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = eg::ShaderStageInfo{ .shaderModule = postVS },
		.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAOBlur.fs.glsl").ToStageInfo(),
		.colorAttachmentFormats = { SSAO_TEXTURE_FORMAT },
		.label = "SSAOBlur",
	});

	const eg::ShaderModuleHandle ssaoFS =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/SSAO.fs.glsl").DefaultVariant();

	for (size_t i = 0; i < std::size(SSAO_SAMPLES); i++)
	{
		const eg::SpecializationConstantEntry specConstantEntries[] = {
			{ 0, SSAO_SAMPLES[i] },          // num samples
			{ 1, 1.0f / SSAO_ROTATIONS_RES } // texture scale
		};
		ssaoPipelines[i] = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo {
			.vertexShader = eg::ShaderStageInfo{ .shaderModule = postVS },
			.fragmentShader = {
				.shaderModule = ssaoFS,
				.specConstants = specConstantEntries,
			},
			.colorAttachmentFormats = { SSAO_TEXTURE_FORMAT },
			.label = "SSAO",
		});
	}
}

static void OnShutdown()
{
	ssaoDepthLinPipeline.Destroy();
	ssaoPipelines = {};
	ssaoBlurPipeline.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

static constexpr uint32_t BLUR_PARAMETERS_BYTES_PER_PASS = sizeof(float) * 4;

SSAO::SSAO()
{
	m_parametersBuffer = eg::Buffer(
		eg::BufferFlags::UniformBuffer | eg::BufferFlags::Update,
		sizeof(glm::vec4) + sizeof(glm::vec4) * SSAO_SAMPLES[2], nullptr
	);

	// Initializes SSAO rotation data
	glm::vec2 rotationsTextureData[SSAO_ROTATIONS_RES * SSAO_ROTATIONS_RES];
	for (uint32_t i = 0; i < SSAO_ROTATIONS_RES * SSAO_ROTATIONS_RES; i++)
	{
		float t = std::uniform_real_distribution<float>(0.0f, eg::TWO_PI)(globalRNG);
		rotationsTextureData[i] = glm::vec2(std::sin(t), std::cos(t));
	}

	// Creates the SSAO rotation texture
	m_ssaoRotationsTexture = eg::Texture::Create2D({
		.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst,
		.mipLevels = 1,
		.width = SSAO_ROTATIONS_RES,
		.height = SSAO_ROTATIONS_RES,
		.format = eg::Format::R32G32_Float,
	});

	// Uploads SSAO rotation data
	m_ssaoRotationsTexture.SetData(
		ToCharSpan<glm::vec2>(rotationsTextureData),
		eg::TextureRange{
			.sizeX = SSAO_ROTATIONS_RES,
			.sizeY = SSAO_ROTATIONS_RES,
			.sizeZ = 1,
		}
	);
	m_ssaoRotationsTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	m_blurParametersBufferSecondOffset = eg::RoundToNextMultiple(
		BLUR_PARAMETERS_BYTES_PER_PASS, eg::GetGraphicsDeviceInfo().uniformBufferOffsetAlignment
	);
	m_blurParametersBuffer = eg::Buffer(
		eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer,
		m_blurParametersBufferSecondOffset + BLUR_PARAMETERS_BYTES_PER_PASS, nullptr
	);

	m_mainPassDS0 = eg::DescriptorSet(ssaoPipelines[0], 0);
	m_mainPassDS0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_mainPassDS0.BindUniformBuffer(m_parametersBuffer, 1);
	m_mainPassDS0.BindTexture(m_ssaoRotationsTexture, 2);
	m_mainPassDS0.BindSampler(samplers::linearRepeat, 3);
	m_mainPassDS0.BindSampler(samplers::linearClamp, 4);
}

static float* ssaoRadius = eg::TweakVarFloat("ssao_radius", 0.5f);
static float* ssaoDepthFadeMin = eg::TweakVarFloat("ssao_df_min", 0.2f);
static float* ssaoDepthFadeRate = eg::TweakVarFloat("ssao_df_rate", 0.05f);
static float* ssaoTolerance = eg::TweakVarFloat("ssao_tol", 0.95f);
static float* ssaoBlur = eg::TweakVarFloat("ssao_blur", 1.0f);

void SSAO::RenderTargetsCreated(const RenderTargets& renderTargets)
{
	if (renderTargets.resX != m_texBlurred.Width() || renderTargets.resY != m_texBlurred.Height())
	{
		m_texLinearDepth = renderTargets.MakeAttachment(SSAO_LINEAR_DEPTH_FORMAT, "SSAOLinearDepth");
		m_texUnblurred = renderTargets.MakeAttachment(SSAO_TEXTURE_FORMAT, "SSAO");
		m_texBlurredOneAxis = renderTargets.MakeAttachment(SSAO_TEXTURE_FORMAT, "SSAOBlurredOneAxis");
		m_texBlurred = renderTargets.MakeAttachment(SSAO_TEXTURE_FORMAT, "SSAOBlurred");

		m_fbLinearDepth = eg::Framebuffer::CreateBasic(m_texLinearDepth);
		m_fbUnblurred = eg::Framebuffer::CreateBasic(m_texUnblurred);
		m_fbBlurredOneAxis = eg::Framebuffer::CreateBasic(m_texBlurredOneAxis);
		m_fbBlurred = eg::Framebuffer::CreateBasic(m_texBlurred);
	}

	m_depthLinearizeDS = eg::DescriptorSet(ssaoDepthLinPipeline, 0);
	m_depthLinearizeDS.BindTexture(renderTargets.gbufferDepth, 0, {}, eg::TextureUsage::DepthStencilReadOnly);

	m_mainPassAttachmentsDS = eg::DescriptorSet(ssaoPipelines[0], 1);
	m_mainPassAttachmentsDS.BindTexture(m_texLinearDepth, 0);
	m_mainPassAttachmentsDS.BindTexture(renderTargets.gbufferColor2, 1);

	m_blurPass1DS = eg::DescriptorSet(ssaoBlurPipeline, 0);
	m_blurPass1DS.BindUniformBuffer(m_blurParametersBuffer, 0, 0, sizeof(float) * 4);
	m_blurPass1DS.BindTexture(m_texUnblurred, 1);
	m_blurPass1DS.BindSampler(samplers::linearClamp, 2);

	m_blurPass2DS = eg::DescriptorSet(ssaoBlurPipeline, 0);
	m_blurPass2DS.BindUniformBuffer(m_blurParametersBuffer, 0, m_blurParametersBufferSecondOffset, sizeof(float) * 4);
	m_blurPass2DS.BindTexture(m_texBlurredOneAxis, 1);
	m_blurPass2DS.BindSampler(samplers::linearClamp, 2);

	const float pixelW = 2.0f / static_cast<float>(renderTargets.resX);
	const float pixelH = 2.0f / static_cast<float>(renderTargets.resY);
	const float blurParameters1[4] = { 2 * pixelW * *ssaoBlur, 0.0f, pixelW / 2, pixelH / 2 };
	const float blurParameters2[4] = { 0.0f, 2 * pixelH * *ssaoBlur, -pixelW / 2, -pixelH / 2 };
	m_blurParametersBuffer.DCUpdateData<float>(0, blurParameters1);
	m_blurParametersBuffer.DCUpdateData<float>(m_blurParametersBufferSecondOffset, blurParameters2);
	m_blurParametersBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

void SSAO::RunSSAOPass()
{
	// ** Initial SSAO Pass **

	const uint32_t numSSAOSamples = SSAO_SAMPLES[static_cast<int>(settings.ssaoQuality) - 1];
	if (numSSAOSamples != m_ssaoSamplesBufferCurrentSamples)
	{
		m_parametersBuffer.UsageHint(eg::BufferUsage::CopyDst);

		eg::UploadBuffer uploadBuffer =
			eg::GetTemporaryUploadBuffer(sizeof(glm::vec4) + sizeof(glm::vec4) * numSSAOSamples);
		glm::vec4* parametersData = reinterpret_cast<glm::vec4*>(uploadBuffer.Map());

		parametersData[0] = glm::vec4(*ssaoRadius, *ssaoDepthFadeMin, 1.0f / *ssaoDepthFadeRate, 1.0f / *ssaoTolerance);

		GenerateSSAOSamples({ parametersData + 1, numSSAOSamples });

		uploadBuffer.Flush();

		eg::DC.CopyBuffer(uploadBuffer.buffer, m_parametersBuffer, uploadBuffer.offset, 0, uploadBuffer.range);

		m_parametersBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
		m_ssaoSamplesBufferCurrentSamples = numSSAOSamples;
	}

	eg::MultiStageGPUTimer timer;

	timer.StartStage("SSAO - Depth Linearize");

	eg::DC.BeginRenderPass(eg::RenderPassBeginInfo{ .framebuffer = m_fbLinearDepth.handle });
	eg::DC.BindPipeline(ssaoDepthLinPipeline);
	eg::DC.BindDescriptorSet(m_depthLinearizeDS, 0);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	m_texLinearDepth.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	timer.StartStage("SSAO - Main");

	eg::DC.BeginRenderPass(eg::RenderPassBeginInfo{ .framebuffer = m_fbUnblurred.handle });
	eg::DC.BindPipeline(ssaoPipelines[static_cast<int>(settings.ssaoQuality) - 1]);
	eg::DC.BindDescriptorSet(m_mainPassDS0, 0);
	eg::DC.BindDescriptorSet(m_mainPassAttachmentsDS, 1);
	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();

	m_texUnblurred.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	// ** Blur passes **

	timer.StartStage("SSAO - Blur");

	eg::DC.BeginRenderPass(eg::RenderPassBeginInfo{ .framebuffer = m_fbBlurredOneAxis.handle });
	eg::DC.BindPipeline(ssaoBlurPipeline);
	eg::DC.BindDescriptorSet(m_blurPass1DS, 0);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	m_texBlurredOneAxis.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	eg::DC.BeginRenderPass(eg::RenderPassBeginInfo{ .framebuffer = m_fbBlurred.handle });
	eg::DC.BindPipeline(ssaoBlurPipeline);
	eg::DC.BindDescriptorSet(m_blurPass2DS, 0);
	eg::DC.Draw(0, 3, 0, 1);
	eg::DC.EndRenderPass();
	m_texBlurred.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}
