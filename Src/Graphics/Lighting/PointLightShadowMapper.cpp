#include "PointLightShadowMapper.hpp"
#include "PointLight.hpp"
#include "../RenderSettings.hpp"
#include "../GraphicsCommon.hpp"

void PointLightShadowDrawArgs::SetPushConstants() const
{
	float pcData[20];
	std::memcpy(pcData, &lightMatrix, sizeof(float) * 16);
	pcData[16] = lightSphere.position.x;
	pcData[17] = lightSphere.position.y;
	pcData[18] = lightSphere.position.z;
	pcData[19] = lightSphere.radius;
	eg::DC.PushConstants(0, sizeof(pcData), pcData);
}

static const glm::vec3 shadowMatrixF[] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0,-1, 0 }, { 0, 0, 1 }, { 0, 0,-1 } };
static const glm::vec3 shadowMatrixU[] = { { 0, -1, 0 }, { 0,-1, 0 }, { 0, 0, 1 }, { 0, 0,-1 }, { 0,-1, 0 }, { 0,-1, 0 } };

PointLightShadowMapper::PointLightShadowMapper()
{
	eg::GraphicsPipelineCreateInfo depthCopyPipelineCI;
	depthCopyPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	depthCopyPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ShadowDepthCopy.fs.glsl").DefaultVariant();
	depthCopyPipelineCI.enableDepthTest = true;
	depthCopyPipelineCI.enableDepthWrite = true;
	m_depthCopyPipeline = eg::Pipeline::Create(depthCopyPipelineCI);
	m_depthCopyPipeline.FramebufferFormatHint(eg::Format::Undefined, SHADOW_MAP_FORMAT);
	
	glm::vec3 localNormals[5] = 
	{
		glm::vec3(1, 0, 1),
		glm::vec3(-1, 0, 1),
		glm::vec3(0, 1, 1),
		glm::vec3(0, -1, 1),
	};
	
	for (uint32_t face = 0; face < 6; face++)
	{
		const glm::mat3 invRotation = glm::transpose(glm::mat3(glm::lookAt(glm::vec3(0), shadowMatrixF[face], shadowMatrixU[face])));
		for (size_t plane = 0; plane < 4; plane++)
		{
			m_frustumPlanes[face][plane] = glm::normalize(invRotation * localNormals[plane]);
		}
	}
}

void PointLightShadowMapper::Invalidate(const eg::Sphere& sphere, const PointLight* onlyThisLight)
{
	for (LightEntry& entry : m_lights)
	{
		if (onlyThisLight && entry.light.get() != onlyThisLight)
			continue;
		if (!entry.light->enabled || !entry.light->castsShadows || !sphere.Intersects(entry.light->GetSphere()))
			continue;
		
		glm::vec3 relPos = sphere.position - entry.light->position;
		
		for (uint32_t face = 0; face < 6; face++)
		{
			bool intersects = true;
			for (const glm::vec3& plane : m_frustumPlanes[face])
			{
				if (glm::dot(plane, relPos) > sphere.radius)
				{
					intersects = false;
					break;
				}
			}
			if (intersects)
			{
				entry.faceInvalidated[face] = true;
			}
		}
	}
}

void PointLightShadowMapper::SetQuality(QualityLevel quality)
{
	if (quality == m_qualityLevel)
		return;
	m_qualityLevel = quality;
	
	switch (quality)
	{
	case QualityLevel::VeryLow:
		m_resolution = 256;
		break;
	case QualityLevel::Low:
		m_resolution = 512;
		break;
	case QualityLevel::Medium:
		m_resolution = 512;
		break;
	case QualityLevel::High:
		m_resolution = 1024;
		break;
	case QualityLevel::VeryHigh:
		m_resolution = 1536;
		break;
	}
	
	for (LightEntry& entry : m_lights)
	{
		entry.staticShadowMap = -1;
		entry.dynamicShadowMap = -1;
		entry.light->shadowMap = eg::TextureRef();
	}
	m_shadowMaps.clear();
}

int PointLightShadowMapper::AllocateShadowMap()
{
	for (size_t i = 0; i < m_shadowMaps.size(); i++)
	{
		if (!m_shadowMaps[i].inUse)
		{
			m_shadowMaps[i].inUse = true;
			return (int)i;
		}
	}
	
	ShadowMap& shadowMap = m_shadowMaps.emplace_back();
	shadowMap.inUse = true;
	
	eg::TextureCreateInfo textureCI;
	textureCI.format = SHADOW_MAP_FORMAT;
	textureCI.width = m_resolution;
	textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ManualBarrier;
	textureCI.mipLevels = 1;
	shadowMap.texture = eg::Texture::CreateCube(textureCI);
	
	for (uint32_t i = 0; i < 6; i++)
	{
		eg::FramebufferAttachment dsAttachment;
		dsAttachment.texture = shadowMap.texture.handle;
		dsAttachment.subresource.firstArrayLayer = i;
		dsAttachment.subresource.numArrayLayers = 1;
		shadowMap.framebuffers[i] = eg::Framebuffer({ }, dsAttachment);
	}
	
	return (int)m_shadowMaps.size() - 1;
}

void PointLightShadowMapper::SetLightSources(const std::vector<std::shared_ptr<PointLight>>& lights)
{
	m_lights.resize(lights.size());
	for (size_t i = 0; i < m_lights.size(); i++)
	{
		m_lights[i].light = lights[i];
		m_lights[i].staticShadowMap = -1;
		m_lights[i].dynamicShadowMap = -1;
		lights[i]->shadowMap = eg::TextureRef();
	}
	
	for (ShadowMap& shadowMap : m_shadowMaps)
	{
		shadowMap.inUse = false;
	}
}

const int MAX_UPDATES_PER_FRAME[] = 
{
	/* VeryLow  */ 5,
	/* Low      */ 5,
	/* Medium   */ 10,
	/* High     */ 15,
	/* VeryHigh */ 20,
};

void PointLightShadowMapper::UpdateShadowMaps(const RenderCallback& prepareCallback,
	const RenderCallback& renderCallback, const eg::Frustum& viewFrustum)
{
	auto gpuTimer = eg::StartGPUTimer("PL Shadows");
	auto cpuTimer = eg::StartCPUTimer("PL Shadows");
	
	m_lastFrameUpdateCount = 0;
	
	if (m_lights.empty())
		return;
	
	eg::DC.DebugLabelBegin("PL Shadows");
	
	PointLightShadowDrawArgs renderArgs;
	
	auto UpdateShadowMap = [&] (int shadowMap, int baseShadowMap, const PointLight& light, uint32_t face)
	{
		std::string debugLabel = "Update SM " + std::to_string(shadowMap) + ":" + std::to_string(face);
		eg::DC.DebugLabelBegin(debugLabel.c_str());
		
		renderArgs.lightSphere = light.GetSphere();
		renderArgs.lightMatrix =
			glm::perspective(glm::radians(90.0f), 1.0f, 0.001f, light.Range()) *
			glm::lookAt(light.position, light.position + shadowMatrixF[face], shadowMatrixU[face]);
		
		if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan)
		{
			renderArgs.lightMatrix = glm::scale(glm::mat4(1), glm::vec3(1, -1, 1)) * renderArgs.lightMatrix;
		}
		
		renderArgs.light = &light;
		renderArgs.frustum = eg::Frustum(glm::inverse(renderArgs.lightMatrix));
		
		prepareCallback(renderArgs);
		
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = m_shadowMaps[shadowMap].framebuffers[face].handle;
		rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
		rpBeginInfo.depthClearValue = 1.0f;
		eg::DC.BeginRenderPass(rpBeginInfo);
		
		if (baseShadowMap != -1)
		{
			eg::DC.BindPipeline(m_depthCopyPipeline);
			
			eg::TextureSubresource subresource;
			subresource.firstArrayLayer = face;
			subresource.numArrayLayers = 1;
			eg::DC.BindTexture(m_shadowMaps[baseShadowMap].texture, 0, 0, &framebufferNearestSampler,
			                   subresource, eg::TextureBindFlags::ArrayLayerAsTexture2D);
			
			eg::DC.Draw(0, 3, 0, 1);
		}
		
		renderCallback(renderArgs);
		
		eg::DC.EndRenderPass();
		
		eg::TextureBarrier barrier;
		barrier.subresource.firstArrayLayer = face;
		barrier.subresource.numArrayLayers = 1;
		barrier.oldUsage = eg::TextureUsage::FramebufferAttachment;
		barrier.newUsage = eg::TextureUsage::ShaderSample;
		barrier.oldAccess = eg::ShaderAccessFlags::None;
		barrier.newAccess = eg::ShaderAccessFlags::Fragment;
		eg::DC.Barrier(m_shadowMaps[shadowMap].texture, barrier);
		
		eg::DC.DebugLabelEnd();
		
		m_lastFrameUpdateCount++;
	};
	
	renderArgs.renderDynamic = false;
	renderArgs.renderStatic = true;
	for (LightEntry& entry : m_lights)
	{
		if (entry.light->enabled && entry.light->castsShadows && !entry.light->willMoveEveryFrame &&
			(entry.staticShadowMap == -1 || entry.HasMoved()) && viewFrustum.Intersects(entry.light->GetSphere()))
		{
			if (entry.staticShadowMap == -1)
			{
				entry.staticShadowMap = AllocateShadowMap();
				if (entry.dynamicShadowMap == -1)
				{
					entry.light->shadowMap = m_shadowMaps[entry.staticShadowMap].texture;
				}
			}
			
			for (uint32_t face = 0; face < 6; face++)
			{
				UpdateShadowMap(entry.staticShadowMap, -1, *entry.light, face);
			}
		}
	}
	
	int remUpdateCount = MAX_UPDATES_PER_FRAME[(int)m_qualityLevel];
	std::optional<size_t> nextDynamicLightUpdatePos;
	for (size_t i = m_dynamicLightUpdatePos; i < m_dynamicLightUpdatePos + m_lights.size(); i++)
	{
		LightEntry& entry = m_lights[i % m_lights.size()];
		if (!entry.light->enabled || !entry.light->castsShadows || !viewFrustum.Intersects(entry.light->GetSphere()))
			continue;
		
		if (remUpdateCount <= 0 && !entry.light->highPriority)
		{
			if (!nextDynamicLightUpdatePos.has_value())
				nextDynamicLightUpdatePos = i % m_lights.size();
			continue;
		}
		
		bool updateAll = entry.HasMoved();
		if (entry.dynamicShadowMap == -1)
		{
			entry.dynamicShadowMap = AllocateShadowMap();
			entry.light->shadowMap = m_shadowMaps[entry.dynamicShadowMap].texture;
			updateAll = true;
		}
		
		for (uint32_t face = 0; face < 6; face++)
		{
			if (updateAll || entry.faceInvalidated[face])
			{
				renderArgs.renderDynamic = true;
				renderArgs.renderStatic = entry.staticShadowMap == -1;
				UpdateShadowMap(entry.dynamicShadowMap, entry.staticShadowMap, *entry.light, face);
				entry.faceInvalidated[face] = false;
				remUpdateCount--;
			}
		}
		
		entry.previousPosition = entry.light->position;
		entry.previousRange = entry.light->Range();
	}
	
	if (nextDynamicLightUpdatePos.has_value())
		m_dynamicLightUpdatePos = *nextDynamicLightUpdatePos;
	
	eg::DC.DebugLabelEnd();
	
	if (m_lastFrameUpdateCount)
	{
		m_lastUpdateFrameIndex = eg::FrameIdx();
	}
}

bool PointLightShadowMapper::LightEntry::HasMoved() const
{
	constexpr float MIN_MOVE_DIST = 0.01f;
	constexpr float MIN_RANGE_DIFF = 0.01f;
	return glm::distance2(light->position, previousPosition) > MIN_MOVE_DIST * MIN_MOVE_DIST ||
	       glm::abs(light->Range() - previousRange) > MIN_RANGE_DIFF;
}
