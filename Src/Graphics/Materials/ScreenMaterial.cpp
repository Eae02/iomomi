#include "ScreenMaterial.hpp"

#include "../RenderSettings.hpp"
#include "MeshDrawArgs.hpp"

static eg::Pipeline screenMatPipeline;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Screen.fs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.colorAttachmentFormats[0] = GB_COLOR_FORMAT;
	pipelineCI.colorAttachmentFormats[1] = GB_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.label = "ScreenGame";
	screenMatPipeline = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	screenMatPipeline.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

constexpr eg::Format SCREEN_TEXTURE_FORMAT = eg::Format::R8G8B8A8_sRGB;

ScreenMaterial::ScreenMaterial(int resX, int resY) : m_resX(resX), m_resY(resY), m_descriptorSet(screenMatPipeline, 0)
{
	m_texture = eg::Texture::Create2D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample,
		.mipLevels = 1,
		.width = eg::ToUnsigned(resX),
		.height = eg::ToUnsigned(resY),
		.format = SCREEN_TEXTURE_FORMAT,
	});

	eg::FramebufferAttachment colorAttachment;
	colorAttachment.texture = m_texture.handle;
	m_framebuffer = eg::Framebuffer({ &colorAttachment, 1 });

	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet.BindTexture(m_texture, 1, &framebufferLinearSampler);
}

void ScreenMaterial::RenderTexture(const eg::ColorLin& clearColor)
{
	m_spriteBatch.Reset();

	if (render)
	{
		render(m_spriteBatch);
	}

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_framebuffer.handle;
	rpBeginInfo.colorAttachments[0].clearValue = clearColor;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	m_spriteBatch.UploadAndRender(
		eg::SpriteBatch::RenderArgs{
			.screenWidth = m_resX,
			.screenHeight = m_resY,
			.framebufferFormat = eg::ColorAndDepthFormat(SCREEN_TEXTURE_FORMAT),
		},
		rpBeginInfo
	);

	m_texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

size_t ScreenMaterial::PipelineHash() const
{
	return typeid(ScreenMaterial).hash_code();
}

bool ScreenMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	if (mDrawArgs->drawMode != MeshDrawMode::Game)
		return false;

	cmdCtx.BindPipeline(screenMatPipeline);

	return true;
}

bool ScreenMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	if (mDrawArgs->drawMode != MeshDrawMode::Game)
		return false;

	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);

	return true;
}

eg::IMaterial::VertexInputConfiguration ScreenMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfig_SoaPXNTI;
}
