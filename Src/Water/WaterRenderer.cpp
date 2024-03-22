#include "WaterRenderer.hpp"

#include "../Graphics/GraphicsCommon.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../Graphics/RenderTargets.hpp"
#include "../Settings.hpp"
#include "../Utils.hpp"

static eg::Texture dummyWaterDepthTexture;
static eg::DescriptorSet dummyWaterDepthTextureDescriptorSet;

static void CreateDummyDepthTexture()
{
	dummyWaterDepthTexture = eg::Texture::Create2D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample,
		.mipLevels = 1,
		.width = 1,
		.height = 1,
		.format = eg::Format::R32G32B32A32_Float,
	});

	const float dummyDepthTextureColor[4] = { 1000, 1000, 1000, 1 };
	dummyWaterDepthTexture.SetData(
		ToCharSpan<float>(dummyDepthTextureColor), eg::TextureRange{ .sizeX = 1, .sizeY = 1, .sizeZ = 1 }
	);

	dummyWaterDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	dummyWaterDepthTextureDescriptorSet = eg::DescriptorSet({ &FRAGMENT_SHADER_TEXTURE_BINDING, 1 });
	dummyWaterDepthTextureDescriptorSet.BindTexture(dummyWaterDepthTexture, 0);
}

static void DestroyDummyDepthTexture()
{
	dummyWaterDepthTexture.Destroy();
	dummyWaterDepthTextureDescriptorSet.Destroy();
}

EG_ON_INIT(CreateDummyDepthTexture)
EG_ON_SHUTDOWN(DestroyDummyDepthTexture)

eg::TextureRef WaterRenderer::GetDummyDepthTexture()
{
	return dummyWaterDepthTexture;
}

eg::DescriptorSetRef WaterRenderer::GetDummyDepthTextureDescriptorSet()
{
	return dummyWaterDepthTextureDescriptorSet;
}

WaterRenderPipelines WaterRenderPipelines::instance;

void WaterRenderPipelines::Init()
{
	auto& sphereDepthFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.fs.glsl");
	auto& sphereVS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterSphere.vs.glsl");

	pipelineDepthMin = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = sphereVS.ToStageInfo("VDepthMin"),
		.fragmentShader = sphereDepthFS.ToStageInfo("VDepthMin"),
		.enableDepthTest = true,
		.enableDepthWrite = true,
		.enableDepthClamp = true,
		.depthCompare = eg::CompareOp::Less,
		.topology = eg::Topology::TriangleList,
		.colorAttachmentFormats = { WaterRenderer::GLOW_INTENSITY_TEXTURE_FORMAT },
		.depthAttachmentFormat = WaterRenderer::DEPTH_TEXTURE_FORMAT,
		.label = "WaterDepthMin",
	});

	pipelineDepthMax = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = sphereVS.ToStageInfo("VDepthMax"),
		.fragmentShader = sphereDepthFS.ToStageInfo("VDepthMax"),
		.enableDepthTest = true,
		.enableDepthWrite = true,
		.enableDepthClamp = true,
		.depthCompare = eg::CompareOp::Greater,
		.topology = eg::Topology::TriangleList,
		.numColorAttachments = 0,
		.depthAttachmentFormat = WaterRenderer::DEPTH_TEXTURE_FORMAT,
		.label = "WaterDepthMax",
	});

	eg::ShaderModuleHandle postVS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	auto& postFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterPost.fs.glsl");

	pipelinePostStdQual = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = eg::ShaderStageInfo(postVS),
		.fragmentShader = postFS.ToStageInfo("VStdQual"),
		.colorAttachmentFormats = { lightColorAttachmentFormat },
		.depthAttachmentFormat = GB_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly,
		.label = "WaterPost[LQ]",
	});

	pipelinePostHighQual = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = eg::ShaderStageInfo(postVS),
		.fragmentShader = postFS.ToStageInfo("VHighQual"),
		.colorAttachmentFormats = { lightColorAttachmentFormat },
		.depthAttachmentFormat = GB_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly,
		.label = "WaterPost[HQ]",
	});

	waterPostDescriptorSet0 = eg::DescriptorSet(pipelinePostStdQual, 0);
	waterPostDescriptorSet0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	waterPostDescriptorSet0.BindUniformBuffer(
		frameDataUniformBuffer, 1, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 8
	);
	waterPostDescriptorSet0.BindTexture(eg::GetAsset<eg::Texture>("Textures/WaterN.png"), 2);
	waterPostDescriptorSet0.BindSampler(samplers::linearRepeatAnisotropic, 3);
	waterPostDescriptorSet0.BindSampler(samplers::nearestClamp, 4);
	waterPostDescriptorSet0.BindSampler(samplers::linearClamp, 5);
}

static void OnShutdown()
{
	WaterRenderPipelines::instance = {};
}
EG_ON_SHUTDOWN(OnShutdown)

struct WaterBlurParams
{
	float blurDirX;
	float blurDirY;
	float blurDistanceFalloff;
	float blurDepthFalloff;
};

void WaterRenderPipelines::CreateDepthBlurPipelines(uint32_t samples)
{
	eg::SpecializationConstantEntry specConstants[] = { { 0, samples } };

	const auto& depthBlurTwoPassFS = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/DepthBlur.fs.glsl");
	eg::GraphicsPipelineCreateInfo pipelineBlurCI;
	pipelineBlurCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	pipelineBlurCI.fragmentShader = {
		.shaderModule = depthBlurTwoPassFS.GetVariant("V1"),
		.specConstants = specConstants,
	};
	pipelineBlurCI.colorAttachmentFormats[0] = WaterRenderer::BLURRED_DEPTH_TEXTURE_FORMAT;
	pipelineBlurCI.label = "WaterBlur1";
	pipelineBlurPass1 = eg::Pipeline::Create(pipelineBlurCI);

	pipelineBlurCI.fragmentShader.shaderModule = depthBlurTwoPassFS.GetVariant("V2");
	pipelineBlurCI.label = "WaterBlur2";
	pipelineBlurCI.colorAttachmentFormats[0] = WaterRenderer::BLURRED_DEPTH_TEXTURE_FORMAT;
	pipelineBlurPass2 = eg::Pipeline::Create(pipelineBlurCI);

	depthBlurSamples = samples;
}

WaterRenderer::WaterRenderer() {}

static void CheckBlurPipelines()
{
	uint32_t blurSamples = qvar::waterBlurSamples(settings.waterQuality);
	if (blurSamples != WaterRenderPipelines::instance.depthBlurSamples)
		WaterRenderPipelines::instance.CreateDepthBlurPipelines(blurSamples);
}

void WaterRenderer::RenderTargetsCreated(const RenderTargets& renderTargets)
{
	CheckBlurPipelines();

	texGlowIntensity = renderTargets.MakeAttachment(WaterRenderer::GLOW_INTENSITY_TEXTURE_FORMAT, "WaterGlowIntensity");

	texMinDepth = renderTargets.MakeAttachment(WaterRenderer::DEPTH_TEXTURE_FORMAT, "WaterMinDepth");
	texMaxDepth = renderTargets.MakeAttachment(WaterRenderer::DEPTH_TEXTURE_FORMAT, "WaterMaxDepth");
	texDepthBlurred1 = renderTargets.MakeAttachment(WaterRenderer::BLURRED_DEPTH_TEXTURE_FORMAT, "WaterDepthBlurred1");

	depthMinFramebuffer = eg::Framebuffer::CreateBasic(texGlowIntensity, texMinDepth);
	depthMaxFramebuffer = eg::Framebuffer(eg::FramebufferCreateInfo{ .depthStencilAttachment = texMaxDepth.handle });
	blur1Framebuffer = eg::Framebuffer::CreateBasic(texDepthBlurred1);
	blur2Framebuffer = eg::Framebuffer::CreateBasic(renderTargets.waterDepthTexture.value());

	m_gbufferDepthDescriptorSet = eg::DescriptorSet(WaterRenderPipelines::instance.pipelineDepthMax, 1);
	m_gbufferDepthDescriptorSet.BindTexture(renderTargets.gbufferDepth, 0, {}, eg::TextureUsage::DepthStencilReadOnly);

	m_blur1DescriptorSet = eg::DescriptorSet(WaterRenderPipelines::instance.pipelineBlurPass1, 0);
	m_blur1DescriptorSet.BindUniformBuffer(
		frameDataUniformBuffer, 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(WaterBlurParams)
	);
	m_blur1DescriptorSet.BindSampler(samplers::nearestClamp, 1);
	m_blur1DescriptorSet.BindTexture(texMinDepth, 2);
	m_blur1DescriptorSet.BindTexture(texMaxDepth, 3);

	m_blur2DescriptorSet = eg::DescriptorSet(WaterRenderPipelines::instance.pipelineBlurPass2, 0);
	m_blur2DescriptorSet.BindUniformBuffer(
		frameDataUniformBuffer, 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(WaterBlurParams)
	);
	m_blur2DescriptorSet.BindSampler(samplers::nearestClamp, 1);
	m_blur2DescriptorSet.BindTexture(texDepthBlurred1, 2);

	m_waterPostDescriptorSet1 = eg::DescriptorSet(WaterRenderPipelines::instance.pipelinePostStdQual, 1);
	m_waterPostDescriptorSet1.BindTexture(renderTargets.waterDepthTexture.value(), 0);
	m_waterPostDescriptorSet1.BindTexture(texMaxDepth, 1);
	m_waterPostDescriptorSet1.BindTexture(texGlowIntensity, 2);
	m_waterPostDescriptorSet1.BindTexture(renderTargets.waterPostInput, 3);
	m_waterPostDescriptorSet1.BindTexture(renderTargets.gbufferDepth, 4, {}, eg::TextureUsage::DepthStencilReadOnly);
}

void WaterRenderer::SetPositionsBuffer(eg::BufferRef positionsBuffer, uint32_t numParticles)
{
	m_waterPositionsDescriptorSet = eg::DescriptorSet(WaterRenderPipelines::instance.pipelineDepthMin, 0);
	m_waterPositionsDescriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_waterPositionsDescriptorSet.BindStorageBuffer(positionsBuffer, 1);

	m_numParticles = numParticles;
}

static float* blurRadius = eg::TweakVarFloat("wblur_radius", 1.0f, 0.0f);
static float* blurDistanceFalloff = eg::TweakVarFloat("wblur_distfall", 0.3f, 0.0f);
static float* blurDepthFalloff = eg::TweakVarFloat("wblur_depthfall", 0.2f, 0.0f);

/*
Water quality settings:
vlow:  4 samples,  SQ-Shader
low:   8 samples, SQ-Shader
med:   8 samples, HQ-Shader
high:  12 samples, HQ-Shader
vhigh: 16 samples, HQ-Shader
*/

void WaterRenderer::RenderEarly(const RenderTargets& renderTargets)
{
	uint32_t blurSamples = qvar::waterBlurSamples(settings.waterQuality);
	float relBlurRad = (*blurRadius * qvar::waterBaseBlurRadius(settings.waterQuality)) / blurSamples;
	float relDistanceFalloff = *blurDistanceFalloff / blurSamples;

	CheckBlurPipelines();

	// ** Depth min pass **
	// This pass renders all water particles to a depth buffer
	// using less compare mode to get the minimum depth bounds.

	eg::MultiStageGPUTimer timer;
	timer.StartStage("Depth");
	eg::DC.DebugLabelBegin("Depth");

	eg::RenderPassBeginInfo depthMinRPBeginInfo;
	depthMinRPBeginInfo.framebuffer = depthMinFramebuffer.handle;
	depthMinRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthMinRPBeginInfo.depthClearValue = 1;
	depthMinRPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	depthMinRPBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(0, 0, 0, 0);
	eg::DC.BeginRenderPass(depthMinRPBeginInfo);

	eg::DC.BindPipeline(WaterRenderPipelines::instance.pipelineDepthMin);
	eg::DC.BindDescriptorSet(m_waterPositionsDescriptorSet, 0);
	eg::DC.Draw(0, 6 * m_numParticles, 0, 1);

	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();

	// ** Depth max pass **
	// This pass renders all water particles that are closer to the camera than the the travel depth
	// to a depth buffer. Greater compare mode is used to get the maximum depth bounds.
	// This is used to render the water surface from below.
	timer.StartStage("Depth Max");
	eg::DC.DebugLabelBegin("Depth Max");

	eg::RenderPassBeginInfo depthMaxRPBeginInfo;
	depthMaxRPBeginInfo.framebuffer = depthMaxFramebuffer.handle;
	depthMaxRPBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	depthMaxRPBeginInfo.depthClearValue = 0;
	eg::DC.BeginRenderPass(depthMaxRPBeginInfo);

	eg::DC.BindPipeline(WaterRenderPipelines::instance.pipelineDepthMax);
	eg::DC.BindDescriptorSet(m_waterPositionsDescriptorSet, 0);
	eg::DC.BindDescriptorSet(m_gbufferDepthDescriptorSet, 1);
	eg::DC.Draw(0, 6 * m_numParticles, 0, 1);

	eg::DC.EndRenderPass();
	eg::DC.DebugLabelEnd();

	texMinDepth.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	texMaxDepth.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	timer.StartStage("Blur");
	eg::DC.DebugLabelBegin("Blur");

	WaterBlurParams blurParams1 = {
		.blurDirX = 0,
		.blurDirY = relBlurRad / static_cast<float>(renderTargets.resY),
		.blurDistanceFalloff = relDistanceFalloff,
		.blurDepthFalloff = *blurDepthFalloff,
	};
	const uint32_t blurParams1Offset = PushFrameUniformData(RefToCharSpan(blurParams1));

	WaterBlurParams blurParams2 = {
		.blurDirX = relBlurRad / static_cast<float>(renderTargets.resX),
		.blurDirY = 0,
		.blurDistanceFalloff = relDistanceFalloff,
		.blurDepthFalloff = *blurDepthFalloff,
	};
	const uint32_t blurParams2Offset = PushFrameUniformData(RefToCharSpan(blurParams2));

	// ** Depth blur pass 1 **
	eg::RenderPassBeginInfo depthBlur1RPBeginInfo;
	depthBlur1RPBeginInfo.framebuffer = blur1Framebuffer.handle;
	depthBlur1RPBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(depthBlur1RPBeginInfo);

	eg::DC.BindPipeline(WaterRenderPipelines::instance.pipelineBlurPass1);
	eg::DC.BindDescriptorSet(m_blur1DescriptorSet, 0, { &blurParams1Offset, 1 });
	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();

	texDepthBlurred1.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	// ** Depth blur pass 2 **
	eg::RenderPassBeginInfo depthBlurBeginInfo;
	depthBlurBeginInfo.framebuffer = blur2Framebuffer.handle;
	depthBlurBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(depthBlurBeginInfo);

	eg::DC.BindPipeline(WaterRenderPipelines::instance.pipelineBlurPass2);
	eg::DC.BindDescriptorSet(m_blur2DescriptorSet, 0, { &blurParams2Offset, 1 });
	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();

	eg::DC.DebugLabelEnd();

	texGlowIntensity.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

static const glm::vec3 waterGlowColor(0.12f, 0.9f, 0.7f);

static float* waterVisibility = eg::TweakVarFloat("water_visibility", 10.0f, 0.0f);
static float* waterNormalMapIntensity = eg::TweakVarFloat("water_nm_intensity", 0.6f, 0.0f);
static float* waterSSRIntensity = eg::TweakVarFloat("water_ssr_intensity", 2.0f, 0.0f);

void WaterRenderer::RenderPost(const RenderTargets& renderTargets)
{
	// ** Post pass **

	eg::GPUTimer timer = eg::StartGPUTimer("Water Post");

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = renderTargets.waterPostStage->framebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.depthStencilReadOnly = true;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB::FromHex(0x0c2b46); // TODO: why?
	eg::DC.BeginRenderPass(rpBeginInfo);

	bool useHQShader = qvar::waterUseHQShader(settings.waterQuality);

	const auto& pipelines = WaterRenderPipelines::instance;
	eg::DC.BindPipeline(useHQShader ? pipelines.pipelinePostHighQual : pipelines.pipelinePostStdQual);

	float waterGlowIntensity = settings.gunFlash ? 4 : 0.5f;

	float params[] = {
		2.0f / renderTargets.resX,
		2.0f / renderTargets.resY,
		*waterVisibility,
		*waterNormalMapIntensity,
		waterGlowColor.r * waterGlowIntensity,
		waterGlowColor.g * waterGlowIntensity,
		waterGlowColor.b * waterGlowIntensity,
		*waterSSRIntensity,
	};
	const uint32_t paramsOffset = PushFrameUniformData(ToCharSpan<float>(params));

	eg::DC.BindDescriptorSet(pipelines.waterPostDescriptorSet0, 0, { &paramsOffset, 1 });
	eg::DC.BindDescriptorSet(m_waterPostDescriptorSet1, 1);

	eg::DC.Draw(0, 3, 0, 1);

	// eg::DC.EndRenderPass();
}
