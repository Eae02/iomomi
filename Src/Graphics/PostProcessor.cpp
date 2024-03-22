#include "PostProcessor.hpp"

#include "../Settings.hpp"
#include "../Utils.hpp"
#include "GraphicsCommon.hpp"

static float* bloomIntensity = eg::TweakVarFloat("bloom_intensity", 0.4f, 0);

static float* vignetteMinRad = eg::TweakVarFloat("vign_min_rad", 0.2f, 0);
static float* vignetteMaxRad = eg::TweakVarFloat("vign_max_rad", 0.75f, 0);
static float* vignettePower = eg::TweakVarFloat("vign_power", 1.2f, 0);

static const eg::DescriptorSetBinding BINDINGS[] = {
	eg::DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 2,
		.type = eg::BindingTypeSampler::Default,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 3,
		.type = eg::BindingTypeUniformBuffer{ .dynamicOffset = true },
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
}; //

eg::Pipeline PostProcessor::CreatePipeline(const PipelineVariantKey& variantKey)
{
	const eg::SpecializationConstantEntry specConstants[] = { { 0, static_cast<uint32_t>(variantKey.enableFXAA) } };

	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	postPipelineCI.fragmentShader = {
		.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.fs.glsl").DefaultVariant(),
		.specConstants = specConstants,
	};
	postPipelineCI.descriptorSetBindings[0] = BINDINGS;
	postPipelineCI.label = "PostProcess";
	postPipelineCI.colorAttachmentFormats[0] = variantKey.outputFormat;
	if (variantKey.outputFormat == eg::Format::DefaultColor)
		postPipelineCI.depthAttachmentFormat = eg::Format::DefaultDepthStencil;
	return eg::Pipeline::Create(postPipelineCI);
}

void PostProcessor::SetRenderTargets(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget)
{
	m_descriptorSet = eg::DescriptorSet(BINDINGS);
	m_descriptorSet.BindTexture(input, 0);
	m_descriptorSet.BindTexture(bloomRenderTarget ? bloomRenderTarget->OutputTexture() : eg::Texture::BlackPixel(), 1);
	m_descriptorSet.BindSampler(samplers::linearClamp, 2);
	m_descriptorSet.BindUniformBuffer(frameDataUniformBuffer, 3, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 6);
}

void PostProcessor::Render(eg::FramebufferHandle output, eg::Format outputFormat, float colorScale)
{
	EG_ASSERT(m_descriptorSet.handle != nullptr);

	PipelineVariantKey pipelineVariantKey = {
		.enableFXAA = settings.enableFXAA,
		.outputFormat = outputFormat,
	};

	eg::Pipeline* pipeline;
	auto pipelineIt = std::find_if(
		m_pipelines.begin(), m_pipelines.end(), [&](const auto& p) { return p.first == pipelineVariantKey; }
	);
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

	const float parameters[] = {
		settings.exposure, *bloomIntensity, colorScale, *vignetteMinRad, 1.0f / (*vignetteMaxRad - *vignetteMinRad),
		*vignettePower
	};
	const uint32_t parametersOffset = PushFrameUniformData(ToCharSpan<float>(parameters));

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = output;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);

	eg::DC.BindPipeline(*pipeline);

	eg::DC.BindDescriptorSet(m_descriptorSet, 0, { &parametersOffset, 1 });

	eg::DC.Draw(0, 3, 0, 1);

	eg::DC.EndRenderPass();
}
