#include "RenderTex.hpp"
#include "WaterRenderer.hpp"
#include "../Settings.hpp"

#include <magic_enum.hpp>

static_assert(magic_enum::enum_count<RenderTex>() == NUM_RENDER_TEXTURES);

static inline eg::TextureFlags GetFlagsForRenderTexture(RenderTex texture)
{
	switch (texture)
	{
	case RenderTex::Lit:
		return eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::CopySrc;
	case RenderTex::LitWithoutBlurredGlass:
		return eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;
	default: return eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
	}
}

eg::Format GetFormatForRenderTexture(RenderTex texture)
{
	switch (texture)
	{
	case RenderTex::GBDepth:            return GB_DEPTH_FORMAT;
	case RenderTex::GBColor1:           return GB_COLOR_FORMAT;
	case RenderTex::GBColor2:           return GB_COLOR_FORMAT;
	case RenderTex::Flags:              return eg::Format::R8_UInt;
	case RenderTex::WaterGlowIntensity: return eg::Format::R8_UNorm;
	case RenderTex::WaterMinDepth:      return eg::Format::Depth16;
	case RenderTex::WaterMaxDepth:      return eg::Format::Depth16;
	case RenderTex::BlurredGlassDepth:  return GB_DEPTH_FORMAT;
	
	case RenderTex::WaterDepthBlurred1:
	case RenderTex::WaterDepthBlurred2:
		if (settings.waterQuality >= WaterRenderer::HighPrecisionMinQL)
			return eg::Format::R32G32_Float;
		return eg::Format::R16G16_Float;
		
	case RenderTex::SSRTemp1:
	case RenderTex::SSRTemp2:
		return eg::Format::R16G16B16A16_Float;
		
	case RenderTex::LitWithoutWater:
	case RenderTex::LitWithoutBlurredGlass:
	case RenderTex::LitWithoutSSR:
	case RenderTex::Lit:
		return settings.HDREnabled() ? LIGHT_COLOR_FORMAT_HDR : LIGHT_COLOR_FORMAT_LDR;
		
	default:
		EG_UNREACHABLE;
	}
}

//Initializes the framebuffer of a framebuffer entry given that all other attributes are set
void RenderTexManager::InitFramebufferEntry(RenderTexManager::FramebufferEntry& entry)
{
	size_t numColorAttachments = 0;
	eg::FramebufferAttachment colorAttachments[2];
	
	if (entry.colorTexture1.has_value())
	{
		colorAttachments[numColorAttachments++].texture = renderTextures[(int)*entry.colorTexture1].handle;
	}
	if (entry.colorTexture2.has_value())
	{
		colorAttachments[numColorAttachments++].texture = renderTextures[(int)*entry.colorTexture2].handle;
	}
	
	eg::FramebufferCreateInfo framebufferCI = { };
	framebufferCI.label = entry.label;
	framebufferCI.colorAttachments = eg::Span<eg::FramebufferAttachment>(colorAttachments, numColorAttachments);
	if (entry.depthTexture.has_value())
	{
		framebufferCI.depthStencilAttachment = renderTextures[(int)*entry.depthTexture].handle;
	}
	
	entry.framebuffer = eg::Framebuffer(framebufferCI);
}

void RenderTexManager::BeginFrame(uint32_t resX, uint32_t resY)
{
	for (size_t i = 0; i < NUM_RENDER_TEXTURES; i++)
	{
		renderTexturesRedirect[i] = (RenderTex)i;
	}
	
	const bool waterHighPrecision = settings.waterQuality >= WaterRenderer::HighPrecisionMinQL;
	
	if (resX != m_resX || resY != m_resY || settings.HDREnabled() != wasHDREnabled || waterHighPrecision != wasWaterHighPrecision)
	{
		m_resX = resX;
		m_resY = resY;
		wasHDREnabled = settings.HDREnabled();
		wasWaterHighPrecision = waterHighPrecision;
		
		for (eg::Texture& tex : renderTextures)
			tex.Destroy();
		for (FramebufferEntry& entry : framebuffers)
			entry.framebuffer.Destroy();
		
		eg::SamplerDescription samplerDesc;
		samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
		samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
		samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
		samplerDesc.minFilter = eg::TextureFilter::Nearest;
		samplerDesc.magFilter = eg::TextureFilter::Nearest;
		
		for (size_t i = 0; i < NUM_RENDER_TEXTURES; i++)
		{
			std::string label = eg::Concat({"RenderTex::", magic_enum::enum_name((RenderTex)i)});
			
			eg::TextureCreateInfo textureCI;
			textureCI.format = GetFormatForRenderTexture((RenderTex)i);
			textureCI.width = resX;
			textureCI.height = resY;
			textureCI.mipLevels = 1;
			textureCI.flags = GetFlagsForRenderTexture((RenderTex)i);
			textureCI.defaultSamplerDescription = &samplerDesc;
			textureCI.label = label.c_str();
			
			renderTextures[i] = eg::Texture::Create2D(textureCI);
		}
		
		for (FramebufferEntry& entry : framebuffers)
			InitFramebufferEntry(entry);
	}
}

RenderTex RenderTexManager::ResolveRedirects(RenderTex renderTex) const
{
	while (renderTexturesRedirect[(int)renderTex] != renderTex)
	{
		renderTex = renderTexturesRedirect[(int)renderTex];
	}
	return renderTex;
}


void RenderTexManager::RedirectRenderTexture(RenderTex texture, RenderTex actual)
{
	renderTexturesRedirect[(int)ResolveRedirects(texture)] = actual;
}

void RenderTexManager::RenderTextureUsageHint(RenderTex texture, eg::TextureUsage usage, eg::ShaderAccessFlags accessFlags)
{
	renderTextures[(int)ResolveRedirects(texture)].UsageHint(usage, accessFlags);
}

eg::TextureRef RenderTexManager::GetRenderTexture(RenderTex texture) const
{
	return renderTextures[(int)ResolveRedirects(texture)];
}

eg::FramebufferHandle RenderTexManager::GetFramebuffer(
	std::optional<RenderTex> colorTexture1,
	std::optional<RenderTex> colorTexture2,
	std::optional<RenderTex> depthTexture,
	const char* label)
{
	if (colorTexture1.has_value())
		colorTexture1 = ResolveRedirects(*colorTexture1);
	if (colorTexture2.has_value())
		colorTexture2 = ResolveRedirects(*colorTexture2);
	if (depthTexture.has_value())
		depthTexture = ResolveRedirects(*depthTexture);
	
	for (const FramebufferEntry& entry : framebuffers)
	{
		if (entry.colorTexture1 == colorTexture1 && entry.colorTexture2 == colorTexture2 &&
			entry.depthTexture == depthTexture)
		{
			return entry.framebuffer.handle;
		}
	}
	
	if (printToConsole)
	{
		auto GetNameForOptRenderTexture = [](std::optional<RenderTex> texture) -> std::string_view
		{
			return texture.has_value() ? magic_enum::enum_name(*texture) : "-";
		};
		eg::Log(eg::LogLevel::Info, "fb", "Added framebuffer ({0} {1} {2})",
		        GetNameForOptRenderTexture(colorTexture1),
		        GetNameForOptRenderTexture(colorTexture2),
		        GetNameForOptRenderTexture(depthTexture));
	}
	
	EG_ASSERT(framebuffers.size() < 100);
	
	FramebufferEntry& newEntry = framebuffers.emplace_back();
	newEntry.colorTexture1 = colorTexture1;
	newEntry.colorTexture2 = colorTexture2;
	newEntry.depthTexture = depthTexture;
	newEntry.label = label;
	InitFramebufferEntry(newEntry);
	
	return newEntry.framebuffer.handle;
}
