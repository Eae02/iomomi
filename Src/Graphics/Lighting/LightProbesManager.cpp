#include "LightProbesManager.hpp"
#include "../DeferredRenderer.hpp"

#pragma pack(push, 1)
struct LightProbeUB
{
	glm::vec3 position;
	float _p1;
	glm::vec3 parallaxRad;
	float _p2;
	glm::vec3 inflInner;
	float _p3;
	glm::vec3 inflFalloff;
	int layer;
};
#pragma pack(pop)

eg::BRDFIntegrationMap* brdfIntegrationMap;

LightProbesManager::LightProbesManager()
{
	eg::GraphicsPipelineCreateInfo ambientPipelineCI;
	ambientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ambientPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Ambient.fs.glsl").GetVariant("VMSAA");
	ambientPipelineCI.setBindModes[1] = eg::BindMode::DescriptorSet;
	m_ambientPipeline = eg::Pipeline::Create(ambientPipelineCI);
	m_ambientPipeline.FramebufferFormatHint(DeferredRenderer::LIGHT_COLOR_FORMAT_LDR);
	m_ambientPipeline.FramebufferFormatHint(DeferredRenderer::LIGHT_COLOR_FORMAT_HDR);
	
	const uint32_t probesUBSize = sizeof(LightProbeUB) * MAX_VISIBLE;
	m_probesUniformBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, probesUBSize, nullptr);
	
	m_descriptorSet = eg::DescriptorSet(m_ambientPipeline, 1);
	
	m_descriptorSet.BindUniformBuffer(m_probesUniformBuffer, 0, 0, probesUBSize);
	m_descriptorSet.BindTexture(brdfIntegrationMap->GetTexture(), 1);
	
	eg::SamplerDescription envMapSamplerDesc;
	
	eg::TextureCreateInfo environmentMapCI;
	environmentMapCI.width = RENDER_RESOLUTION;
	environmentMapCI.format = DeferredRenderer::LIGHT_COLOR_FORMAT_HDR;
	environmentMapCI.mipLevels = 1;
	environmentMapCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ManualBarrier;
	environmentMapCI.defaultSamplerDescription = &envMapSamplerDesc;
	m_environmentMap = eg::Texture::CreateCube(environmentMapCI);
	
	m_envMapRenderTargets.reserve(6);
	for (uint32_t i = 0; i < 6; i++)
	{
		m_envMapRenderTargets.emplace_back(RENDER_RESOLUTION, RENDER_RESOLUTION, 1, m_environmentMap, i);
	}
}

void LightProbesManager::PrepareWorld(const std::vector<LightProbe>& lightProbes,
	const std::function<void(const RenderArgs&)>& renderCallback)
{
	if (lightProbes.size() > m_maxProbes || m_maxProbes == 0)
	{
		eg::gal::DeviceWaitIdle();
		
		m_maxProbes = eg::RoundToNextMultiple(std::max<uint32_t>(lightProbes.size(), 1), 32);
		
		eg::SamplerDescription mapSamplerDesc;
		
		eg::TextureCreateInfo irradianceMapCI;
		irradianceMapCI.width = IRRADIANCE_MAP_RESOLUTION;
		irradianceMapCI.arrayLayers = m_maxProbes;
		irradianceMapCI.format = eg::IrradianceMapGenerator::MAP_FORMAT;
		irradianceMapCI.mipLevels = 1;
		irradianceMapCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::StorageImage | eg::TextureFlags::ManualBarrier;
		irradianceMapCI.defaultSamplerDescription = &mapSamplerDesc;
		m_irradianceMaps = eg::Texture::CreateCubeArray(irradianceMapCI);
		
		eg::TextureCreateInfo spfMapCI;
		spfMapCI.width = SPF_MAP_RESOLUTION;
		spfMapCI.arrayLayers = m_maxProbes;
		spfMapCI.format = eg::IrradianceMapGenerator::MAP_FORMAT;
		spfMapCI.mipLevels = SPF_MAP_MIP_LEVELS;
		spfMapCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::StorageImage | eg::TextureFlags::ManualBarrier;
		spfMapCI.defaultSamplerDescription = &mapSamplerDesc;
		m_spfMaps = eg::Texture::CreateCubeArray(spfMapCI);
	}
	
	m_probes = lightProbes;
	
	eg::TextureSubresource sampleSubresource;
	sampleSubresource.numArrayLayers = std::max<uint32_t>(m_probes.size(), 1) * 6;
	m_descriptorSet.BindTexture(m_irradianceMaps, 2, nullptr, sampleSubresource);
	m_descriptorSet.BindTexture(m_spfMaps, 3, nullptr, sampleSubresource);
	
	if (m_probes.empty())
	{
		eg::TextureBarrier irradianceMapBarrierDummy;
		irradianceMapBarrierDummy.oldUsage = eg::TextureUsage::Undefined;
		irradianceMapBarrierDummy.newUsage = eg::TextureUsage::ShaderSample;
		irradianceMapBarrierDummy.newAccess = eg::ShaderAccessFlags::Fragment;
		irradianceMapBarrierDummy.subresource = sampleSubresource;
		eg::DC.Barrier(m_irradianceMaps, irradianceMapBarrierDummy);
		
		eg::TextureBarrier spfMapBarrierDummy;
		spfMapBarrierDummy.oldUsage = eg::TextureUsage::Undefined;
		spfMapBarrierDummy.newUsage = eg::TextureUsage::ShaderSample;
		spfMapBarrierDummy.newAccess = eg::ShaderAccessFlags::Fragment;
		spfMapBarrierDummy.subresource = sampleSubresource;
		eg::DC.Barrier(m_spfMaps, spfMapBarrierDummy);
		
		return;
	}
	
	glm::mat4 viewMatrices[6];
	
	const glm::vec3 forward[] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0,-1, 0 }, { 0, 0, 1 }, { 0, 0,-1 } };
	const glm::vec3 up[] = { { 0, -1, 0 }, { 0,-1, 0 }, { 0, 0, 1 }, { 0, 0,-1 }, { 0,-1, 0 }, { 0,-1, 0 } };
	for (int i = 0; i < 6; i++)
	{
		viewMatrices[i] = glm::lookAt(glm::vec3(0), forward[i], up[i]);
	}
	
	for (uint32_t l = 0; l < m_probes.size(); l++)
	{
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1), -m_probes[l].position);
		glm::mat4 invTranslationMatrix = glm::translate(glm::mat4(1), m_probes[l].position);
		for (int i = 0; i < 6; i++)
		{
			glm::mat4 projMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
			glm::mat4 invProjMatrix = glm::inverse(projMatrix);
			
			RenderArgs renderArgs;
			renderArgs.cameraPosition = m_probes[l].position;
			renderArgs.renderTarget = &m_envMapRenderTargets[i];
			renderArgs.viewProj = projMatrix * viewMatrices[i] * translationMatrix;
			renderArgs.invViewProj = invTranslationMatrix * glm::transpose(viewMatrices[i]) * invProjMatrix;
			renderCallback(renderArgs);
		}
		
		eg::TextureBarrier envMapBarrierBeforeGen;
		envMapBarrierBeforeGen.oldUsage = eg::TextureUsage::FramebufferAttachment;
		envMapBarrierBeforeGen.newUsage = eg::TextureUsage::ShaderSample;
		envMapBarrierBeforeGen.newAccess = eg::ShaderAccessFlags::Compute;
		eg::DC.Barrier(m_environmentMap, envMapBarrierBeforeGen);
		
		eg::TextureBarrier irradianceMapBarrierBeforeGen;
		irradianceMapBarrierBeforeGen.oldUsage = eg::TextureUsage::Undefined;
		irradianceMapBarrierBeforeGen.newUsage = eg::TextureUsage::ILSWrite;
		irradianceMapBarrierBeforeGen.newAccess = eg::ShaderAccessFlags::Compute;
		irradianceMapBarrierBeforeGen.subresource.firstArrayLayer = l * 6;
		irradianceMapBarrierBeforeGen.subresource.numArrayLayers = 6;
		eg::DC.Barrier(m_irradianceMaps, irradianceMapBarrierBeforeGen);
		
		eg::TextureBarrier spfMapBarrierBeforeGen;
		spfMapBarrierBeforeGen.oldUsage = eg::TextureUsage::Undefined;
		spfMapBarrierBeforeGen.newUsage = eg::TextureUsage::ILSWrite;
		spfMapBarrierBeforeGen.newAccess = eg::ShaderAccessFlags::Compute;
		spfMapBarrierBeforeGen.subresource.firstArrayLayer = l * 6;
		spfMapBarrierBeforeGen.subresource.numArrayLayers = 6;
		eg::DC.Barrier(m_spfMaps, spfMapBarrierBeforeGen);
		
		m_irradianceMapGenerator.Generate(eg::DC, m_environmentMap, m_irradianceMaps, l);
		
		m_spfMapGenerator.Generate(eg::DC, m_environmentMap, m_spfMaps, l);
		
		eg::TextureBarrier irradianceMapBarrierAfterGen;
		irradianceMapBarrierAfterGen.oldUsage = eg::TextureUsage::ILSWrite;
		irradianceMapBarrierAfterGen.newUsage = eg::TextureUsage::ShaderSample;
		irradianceMapBarrierAfterGen.oldAccess = eg::ShaderAccessFlags::Compute;
		irradianceMapBarrierAfterGen.newAccess = eg::ShaderAccessFlags::Fragment;
		irradianceMapBarrierAfterGen.subresource.firstArrayLayer = l * 6;
		irradianceMapBarrierAfterGen.subresource.numArrayLayers = 6;
		eg::DC.Barrier(m_irradianceMaps, irradianceMapBarrierAfterGen);
		
		eg::TextureBarrier spfMapBarrierAfterGen;
		spfMapBarrierAfterGen.oldUsage = eg::TextureUsage::ILSWrite;
		spfMapBarrierAfterGen.newUsage = eg::TextureUsage::ShaderSample;
		spfMapBarrierAfterGen.oldAccess = eg::ShaderAccessFlags::Compute;
		spfMapBarrierAfterGen.newAccess = eg::ShaderAccessFlags::Fragment;
		spfMapBarrierAfterGen.subresource.firstArrayLayer = l * 6;
		spfMapBarrierAfterGen.subresource.numArrayLayers = 6;
		eg::DC.Barrier(m_spfMaps, spfMapBarrierAfterGen);
		
		eg::TextureBarrier envMapBarrierAfterGen;
		envMapBarrierAfterGen.oldUsage = eg::TextureUsage::ShaderSample;
		envMapBarrierAfterGen.newUsage = eg::TextureUsage::FramebufferAttachment;
		envMapBarrierAfterGen.oldAccess = eg::ShaderAccessFlags::Compute;
		eg::DC.Barrier(m_environmentMap, envMapBarrierAfterGen);
	}
}

void LightProbesManager::PrepareForDraw(const glm::vec3& cameraPosition)
{
	int selectedProbes[MAX_VISIBLE];
	float probeDistances[MAX_VISIBLE];
	std::fill_n(selectedProbes, MAX_VISIBLE, -1);
	std::fill_n(probeDistances, MAX_VISIBLE, INFINITY);
	
	for (int i = 0; i < (int)m_probes.size(); i++)
	{
		float distance2 = glm::distance2(cameraPosition, m_probes[i].position);
		if (distance2 > probeDistances[MAX_VISIBLE - 1])
			continue;
		
		for (int j = MAX_VISIBLE - 1; j >= 0; j--)
		{
			if (j != 0 && distance2 < probeDistances[j - 1])
			{
				probeDistances[j] = probeDistances[j - 1];
				selectedProbes[j] = selectedProbes[j - 1];
			}
			else
			{
				probeDistances[j] = distance2;
				selectedProbes[j] = i;
			}
		}
	}
	
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(sizeof(LightProbeUB) * MAX_VISIBLE);
	LightProbeUB* probes = reinterpret_cast<LightProbeUB*>(uploadBuffer.Map());
	for (uint32_t i = 0; i < MAX_VISIBLE; i++)
	{
		probes[i].layer = selectedProbes[i];
		if (selectedProbes[i] == -1)
			continue;
		
		const LightProbe& probe = m_probes[selectedProbes[i]];
		probes[i].position = probe.position;
		probes[i].parallaxRad = probe.parallaxRad;
		probes[i].inflInner = probe.influenceRad;
		probes[i].inflFalloff = glm::vec3(1.0f) / probe.influenceFade;
	}
	uploadBuffer.Flush();
	
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_probesUniformBuffer, uploadBuffer.offset, 0, uploadBuffer.range);
	m_probesUniformBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

void LightProbesManager::Bind() const
{
	eg::DC.BindPipeline(m_ambientPipeline);
	eg::DC.BindDescriptorSet(m_descriptorSet, 1);
}
