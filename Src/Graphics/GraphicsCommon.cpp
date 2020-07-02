#include "GraphicsCommon.hpp"
#include "WaterRenderer.hpp"
#include "../Settings.hpp"

#include <magic_enum.hpp>

const glm::ivec3 cubeMesh::vertices[8] =
{
	{ -1, -1, -1 },
	{ -1,  1, -1 },
	{ -1, -1,  1 },
	{ -1,  1,  1 },
	{  1, -1, -1 },
	{  1,  1, -1 },
	{  1, -1,  1 },
	{  1,  1,  1 },
};

const uint32_t cubeMesh::faces[6][4] =
{
	{ 0, 1, 2, 3 },
	{ 4, 5, 6, 7 },
	{ 0, 2, 4, 6 },
	{ 1, 3, 5, 7 },
	{ 0, 1, 4, 5 },
	{ 2, 3, 6, 7 }
};

const std::pair<uint32_t, uint32_t> cubeMesh::edges[12] =
{
	{ 0, 4 },
	{ 1, 5 },
	{ 2, 6 },
	{ 3, 7 },
	{ 0, 1 },
	{ 0, 2 },
	{ 1, 3 },
	{ 2, 3 },
	{ 4, 5 },
	{ 4, 6 },
	{ 5, 7 },
	{ 6, 7 }
};

static eg::Sampler commonTextureSampler;

static void OnInit()
{
	eg::SamplerDescription samplerDescription;
	samplerDescription.maxAnistropy = 16;
	commonTextureSampler = eg::Sampler(samplerDescription);
}

static void OnShutdown()
{
	commonTextureSampler = { };
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

const eg::Sampler& GetCommonTextureSampler()
{
	return commonTextureSampler;
}

ResizableBuffer::ResizableBuffer()
{
	m_capacity = 0;
}

void ResizableBuffer::EnsureSize(uint32_t elements, uint32_t elemSize)
{
	uint64_t bytes = eg::RoundToNextMultiple((uint64_t)std::max(elements, 1U), 1024) * elemSize;
	if (bytes > m_capacity)
	{
		buffer = eg::Buffer(flags, bytes, nullptr);
		m_capacity = bytes;
	}
}

const int32_t WATER_MODE_BEFORE = 0;
const int32_t WATER_MODE_AFTER = 1;

eg::Format GetFormatForRenderTexture(RenderTex texture)
{
	switch (texture)
	{
	case RenderTex::GBDepth:       return GB_DEPTH_FORMAT;
	case RenderTex::GBColor1:      return GB_COLOR_FORMAT;
	case RenderTex::GBColor2:      return GB_COLOR_FORMAT;
	case RenderTex::Flags:         return eg::Format::R8_UInt;
	case RenderTex::WaterMinDepth: return eg::Format::Depth32;
	case RenderTex::WaterMaxDepth: return eg::Format::Depth32;
	
	case RenderTex::WaterTravelDepth:
		if (settings.waterQuality >= WaterRenderer::HighPrecisionMinQL)
			return eg::Format::R32_Float;
		return eg::Format::R16_Float;
		
	case RenderTex::WaterDepthBlurred1:
	case RenderTex::WaterDepthBlurred2:
		if (settings.waterQuality >= WaterRenderer::HighPrecisionMinQL)
			return eg::Format::R32G32B32A32_Float;
		return eg::Format::R16G16B16A16_Float;
		
	case RenderTex::LitWithoutWater:
	case RenderTex::LitWithoutSSR:
	case RenderTex::Lit:
		return settings.HDREnabled() ? LIGHT_COLOR_FORMAT_HDR : LIGHT_COLOR_FORMAT_LDR;
		
	default:
		EG_UNREACHABLE;
	}
}

static constexpr int NUM_RENDER_TEXTURES = magic_enum::enum_count<RenderTex>();

static eg::Texture renderTextures[NUM_RENDER_TEXTURES];

static RenderTex renderTexturesRedirect[NUM_RENDER_TEXTURES];

struct FramebufferEntry
{
	std::optional<RenderTex> colorTexture1;
	std::optional<RenderTex> colorTexture2;
	std::optional<RenderTex> depthTexture;
	const char* label;
	eg::Framebuffer framebuffer;
};
static std::vector<FramebufferEntry> framebuffers;

static void DestroyRenderTextures()
{
	for (eg::Texture& tex : renderTextures)
		tex.Destroy();
	for (FramebufferEntry& entry : framebuffers)
		entry.framebuffer.Destroy();
}

EG_ON_SHUTDOWN(DestroyRenderTextures)

//Initializes the framebuffer of a framebuffer entry given that all other attributes are set
static inline void InitFramebufferEntry(FramebufferEntry& entry)
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

static int oldScreenResX = -1;
static int oldScreenResY = -1;
static bool wasHDREnabled = false;
static bool wasWaterHighPrecision = false;

void MaybeRecreateRenderTextures()
{
	for (int i = 0; i < NUM_RENDER_TEXTURES; i++)
	{
		renderTexturesRedirect[i] = (RenderTex)i;
	}
	
	const bool waterHighPrecision = settings.waterQuality >= WaterRenderer::HighPrecisionMinQL;
	
	if (eg::CurrentResolutionX() != oldScreenResX ||
		eg::CurrentResolutionY() != oldScreenResY ||
		settings.HDREnabled() != wasHDREnabled ||
		waterHighPrecision != wasWaterHighPrecision)
	{
		oldScreenResX = eg::CurrentResolutionX();
		oldScreenResY = eg::CurrentResolutionY();
		wasHDREnabled = settings.HDREnabled();
		wasWaterHighPrecision = waterHighPrecision;
		
		DestroyRenderTextures();
		
		eg::SamplerDescription samplerDesc;
		samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
		samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
		samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
		samplerDesc.minFilter = eg::TextureFilter::Linear;
		samplerDesc.magFilter = eg::TextureFilter::Linear;
		
		for (int i = 0; i < NUM_RENDER_TEXTURES; i++)
		{
			std::string label = eg::Concat({"RenderTex::", magic_enum::enum_name((RenderTex)i)});
			
			eg::TextureCreateInfo textureCI;
			textureCI.format = GetFormatForRenderTexture((RenderTex)i);
			textureCI.width = eg::CurrentResolutionX();
			textureCI.height = eg::CurrentResolutionY();
			textureCI.mipLevels = 1;
			textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
			textureCI.defaultSamplerDescription = &samplerDesc;
			textureCI.label = label.c_str();
			
			renderTextures[i] = eg::Texture::Create2D(textureCI);
		}
		
		for (FramebufferEntry& entry : framebuffers)
			InitFramebufferEntry(entry);
	}
}

static inline RenderTex ResolveRedirects(RenderTex renderTex)
{
	while (renderTexturesRedirect[(int)renderTex] != renderTex)
	{
		renderTex = renderTexturesRedirect[(int)renderTex];
	}
	return renderTex;
}

void RedirectRenderTexture(RenderTex texture, RenderTex actual)
{
	renderTexturesRedirect[(int)ResolveRedirects(texture)] = actual;
}

void RenderTextureUsageHint(RenderTex texture, eg::TextureUsage usage, eg::ShaderAccessFlags accessFlags)
{
	renderTextures[(int)ResolveRedirects(texture)].UsageHint(usage, accessFlags);
}

eg::TextureRef GetRenderTexture(RenderTex texture)
{
	return renderTextures[(int)ResolveRedirects(texture)];
}

eg::FramebufferHandle GetFramebuffer(
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
	
#ifndef NDEBUG
	auto GetNameForOptRenderTexture = [] (std::optional<RenderTex> texture) -> std::string_view
	{
		return texture.has_value() ? magic_enum::enum_name(*texture) : "-";
	};
	eg::Log(eg::LogLevel::Info, "fb", "Added framebuffer ({0} {1} {2})",
		GetNameForOptRenderTexture(colorTexture1),
		GetNameForOptRenderTexture(colorTexture2),
		GetNameForOptRenderTexture(depthTexture));
#endif
	
	EG_ASSERT(framebuffers.size() < 100);
	
	FramebufferEntry& newEntry = framebuffers.emplace_back();
	newEntry.colorTexture1 = colorTexture1;
	newEntry.colorTexture2 = colorTexture2;
	newEntry.depthTexture = depthTexture;
	newEntry.label = label;
	InitFramebufferEntry(newEntry);
	
	return newEntry.framebuffer.handle;
}
