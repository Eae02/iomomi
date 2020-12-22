#include "PointLightShadowMapper.hpp"
#include "PointLight.hpp"
#include "../RenderSettings.hpp"

PointLightShadowMapper::PointLightShadowMapper()
	: m_matricesBuffer(eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst, BUFFER_SIZE, nullptr)
{
	
}

void PointLightShadowMapper::Invalidate(const eg::Sphere& sphere)
{
	for (ShadowMap& shadowMap : m_shadowMaps)
	{
		if (shadowMap.inUse && shadowMap.sphere.Intersects(sphere))
		{
			shadowMap.outOfDate = true;
		}
	}
}

void PointLightShadowMapper::InvalidateAll()
{
	for (ShadowMap& shadowMap : m_shadowMaps)
	{
		shadowMap.outOfDate = true;
		shadowMap.inUse = false;
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
		m_resolution = 128;
		break;
	case QualityLevel::Low:
	case QualityLevel::Medium:
		m_resolution = 256;
		break;
	case QualityLevel::High:
		m_resolution = 512;
		break;
	case QualityLevel::VeryHigh:
		m_resolution = 1024;
		break;
	}
	
	m_shadowMaps.clear();
}

const int MAX_UPDATES_PER_FRAME[] = 
{
	/* VeryLow  */ 2,
	/* Low      */ 2,
	/* Medium   */ 3,
	/* High     */ 4,
	/* VeryHigh */ 5,
};

void PointLightShadowMapper::UpdateShadowMaps(std::vector<PointLightDrawData>& pointLights,
	const RenderCallback& renderCallback)
{
	auto gpuTimer = eg::StartGPUTimer("PL Shadows");
	auto cpuTimer = eg::StartCPUTimer("PL Shadows");
	
	m_lastFrameUpdateCount = 0;
	
	if (pointLights.empty())
		return;
	
	eg::DC.DebugLabelBegin("PL Shadows");
	
	//Updates which shadow maps are available
	for (ShadowMap& shadowMap : m_shadowMaps)
	{
		shadowMap.inUse = std::any_of(pointLights.begin(), pointLights.end(),
			[&] (const PointLightDrawData& pointLight)
		{
			return pointLight.instanceID == shadowMap.currentLightID;
		});
	}
	
	std::vector<size_t> optionalUpdates;
	int remainingUpdates = MAX_UPDATES_PER_FRAME[(int)m_qualityLevel];
	
	//Assigns a shadow map to each point light
	for (PointLightDrawData& pointLight : pointLights)
	{
		//TODO: Implement this
		//if (!pointLight.castsShadows)
		//	continue;
		
		size_t shadowMapIndex = SIZE_MAX;
		for (size_t i = 0; i < m_shadowMaps.size(); i++)
		{
			if (m_shadowMaps[i].currentLightID == pointLight.instanceID || !m_shadowMaps[i].inUse)
			{
				shadowMapIndex = i;
				break;
			}
		}
		
		if (shadowMapIndex == SIZE_MAX)
		{
			shadowMapIndex = AddShadowMap();
		}
		else if (glm::distance2(m_shadowMaps[shadowMapIndex].sphere.position, pointLight.pc.position) > 0.01f || pointLight.invalidate)
		{
			m_shadowMaps[shadowMapIndex].outOfDate = true;
		}
		
		pointLight.invalidate = false;
		pointLight.shadowMap = m_shadowMaps[shadowMapIndex].texture;
		m_shadowMaps[shadowMapIndex].sphere = eg::Sphere(pointLight.pc.position, pointLight.pc.range);
		m_shadowMaps[shadowMapIndex].currentLightID = pointLight.instanceID; //Problematic
		if (!m_shadowMaps[shadowMapIndex].inUse)
		{
			UpdateShadowMap(shadowMapIndex, renderCallback);
			remainingUpdates--;
		}
		else if (m_shadowMaps[shadowMapIndex].outOfDate)
		{
			optionalUpdates.push_back(shadowMapIndex);
		}
	}
	
	std::sort(optionalUpdates.begin(), optionalUpdates.end(), [&] (size_t a, size_t b)
	{
		return m_shadowMaps[a].lastUpdateFrame < m_shadowMaps[b].lastUpdateFrame;
	});
	
	for (size_t i = 0; i < optionalUpdates.size() && remainingUpdates > 0; i++)
	{
		UpdateShadowMap(optionalUpdates[i], renderCallback);
		remainingUpdates--;
	}
	
	eg::DC.DebugLabelEnd();
}

static void InitShadowMatrices(const eg::Sphere& lightSphere, void* memory)
{
	glm::mat4* matrices = static_cast<glm::mat4*>(memory);
	glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.001f, lightSphere.radius);
	
	const glm::vec3 forward[] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0,-1, 0 }, { 0, 0, 1 }, { 0, 0,-1 } };
	const glm::vec3 up[] = { { 0, -1, 0 }, { 0,-1, 0 }, { 0, 0, 1 }, { 0, 0,-1 }, { 0,-1, 0 }, { 0,-1, 0 } };
	for (int i = 0; i < 6; i++)
	{
		matrices[i] = shadowProj * glm::lookAt(lightSphere.position, lightSphere.position + forward[i], up[i]);
	}
	
	float* spherePtr = reinterpret_cast<float*>(matrices + 6);
	spherePtr[0] = lightSphere.position.x;
	spherePtr[1] = lightSphere.position.y;
	spherePtr[2] = lightSphere.position.z;
	spherePtr[3] = lightSphere.radius;
}

void PointLightShadowMapper::UpdateShadowMap(size_t index, const RenderCallback& renderCallback)
{
	m_lastUpdateFrameIndex = eg::FrameIdx();
	m_lastFrameUpdateCount++;
	
	std::string debugLabel = "Update SM " + std::to_string(index);
	eg::DC.DebugLabelBegin(debugLabel.c_str());
	
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(BUFFER_SIZE);
	InitShadowMatrices(m_shadowMaps[index].sphere, uploadBuffer.Map());
	
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_matricesBuffer, uploadBuffer.offset, 0, uploadBuffer.range);
	m_matricesBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Geometry | eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_shadowMaps[index].framebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	PointLightShadowRenderArgs renderArgs;
	renderArgs.lightSphere = m_shadowMaps[index].sphere;
	renderArgs.matricesBuffer = m_matricesBuffer;
	
	renderCallback(renderArgs);
	
	eg::DC.EndRenderPass();
	
	m_shadowMaps[index].texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	m_shadowMaps[index].inUse = true;
	m_shadowMaps[index].outOfDate = false;
	m_shadowMaps[index].lastUpdateFrame = eg::FrameIdx();
	
	eg::DC.DebugLabelEnd();
}

size_t PointLightShadowMapper::AddShadowMap()
{
	const size_t idx = m_shadowMaps.size();
	
	ShadowMap& shadowMap = m_shadowMaps.emplace_back();
	shadowMap.outOfDate = true;
	shadowMap.inUse = false;
	shadowMap.lastUpdateFrame = 0;
	
	eg::TextureCreateInfo textureCI;
	textureCI.format = SHADOW_MAP_FORMAT;
	textureCI.width = m_resolution;
	textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment;
	textureCI.mipLevels = 1;
	shadowMap.texture = eg::Texture::CreateCube(textureCI);
	
	eg::FramebufferAttachment dsAttachment;
	dsAttachment.texture = shadowMap.texture.handle;
	dsAttachment.subresource.numArrayLayers = 6;
	shadowMap.framebuffer = eg::Framebuffer({}, dsAttachment);
	
	return idx;
}
