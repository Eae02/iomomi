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

void PointLightShadowMapper::SetResolution(uint32_t resolution)
{
	m_resolution = resolution;
	m_shadowMaps.clear();
}

void PointLightShadowMapper::UpdateShadowMaps(
	std::vector<PointLightDrawData>& pointLights, const RenderCallback& renderCallback, uint32_t maxUpdates)
{
	if (pointLights.empty())
		return;
	
	//Updates which shadow maps are available
	for (ShadowMap& shadowMap : m_shadowMaps)
	{
		shadowMap.inUse = std::any_of(pointLights.begin(), pointLights.end(),
			[&] (const PointLightDrawData& pointLight)
		{
			return pointLight.instanceID == shadowMap.currentLightID;
		});
	}
	
	m_updateCandidates.clear();
	
	//Assigns a shadow map to each point light
	for (PointLightDrawData& pointLight : pointLights)
	{
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
		
		pointLight.shadowMap = m_shadowMaps[shadowMapIndex].texture;
		m_shadowMaps[shadowMapIndex].sphere = eg::Sphere(pointLight.pc.position, pointLight.pc.range);
		m_shadowMaps[shadowMapIndex].currentLightID = pointLight.instanceID; //Problematic
		if (m_shadowMaps[shadowMapIndex].outOfDate)
		{
			m_updateCandidates.push_back(shadowMapIndex);
		}
	}
	
	//Sorts update candidates, first by whether they are in use or not and secondly by their distance to the camera.
	const glm::vec3& cameraPos = RenderSettings::instance->cameraPosition;
	std::sort(m_updateCandidates.begin(), m_updateCandidates.end(), [&] (size_t a, size_t b)
	{
		if (m_shadowMaps[a].inUse == m_shadowMaps[b].inUse)
		{
			return glm::distance2(m_shadowMaps[a].sphere.position, cameraPos) <
			       glm::distance2(m_shadowMaps[b].sphere.position, cameraPos);
		}
		return m_shadowMaps[a].inUse < m_shadowMaps[b].inUse;
	});
	
	//Updates shadow maps
	uint32_t numToUpdate = maxUpdates;
	for (size_t shadowMapIdx : m_updateCandidates)
	{
		if (numToUpdate > 0)
			numToUpdate--;
		else if (m_shadowMaps[shadowMapIdx].inUse)
			break;
		
		UpdateShadowMap(shadowMapIdx, renderCallback);
	}
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
}

size_t PointLightShadowMapper::AddShadowMap()
{
	const size_t idx = m_shadowMaps.size();
	
	ShadowMap& shadowMap = m_shadowMaps.emplace_back();
	shadowMap.outOfDate = true;
	shadowMap.inUse = false;
	
	eg::TextureCubeCreateInfo textureCI;
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
