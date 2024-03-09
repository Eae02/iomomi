#include "GravityGunRenderer.hpp"
#include "../Settings.hpp"
#include "RenderSettings.hpp"
#include "RenderTex.hpp"
#include "Vertex.hpp"

GravityGunRenderer::GravityGunRenderer(const eg::Model& model) : m_model(&model)
{
	const eg::ShaderModuleAsset& vertexShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D-PCTransform.vs.glsl");

	eg::GraphicsPipelineCreateInfo mainPipelineCI;
	mainPipelineCI.vertexShader = vertexShader.ToStageInfo();
	mainPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityGun.fs.glsl").ToStageInfo();
	InitPipelineVertexStateSoaPXNT(mainPipelineCI);
	mainPipelineCI.enableDepthWrite = true;
	mainPipelineCI.enableDepthTest = true;
	mainPipelineCI.cullMode = eg::CullMode::Back;
	mainPipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	mainPipelineCI.label = "GravityGun";
	mainPipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	mainPipelineCI.depthAttachmentFormat = GetFormatForRenderTexture(RenderTex::GravityGunDepth, true);
	m_mainMaterialPipeline = eg::Pipeline::Create(mainPipelineCI);

	eg::GraphicsPipelineCreateInfo glowPipelineCI;
	glowPipelineCI.vertexShader = vertexShader.ToStageInfo();
	glowPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityGunMid.fs.glsl").ToStageInfo();
	InitPipelineVertexStateSoaPXNT(glowPipelineCI);
	glowPipelineCI.enableDepthWrite = false;
	glowPipelineCI.enableDepthTest = true;
	glowPipelineCI.cullMode = eg::CullMode::None;
	glowPipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	glowPipelineCI.label = "GravityGunMid";
	glowPipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	glowPipelineCI.depthAttachmentFormat = GetFormatForRenderTexture(RenderTex::GravityGunDepth, true);
	m_glowMaterialPipeline = eg::Pipeline::Create(glowPipelineCI);

	eg::BufferRef renderSettingsBuffer = RenderSettings::instance->Buffer();

	m_mainMaterialDescriptorSet = eg::DescriptorSet(m_mainMaterialPipeline, 0);
	m_mainMaterialDescriptorSet.BindUniformBuffer(renderSettingsBuffer, 0);
	m_mainMaterialDescriptorSet.BindTexture(
		eg::GetAsset<eg::Texture>("Textures/GravityGunMainA.png"), 1, &commonTextureSampler
	);
	m_mainMaterialDescriptorSet.BindTexture(
		eg::GetAsset<eg::Texture>("Textures/GravityGunMainN.png"), 2, &commonTextureSampler
	);
	m_mainMaterialDescriptorSet.BindTexture(
		eg::GetAsset<eg::Texture>("Textures/GravityGunMainM.png"), 3, &commonTextureSampler
	);

	m_glowMaterialDescriptorSet = eg::DescriptorSet(m_glowMaterialPipeline, 0);
	m_glowMaterialDescriptorSet.BindUniformBuffer(renderSettingsBuffer, 0);
	m_glowMaterialDescriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 1, &commonTextureSampler);

	m_glowMaterialIndex = model.GetMaterialIndex("EnergyCyl");
	m_mainMaterialIndex = model.GetMaterialIndex("Main");
}

void GravityGunRenderer::Draw(
	std::span<const glm::mat4> meshTransforms, float glowIntensityBoost, RenderTexManager& renderTexManager
)
{
	glm::mat4 projectionMatrix;
	float fov = glm::radians(settings.fieldOfViewDeg);
	float aspectRatio = static_cast<float>(renderTexManager.ResX()) / static_cast<float>(renderTexManager.ResY());
	const float zNear = 0.01f;
	const float zFar = 10.0f;
	if (eg::GetGraphicsDeviceInfo().depthRange == eg::DepthRange::ZeroToOne)
	{
		projectionMatrix = glm::perspectiveFovZO(fov, aspectRatio, 1.0f, zNear, zFar);
	}
	else
	{
		projectionMatrix = glm::perspectiveFovNO(fov, aspectRatio, 1.0f, zNear, zFar);
	}

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer =
		renderTexManager.GetFramebuffer(RenderTex::Lit, std::nullopt, RenderTex::GravityGunDepth, "GravityGun");
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	eg::DC.BeginRenderPass(rpBeginInfo);

	constexpr uint32_t WORLD_MATRIX_PC_OFFSET = sizeof(float) * 4 * 4;
	constexpr uint32_t GLOW_INTENSITY_BOOST_PC_OFFSET = sizeof(float) * (4 * 4 + 4 * 3);

	auto DrawByMaterial = [&](int materialIndex)
	{
		for (size_t i = 0; i < m_model->NumMeshes(); i++)
		{
			const auto& mesh = m_model->GetMesh(i);
			if (mesh.materialIndex == materialIndex)
			{
				glm::mat3x4 meshTransform = glm::transpose(meshTransforms[i]);
				eg::DC.PushConstants(WORLD_MATRIX_PC_OFFSET, meshTransform);
				eg::DC.DrawIndexed(mesh.firstIndex, mesh.numIndices, mesh.firstVertex, 0, 1);
			}
		}
	};

	eg::DC.BindPipeline(m_mainMaterialPipeline);
	eg::DC.BindDescriptorSet(m_mainMaterialDescriptorSet, 0);
	eg::DC.PushConstants(0, projectionMatrix);
	m_model->BuffersDescriptor().Bind(eg::DC, 0b111);
	DrawByMaterial(m_mainMaterialIndex);

	eg::DC.BindPipeline(m_glowMaterialPipeline);
	eg::DC.BindDescriptorSet(m_glowMaterialDescriptorSet, 0);
	eg::DC.PushConstants(0, projectionMatrix);
	eg::DC.PushConstants(GLOW_INTENSITY_BOOST_PC_OFFSET, glowIntensityBoost);
	DrawByMaterial(m_glowMaterialIndex);

	eg::DC.EndRenderPass();
}
