#include "SelectionRenderer.hpp"
#include "../Graphics/GraphicsCommon.hpp"
#include "../Graphics/Vertex.hpp"
#include "../Utils.hpp"
#include "EditorGraphics.hpp"

static eg::Pipeline modelPipeline;
static eg::Pipeline postPipeline;

static eg::DescriptorSet modelPipelineDS;

struct SelectionDrawParams
{
	glm::mat4 transform;
	float intensity;
};

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo modelPipelineCI;
	modelPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EdSelection.vs.glsl").ToStageInfo();
	modelPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EdSelection.fs.glsl").ToStageInfo();
	modelPipelineCI.cullMode = eg::CullMode::None;
	modelPipelineCI.vertexBindings[VERTEX_BINDING_POSITION] = { VERTEX_STRIDE_POSITION, eg::InputRate::Vertex };
	modelPipelineCI.vertexAttributes[0] = { VERTEX_BINDING_POSITION, eg::DataType::Float32, 3, 0 };
	modelPipelineCI.numColorAttachments = 1;
	modelPipelineCI.colorAttachmentFormats[0] = eg::Format::R8_UNorm;
	modelPipelineCI.label = "EdSelection";
	modelPipeline = eg::Pipeline::Create(modelPipelineCI);

	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").ToStageInfo();
	postPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EdSelectionPost.fs.glsl").ToStageInfo();
	postPipelineCI.cullMode = eg::CullMode::None;
	postPipelineCI.numColorAttachments = 1;
	postPipelineCI.label = "EdSelectionPost";
	postPipelineCI.blendStates[0] = eg::AlphaBlend;
	postPipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	postPipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	postPipeline = eg::Pipeline::Create(postPipelineCI);

	modelPipelineDS = eg::DescriptorSet(modelPipeline, 0);
	modelPipelineDS.BindUniformBuffer(
		frameDataUniformBuffer, 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(SelectionDrawParams)
	);
}

static void OnShutdown()
{
	modelPipeline.Destroy();
	postPipeline.Destroy();
	modelPipelineDS.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

void SelectionRenderer::BeginFrame(uint32_t resX, uint32_t resY)
{
	m_hasRendered = false;

	if (m_texture.handle == nullptr || resX != m_texture.Width() || resY != m_texture.Height())
	{
		eg::TextureCreateInfo textureCI;
		textureCI.width = resX;
		textureCI.height = resY;
		textureCI.mipLevels = 1;

		textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		textureCI.format = eg::Format::R8_UNorm;
		m_texture = eg::Texture::Create2D(textureCI);

		eg::FramebufferAttachment colorAttachment(m_texture.handle);
		m_framebuffer = eg::Framebuffer({ &colorAttachment, 1 });

		m_postDescriptorSet = eg::DescriptorSet(postPipeline, 0);
		m_postDescriptorSet.BindTexture(m_texture, 0);
		m_postDescriptorSet.BindSampler(samplers::linearClamp, 1);
		m_postDescriptorSet.BindUniformBuffer(
			frameDataUniformBuffer, 2, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 3
		);
	}
}

void SelectionRenderer::EndDraw()
{
	if (m_hasRendered)
	{
		eg::DC.EndRenderPass();
		m_texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}
}

void SelectionRenderer::Draw(float intensity, const glm::mat4& transform, const eg::Model& model, int meshIndex)
{
	if (!m_hasRendered)
	{
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(0, 0, 0, 0);
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
		rpBeginInfo.framebuffer = m_framebuffer.handle;
		eg::DC.BeginRenderPass(rpBeginInfo);

		eg::DC.BindPipeline(modelPipeline);

		m_hasRendered = true;
	}

	model.BuffersDescriptor().Bind(eg::DC, 0b1);

	const SelectionDrawParams params = {
		.transform = viewProjection * transform,
		.intensity = intensity,
	};
	uint32_t paramsOffset = PushFrameUniformData(RefToCharSpan(params));
	eg::DC.BindDescriptorSet(modelPipelineDS, 0, { &paramsOffset, 1 });

	if (meshIndex != -1)
	{
		const auto& mesh = model.GetMesh(meshIndex);
		eg::DC.DrawIndexed(mesh.firstIndex, mesh.numIndices, mesh.firstVertex, 0, 1);
	}
	else
	{
		for (size_t m = 0; m < model.NumMeshes(); m++)
		{
			const auto& mesh = model.GetMesh(m);
			eg::DC.DrawIndexed(mesh.firstIndex, mesh.numIndices, mesh.firstVertex, 0, 1);
		}
	}
}

void SelectionRenderer::EndFrame()
{
	if (!m_hasRendered)
		return;

	eg::DC.BindPipeline(postPipeline);

	eg::ColorLin color(eg::ColorSRGB::FromHex(0xb9ddf3));

	uint32_t paramsOffset = PushFrameUniformData(ToCharSpan<float>({ &color.r, 3 }));
	eg::DC.BindDescriptorSet(m_postDescriptorSet, 0, { &paramsOffset, 1 });

	eg::DC.Draw(0, 3, 0, 1);
}
