#include "DeferredRenderer.hpp"

#include "../Settings.hpp"
#include "../Utils.hpp"
#include "../Water/WaterSimulationShaders.hpp"
#include "Lighting/LightMeshes.hpp"
#include "Lighting/PointLightShadowMapper.hpp"
#include "RenderSettings.hpp"
#include "RenderTargets.hpp"

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

struct PointLightParameters
{
	glm::vec3 position;
	float range;
	glm::vec3 radiance;
	float _radiancePad;
	float causticsScale;
	float causticsColorOffset;
	float causticsPanSpeed;
	float causticsTexScale;
	float shadowSampleDist;
	float specularIntensity;
};

eg::Pipeline DeferredRenderer::m_ambientPipelineWithSSAO;
eg::Pipeline DeferredRenderer::m_ambientPipelineWithoutSSAO;
eg::Pipeline DeferredRenderer::m_pointLightPipelines[2][2];

void DeferredRenderer::RenderTargetsCreated(const RenderTargets& renderTargets)
{
	const eg::FramebufferAttachment geometryColorAttachments[] = {
		renderTargets.gbufferColor1.handle,
		renderTargets.gbufferColor2.handle,
	};
	m_geometryPassFramebuffer = eg::Framebuffer(geometryColorAttachments, renderTargets.gbufferDepth.handle);

	m_ambientDS0 = eg::DescriptorSet(m_ambientPipelineWithoutSSAO, 0);
	m_ambientDS0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_ambientDS0.BindTexture(renderTargets.gbufferColor1, 1);
	m_ambientDS0.BindTexture(renderTargets.gbufferColor2, 2);
	m_ambientDS0.BindTexture(renderTargets.gbufferDepth, 3, {}, eg::TextureUsage::DepthStencilReadOnly);
	m_ambientDS0.BindUniformBuffer(frameDataUniformBuffer, 4, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 4);

	m_pointLightDS0 = eg::DescriptorSet(m_pointLightPipelines[0][0], 0);
	m_pointLightDS0.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_pointLightDS0.BindTexture(renderTargets.gbufferColor1, 1);
	m_pointLightDS0.BindTexture(renderTargets.gbufferColor2, 2);
	m_pointLightDS0.BindTexture(renderTargets.gbufferDepth, 3, {}, eg::TextureUsage::DepthStencilReadOnly);
	m_pointLightDS0.BindTexture(renderTargets.GetWaterDepthTextureOrDummy(), 4);
	m_pointLightDS0.BindTexture(eg::GetAsset<eg::Texture>("Textures/Caustics"), 5);
	m_pointLightDS0.BindSampler(samplers::linearRepeatAnisotropic, 6);
	m_pointLightDS0.BindSampler(samplers::shadowMap, 7);

	m_pointLightParametersDS = eg::DescriptorSet(m_pointLightPipelines[0][0], 1);
	m_pointLightParametersDS.BindUniformBuffer(
		frameDataUniformBuffer, 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(PointLightParameters)
	);
}

void DeferredRenderer::CreatePipelines()
{
	const eg::ShaderModuleAsset& ambientFragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/Ambient.fs.glsl");

	eg::GraphicsPipelineCreateInfo ambientPipelineCI = {
		.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo(),
		.colorAttachmentFormats = { lightColorAttachmentFormat },
		.blendStates = { eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One) },
		.depthAttachmentFormat = GB_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly,
	};

	ambientPipelineCI.fragmentShader = ambientFragmentShader.ToStageInfo("VSSAO");
	ambientPipelineCI.label = "Ambient[SSAO]";
	m_ambientPipelineWithSSAO = eg::Pipeline::Create(ambientPipelineCI);

	ambientPipelineCI.fragmentShader = ambientFragmentShader.ToStageInfo("VNoSSAO");
	ambientPipelineCI.label = "Ambient[NoSSAO]";
	m_ambientPipelineWithoutSSAO = eg::Pipeline::Create(ambientPipelineCI);

	eg::SpecializationConstantEntry pointLightSpecConstants[2];
	pointLightSpecConstants[0].constantID = 0;
	pointLightSpecConstants[1].constantID = 1;

	eg::GraphicsPipelineCreateInfo plPipelineCI = {
		.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.vs.glsl").ToStageInfo(),
		.fragmentShader = {
			.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/PointLight.fs.glsl").DefaultVariant(),
			.specConstants = pointLightSpecConstants,
		},
		.cullMode = eg::CullMode::Back,
		.colorAttachmentFormats = { lightColorAttachmentFormat },
		.blendStates = { eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One) },
		.depthAttachmentFormat = GB_DEPTH_FORMAT,
		.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly,
		.vertexBindings = { { sizeof(float) * 3, eg::InputRate::Vertex } },
		.vertexAttributes = { { 0, eg::DataType::Float32, 3, 0 } },
	};
	plPipelineCI.descriptorSetBindings[2] = { &PointLightShadowMapper::SHADOW_MAP_DS_BINDING, 1 };

	for (uint32_t shadowMode = 0; shadowMode < 2; shadowMode++)
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
}

void DeferredRenderer::DestroyPipelines()
{
	m_ambientPipelineWithSSAO.Destroy();
	m_ambientPipelineWithoutSSAO.Destroy();
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			m_pointLightPipelines[i][j].Destroy();
}

void DeferredRenderer::SetSSAOTexture(std::optional<eg::TextureRef> ssaoTexture)
{
	if (ssaoTexture.has_value())
	{
		m_ssaoTextureDescriptorSet = eg::DescriptorSet(m_ambientPipelineWithSSAO, 1);
		m_ssaoTextureDescriptorSet->BindTexture(*ssaoTexture, 0);
	}
	else
	{
		m_ssaoTextureDescriptorSet = std::nullopt;
	}
}

void DeferredRenderer::BeginGeometry() const
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_geometryPassFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB(0.3f, 0.1f, 0.1f, 1);
	rpBeginInfo.colorAttachments[1].clearValue = eg::ColorLin(0, 0, 1, 1);
	eg::DC.BeginRenderPass(rpBeginInfo);
}

static int* ambientOnlyOcc = eg::TweakVarInt("amb_only_occ", 0, 0, 1);

static float* ambientIntensity = eg::TweakVarFloat("ambient_intensity", 0.1f);

void DeferredRenderer::BeginLighting(const RenderTargets& renderTargets)
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = renderTargets.lightingStage.framebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.depthStencilReadOnly = true;

	eg::DC.BeginRenderPass(rpBeginInfo);

	eg::ColorLin ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc));
	if (*unlit == 0)
	{
		ambientColor = ambientColor.ScaleRGB(*ambientIntensity);
	}

	if (m_ssaoTextureDescriptorSet.has_value())
	{
		eg::DC.BindPipeline(m_ambientPipelineWithSSAO);
		eg::DC.BindDescriptorSet(m_ssaoTextureDescriptorSet.value(), 1);
	}
	else
	{
		eg::DC.BindPipeline(m_ambientPipelineWithoutSSAO);
	}

	const float ambientParams[4] = {
		ambientColor.r,
		ambientColor.g,
		ambientColor.b,
		static_cast<float>(*ambientOnlyOcc),
	};
	const uint32_t ambientParamsOffset = PushFrameUniformData(ToCharSpan<float>(ambientParams));

	eg::DC.BindDescriptorSet(m_ambientDS0, 0, { &ambientParamsOffset, 1 });

	eg::DC.Draw(0, 3, 0, 1);
}

static float* causticsIntensity = eg::TweakVarFloat("cau_intensity", 10);
static float* causticsColorOffset = eg::TweakVarFloat("cau_clr_offset", 1.5f);
static float* causticsPanSpeed = eg::TweakVarFloat("cau_pan_speed", 0.05f);
static float* causticsTexScale = eg::TweakVarFloat("cau_tex_scale", 0.5f);

void DeferredRenderer::DrawPointLights(
	const std::vector<std::shared_ptr<PointLight>>& pointLights, const RenderTargets& renderTargets,
	uint32_t shadowResolution
) const
{
	if (pointLights.empty() || *unlit == 1)
		return;

	auto gpuTimer = eg::StartGPUTimer("Point Lights");
	auto cpuTimer = eg::StartCPUTimer("Point Lights");

	float shadowSoftness = qvar::shadowSoftness(settings.shadowQuality);

	const bool softShadows = shadowSoftness > 0;
	const bool renderCaustics =
		renderTargets.waterDepthTexture.has_value() && qvar::waterRenderCaustics(settings.waterQuality);

	eg::DC.BindPipeline(m_pointLightPipelines[softShadows][renderCaustics]);

	BindPointLightMesh();

	eg::DC.BindDescriptorSet(m_pointLightDS0, 0);

	PointLightParameters params = {
		.causticsScale = *causticsIntensity,
		.causticsColorOffset = *causticsColorOffset,
		.causticsPanSpeed = *causticsPanSpeed,
		.causticsTexScale = *causticsTexScale,
		.shadowSampleDist = shadowSoftness / static_cast<float>(shadowResolution),
	};

	for (const std::shared_ptr<PointLight>& light : pointLights)
	{
		if (!light->enabled || light->shadowMapDescriptorSet.handle == nullptr)
			continue;

		params.position = light->position;
		params.radiance = light->Radiance();
		params.range = light->Range();
		params.specularIntensity = light->enableSpecularHighlights ? 1 : 0;

		const uint32_t uniformDataOffset = PushFrameUniformData(RefToCharSpan(params));
		eg::DC.BindDescriptorSet(m_pointLightParametersDS, 1, { &uniformDataOffset, 1 });

		eg::DC.BindDescriptorSet(light->shadowMapDescriptorSet, 2);

		eg::DC.DrawIndexed(0, POINT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
