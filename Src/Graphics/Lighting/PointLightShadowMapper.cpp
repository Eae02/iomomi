#include "PointLightShadowMapper.hpp"

#include "../GraphicsCommon.hpp"
#include "PointLight.hpp"

static const glm::vec3 shadowMatrixF[] = { { 1, 0, 0 },  { -1, 0, 0 }, { 0, 1, 0 },
	                                       { 0, -1, 0 }, { 0, 0, 1 },  { 0, 0, -1 } };
static const glm::vec3 shadowMatrixU[] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 },
	                                       { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };

const eg::DescriptorSetBinding PointLightShadowMapper::SHADOW_MAP_DS_BINDING = {
	.binding = 0,
	.type =
		eg::BindingTypeTexture{
			.viewType = eg::TextureViewType::Cube,
			.sampleMode = eg::TextureSampleMode::Depth,
		},
	.shaderAccess = eg::ShaderAccessFlags::Fragment,
};

PointLightShadowMapper::PointLightShadowMapper()
{
	m_depthCopyPipeline = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo(),
		.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ShadowDepthCopy.fs.glsl").ToStageInfo(),
		.enableDepthTest = true,
		.enableDepthWrite = true,
		.numColorAttachments = 0,
		.depthAttachmentFormat = SHADOW_MAP_FORMAT,
		.label = "ShadowDepthCopy",
	});

	glm::vec3 localNormals[5] = {
		glm::vec3(1, 0, 1),
		glm::vec3(-1, 0, 1),
		glm::vec3(0, 1, 1),
		glm::vec3(0, -1, 1),
	};

	for (uint32_t face = 0; face < 6; face++)
	{
		glm::mat3 invRotation = glm::transpose(glm::lookAt(glm::vec3(0), shadowMatrixF[face], shadowMatrixU[face]));
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
	m_resolution = qvar::shadowResolution(quality);

	for (LightEntry& entry : m_lights)
	{
		entry.staticShadowMap = -1;
		entry.dynamicShadowMap = -1;
		entry.light->shadowMapDescriptorSet = nullptr;
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
			return static_cast<int>(i);
		}
	}

	ShadowMap& shadowMap = m_shadowMaps.emplace_back();
	shadowMap.inUse = true;

	shadowMap.texture = eg::Texture::CreateCube({
		.flags =
			eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ManualBarrier,
		.mipLevels = 1,
		.width = m_resolution,
		.height = m_resolution,
		.format = SHADOW_MAP_FORMAT,
	});

	shadowMap.wholeTextureView = shadowMap.texture.GetView();

	shadowMap.descriptorSet = eg::DescriptorSet({ &SHADOW_MAP_DS_BINDING, 1 });
	shadowMap.descriptorSet.BindTextureView(shadowMap.wholeTextureView, 0);

	for (uint32_t i = 0; i < 6; i++)
	{
		eg::FramebufferAttachment dsAttachment;
		dsAttachment.texture = shadowMap.texture.handle;
		dsAttachment.subresource.firstArrayLayer = i;
		dsAttachment.subresource.numArrayLayers = 1;
		shadowMap.framebuffers[i] = eg::Framebuffer({}, dsAttachment);

		if (eg::HasFlag(eg::GetGraphicsDeviceInfo().features, eg::DeviceFeatureFlags::PartialTextureViews))
		{
			shadowMap.layerViews[i] =
				shadowMap.texture.GetView(dsAttachment.subresource.AsSubresource(), eg::TextureViewType::Flat2D);

			shadowMap.layerDescriptorSets[i] = eg::DescriptorSet(m_depthCopyPipeline, 0);
			shadowMap.layerDescriptorSets[i].BindTextureView(shadowMap.layerViews[i], 0);
		}
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
		lights[i]->shadowMapDescriptorSet = nullptr;
	}

	for (ShadowMap& shadowMap : m_shadowMaps)
	{
		shadowMap.inUse = false;
	}
}

struct LightParametersData
{
	glm::mat4 lightMatrix;
	glm::vec4 lightSphere;
};

void PointLightShadowMapper::UpdateShadowMaps(
	const RenderCallback& prepareCallback, const RenderCallback& renderCallback, const eg::Frustum& viewFrustum
)
{
	auto gpuTimer = eg::StartGPUTimer("PL Shadows");
	auto cpuTimer = eg::StartCPUTimer("PL Shadows");

	m_lastFrameUpdateCount = 0;

	if (m_lights.empty())
		return;

	eg::DC.DebugLabelBegin("PL Shadows");

	PointLightShadowDrawArgs renderArgs;

	uint32_t lightParametersBufferOffset = 0;

	auto UpdateShadowMap = [&](int shadowMap, int baseShadowMap, const PointLight& light, uint32_t face)
	{
		std::string debugLabel = "Update SM " + std::to_string(shadowMap) + ":" + std::to_string(face);
		eg::DC.DebugLabelBegin(debugLabel.c_str());

		renderArgs.lightSphere = light.GetSphere();
		renderArgs.lightMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.001f, light.Range()) *
		                         glm::lookAt(light.position, light.position + shadowMatrixF[face], shadowMatrixU[face]);

		if (FlippedLightMatrix())
		{
			renderArgs.lightMatrix = glm::scale(glm::mat4(1), glm::vec3(1, -1, 1)) * renderArgs.lightMatrix;
		}

		// Reallocates the light parameters buffer if it is too small
		if (lightParametersBufferOffset + sizeof(LightParametersData) > m_lightParametersBufferSize)
		{
			constexpr uint64_t BUFFER_SIZE_INCREMENT = 32 * 1024;
			if (m_lightParametersBufferSize == 0)
				m_lightParametersBufferSize = BUFFER_SIZE_INCREMENT;
			else
				m_lightParametersBufferSize += BUFFER_SIZE_INCREMENT;

			m_lightParametersBuffer = eg::Buffer(eg::BufferCreateInfo{
				.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
				.size = m_lightParametersBufferSize,
			});

			m_lightParametersDescriptorSet = eg::DescriptorSet(PointLightShadowDrawArgs::PARAMETERS_DS_BINDINGS);
			m_lightParametersDescriptorSet.BindUniformBuffer(
				m_lightParametersBuffer, 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(LightParametersData)
			);
		}

		// Uploads data to the parameters buffer
		eg::UploadBuffer parametersUploadBuffer = eg::GetTemporaryUploadBuffer(sizeof(LightParametersData));
		*static_cast<LightParametersData*>(parametersUploadBuffer.Map()) = {
			.lightMatrix = renderArgs.lightMatrix,
			.lightSphere = glm::vec4(renderArgs.lightSphere.position, renderArgs.lightSphere.radius),
		};
		parametersUploadBuffer.Flush();
		eg::DC.CopyBuffer(
			parametersUploadBuffer.buffer, m_lightParametersBuffer, parametersUploadBuffer.offset,
			lightParametersBufferOffset, sizeof(LightParametersData)
		);
		eg::DC.Barrier(
			m_lightParametersBuffer,
			eg::BufferBarrier{
				.oldUsage = eg::BufferUsage::CopyDst,
				.newUsage = eg::BufferUsage::UniformBuffer,
				.newAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment,
				.offset = lightParametersBufferOffset,
				.range = sizeof(LightParametersData),
			}
		);

		renderArgs.parametersDescriptorSet = m_lightParametersDescriptorSet;
		renderArgs.parametersBufferOffset = lightParametersBufferOffset;
		lightParametersBufferOffset = eg::RoundToNextMultiple(
			lightParametersBufferOffset + sizeof(LightParametersData),
			eg::GetGraphicsDeviceInfo().uniformBufferOffsetAlignment
		);

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
			eg::DC.BindDescriptorSet(m_shadowMaps[baseShadowMap].layerDescriptorSets[face], 0);
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

	// Updates static shadow maps, disabled if partial texture views are not supported
	if (eg::HasFlag(eg::GetGraphicsDeviceInfo().features, eg::DeviceFeatureFlags::PartialTextureViews))
	{
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
						entry.light->shadowMapDescriptorSet = m_shadowMaps[entry.staticShadowMap].descriptorSet;
					}
				}

				for (uint32_t face = 0; face < 6; face++)
				{
					UpdateShadowMap(entry.staticShadowMap, -1, *entry.light, face);
				}
			}
		}
	}

	int remUpdateCount = qvar::shadowUpdateLimitPerFrame(m_qualityLevel);
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
			entry.light->shadowMapDescriptorSet = m_shadowMaps[entry.dynamicShadowMap].descriptorSet;
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

bool PointLightShadowMapper::FlippedLightMatrix()
{
	return eg::CurrentGraphicsAPI() != eg::GraphicsAPI::OpenGL;
}
