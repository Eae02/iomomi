#include "Renderer.hpp"
#include "Lighting/LightMeshes.hpp"
#include "RenderSettings.hpp"

Renderer::Renderer()
{
	eg::ShaderProgram postProgram;
	postProgram.AddStageFromAsset("Shaders/Post.vs.glsl");
	postProgram.AddStageFromAsset("Shaders/Post.fs.glsl");
	
	eg::FixedFuncState postFFState;
	postFFState.depthFormat = eg::Format::DefaultDepthStencil;
	postFFState.attachments[0].format = eg::Format::DefaultColor;
	m_postPipeline = postProgram.CreatePipeline(postFFState);
	
	eg::ShaderProgram ambientProgram;
	ambientProgram.AddStageFromAsset("Shaders/Post.vs.glsl");
	ambientProgram.AddStageFromAsset("Shaders/DeferredBase.fs.glsl");
	
	eg::FixedFuncState ambientFFState;
	ambientFFState.attachments[0].format = LIGHT_COLOR_FORMAT;
	m_ambientPipeline = ambientProgram.CreatePipeline(ambientFFState);
	
	eg::ShaderProgram spotLightProgram;
	spotLightProgram.AddStageFromAsset("Shaders/SpotLight.vs.glsl");
	spotLightProgram.AddStageFromAsset("Shaders/SpotLight.fs.glsl");
	
	eg::FixedFuncState spotLightFFState;
	spotLightFFState.attachments[0].format = LIGHT_COLOR_FORMAT;
	spotLightFFState.attachments[0].blend = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::One);
	spotLightFFState.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	spotLightFFState.vertexBindings[0] = { sizeof(float) * 3, eg::InputRate::Vertex };
	spotLightFFState.cullMode = eg::CullMode::Front;
	m_spotLightPipeline = spotLightProgram.CreatePipeline(spotLightFFState);
	
	eg::SamplerDescription attachmentSamplerDesc;
	attachmentSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.minFilter = eg::TextureFilter::Nearest;
	attachmentSamplerDesc.magFilter = eg::TextureFilter::Nearest;
	attachmentSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
	m_attachmentSampler = eg::Sampler(attachmentSamplerDesc);
}

void Renderer::BeginGeometry()
{
	if (m_resX != eg::CurrentResolutionX() || m_resY != eg::CurrentResolutionY())
	{
		m_resX = eg::CurrentResolutionX();
		m_resY = eg::CurrentResolutionY();
		
		eg::Texture2DCreateInfo colorTexCreateInfo;
		colorTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		colorTexCreateInfo.width = (uint32_t)m_resX;
		colorTexCreateInfo.height = (uint32_t)m_resY;
		colorTexCreateInfo.mipLevels = 1;
		colorTexCreateInfo.format = eg::Format::R8G8B8A8_UNorm;
		m_gbColor1Texture = eg::Texture::Create2D(colorTexCreateInfo);
		m_gbColor2Texture = eg::Texture::Create2D(colorTexCreateInfo);
		
		eg::Texture2DCreateInfo depthTexCreateInfo;
		depthTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		depthTexCreateInfo.width = (uint32_t)m_resX;
		depthTexCreateInfo.height = (uint32_t)m_resY;
		depthTexCreateInfo.mipLevels = 1;
		depthTexCreateInfo.format = DEPTH_FORMAT;
		m_gbDepthTexture = eg::Texture::Create2D(depthTexCreateInfo);
		
		const eg::TextureRef gbTextures[] = { m_gbColor1Texture, m_gbColor2Texture };
		m_gbFramebuffer = eg::Framebuffer(gbTextures, m_gbDepthTexture);
		
		eg::Texture2DCreateInfo lightOutTexCreateInfo;
		lightOutTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		lightOutTexCreateInfo.width = (uint32_t)m_resX;
		lightOutTexCreateInfo.height = (uint32_t)m_resY;
		lightOutTexCreateInfo.mipLevels = 1;
		lightOutTexCreateInfo.format = LIGHT_COLOR_FORMAT;
		m_lightOutTexture = eg::Texture::Create2D(lightOutTexCreateInfo);
		
		m_lightOutFramebuffer = eg::Framebuffer({ &m_lightOutTexture, 1 });
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_gbFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void Renderer::BeginLighting()
{
	eg::DC.EndRenderPass();
	
	m_gbColor1Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	m_gbColor2Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	m_gbDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_lightOutFramebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_ambientPipeline);
	
	eg::DC.BindTexture(m_gbColor1Texture, 0, &m_attachmentSampler);
	
	auto ambientColor = eg::ColorLin(eg::ColorSRGB::FromHex(0xf6f9fc)).ScaleRGB(0.3f);
	eg::DC.PushConstants(0, sizeof(float) * 3, &ambientColor.r);
	
	eg::DC.Draw(0, 3, 0, 1);
}

void Renderer::End()
{
	eg::DC.EndRenderPass();
	
	m_lightOutTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_postPipeline);
	
	eg::DC.BindTexture(m_lightOutTexture, 0, &m_attachmentSampler);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	
	const float CROSSHAIR_SIZE = 32;
	const float CROSSHAIR_OPACITY = 0.75f;
	eg::SpriteBatch::overlay.Draw(eg::GetAsset<eg::Texture>("Textures/Crosshair.png"),
	    eg::Rectangle::CreateCentered(eg::CurrentResolutionX() / 2.0f, eg::CurrentResolutionY() / 2.0f, CROSSHAIR_SIZE, CROSSHAIR_SIZE),
	    eg::ColorLin(eg::Color::White.ScaleAlpha(CROSSHAIR_OPACITY)), eg::FlipFlags::Normal);
}

void Renderer::DrawSpotLights(const std::vector<SpotLightDrawData>& spotLights)
{
	if (spotLights.empty())
		return;
	
	eg::DC.BindPipeline(m_spotLightPipeline);
	
	BindSpotLightMesh();
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	eg::DC.BindTexture(m_gbColor1Texture, 1, &m_attachmentSampler);
	eg::DC.BindTexture(m_gbColor2Texture, 2, &m_attachmentSampler);
	eg::DC.BindTexture(m_gbDepthTexture, 3, &m_attachmentSampler);
	
	for (const SpotLightDrawData& spotLight : spotLights)
	{
		eg::DC.PushConstants(0, spotLight);
		
		eg::DC.DrawIndexed(0, SPOT_LIGHT_MESH_INDICES, 0, 0, 1);
	}
}
