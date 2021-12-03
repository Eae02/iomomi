#pragma once

enum class RenderTex
{
	GBDepth,
	GBColor1,
	GBColor2,
	WaterGlowIntensity,
	WaterMinDepth,
	WaterMaxDepth,
	WaterDepthBlurred1,
	WaterDepthBlurred2,
	SSAOUnblurred,
	SSAOTempBlur,
	SSAO,
	LitWithoutWater,
	LitWithoutSSR,
	SSRTemp1,
	SSRTemp2,
	SSRDepth,
	BlurredGlassDepth,
	LitWithoutBlurredGlass,
	Lit
};

constexpr size_t NUM_RENDER_TEXTURES = 19;

constexpr eg::Format GB_DEPTH_FORMAT = eg::Format::Depth32;
constexpr eg::Format GB_COLOR_FORMAT = eg::Format::R8G8B8A8_UNorm;
constexpr eg::Format LIGHT_COLOR_FORMAT_LDR = eg::Format::R8G8B8A8_UNorm;
constexpr eg::Format LIGHT_COLOR_FORMAT_HDR = eg::Format::R16G16B16A16_Float;

eg::Format GetFormatForRenderTexture(RenderTex texture);

class RenderTexManager
{
public:
	RenderTexManager() = default;
	
	void BeginFrame(uint32_t resX, uint32_t resY);
	
	void RedirectRenderTexture(RenderTex texture, RenderTex actual);
	
	void RenderTextureUsageHint(RenderTex texture, eg::TextureUsage usage, eg::ShaderAccessFlags accessFlags);
	
	//Prepares a render texture for sampling in a fragment shader
	inline void RenderTextureUsageHintFS(RenderTex texture)
	{
		RenderTextureUsageHint(texture, eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}
	
	eg::TextureRef GetRenderTexture(RenderTex texture) const;
	
	RenderTex ResolveRedirects(RenderTex renderTex) const;
	
	//Gets a framebuffer made up of a set of render textures
	eg::FramebufferHandle GetFramebuffer(
		std::optional<RenderTex> colorTexture1,
		std::optional<RenderTex> colorTexture2,
		std::optional<RenderTex> depthTexture,
		const char* label);
	
	bool printToConsole = false;
	
	uint32_t ResX() const { return m_resX; }
	uint32_t ResY() const { return m_resY; }
	
private:
	uint32_t m_resX = 0;
	uint32_t m_resY = 0;
	
	bool wasHDREnabled = false;
	bool wasWaterHighPrecision = false;
	
	eg::Texture renderTextures[NUM_RENDER_TEXTURES];
	RenderTex renderTexturesRedirect[NUM_RENDER_TEXTURES];
	
	struct FramebufferEntry
	{
		std::optional<RenderTex> colorTexture1;
		std::optional<RenderTex> colorTexture2;
		std::optional<RenderTex> depthTexture;
		const char* label;
		eg::Framebuffer framebuffer;
	};
	std::vector<FramebufferEntry> framebuffers;
	
	void InitFramebufferEntry(RenderTexManager::FramebufferEntry& entry);
};
