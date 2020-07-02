#include "DeferredRenderer.hpp"
#include "Lighting/LightMeshes.hpp"
#include "RenderSettings.hpp"
#include "GraphicsCommon.hpp"
#include "../Settings.hpp"

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
}

void DeferredRenderer::CreatePipelines()
{
	eg::GraphicsPipelineCreateInfo ambientPipelineCI;
	ambientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ambientPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Lighting/Ambient.fs.glsl").DefaultVariant();
	ambientPipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	m_ambientPipeline = eg::Pipeline::Create(ambientPipelineCI);
	m_ambientPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_ambientPipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
	
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
}

void DeferredRenderer::BeginGeometry() const
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = GetFramebuffer(RenderTex::GBColor1, RenderTex::GBColor2, RenderTex::GBDepth, "Geometry");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB(0.3f, 0.1f, 0.1f, 1);
	rpBeginInfo.colorAttachments[1].clearValue = eg::ColorLin(0, 0, 1, 1);
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::BeginGeometryFlags() const
{
	eg::DC.EndRenderPass();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = GetFramebuffer(RenderTex::Flags, {}, RenderTex::GBDepth, "GeometryFlags");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB(0, 0, 0, 0);
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::EndGeometry() const
{
	eg::DC.EndRenderPass();
	
	RenderTextureUsageHintFS(RenderTex::GBColor1);
	RenderTextureUsageHintFS(RenderTex::GBColor2);
	RenderTextureUsageHintFS(RenderTex::GBDepth);
	RenderTextureUsageHintFS(RenderTex::Flags);
}

void DeferredRenderer::BeginTransparent(RenderTex destinationTexture)
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = GetFramebuffer(destinationTexture, {}, RenderTex::GBDepth, "Transparent");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void DeferredRenderer::EndTransparent()
{
	eg::DC.EndRenderPass();
}

void DeferredRenderer::BeginLighting()
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = GetFramebuffer(RenderTex::LitWithoutWater, {}, {}, "Lighting");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::ColorLin ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc));
	if (!unlit)
	{
		ambientColor = ambientColor.ScaleRGB(0.2f);
	}
	
	eg::DC.BindPipeline(m_ambientPipeline);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor1), 0, 1);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBDepth), 0, 3);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::Flags), 0, 4);
	
	eg::DC.PushConstants(0, sizeof(float) * 3, &ambientColor.r);
	
	eg::DC.Draw(0, 3, 0, 1);
}

void DeferredRenderer::End() const
{
	eg::DC.EndRenderPass();
}

void DeferredRenderer::DrawSpotLights(const std::vector<SpotLightDrawData>& spotLights) const
{
	if (spotLights.empty() || unlit)
		return;
	
	auto gpuTimer = eg::StartGPUTimer("Spot Lights");
	auto cpuTimer = eg::StartCPUTimer("Spot Lights");
	
	eg::DC.BindPipeline(m_spotLightPipeline);
	
	BindSpotLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor1), 0, 1);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBDepth), 0, 3);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::Flags), 0, 4);
	
	for (const SpotLightDrawData& spotLight : spotLights)
	{
		eg::DC.PushConstants(0, spotLight);
		
		eg::DC.DrawIndexed(0, SPOT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}

void DeferredRenderer::DrawPointLights(const std::vector<PointLightDrawData>& pointLights) const
{
	if (pointLights.empty() || unlit)
		return;
	
	auto gpuTimer = eg::StartGPUTimer("Point Lights");
	auto cpuTimer = eg::StartCPUTimer("Point Lights");
	
	bool softShadows = settings.shadowQuality >= QualityLevel::Medium;
	
	eg::DC.BindPipeline(softShadows ? m_pointLightPipelineSoftShadows : m_pointLightPipelineHardShadows);
	
	BindPointLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor1), 0, 1);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor2), 0, 2);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBDepth), 0, 3);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::Flags), 0, 4);
	
	for (const PointLightDrawData& pointLight : pointLights)
	{
		eg::DC.BindTexture(pointLight.shadowMap, 0, 5, &m_shadowMapSampler);
		
		eg::DC.PushConstants(0, pointLight.pc);
		
		eg::DC.DrawIndexed(0, POINT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
