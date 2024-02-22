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
	SSAOGBDepthLinear,
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
	Lit,
	GravityGunDepth,
};

constexpr eg::Format GB_DEPTH_FORMAT = eg::Format::Depth16;
constexpr eg::Format GB_COLOR_FORMAT = eg::Format::R8G8B8A8_UNorm;

extern eg::Format lightColorAttachmentFormat;

void SetLightColorAttachmentFormat(bool enableHDR);

eg::Format GetFormatForRenderTexture(RenderTex texture, bool requireFixed = false);

void AssertRenderTextureFormatSupport();

class RenderTexManager
{
public:
	RenderTexManager();

	void BeginFrame(uint32_t resX, uint32_t resY);

	void RedirectRenderTexture(RenderTex texture, RenderTex actual);

	void RenderTextureUsageHint(RenderTex texture, eg::TextureUsage usage, eg::ShaderAccessFlags accessFlags);

	// Prepares a render texture for sampling in a fragment shader
	inline void RenderTextureUsageHintFS(RenderTex texture)
	{
		RenderTextureUsageHint(texture, eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}

	eg::TextureRef GetRenderTexture(RenderTex texture) const;

	RenderTex ResolveRedirects(RenderTex renderTex) const;

	// Gets a framebuffer made up of a set of render textures
	eg::FramebufferHandle GetFramebuffer(
		std::optional<RenderTex> colorTexture1, std::optional<RenderTex> colorTexture2,
		std::optional<RenderTex> depthTexture, const char* label);

	bool printToConsole = false;

	uint32_t ResX() const { return m_resX; }
	uint32_t ResY() const { return m_resY; }
	uint32_t Generation() const { return m_generation; }

private:
	uint32_t m_resX = 0;
	uint32_t m_resY = 0;

	uint32_t m_generation = 1;

	bool wasWaterHighPrecision = false;

	std::vector<eg::Texture> renderTextures;
	std::vector<RenderTex> renderTexturesRedirect;

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

class DescriptorSetRenderTexBinding
{
public:
	DescriptorSetRenderTexBinding() = default;

	void Update(
		eg::DescriptorSet& descriptorSet, uint32_t binding, const RenderTexManager& renderTexManager, RenderTex texture,
		const eg::Sampler* sampler = nullptr);

private:
	uint32_t m_generation = 0;
	RenderTex m_redirectedRenderTex;
};
