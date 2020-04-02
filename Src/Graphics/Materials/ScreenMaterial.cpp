#include "ScreenMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../DeferredRenderer.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline screenMatPipeline;

static const eg::StencilState stencilState = DeferredRenderer::MakeStencilState(0);

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Screen.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(ScreenMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(ScreenMaterial::InstanceData, transform) + 0 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, offsetof(ScreenMaterial::InstanceData, transform) + 16 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, offsetof(ScreenMaterial::InstanceData, transform) + 32 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 2, offsetof(ScreenMaterial::InstanceData, textureScale) };
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.backStencilState = stencilState;
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
