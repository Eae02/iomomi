#include "GravityGunRenderer.hpp"
#include "../Settings.hpp"
#include "RenderSettings.hpp"
#include "RenderTargets.hpp"
#include "Vertex.hpp"

struct ParametersUB
{
	glm::mat4 viewProj;
	float glowIntensityBoost;
};

GravityGunRenderer::GravityGunRenderer(const eg::Model& model) : m_model(&model)
{
	const eg::ShaderModuleAsset& vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityGun.vs.glsl");

	eg::GraphicsPipelineCreateInfo mainPipelineCI;
	mainPipelineCI.vertexShader = vertexShader.ToStageInfo();
	mainPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityGun.fs.glsl").ToStageInfo();
	InitPipelineVertexStateSoaPXNT(mainPipelineCI);
	mainPipelineCI.enableDepthWrite = true;
	mainPipelineCI.enableDepthTest = true;
	mainPipelineCI.cullMode = eg::CullMode::Back;
	mainPipelineCI.label = "GravityGun";
	mainPipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	mainPipelineCI.depthAttachmentFormat = eg::Format::Depth16;
	m_mainMaterialPipeline = eg::Pipeline::Create(mainPipelineCI);

	eg::GraphicsPipelineCreateInfo glowPipelineCI;
	glowPipelineCI.vertexShader = vertexShader.ToStageInfo();
	glowPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityGunMid.fs.glsl").ToStageInfo();
	InitPipelineVertexStateSoaPXNT(glowPipelineCI);
	glowPipelineCI.enableDepthWrite = false;
	glowPipelineCI.enableDepthTest = true;
	glowPipelineCI.cullMode = eg::CullMode::None;
	glowPipelineCI.label = "GravityGunMid";
	glowPipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	glowPipelineCI.depthAttachmentFormat = eg::Format::Depth16;
	m_glowMaterialPipeline = eg::Pipeline::Create(glowPipelineCI);

	eg::BufferRef renderSettingsBuffer = RenderSettings::instance->Buffer();

	m_parametersBuffer =
		eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, sizeof(ParametersUB), nullptr);

	m_mainMaterialDescriptorSet = eg::DescriptorSet(m_mainMaterialPipeline, 0);
	m_mainMaterialDescriptorSet.BindUniformBuffer(renderSettingsBuffer, 0);
	m_mainMaterialDescriptorSet.BindUniformBuffer(m_parametersBuffer, 1);
	m_mainMaterialDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravityGunMainA.png"), 2);
	m_mainMaterialDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravityGunMainN.png"), 3);
	m_mainMaterialDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GravityGunMainM.png"), 4);
	m_mainMaterialDescriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 5);

	m_glowMaterialDescriptorSet = eg::DescriptorSet(m_glowMaterialPipeline, 0);
	m_glowMaterialDescriptorSet.BindUniformBuffer(renderSettingsBuffer, 0);
	m_glowMaterialDescriptorSet.BindUniformBuffer(m_parametersBuffer, 1);
	m_glowMaterialDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 2);
	m_glowMaterialDescriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 3);

	m_glowMaterialIndex = model.GetMaterialIndex("EnergyCyl");
	m_mainMaterialIndex = model.GetMaterialIndex("Main");

	m_meshTransformsStride =
		eg::RoundToNextMultiple(sizeof(glm::mat4), eg::GetGraphicsDeviceInfo().uniformBufferOffsetAlignment);
}

void GravityGunRenderer::Draw(
	std::span<const glm::mat4> meshTransforms, float glowIntensityBoost, const RenderTargets& renderTargets
)
{
	uint32_t neededMeshTransformBytes = m_meshTransformsStride * meshTransforms.size();
	if (m_meshTransformsBufferSize < neededMeshTransformBytes)
	{
		m_meshTransformsBufferSize = neededMeshTransformBytes;
		m_meshTransformsBuffer =
			eg::Buffer(eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst, neededMeshTransformBytes, nullptr);

		m_meshTransformsDescriptorSet = eg::DescriptorSet(m_mainMaterialPipeline, 1);
		m_meshTransformsDescriptorSet.BindUniformBuffer(
			m_meshTransformsBuffer, 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(glm::mat4)
		);
	}

	eg::UploadBuffer meshTransformsUploadBuffer = eg::GetTemporaryUploadBuffer(neededMeshTransformBytes);
	void* meshTransformsMemory = meshTransformsUploadBuffer.Map();
	for (size_t i = 0; i < meshTransforms.size(); i++)
	{
		std::memcpy(
			static_cast<char*>(meshTransformsMemory) + m_meshTransformsStride * i, &meshTransforms[i], sizeof(glm::mat4)
		);
	}
	meshTransformsUploadBuffer.Flush();

	eg::DC.CopyBuffer(
		meshTransformsUploadBuffer.buffer, m_meshTransformsBuffer, meshTransformsUploadBuffer.offset, 0,
		neededMeshTransformBytes
	);
	m_meshTransformsBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex);

	ParametersUB parametersUBData;
	parametersUBData.glowIntensityBoost = glowIntensityBoost;
	float fov = glm::radians(settings.fieldOfViewDeg);
	float aspectRatio = static_cast<float>(eg::CurrentResolutionX()) / static_cast<float>(eg::CurrentResolutionY());
	const float zNear = 0.01f;
	const float zFar = 10.0f;
	if (eg::GetGraphicsDeviceInfo().depthRange == eg::DepthRange::ZeroToOne)
	{
		parametersUBData.viewProj = glm::perspectiveFovZO(fov, aspectRatio, 1.0f, zNear, zFar);
	}
	else
	{
		parametersUBData.viewProj = glm::perspectiveFovNO(fov, aspectRatio, 1.0f, zNear, zFar);
	}

	m_parametersBuffer.DCUpdateData(0, sizeof(ParametersUB), &parametersUBData);
	m_parametersBuffer.UsageHint(
		eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment
	);

	if (m_renderTargetsUID != renderTargets.uid)
	{
		m_renderTargetsUID = renderTargets.uid;
		if (m_depthAttachment.Width() != renderTargets.resX || m_depthAttachment.Height() != renderTargets.resY)
		{
			m_depthAttachment = eg::Texture::Create2D(eg::TextureCreateInfo{
				.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::TransientAttachment,
				.mipLevels = 1,
				.width = renderTargets.resX,
				.height = renderTargets.resY,
				.format = eg::Format::Depth16,
			});
		}

		m_framebuffer = eg::Framebuffer::CreateBasic(renderTargets.finalLitTexture, m_depthAttachment);
	}

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_framebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthStoreOp = eg::AttachmentStoreOp::Discard;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	eg::DC.BeginRenderPass(rpBeginInfo);

	auto DrawByMaterial = [&](int materialIndex)
	{
		for (size_t i = 0; i < m_model->NumMeshes(); i++)
		{
			const auto& mesh = m_model->GetMesh(i);
			if (mesh.materialIndex == materialIndex)
			{
				uint32_t meshTransformOffset = m_meshTransformsStride * i;
				eg::DC.BindDescriptorSet(m_meshTransformsDescriptorSet, 1, { &meshTransformOffset, 1 });
				eg::DC.DrawIndexed(mesh.firstIndex, mesh.numIndices, mesh.firstVertex, 0, 1);
			}
		}
	};

	eg::DC.BindPipeline(m_mainMaterialPipeline);
	eg::DC.BindDescriptorSet(m_mainMaterialDescriptorSet, 0);
	m_model->BuffersDescriptor().Bind(eg::DC, 0b111);
	DrawByMaterial(m_mainMaterialIndex);

	eg::DC.BindPipeline(m_glowMaterialPipeline);
	eg::DC.BindDescriptorSet(m_glowMaterialDescriptorSet, 0);
	DrawByMaterial(m_glowMaterialIndex);

	eg::DC.EndRenderPass();
}
