#include "ScreenMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline screenMatPipeline;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Screen.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.numColorAttachments = 2;
	pipelineCI.label = "ScreenGame";
	screenMatPipeline = eg::Pipeline::Create(pipelineCI);
	screenMatPipeline.FramebufferFormatHint(DeferredRenderer::GEOMETRY_FB_FORMAT);
}

static void OnShutdown()
{
	screenMatPipeline.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

ScreenMaterial::ScreenMaterial(int resX, int resY)
	: m_resX(resX), m_resY(resY), m_descriptorSet(screenMatPipeline, 0)
{
	eg::SamplerDescription samplerDesc;
	samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	
	eg::TextureCreateInfo texCreateInfo;
	texCreateInfo.mipLevels = 1;
	texCreateInfo.width = resX;
	texCreateInfo.height = resY;
	texCreateInfo.format = eg::Format::R8G8B8A8_sRGB;
	texCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	texCreateInfo.defaultSamplerDescription = &samplerDesc;
	m_texture = eg::Texture::Create2D(texCreateInfo);
	
	eg::FramebufferAttachment colorAttachment;
	colorAttachment.texture = m_texture.handle;
	m_framebuffer = eg::Framebuffer({ &colorAttachment, 1 });
	
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	m_descriptorSet.BindTexture(m_texture, 1);
}

void ScreenMaterial::RenderTexture(const eg::ColorLin& clearColor)
{
	m_spriteBatch.Begin();
	
	if (render)
	{
		render(m_spriteBatch);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_framebuffer.handle;
	rpBeginInfo.colorAttachments[0].clearValue = clearColor;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	m_spriteBatch.End(m_resX, m_resY, rpBeginInfo);
	
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
