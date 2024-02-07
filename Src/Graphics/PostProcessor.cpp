#include "PostProcessor.hpp"

#include "../Settings.hpp"
#include "GraphicsCommon.hpp"
#include "RenderTex.hpp"

static float* bloomIntensity = eg::TweakVarFloat("bloom_intensity", 0.4f, 0);

static float* vignetteMinRad = eg::TweakVarFloat("vign_min_rad", 0.2f, 0);
static float* vignetteMaxRad = eg::TweakVarFloat("vign_max_rad", 0.75f, 0);
static float* vignettePower = eg::TweakVarFloat("vign_power", 1.2f, 0);

eg::Pipeline PostProcessor::CreatePipeline(const PipelineVariantKey& variantKey)
{
	uint32_t enableFXAA = variantKey.enableFXAA;

	eg::SpecializationConstantEntry specConstEntries[1];
	specConstEntries[0].constantID = 0;
	specConstEntries[0].size = sizeof(uint32_t);
	specConstEntries[0].offset = 0;

	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	postPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.fs.glsl").DefaultVariant();
	postPipelineCI.label = "PostProcess";
	postPipelineCI.fragmentShader.specConstants = specConstEntries;
	postPipelineCI.fragmentShader.specConstantsData = &enableFXAA;
	postPipelineCI.fragmentShader.specConstantsDataSize = sizeof(enableFXAA);
	postPipelineCI.colorAttachmentFormats[0] = variantKey.outputFormat;
	if (variantKey.outputFormat == eg::Format::DefaultColor)
		postPipelineCI.depthAttachmentFormat = eg::Format::DefaultDepthStencil;
	return eg::Pipeline::Create(postPipelineCI);
}

void PostProcessor::Render(
	eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget, eg::FramebufferHandle output,
	eg::Format outputFormat, uint32_t outputResX, uint32_t outputResY, float colorScale)
{
	PipelineVariantKey pipelineVariantKey = {
		.enableFXAA = settings.enableFXAA,
		.outputFormat = outputFormat,
	};

	eg::Pipeline* pipeline;
	auto pipelineIt = std::find_if(
		m_pipelines.begin(), m_pipelines.end(), [&](const auto& p) { return p.first == pipelineVariantKey; });
	if (pipelineIt != m_pipelines.end())
	{
		pipeline = &pipelineIt->second;
	}
	else
	{
		eg::Pipeline newPipeline = CreatePipeline(pipelineVariantKey);
		m_pipelines.emplace_back(pipelineVariantKey, std::move(newPipeline));
		pipeline = &m_pipelines.back().second;
	}

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = output;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;

	eg::DC.BeginRenderPass(rpBeginInfo);

	const float pc[] = {
		settings.exposure, *bloomIntensity, colorScale, *vignetteMinRad, 1.0f / (*vignetteMaxRad - *vignetteMinRad),
		*vignettePower
	};

	eg::DC.BindPipeline(*pipeline);
	eg::DC.PushConstants(0, sizeof(pc), pc);

	eg::DC.BindTexture(input, 0, 0, &framebufferLinearSampler);

	eg::TextureRef bloomTexture = bloomRenderTarget ? bloomRenderTarget->OutputTexture() : blackPixelTexture;
	eg::DC.BindTexture(bloomTexture, 0, 1, &framebufferLinearSampler);

	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();
}
