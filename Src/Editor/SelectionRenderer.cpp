#include "SelectionRenderer.hpp"
#include "../Graphics/GraphicsCommon.hpp"
#include "EditorGraphics.hpp"

static eg::Pipeline modelPipeline;
static eg::Pipeline postPipeline;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo modelPipelineCI;
	modelPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EdSelection.vs.glsl").DefaultVariant();
	modelPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EdSelection.fs.glsl").DefaultVariant();
	modelPipelineCI.cullMode = eg::CullMode::None;
	modelPipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	modelPipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	modelPipelineCI.numColorAttachments = 1;
	modelPipelineCI.colorAttachmentFormats[0] = eg::Format::R8_UNorm;
	modelPipelineCI.label = "EdSelection";
	modelPipeline = eg::Pipeline::Create(modelPipelineCI);

	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	postPipelineCI.fragmentShader =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/EdSelectionPost.fs.glsl").DefaultVariant();
	postPipelineCI.cullMode = eg::CullMode::None;
	postPipelineCI.numColorAttachments = 1;
	postPipelineCI.label = "EdSelectionPost";
	postPipelineCI.blendStates[0] = eg::AlphaBlend;
	postPipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	postPipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	postPipeline = eg::Pipeline::Create(postPipelineCI);
}

static void OnShutdown()
{
	modelPipeline.Destroy();
	postPipeline.Destroy();
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

	model.Bind();

	struct PC
	{
		glm::mat4 transform;
		float intensity;
	} pc;
	pc.transform = viewProjection * transform;
	pc.intensity = intensity;

	eg::DC.PushConstants(0, pc);

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

	eg::ColorLin color(eg::ColorSRGB::FromHex(0xb9ddf3));

	eg::DC.BindPipeline(postPipeline);
	eg::DC.BindTexture(m_texture, 0, 0, &linearClampToEdgeSampler);
	eg::DC.PushConstants(0, sizeof(float) * 3, &color);
	eg::DC.Draw(0, 3, 0, 1);
}
